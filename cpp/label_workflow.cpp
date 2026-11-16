#include "qupathxx/label_workflow.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "label_analysis_internal.hpp"
#include "reader_file_guard.hpp"
#include "reader_plugin_link.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "geometry_archive.hpp"
#include "export_output.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <future>
#include <iomanip>
#include <locale>
#include <sstream>

namespace qpx {
namespace {
using Json = nlohmann::json;
using Value = ImageDataValue;
constexpr const char* records_key = "qpx.label-analysis/1";
constexpr const char* run_key = "qpx.label-analysis.run/1";
constexpr const char* geometry_key = "qpx.label-analysis.geometry/1";
constexpr const char* counts_key = "qpx.subcellular-counts.run/1";
constexpr const char *facts_key = "qpx.label-analysis.facts/1",
                     *counts_facts_key = "qpx.subcellular-counts.facts/1";
constexpr const char* counts_properties_key = "qpx.subcellular-counts.properties/1";
constexpr const char *spot_count = "Subcellular spot count",
                     *spot_pixels = "Subcellular spot pixels";
std::string decimal(std::uint64_t n) {
    char b[32];
    const auto r = std::to_chars(b, b + 32, n);
    return {b, r.ptr};
}
bool digest(const std::string& value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::uint64_t unsigned_value(const Json& j, std::uint64_t maximum = UINT64_MAX) {
    std::uint64_t n;
    if (j.is_number_unsigned())
        n = j.get<std::uint64_t>();
    else if (j.is_number_integer() && j.get<std::int64_t>() >= 0)
        n = std::uint64_t(j.get<std::int64_t>());
    else
        throw std::invalid_argument("Invalid unsigned label recipe integer");
    if (n > maximum)
        throw std::invalid_argument("Label recipe integer exceeds limit");
    return n;
}
double real_value(const Json& j) {
    if (!j.is_number())
        throw std::invalid_argument("Invalid label recipe number");
    const auto v = j.get<double>();
    if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite label recipe number");
    return v;
}
std::uint64_t id_value(const Value& v) {
    const auto* s = std::get_if<std::string>(&v.value);
    if (!s || s->empty())
        throw std::invalid_argument("Invalid label record object ID");
    std::uint64_t n = 0;
    const auto r = std::from_chars(s->data(), s->data() + s->size(), n);
    if (r.ec != std::errc{} || r.ptr != s->data() + s->size() || !n || decimal(n) != *s)
        throw std::invalid_argument("Noncanonical label record object ID");
    return n;
}
const Value::Map& map_value(const Value& v) {
    const auto* p = std::get_if<Value::Map>(&v.value);
    if (!p)
        throw std::invalid_argument("Invalid label record map");
    return *p;
}
const std::string& text_value(const Value& v) {
    const auto* p = std::get_if<std::string>(&v.value);
    if (!p)
        throw std::invalid_argument("Invalid label record text");
    return *p;
}
Value::Map records(const Document& doc) {
    const auto p = doc.image_data_properties();
    if (!p)
        return {};
    const auto found = p->values().find(records_key);
    return found == p->values().end() ? Value::Map{} : map_value(found->second);
}
void keys(const Json& j, std::initializer_list<const char*> expected) {
    if (!j.is_object() || j.size() != expected.size())
        throw std::invalid_argument("Unsupported label recipe members");
    for (const auto* k : expected)
        if (!j.contains(k))
            throw std::invalid_argument("Missing label recipe member");
}
Json parse(const std::string& text) {
    if (text.size() > 4096)
        throw std::length_error("Label recipe text exceeds limit");
    detail::StrictJson sax("label recipe");
    Json::sax_parse(text, &sax);
    return Json::parse(text);
}
bool same_metadata(const ImageMetadata& a, const ImageMetadata& b) {
    if (a.width != b.width || a.height != b.height || a.vendor != b.vendor ||
        a.pixel_type != b.pixel_type || a.layout != b.layout ||
        a.pixel_width_um != b.pixel_width_um || a.pixel_height_um != b.pixel_height_um ||
        a.levels.size() != b.levels.size())
        return false;
    for (std::size_t i = 0; i < a.levels.size(); ++i)
        if (a.levels[i].width != b.levels[i].width || a.levels[i].height != b.levels[i].height ||
            a.levels[i].downsample != b.levels[i].downsample)
            return false;
    return true;
}
RegionRequest crop(const Document& doc, std::uint64_t area) {
    doc.require_object_display_plane(area);
    const auto a = doc.annotation(area);
    if (!a.geometry.is_area())
        throw std::invalid_argument("Label analysis requires an area ROI");
    const auto b = a.geometry.bounds();
    const auto& m = doc.metadata();
    if (m.width < 1 || m.height < 1 || m.width > ScientificGridAxis::coordinate_limit ||
        m.height > ScientificGridAxis::coordinate_limit)
        throw std::invalid_argument("Label image dimensions exceed the scientific-grid contract");
    if (b.x < 0 || b.y < 0 || b.width > double(m.width) - b.x || b.height > double(m.height) - b.y)
        throw std::invalid_argument("Label ROI must lie inside the image");
    // Conservative pixel bounds: the native predicate decides whether boundary
    // centers participate (rectangles are half-open; polygon coverage differs).
    const auto x = std::int64_t(std::floor(b.x)), y = std::int64_t(std::floor(b.y));
    const auto right = std::int64_t(std::ceil(b.x + b.width)),
               bottom = std::int64_t(std::ceil(b.y + b.height));
    detail::label_pixels(right - x, bottom - y);
    return {x, y, 0, right - x, bottom - y};
}
void source_current(const ImageServer& source, const ImageMetadata& metadata,
                    const std::shared_ptr<const ReaderSource>& observed,
                    const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    if (source.closed() || !same_metadata(source.metadata(), metadata))
        throw WorkflowInputChanged("Label image source/metadata changed");
    const auto current = source.reader_source();
    if (!observed || !current)
        throw WorkflowInputChanged("Label analysis requires native source identity");
    if (observed->primary) {
        if (!detail::same_reader_files(*observed, *current))
            throw WorkflowInputChanged("Label source file set changed");
        auto actual = *observed;
        std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
        guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(actual.primary->path));
        actual.primary = guards.back()->version();
        for (auto& file : actual.companions) {
            workflow_checkpoint(cancelled);
            guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(file.path));
            file = *guards.back()->version();
        }
        for (const auto& g : guards)
            g->verify();
        if (!detail::same_reader_files(actual, *observed))
            throw WorkflowInputChanged("Label source bytes/version changed");
    } else if (current->generation != observed->generation || current->reader != observed->reader ||
               current->locator != observed->locator || current->scope != observed->scope)
        throw WorkflowInputChanged("Label source generation changed");
    source.require_raw_current(cancelled); // directory/chunk and plugin-specific authority
}
std::string geometry_hash(const Roi& r) {
    const auto bytes = GeometryArchiveCodec::encode(r);
    Sha256 h;
    h.update(bytes.data(), bytes.size());
    return h.finish();
}
std::string publication_facts(const Roi& roi, std::uint64_t parent, ObjectPlane plane,
                              const std::string& classification,
                              const std::map<std::string, std::string>& metadata,
                              const std::map<std::string, double>& measurements) {
    Sha256 h;
    detail::label_hash_text(h, "qpx.label-publication-facts/1");
    detail::label_hash_text(h, geometry_hash(roi));
    h.integer(parent);
    h.integer(std::uint64_t(plane.c));
    h.integer(plane.z);
    h.integer(plane.t);
    detail::label_hash_text(h, classification);
    for (const auto& item : metadata)
        if (item.first != facts_key) {
            detail::label_hash_text(h, item.first);
            detail::label_hash_text(h, item.second);
        }
    for (const auto& item : measurements) {
        detail::label_hash_text(h, item.first);
        detail::label_hash_real(h, item.second);
    }
    return h.finish();
}
std::string count_facts(const std::string& run, std::uint64_t parent,
                        const std::string& previous_properties,
                        const std::map<std::string, double>& measurements) {
    Sha256 h;
    detail::label_hash_text(h, "qpx.subcellular-count-facts/1");
    detail::label_hash_text(h, run);
    h.integer(parent);
    detail::label_hash_text(h, previous_properties);
    detail::label_hash_real(h, measurements.at(spot_count));
    detail::label_hash_real(h, measurements.at(spot_pixels));
    return h.finish();
}
void observed_current(const ReaderSource& observed, const std::atomic<bool>* cancelled) {
    if (!observed.primary)
        throw WorkflowInputChanged("External label file observation is missing");
    auto actual = observed;
    std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
    guards.push_back(std::make_unique<detail::ReaderFileGuard>(observed.primary->path));
    actual.primary = guards.back()->version();
    for (auto& file : actual.companions) {
        workflow_checkpoint(cancelled);
        guards.push_back(std::make_unique<detail::ReaderFileGuard>(file.path));
        file = *guards.back()->version();
    }
    for (const auto& guard : guards)
        guard->verify();
    if (!detail::same_reader_files(actual, observed))
        throw WorkflowInputChanged("External label source changed");
}
} // namespace
struct LabelAnalysisRequest::Data {
    struct Parent {
        std::uint64_t id, revision;
        Roi geometry;
        ObjectPlane plane;
    };
    std::thread::id owner = std::this_thread::get_id();
    std::shared_ptr<const char> capture = std::make_shared<const char>(), document;
    std::shared_ptr<ImageServer> source;
    ImageMetadata metadata;
    std::shared_ptr<const ReaderSource> observed;
    std::shared_ptr<const AnalysisProvenance> provenance;
    std::uint64_t area, revision;
    Roi roi;
    ObjectPlane plane;
    RegionRequest region;
    ScientificGrid grid;
    LabelAnalysisLimits limits;
    LabelAnalysisOptions options;
    std::vector<Parent> parents;
    std::optional<RawPixelSourceSnapshot> quantitative;
    RawRegionRequest raw;
    std::size_t fixed = 0;
    std::string expected, expected_input, expected_raster;
    Data(const Document& doc, std::uint64_t id, LabelAnalysisLimits l)
        : source(doc.server()), metadata(doc.metadata()), area(id),
          revision(doc.annotation(id).revision), roi(doc.annotation(id).geometry),
          plane(doc.active_plane()), region(crop(doc, id)),
          grid(metadata.width, metadata.height, {region.x, region.width, region.width},
               {region.y, region.height, region.height}, l.work_bytes),
          limits(l) {
        limits.validate();
        if (!source || source->closed())
            throw std::invalid_argument("Label analysis requires an open image");
        // Project links acquire their observed file set when the native reader
        // opens. Capture that resolved authority, never its unopened placeholder.
        source->require_raw_current();
        const auto r = source->reader_source();
        if (!r)
            throw std::invalid_argument("Label analysis requires a native identified image source");
        r->validate();
        auto owned = std::make_shared<ReaderSource>(*r);
        if (r->primary)
            owned->primary = std::make_shared<const FileVersion>(*r->primary);
        observed = std::move(owned);
        provenance = capture_provenance(*source, id, revision, doc.revision(), false,
                                        doc.pixel_width_um(), doc.pixel_height_um());
        fixed = sizeof(Data) + metadata.levels.capacity() * sizeof(ImageLevel) +
                metadata.vendor.capacity() + roi.payload_bytes() + observed->payload_bytes() +
                provenance->payload_bytes() + 8192;
    }
    void capture_raw(const Document& doc, RawRegionRequest channels) {
        quantitative =
            raw_pixel_capture_lattice_source(doc, ScientificRawLattice(grid), limits.work_bytes);
        channels.region = region;
        channels.validate(quantitative->metadata);
        raw = channels;
        if (channels.channels > 4096)
            throw std::invalid_argument("Label raw channel block exceeds4096 channels");
        doc.require_object_raw_plane(area, channels.first_channel, channels.channels, channels.z,
                                     channels.t);
        plane = {-1, channels.z, channels.t};
        if (quantitative->metadata.width != metadata.width ||
            quantitative->metadata.height != metadata.height ||
            quantitative->metadata.levels.empty() ||
            quantitative->metadata.levels[0].downsample != 1)
            throw std::invalid_argument("Label raw/display source geometry differs");
        detail::label_charge(fixed, raw_pixel_source_snapshot_bytes(*quantitative), 1,
                             limits.work_bytes);
        std::size_t used = fixed;
        detail::label_charge(used, std::size_t(region.width) * std::size_t(region.height),
                             channels.channels * sample_bytes(quantitative->metadata.sample_type) +
                                 5,
                             limits.work_bytes);
    }
};
void LabelAnalysisLimits::validate() const {
    detail::label_work_limit(work_bytes);
    contours.validate();
    if (!max_membership_tests || max_membership_tests > 1000000000000ULL)
        throw std::invalid_argument("Invalid label membership work limit");
}
LabelAnalysisRequest::LabelAnalysisRequest(std::shared_ptr<const Data> data)
    : data_(std::move(data)) {}
LabelAnalysisRequest::LabelAnalysisRequest(const Document& doc, std::uint64_t area,
                                           SuperpixelOptions options, LabelAnalysisLimits limits) {
    options.validate();
    auto d = std::make_shared<Data>(doc, area, limits);
    d->document = doc.analysis_identity_.token;
    d->limits.work_bytes = std::min(d->limits.work_bytes, options.work_bytes);
    options.work_bytes = d->limits.work_bytes;
    d->options = options;
    std::size_t used = d->fixed;
    detail::label_charge(used, std::size_t(d->region.width) * std::size_t(d->region.height), 5,
                         d->limits.work_bytes);
    data_ = std::move(d);
}
LabelAnalysisRequest LabelAnalysisRequest::subcellular(const Document& doc, std::uint64_t area,
                                                       std::vector<std::uint64_t> cells,
                                                       RawRegionRequest channels,
                                                       SubcellularOptions options,
                                                       LabelAnalysisLimits limits) {
    options.validate();
    if (cells.empty() || cells.size() > 10000)
        throw std::invalid_argument("Select1..10000 distinct parent cells for spots");
    auto d = std::make_shared<Data>(doc, area, limits);
    d->document = doc.analysis_identity_.token;
    d->limits.work_bytes = std::min(d->limits.work_bytes, options.work_bytes);
    options.work_bytes = d->limits.work_bytes;
    d->options = options;
    d->capture_raw(doc, channels);
    const auto* metadata = &d->quantitative->metadata;
    if (options.channel >= channels.channels)
        throw std::invalid_argument("Spot channel is outside selected raw block");
    std::sort(cells.begin(), cells.end());
    if (std::adjacent_find(cells.begin(), cells.end()) != cells.end())
        throw std::invalid_argument("Duplicate subcellular parent cell");
    d->parents.reserve(cells.size());
    std::size_t used = d->fixed;
    detail::label_charge(used, cells.size(), sizeof(Data::Parent), d->limits.work_bytes);
    for (auto id : cells) {
        const auto a = doc.annotation(id);
        if (a.kind != ObjectKind::Cell || !a.geometry.is_area())
            throw std::invalid_argument("Subcellular parent must be a cell");
        doc.require_object_raw_plane(id, channels.first_channel, channels.channels, channels.z,
                                     channels.t);
        if (!d->roi.covers(a.geometry))
            throw std::invalid_argument("Parent cell is not completely inside the selected area");
        detail::label_charge(used, a.geometry.payload_bytes(), 1, d->limits.work_bytes);
        d->parents.push_back({id, a.revision, a.geometry, a.plane});
    }
    d->fixed = used;
    detail::label_charge(used, std::size_t(d->region.width) * std::size_t(d->region.height),
                         channels.channels * sample_bytes(metadata->sample_type) + 5,
                         d->limits.work_bytes);
    return LabelAnalysisRequest(std::move(d));
}
LabelAnalysisRequest LabelAnalysisRequest::external_mask(const Document& doc, std::uint64_t area,
                                                         RawRegionRequest channels,
                                                         ExternalLabelMaskOptions options,
                                                         LabelAnalysisLimits limits) {
    options.validate();
    auto d = std::make_shared<Data>(doc, area, limits);
    d->document = doc.analysis_identity_.token;
    d->capture_raw(doc, channels);
    detail::label_charge(d->fixed,
                         options.path.capacity() + options.producer.capacity() +
                             options.producer_version.capacity() + options.model_sha256.capacity() +
                             options.input_sha256.capacity(),
                         1, d->limits.work_bytes);
    d->options = std::move(options);
    return LabelAnalysisRequest(std::move(d));
}
bool LabelAnalysisRequest::matches(const Document& doc) const {
    workflow_require_owner(data_->owner);
    const auto& d = *data_;
    if (doc.analysis_identity_.token != d.document || doc.server() != d.source ||
        d.source->closed() || !same_metadata(doc.metadata(), d.metadata) ||
        doc.pixel_width_um() != d.provenance->effective_pixel_width_um ||
        doc.pixel_height_um() != d.provenance->effective_pixel_height_um)
        return false;
    try {
        const auto& area = doc.find(d.area);
        if (area.revision != d.revision)
            return false;
        doc.require_object_display_plane(d.area);
        if (d.options.index() != 0)
            doc.require_object_raw_plane(d.area, d.raw.first_channel, d.raw.channels, d.raw.z,
                                         d.raw.t);
        for (const auto& parent : d.parents) {
            const auto& current = doc.find(parent.id);
            if (current.revision != parent.revision)
                return false;
            doc.require_object_raw_plane(parent.id, d.raw.first_channel, d.raw.channels, d.raw.z,
                                         d.raw.t);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
LabelAnalysisMethod LabelAnalysisRequest::method() const {
    return LabelAnalysisMethod(data_->options.index());
}
std::uint64_t LabelAnalysisRequest::object_id() const {
    return data_->area;
}
const ScientificGrid& LabelAnalysisRequest::grid() const {
    return data_->grid;
}
const LabelAnalysisLimits& LabelAnalysisRequest::limits() const {
    return data_->limits;
}

struct LabelAnalysisRun::Data {
    std::shared_ptr<const LabelAnalysisRequest::Data> request;
    LabelRaster raster;
    std::shared_ptr<const TissueContours> contours;
    std::vector<LabelRegionMeasurements> measurements;
    std::vector<std::uint64_t> parents;
    std::vector<SubcellularCellCount> counts;
    std::vector<std::uint32_t> source_labels;
    std::shared_ptr<const ReaderSource> mask_source;
    LabelAnalysisReport report;
    std::string input, identity, kernel_input, kernel_result;
    Data(std::shared_ptr<const LabelAnalysisRequest::Data> r, LabelRaster value, std::string in,
         std::string result)
        : request(std::move(r)), raster(std::move(value)), kernel_input(std::move(in)),
          kernel_result(std::move(result)) {}
};
struct LabelAnalysisKernel {
    static const auto& request(const LabelAnalysisRun& r) {
        return *r.data_->request;
    }
    static const auto& mask_source(const LabelAnalysisRun& r) {
        return r.data_->mask_source;
    }
    static const LabelRaster& raster(const LabelAnalysisRun::Data& d) {
        return d.raster;
    }
    static void sources_current(const LabelAnalysisRun& run, const std::atomic<bool>* cancelled) {
        const auto& d = *run.data_->request;
        source_current(*d.source, d.metadata, d.observed, cancelled);
        if (run.data_->mask_source)
            observed_current(*run.data_->mask_source, cancelled);
    }
    static std::size_t retained(const LabelAnalysisRun::Data& d) {
        auto bytes = d.request->fixed + sizeof(d) + d.raster.payload_bytes() +
                     d.parents.capacity() * sizeof(std::uint64_t) +
                     d.counts.capacity() * sizeof(SubcellularCellCount) +
                     d.measurements.capacity() * sizeof(LabelRegionMeasurements) +
                     d.source_labels.capacity() * 4 +
                     (d.mask_source ? d.mask_source->payload_bytes() : 0) + d.input.capacity() +
                     d.identity.capacity() + d.kernel_input.capacity() + d.kernel_result.capacity();
        for (const auto& m : d.measurements)
            bytes += m.channels.capacity() * sizeof(RawChannelIntensity);
        return bytes;
    }
    static std::size_t encoding_work(const LabelAnalysisRun& run) {
        const auto& d = *run.data_;
        const auto extra = retained(d) - d.raster.payload_bytes() + d.contours->retained_bytes();
        if (extra >= d.request->limits.work_bytes)
            throw WorkflowCaptureLimit("Label archive has no remaining work");
        return d.request->limits.work_bytes - extra;
    }
    static Json parameters(const LabelAnalysisRequest::Data& d) {
        if (d.options.index() == 0) {
            const auto& p = std::get<SuperpixelOptions>(d.options);
            return {{"method", 0},
                    {"region", p.region_size},
                    {"iterations", p.iterations},
                    {"compactness", p.compactness},
                    {"color", int(p.color_space)},
                    {"max_regions", p.max_regions},
                    {"max_tests", p.max_assignment_tests}};
        }
        if (d.options.index() == 2) {
            const auto& p = std::get<ExternalLabelMaskOptions>(d.options);
            return {{"method", 2},
                    {"path", p.path},
                    {"producer", p.producer},
                    {"version", p.producer_version},
                    {"model", p.model_sha256},
                    {"input", p.input_sha256},
                    {"max_regions", p.max_regions},
                    {"first_channel", d.raw.first_channel},
                    {"channels", d.raw.channels},
                    {"z", d.raw.z},
                    {"t", d.raw.t}};
        }
        const auto& p = std::get<SubcellularOptions>(d.options);
        return {{"method", 1},
                {"channel", p.channel},
                {"threshold", p.threshold},
                {"sigma_x", p.sigma_x},
                {"sigma_y", p.sigma_y},
                {"background_x", p.background_sigma_x},
                {"background_y", p.background_sigma_y},
                {"min_pixels", p.min_pixels},
                {"max_pixels", p.max_pixels},
                {"connectivity", int(p.connectivity)},
                {"max_spots", p.max_spots},
                {"first_channel", d.raw.first_channel},
                {"channels", d.raw.channels},
                {"z", d.raw.z},
                {"t", d.raw.t}};
    }
    static std::string context(const LabelAnalysisRequest::Data& d) {
        Sha256 h;
        detail::label_hash_text(h, "qpx.label-analysis-context/1");
        h.integer(d.metadata.width);
        h.integer(d.metadata.height);
        h.integer(d.area);
        for (auto n : {d.region.x, d.region.y, d.region.width, d.region.height})
            h.integer(n);
        for (auto n : std::array<std::uint64_t, 3>{std::uint64_t(d.plane.c), d.plane.z, d.plane.t})
            h.integer(n);
        for (const auto& v :
             {d.provenance->effective_pixel_width_um, d.provenance->effective_pixel_height_um}) {
            h.integer(bool(v));
            if (v)
                detail::label_hash_real(h, *v);
        }
        detail::label_hash_text(h, geometry_hash(d.roi));
        detail::hash_reader_plugin_binding(h, *d.source);
        if (d.options.index() == 0)
            if (const auto view = d.source->raw_view())
                detail::label_hash_text(h, tiff_view_encode(*view));
        if (d.options.index() != 0) {
            detail::label_hash_text(h, d.quantitative->epoch);
            for (auto n : {d.raw.first_channel, d.raw.channels, d.raw.z, d.raw.t})
                h.integer(n);
        }
        for (const auto& p : d.parents) {
            h.integer(p.id);
            detail::label_hash_text(h, geometry_hash(p.geometry));
        }
        return h.finish();
    }
    static RawPixelBuffer read_raw(const LabelAnalysisRequest::Data& d,
                                   const std::atomic<bool>* cancelled) {
        if (!raw_pixel_publication_lattice_source_matches(*d.source, *d.quantitative,
                                                          ScientificRawLattice(d.grid)))
            throw WorkflowInputChanged("Label raw source/metadata changed");
        auto pixels = d.source->read_raw(d.raw, cancelled);
        source_current(*d.source, d.metadata, d.observed, cancelled);
        if (pixels.width() != d.region.width || pixels.height() != d.region.height ||
            pixels.channels() != d.raw.channels ||
            pixels.sample_type() != d.quantitative->metadata.sample_type ||
            !raw_pixel_publication_lattice_source_matches(*d.source, *d.quantitative,
                                                          ScientificRawLattice(d.grid)))
            throw WorkflowInputChanged("Label decoder returned another sample lattice");
        return pixels;
    }
    static std::string raw_identity(const LabelAnalysisRequest::Data& d,
                                    const RawPixelBuffer& pixels,
                                    const std::atomic<bool>* cancelled) {
        Sha256 input;
        detail::label_hash_text(input, context(d));
        detail::label_hash_text(input, detail::label_raw_input(pixels, cancelled));
        return input.finish();
    }
    static LabelAnalysisInputInfo input(const Document& doc, std::uint64_t area,
                                        RawRegionRequest channels, LabelAnalysisLimits limits,
                                        const std::atomic<bool>* cancelled) {
        workflow_checkpoint(cancelled);
        LabelAnalysisRequest::Data d(doc, area, limits);
        d.options = ExternalLabelMaskOptions{};
        d.capture_raw(doc, channels);
        auto raw = read_raw(d, cancelled);
        auto identity = raw_identity(d, raw, cancelled);
        source_current(*d.source, d.metadata, d.observed, cancelled);
        return {std::move(identity), d.raw, raw.sample_type()};
    }
    static std::shared_ptr<const LabelAnalysisRun> run(const LabelAnalysisRequest& request,
                                                       const std::atomic<bool>* cancelled) {
        const auto& d = *request.data_;
        workflow_checkpoint(cancelled);
        source_current(*d.source, d.metadata, d.observed, cancelled);
        const auto n = std::size_t(d.region.width) * std::size_t(d.region.height);
        std::shared_ptr<LabelAnalysisRun::Data> out;
        if (d.options.index() == 0) {
            auto pixels = d.source->read_region(d.region);
            source_current(*d.source, d.metadata, d.observed, cancelled);
            if (pixels.width() != d.region.width || pixels.height() != d.region.height)
                throw WorkflowInputChanged("SLIC decoder returned another crop");
            std::vector<std::uint8_t> domain(n);
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                domain[i] = d.roi.contains_sample(
                    {d.grid.x().center(i % std::size_t(d.region.width)).binary64(),
                     d.grid.y().center(i / std::size_t(d.region.width)).binary64()},
                    RoiSamplingPolicy::GeometryCoverage);
            }
            auto options = std::get<SuperpixelOptions>(d.options);
            if (d.fixed >= options.work_bytes)
                throw WorkflowCaptureLimit("SLIC capture exceeds budget");
            options.work_bytes -= d.fixed;
            auto native = slic_superpixels(pixels, domain, options, cancelled);
            out = std::make_shared<LabelAnalysisRun::Data>(request.data_, native.raster(),
                                                           native.input_sha256(), native.sha256());
            std::size_t measurement_work = d.fixed + pixels.bytes().capacity() + domain.capacity() +
                                           out->raster.payload_bytes();
            detail::label_charge(measurement_work, n, 4, d.limits.work_bytes);
            RawPixelBuffer raw(pixels.width(), pixels.height(), 3, SampleType::UInt8);
            auto& rgb = std::get<std::vector<std::uint8_t>>(raw.writable_storage());
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                raw.writable_valid()[i] = pixels.bytes()[i * 4 + 3] == 255;
                for (unsigned c = 0; c < 3; ++c)
                    rgb[i * 3 + c] = pixels.bytes()[i * 4 + c];
            }
            const auto extra = d.fixed + pixels.bytes().capacity() + domain.capacity();
            if (extra >= d.limits.work_bytes)
                throw WorkflowCaptureLimit("SLIC measurements exceed budget");
            out->measurements = measure_raw_label_regions(raw, raster(*out),
                                                          d.limits.work_bytes - extra, cancelled);
            out->parents.assign(raster(*out).regions().size(), d.area);
        } else if (d.options.index() == 1) {
            auto pixels = read_raw(d, cancelled);
            std::vector<std::uint32_t> parent_labels(n, 0);
            std::uint64_t visits = 0;
            for (std::size_t k = 0; k < d.parents.size(); ++k) {
                const auto& parent = d.parents[k];
                const auto b = parent.geometry.bounds();
                const auto x0 = std::max(d.region.x, std::int64_t(std::floor(b.x))),
                           y0 = std::max(d.region.y, std::int64_t(std::floor(b.y)));
                const auto x1 = std::min(d.region.x + d.region.width,
                                         std::int64_t(std::ceil(b.x + b.width))),
                           y1 = std::min(d.region.y + d.region.height,
                                         std::int64_t(std::ceil(b.y + b.height)));
                for (auto y = y0; y < y1; ++y) {
                    workflow_checkpoint(cancelled);
                    for (auto x = x0; x < x1; ++x) {
                        if (visits++ == d.limits.max_membership_tests)
                            throw WorkflowCaptureLimit("Spot cell-membership work exceeded");
                        if (!parent.geometry.contains_sample({double(x) + .5, double(y) + .5},
                                                             RoiSamplingPolicy::GeometryCoverage))
                            continue;
                        auto& label = parent_labels[std::size_t(y - d.region.y) *
                                                        std::size_t(d.region.width) +
                                                    std::size_t(x - d.region.x)];
                        if (label)
                            throw std::invalid_argument(
                                "Selected parent cells overlap on the processing lattice");
                        label = std::uint32_t(k + 1);
                    }
                }
            }
            {
                std::vector<std::uint8_t> seen(d.parents.size(), 0);
                for (auto p : parent_labels)
                    if (p)
                        seen[p - 1] = 1;
                if (std::find(seen.begin(), seen.end(), 0) != seen.end())
                    throw std::invalid_argument(
                        "A selected cell contains no processing-pixel centers");
            }
            auto options = std::get<SubcellularOptions>(d.options);
            options.work_bytes -= d.fixed;
            if (pixels.payload_bytes() >= options.work_bytes)
                throw WorkflowCaptureLimit("Parent raster has no remaining work");
            LabelRaster parent_mask(d.region.width, d.region.height, std::move(parent_labels),
                                    {d.parents.size(), options.work_bytes - pixels.payload_bytes()},
                                    cancelled);
            auto result = detect_subcellular_spots(pixels, parent_mask, options, cancelled);
            std::size_t bytes = d.fixed + pixels.payload_bytes() + parent_mask.payload_bytes() +
                                result.raster_.payload_bytes() +
                                result.spots_.capacity() * sizeof(SubcellularSpot) +
                                result.cells_.capacity() * sizeof(SubcellularCellCount) + 4096;
            for (const auto& s : result.spots_)
                detail::label_charge(bytes, s.raw.channels.capacity(), sizeof(RawChannelIntensity),
                                     d.limits.work_bytes);
            detail::label_charge(bytes, result.spots_.size(),
                                 sizeof(LabelRegionMeasurements) + sizeof(std::uint64_t),
                                 d.limits.work_bytes);
            out = std::make_shared<LabelAnalysisRun::Data>(request.data_, result.raster(),
                                                           result.input_sha256(), result.sha256());
            out->report = {result.rejected_small(), result.rejected_large(),
                           result.nonfinite_pixels(), result.invalid_pixels()};
            out->counts = std::move(result.cells_);
            out->measurements.reserve(result.spots_.size());
            out->parents.reserve(result.spots_.size());
            for (auto& spot : result.spots_) {
                out->parents.push_back(d.parents[spot.parent_label - 1].id);
                out->measurements.push_back({spot.label, spot.raw.pixels, spot.raw.valid_pixels,
                                             std::move(spot.raw.channels)});
            }
        } else {
            const auto& p = std::get<ExternalLabelMaskOptions>(d.options);
            auto pixels = read_raw(d, cancelled);
            const auto input = raw_identity(d, pixels, cancelled);
            if (input != p.input_sha256)
                throw WorkflowInputChanged(
                    "External mask names a different raw input/ROI/plane/calibration");
            const auto extra = d.fixed + pixels.payload_bytes();
            if (extra >= d.limits.work_bytes)
                throw WorkflowCaptureLimit("No remaining work for external mask");
            auto imported = import_label_mask_tiff(p, d.region.width, d.region.height,
                                                   d.limits.work_bytes - extra, cancelled);
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                if (imported.raster.labels()[i] &&
                    !d.roi.contains_sample(
                        {d.grid.x().center(i % std::size_t(d.region.width)).binary64(),
                         d.grid.y().center(i / std::size_t(d.region.width)).binary64()},
                        RoiSamplingPolicy::GeometryCoverage))
                    throw std::invalid_argument(
                        "External mask foreground lies outside the selected ROI");
            }
            Sha256 identity;
            detail::label_hash_text(identity, "qpx.external-label-mask/1");
            for (const auto& s : {p.producer, p.producer_version, p.model_sha256, input,
                                  imported.file_sha256, imported.raster.sha256()})
                detail::label_hash_text(identity, s);
            out = std::make_shared<LabelAnalysisRun::Data>(
                request.data_, imported.raster, detail::label_raw_input(pixels, cancelled),
                identity.finish());
            out->source_labels = std::move(imported.source_labels);
            out->mask_source = std::move(imported.source);
            auto measurement_extra =
                d.fixed + out->source_labels.capacity() * 4 + out->mask_source->payload_bytes();
            if (measurement_extra >= d.limits.work_bytes)
                throw WorkflowCaptureLimit("No work remains for external-mask measurements");
            out->measurements = measure_raw_label_regions(
                pixels, out->raster, d.limits.work_bytes - measurement_extra, cancelled);
            out->parents.assign(out->measurements.size(), d.area);
            observed_current(*out->mask_source, cancelled);
        }
        source_current(*d.source, d.metadata, d.observed, cancelled);
        auto contour_options = d.limits.contours;
        const auto contour_extra = retained(*out);
        if (contour_extra >= d.limits.work_bytes)
            throw WorkflowCaptureLimit("Label contours exceed remaining work");
        contour_options.work_bytes =
            std::min(contour_options.work_bytes, d.limits.work_bytes - contour_extra);
        out->contours = trace_label_raster(raster(*out), d.grid, d.roi, contour_options, cancelled);
        if (out->contours->components().size() != raster(*out).regions().size())
            throw std::runtime_error("Label contour publication lost regions");
        Sha256 input;
        detail::label_hash_text(input, context(d));
        detail::label_hash_text(input, out->kernel_input);
        out->input = input.finish();
        Sha256 identity;
        detail::label_hash_text(identity, "qpx.label-analysis-run/1");
        detail::label_hash_text(identity, out->input);
        detail::label_hash_text(identity, out->kernel_result);
        out->identity = identity.finish();
        if (!d.expected.empty() && (out->identity != d.expected || out->input != d.expected_input ||
                                    out->raster.sha256() != d.expected_raster))
            throw WorkflowInputChanged("Saved label recipe/input/result identity changed");
        source_current(*d.source, d.metadata, d.observed, cancelled);
        if (out->mask_source)
            observed_current(*out->mask_source, cancelled);
        return std::shared_ptr<const LabelAnalysisRun>(new LabelAnalysisRun(std::move(out)));
    }
    static void current(const Document& doc, const LabelAnalysisRequest& request,
                        const LabelAnalysisRun& result, const std::atomic<bool>* cancelled) {
        if (!request.matches(doc) || !result.matches(request))
            throw WorkflowInputChanged("Label analysis belongs to a different/stale capture");
        sources_current(result, cancelled);
    }
    static std::string keep(Document&, const LabelAnalysisRequest&, const LabelAnalysisRun&,
                            const std::atomic<bool>*);
    static std::vector<std::uint64_t> publish(Document&, const LabelAnalysisRequest&,
                                              const LabelAnalysisRun&, std::string,
                                              const std::atomic<bool>*);
};
LabelAnalysisRun::LabelAnalysisRun(std::shared_ptr<const Data> d) : data_(std::move(d)) {}
LabelAnalysisMethod LabelAnalysisRun::method() const {
    return LabelAnalysisMethod(data_->request->options.index());
}
const LabelRaster& LabelAnalysisRun::raster() const {
    return LabelAnalysisKernel::raster(*data_);
}
const std::shared_ptr<const TissueContours>& LabelAnalysisRun::contours() const {
    return data_->contours;
}
const std::vector<LabelRegionMeasurements>& LabelAnalysisRun::measurements() const {
    return data_->measurements;
}
const std::vector<SubcellularCellCount>& LabelAnalysisRun::cell_counts() const {
    return data_->counts;
}
const std::vector<std::uint64_t>& LabelAnalysisRun::parent_ids() const {
    return data_->parents;
}
const std::vector<std::uint32_t>& LabelAnalysisRun::source_labels() const {
    return data_->source_labels;
}
const LabelAnalysisOptions& LabelAnalysisRun::options() const {
    return data_->request->options;
}
std::optional<RawRegionRequest> LabelAnalysisRun::raw_channels() const {
    return method() == LabelAnalysisMethod::Superpixels
               ? std::nullopt
               : std::optional<RawRegionRequest>{data_->request->raw};
}
const LabelAnalysisReport& LabelAnalysisRun::report() const {
    return data_->report;
}
const ScientificGrid& LabelAnalysisRun::grid() const {
    return data_->request->grid;
}
const std::string& LabelAnalysisRun::input_sha256() const {
    return data_->input;
}
const std::string& LabelAnalysisRun::sha256() const {
    return data_->identity;
}
const char* LabelAnalysisRun::measurement_domain() const {
    return method() == LabelAnalysisMethod::Superpixels ? "decoded-device-RGB8"
                                                        : "original-raw-channel-samples";
}
const std::shared_ptr<const AnalysisProvenance>& LabelAnalysisRun::provenance() const {
    return data_->request->provenance;
}
bool LabelAnalysisRun::matches(const LabelAnalysisRequest& r) const {
    return data_->request->capture == r.data_->capture;
}
std::shared_ptr<const LabelAnalysisRun> run_label_analysis(const LabelAnalysisRequest& r,
                                                           const std::atomic<bool>* cancelled) {
    return LabelAnalysisKernel::run(r, cancelled);
}
void require_current_label_analysis_inputs(const LabelAnalysisRun& run,
                                           const std::atomic<bool>* cancelled) {
    LabelAnalysisKernel::sources_current(run, cancelled);
}
LabelAnalysisInputInfo label_analysis_input_info(const Document& doc, std::uint64_t area,
                                                 RawRegionRequest channels,
                                                 LabelAnalysisLimits limits,
                                                 const std::atomic<bool>* cancelled) {
    return LabelAnalysisKernel::input(doc, area, channels, limits, cancelled);
}
std::string label_analysis_raw_input_sha256(const Document& doc, std::uint64_t area,
                                            RawRegionRequest channels, LabelAnalysisLimits limits,
                                            const std::atomic<bool>* cancelled) {
    return label_analysis_input_info(doc, area, channels, limits, cancelled).sha256;
}

std::string LabelAnalysisKernel::keep(Document& doc, const LabelAnalysisRequest& request,
                                      const LabelAnalysisRun& result,
                                      const std::atomic<bool>* cancelled) {
    current(doc, request, result, cancelled);
    const auto& d = *request.data_;
    Value::List parents;
    parents.reserve(d.parents.size());
    for (const auto& p : d.parents)
        parents.push_back(Value{decimal(p.id)});
    auto all = records(doc);
    if (all.count(result.sha256()))
        throw WorkflowOutputExists("Label result is already kept");
    const auto settings = parameters(d).dump();
    if (settings.size() > 4096)
        throw std::length_error("Label settings exceed metadata text limit");
    all.emplace(result.sha256(), Value{Value::Map{{"area", Value{decimal(d.area)}},
                                                  {"parameters", Value{settings}},
                                                  {"parents", Value{std::move(parents)}},
                                                  {"input", Value{result.input_sha256()}},
                                                  {"raster", Value{result.raster().sha256()}}}});
    auto values =
        doc.image_data_properties() ? doc.image_data_properties()->values() : Value::Map{};
    values[records_key] = Value{std::move(all)};
    auto properties = std::make_shared<const ImageDataProperties>(std::move(values));
    current(doc, request, result, cancelled);
    doc.set_image_data_properties(std::move(properties));
    return result.sha256();
}
std::string keep_label_analysis(Document& doc, const LabelAnalysisRequest& request,
                                const LabelAnalysisRun& run, const std::atomic<bool>* cancelled) {
    return LabelAnalysisKernel::keep(doc, request, run, cancelled);
}
std::vector<std::string> label_analysis_ids(const Document& doc) {
    std::vector<std::string> ids;
    for (const auto& pair : records(doc)) {
        if (!digest(pair.first))
            throw std::invalid_argument("Invalid stored label result ID");
        map_value(pair.second);
        ids.push_back(pair.first);
    }
    return ids;
}
LabelAnalysisRequest replay_label_analysis(const Document& doc, const std::string& id,
                                           LabelAnalysisLimits limits) {
    if (!digest(id))
        throw std::invalid_argument("Invalid label result ID");
    limits.validate();
    const auto all = records(doc);
    const auto found = all.find(id);
    if (found == all.end())
        throw std::out_of_range("Label analysis record not found");
    const auto& r = map_value(found->second);
    if (r.size() != 5 || !r.count("area") || !r.count("parameters") || !r.count("parents") ||
        !r.count("input") || !r.count("raster") || !digest(text_value(r.at("input"))) ||
        !digest(text_value(r.at("raster"))))
        throw std::invalid_argument("Unsupported label analysis record");
    const auto* saved_parents = std::get_if<Value::List>(&r.at("parents").value);
    if (!saved_parents || saved_parents->size() > 10000)
        throw std::invalid_argument("Invalid stored parent-cell list");
    std::vector<std::uint64_t> parents;
    for (const auto& p : *saved_parents)
        parents.push_back(id_value(p));
    const auto area = id_value(r.at("area"));
    const auto p = parse(text_value(r.at("parameters")));
    if (!p.is_object() || !p.contains("method"))
        throw std::invalid_argument("Missing label analysis method");
    const auto method = unsigned_value(p.at("method"), 2);
    auto request = [&] {
        if (!method) {
            keys(p, {"method", "region", "iterations", "compactness", "color", "max_regions",
                     "max_tests"});
            if (!parents.empty())
                throw std::invalid_argument("SLIC recipe contains parent cells");
            SuperpixelOptions o;
            o.region_size = std::uint32_t(unsigned_value(p.at("region"), 4096));
            o.iterations = std::uint32_t(unsigned_value(p.at("iterations"), 100));
            o.compactness = real_value(p.at("compactness"));
            o.color_space = SuperpixelColorSpace(unsigned_value(p.at("color"), 1));
            o.max_regions = std::size_t(unsigned_value(p.at("max_regions"), 1000000));
            o.max_assignment_tests = unsigned_value(p.at("max_tests"), 1000000000000ULL);
            o.work_bytes = limits.work_bytes;
            return LabelAnalysisRequest(doc, area, o, limits);
        }
        if (method == 2) {
            keys(p, {"method", "path", "producer", "version", "model", "input", "max_regions",
                     "first_channel", "channels", "z", "t"});
            if (!parents.empty())
                throw std::invalid_argument("External mask recipe contains parent cells");
            ExternalLabelMaskOptions o;
            o.path = p.at("path").get<std::string>();
            o.producer = p.at("producer").get<std::string>();
            o.producer_version = p.at("version").get<std::string>();
            o.model_sha256 = p.at("model").get<std::string>();
            o.input_sha256 = p.at("input").get<std::string>();
            o.max_regions = std::size_t(unsigned_value(p.at("max_regions"), 1000000));
            RawRegionRequest channels;
            channels.first_channel =
                std::uint32_t(unsigned_value(p.at("first_channel"), UINT32_MAX));
            channels.channels = std::uint32_t(unsigned_value(p.at("channels"), 4096));
            channels.z = std::uint32_t(unsigned_value(p.at("z"), 65535));
            channels.t = std::uint32_t(unsigned_value(p.at("t"), 65535));
            return LabelAnalysisRequest::external_mask(doc, area, channels, std::move(o), limits);
        }
        keys(p, {"method", "channel", "threshold", "sigma_x", "sigma_y", "background_x",
                 "background_y", "min_pixels", "max_pixels", "connectivity", "max_spots",
                 "first_channel", "channels", "z", "t"});
        SubcellularOptions o;
        o.channel = std::uint32_t(unsigned_value(p.at("channel"), 4095));
        o.threshold = real_value(p.at("threshold"));
        o.sigma_x = real_value(p.at("sigma_x"));
        o.sigma_y = real_value(p.at("sigma_y"));
        o.background_sigma_x = real_value(p.at("background_x"));
        o.background_sigma_y = real_value(p.at("background_y"));
        o.min_pixels = unsigned_value(p.at("min_pixels"), UINT32_MAX);
        o.max_pixels = unsigned_value(p.at("max_pixels"), UINT32_MAX);
        o.connectivity = SpotConnectivity(unsigned_value(p.at("connectivity"), 1));
        o.max_spots = std::size_t(unsigned_value(p.at("max_spots"), 1000000));
        o.work_bytes = limits.work_bytes;
        RawRegionRequest channels;
        channels.first_channel = std::uint32_t(unsigned_value(p.at("first_channel"), UINT32_MAX));
        channels.channels = std::uint32_t(unsigned_value(p.at("channels"), 4096));
        channels.z = std::uint32_t(unsigned_value(p.at("z"), INT32_MAX));
        channels.t = std::uint32_t(unsigned_value(p.at("t"), INT32_MAX));
        return LabelAnalysisRequest::subcellular(doc, area, std::move(parents), channels, o,
                                                 limits);
    }();
    auto captured = std::make_shared<LabelAnalysisRequest::Data>(*request.data_);
    captured->expected = id;
    captured->expected_input = text_value(r.at("input"));
    captured->expected_raster = text_value(r.at("raster"));
    return LabelAnalysisRequest(std::move(captured));
}
void remove_label_analysis(Document& doc, const std::string& id) {
    auto all = records(doc);
    if (!all.erase(id))
        throw std::out_of_range("Label result not found");
    for (const auto& a : doc.annotations())
        if (a.properties)
            for (const auto* key : {run_key, counts_key}) {
                const auto found = a.properties->metadata().find(key);
                if (found != a.properties->metadata().end() && found->second == id)
                    throw std::invalid_argument(
                        "Published label objects/counts still reference this result");
            }
    auto values = doc.image_data_properties()->values();
    if (all.empty())
        values.erase(records_key);
    else
        values[records_key] = Value{std::move(all)};
    doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
}
std::vector<std::uint64_t> LabelAnalysisKernel::publish(Document& doc,
                                                        const LabelAnalysisRequest& request,
                                                        const LabelAnalysisRun& run,
                                                        std::string prefix,
                                                        const std::atomic<bool>* cancelled) {
    current(doc, request, run, cancelled);
    detail::bounded_utf8(prefix, 1024);
    const auto kept = records(doc);
    const auto found = kept.find(run.sha256());
    if (found == kept.end())
        throw std::invalid_argument("Keep the label result before publishing its objects");
    const auto& d = *request.data_;
    const auto& saved = map_value(found->second);
    if (saved.size() != 5 || !saved.count("area") || !saved.count("parameters") ||
        !saved.count("parents") || !saved.count("input") || !saved.count("raster") ||
        id_value(saved.at("area")) != d.area ||
        text_value(saved.at("parameters")) != parameters(d).dump() ||
        text_value(saved.at("input")) != run.input_sha256() ||
        text_value(saved.at("raster")) != run.raster().sha256())
        throw WorkflowInputChanged("Kept label recipe/input/result association was changed");
    const auto* parents = std::get_if<Value::List>(&saved.at("parents").value);
    if (!parents || parents->size() != d.parents.size())
        throw WorkflowInputChanged("Kept parent-cell cohort was changed");
    for (std::size_t i = 0; i < parents->size(); ++i)
        if (id_value((*parents)[i]) != d.parents[i].id)
            throw WorkflowInputChanged("Kept parent-cell identity was changed");
    for (const auto& a : doc.annotations())
        if (a.properties)
            for (const auto* key : {run_key, counts_key}) {
                const auto found = a.properties->metadata().find(key);
                if (found != a.properties->metadata().end() && found->second == run.sha256())
                    throw WorkflowOutputExists("Label objects/counts are already published");
            }
    std::vector<ObjectEdit> edits;
    edits.reserve(run.contours()->components().size() + d.parents.size());
    for (const auto& c : run.contours()->components()) {
        workflow_checkpoint(cancelled);
        const auto index = std::size_t(c.label - 1);
        const auto parent = run.parent_ids().at(index);
        auto geometry = c.geometry;
        auto plane = d.plane;
        if (request.method() == LabelAnalysisMethod::Subcellular) {
            const auto found = std::lower_bound(d.parents.begin(), d.parents.end(), parent,
                                                [](const auto& p, auto id) { return p.id < id; });
            if (found == d.parents.end() || found->id != parent)
                throw std::logic_error("Unknown native spot parent");
            const auto clipped =
                geometry.boolean_operation(found->geometry, BooleanOp::Intersection);
            if (!clipped)
                throw std::runtime_error("Spot geometry vanished when clipped to its parent cell");
            geometry = *clipped;
            if (found->plane.c >= 0)
                plane.c = found->plane.c;
        }
        std::map<std::string, std::string> metadata{
            {run_key, run.sha256()},
            {"qpx.label-analysis.input/1", run.input_sha256()},
            {"qpx.label-analysis.label/1", decimal(c.label)},
            {"qpx.label-analysis.raster/1", run.raster().sha256()},
            {"qpx.label-analysis.method/1",
             request.method() == LabelAnalysisMethod::Superpixels   ? "slic"
             : request.method() == LabelAnalysisMethod::Subcellular ? "subcellular"
                                                                    : "external-mask"},
            {"qpx.label-analysis.domain/1", run.measurement_domain()},
            {geometry_key, geometry_hash(geometry)}};
        const auto& measured = run.measurements().at(index);
        std::map<std::string, double> measurements{
            {"Label pixels", double(measured.pixels)},
            {"Label valid pixels", double(measured.valid_pixels)}};
        for (std::size_t channel = 0; channel < measured.channels.size(); ++channel) {
            const auto& v = measured.channels[channel];
            const auto key = (request.method() != LabelAnalysisMethod::Superpixels
                                  ? "Raw channel " + decimal(d.raw.first_channel + channel)
                                  : "Decoded RGB component " + decimal(channel)) +
                             ": ";
            measurements[key + "finite samples"] = double(v.finite_samples);
            measurements[key + "nonfinite samples"] = double(v.nonfinite_samples);
            for (const auto& pair :
                 std::initializer_list<std::pair<const char*, std::optional<double>>>{
                     {"sum", v.sum},
                     {"mean", v.mean},
                     {"minimum", v.minimum},
                     {"maximum", v.maximum},
                     {"sample SD", v.sample_standard_deviation}})
                if (pair.second)
                    measurements[key + pair.first] = *pair.second;
        }
        if (request.method() == LabelAnalysisMethod::ExternalMask) {
            const auto& p = std::get<ExternalLabelMaskOptions>(d.options);
            metadata["qpx.external-mask.source-label/1"] = decimal(run.source_labels().at(index));
            metadata["qpx.external-mask.producer/1"] = p.producer;
            metadata["qpx.external-mask.version/1"] = p.producer_version;
            metadata["qpx.external-mask.model/1"] = p.model_sha256;
        }
        metadata[facts_key] =
            publication_facts(geometry, parent, plane, "", metadata, measurements);
        auto edit =
            ObjectEdit::create_geometry(std::move(geometry), prefix + " " + decimal(c.label),
                                        parent, "", ObjectKind::Detection, {}, plane);
        edit.value.properties =
            std::make_shared<const ObjectProperties>(std::move(metadata), std::move(measurements));
        edits.push_back(std::move(edit));
    }
    for (std::size_t i = 0; i < d.parents.size(); ++i) {
        workflow_checkpoint(cancelled);
        const auto a = doc.annotation(d.parents[i].id);
        auto metadata =
            a.properties ? a.properties->metadata() : std::map<std::string, std::string>{};
        auto measurements =
            a.properties ? a.properties->measurements() : std::map<std::string, double>{};
        if (a.locked())
            throw std::invalid_argument(
                "A selected parent cell is locked; no spots/counts were published");
        if (metadata.count(counts_key) || metadata.count(counts_facts_key) ||
            metadata.count(counts_properties_key) || measurements.count(spot_count) ||
            measurements.count(spot_pixels))
            throw WorkflowOutputExists(
                "A selected cell already has subcellular counts; remove that publication first");
        metadata[counts_key] = run.sha256();
        measurements[spot_count] = double(run.cell_counts().at(i).spots);
        measurements[spot_pixels] = double(run.cell_counts().at(i).spot_pixels);
        metadata[counts_properties_key] = a.properties ? "present" : "absent";
        metadata[counts_facts_key] =
            count_facts(run.sha256(), a.id, metadata.at(counts_properties_key), measurements);
        auto properties = std::make_shared<const ObjectProperties>(
            std::move(metadata), std::move(measurements), a.locked(), a.color_rgb(),
            a.class_probability());
        edits.push_back(ObjectEdit::properties(a.id, std::move(properties)));
    }
    current(doc, request, run, cancelled);
    return doc.execute(edits, request.method() == LabelAnalysisMethod::Superpixels
                                  ? "Add SLIC regions"
                                  : "Add subcellular spots");
}
std::vector<std::uint64_t> publish_label_analysis(Document& doc,
                                                  const LabelAnalysisRequest& request,
                                                  const LabelAnalysisRun& result,
                                                  std::string prefix,
                                                  const std::atomic<bool>* cancelled) {
    return LabelAnalysisKernel::publish(doc, request, result, std::move(prefix), cancelled);
}
std::vector<std::uint64_t> remove_label_publication(Document& doc, const std::string& id) {
    if (!digest(id))
        throw std::invalid_argument("Invalid label result ID");
    const auto annotations = doc.annotations();
    std::set<std::uint64_t> owned;
    std::vector<ObjectEdit> edits;
    for (const auto& a : annotations)
        if (a.properties) {
            const auto& metadata = a.properties->metadata();
            const auto label = metadata.find(run_key);
            if (label != metadata.end() && label->second == id) {
                const auto shape = metadata.find(geometry_key), facts = metadata.find(facts_key);
                if (a.kind != ObjectKind::Detection || a.locked() || shape == metadata.end() ||
                    geometry_hash(a.geometry) != shape->second || facts == metadata.end() ||
                    publication_facts(a.geometry, a.parent_id, a.plane, a.class_name, metadata,
                                      a.properties->measurements()) != facts->second)
                    throw std::invalid_argument("A published label object was edited/locked; "
                                                "preserve it or review deletion explicitly");
                owned.insert(a.id);
                edits.push_back(ObjectEdit::remove(a.id));
            }
            const auto counts = metadata.find(counts_key);
            if (counts != metadata.end() && counts->second == id) {
                const auto facts = metadata.find(counts_facts_key),
                           previous = metadata.find(counts_properties_key);
                if (a.kind != ObjectKind::Cell || a.locked() || facts == metadata.end() ||
                    previous == metadata.end() ||
                    (previous->second != "present" && previous->second != "absent") ||
                    !a.properties->measurements().count(spot_count) ||
                    !a.properties->measurements().count(spot_pixels) ||
                    count_facts(id, a.id, previous->second, a.properties->measurements()) !=
                        facts->second)
                    throw std::invalid_argument(
                        "Published parent counts were edited/locked; nothing was removed");
                auto copy = metadata;
                copy.erase(counts_key);
                copy.erase(counts_facts_key);
                copy.erase(counts_properties_key);
                auto measurements = a.properties->measurements();
                measurements.erase(spot_count);
                measurements.erase(spot_pixels);
                std::shared_ptr<const ObjectProperties> properties;
                if (previous->second == "present" || !copy.empty() || !measurements.empty() ||
                    a.color_rgb() || a.class_probability())
                    properties = std::make_shared<const ObjectProperties>(
                        std::move(copy), std::move(measurements), a.locked(), a.color_rgb(),
                        a.class_probability());
                edits.push_back(ObjectEdit::properties(a.id, std::move(properties)));
            }
        }
    for (const auto& a : annotations)
        if (owned.count(a.parent_id) && !owned.count(a.id))
            throw std::invalid_argument(
                "Published label has user-added descendants; nothing was removed");
    if (edits.empty())
        throw std::out_of_range("Label publication not found");
    doc.execute(edits, "Remove label publication");
    return {owned.begin(), owned.end()};
}
namespace {
std::vector<std::string> protected_outputs(const LabelAnalysisRun& run,
                                           const std::vector<std::string>& extra) {
    auto files = extra;
    const auto& p = *run.provenance();
    if (!p.locator.empty())
        files.push_back(p.locator);
    if (p.reader_source) {
        if (p.reader_source->primary)
            files.push_back(p.reader_source->primary->path);
        for (const auto& c : p.reader_source->companions)
            files.push_back(c.path);
    }
    if (const auto& mask = LabelAnalysisKernel::mask_source(run)) {
        if (mask->primary)
            files.push_back(mask->primary->path);
        for (const auto& c : mask->companions)
            files.push_back(c.path);
    }
    return files;
}
void protect_output(const LabelAnalysisRun& run, const std::string& path,
                    const std::vector<std::string>& extra) {
    detail::guard_export_output(path, protected_outputs(run, extra));
    detail::require_new_export(path);
    const auto& p = *run.provenance();
    if (p.reader_source && p.reader_source->scope == "directory_store") {
        const auto root =
            std::filesystem::weakly_canonical(std::filesystem::u8path(p.locator)).native();
        const auto target =
            std::filesystem::weakly_canonical(std::filesystem::u8path(detail::export_path(path)))
                .native();
        bool inside = false;
#ifdef _WIN32
        inside = target.size() > root.size() &&
                 (target[root.size()] == L'\\' || target[root.size()] == L'/') &&
                 CompareStringOrdinal(root.c_str(), int(root.size()), target.c_str(),
                                      int(root.size()), TRUE) == CSTR_EQUAL;
#else
        inside = target.size() > root.size() && target.compare(0, root.size(), root) == 0 &&
                 target[root.size()] == '/';
#endif
        if (inside)
            throw std::invalid_argument("Label export lies inside its protected source store");
    }
}
} // namespace
std::size_t save_label_analysis_csv(const LabelAnalysisRun& run, const std::string& path,
                                    std::size_t bytes,
                                    const std::vector<std::string>& protected_paths,
                                    const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto& d = LabelAnalysisKernel::request(run);
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    detail::AtomicExportFile file(path, bytes, cancelled);
    const std::string header =
        "run_sha256,input_sha256,value_domain,label,source_label,parent_object_id,pixels,valid_"
        "pixels,source_channel_or_rgb_component,finite_samples,nonfinite_samples,sum,mean,minimum,"
        "maximum,sample_sd\n";
    file.write(header.data(), header.size());
    for (std::size_t i = 0; i < run.measurements().size(); ++i) {
        workflow_checkpoint(cancelled);
        const auto& m = run.measurements()[i];
        for (std::size_t c = 0; c < m.channels.size(); ++c) {
            const auto& v = m.channels[c];
            std::ostringstream row;
            row.imbue(std::locale::classic());
            row << std::setprecision(17);
            const auto channel =
                run.method() != LabelAnalysisMethod::Superpixels ? d.raw.first_channel + c : c;
            row << run.sha256() << ',' << run.input_sha256() << ',' << run.measurement_domain()
                << ',' << m.label << ','
                << (run.source_labels().empty() ? m.label : run.source_labels().at(i)) << ','
                << run.parent_ids().at(i) << ',' << m.pixels << ',' << m.valid_pixels << ','
                << channel << ',' << v.finite_samples << ',' << v.nonfinite_samples;
            for (const auto& x :
                 {v.sum, v.mean, v.minimum, v.maximum, v.sample_standard_deviation}) {
                row << ',';
                if (x)
                    row << *x;
            }
            row << '\n';
            const auto text = row.str();
            file.write(text.data(), text.size());
        }
    }
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    detail::commit_export_output(file, path, ExportWritePolicy::CreateNew,
                                 protected_outputs(run, protected_paths));
    return file.bytes();
}
void save_label_analysis_raster(const LabelAnalysisRun& run, const std::string& path,
                                const std::vector<std::string>& protected_paths,
                                const std::atomic<bool>* cancelled) {
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    const auto data = encode_label_raster(
        run.raster(), {1000000, LabelAnalysisKernel::encoding_work(run)}, cancelled);
    detail::AtomicExportFile file(path, data.size(), cancelled);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    detail::commit_export_output(file, path, ExportWritePolicy::CreateNew,
                                 protected_outputs(run, protected_paths));
}
std::size_t save_subcellular_counts_csv(const LabelAnalysisRun& run, const std::string& path,
                                        std::size_t bytes,
                                        const std::vector<std::string>& protected_paths,
                                        const std::atomic<bool>* cancelled) {
    if (run.method() != LabelAnalysisMethod::Subcellular)
        throw std::invalid_argument("Cell-count CSV requires a subcellular run");
    const auto& d = LabelAnalysisKernel::request(run);
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    detail::AtomicExportFile file(path, bytes, cancelled);
    const std::string header = "run_sha256,input_sha256,parent_object_id,spots,spot_pixels\n";
    file.write(header.data(), header.size());
    for (std::size_t i = 0; i < run.cell_counts().size(); ++i) {
        workflow_checkpoint(cancelled);
        const auto& c = run.cell_counts()[i];
        const auto row = run.sha256() + "," + run.input_sha256() + "," +
                         decimal(d.parents.at(i).id) + "," + decimal(c.spots) + "," +
                         decimal(c.spot_pixels) + "\n";
        file.write(row.data(), row.size());
    }
    require_current_label_analysis_inputs(run, cancelled);
    protect_output(run, path, protected_paths);
    detail::commit_export_output(file, path, ExportWritePolicy::CreateNew,
                                 protected_outputs(run, protected_paths));
    return file.bytes();
}
struct LabelAnalysisJob::Impl {
    std::thread::id owner = std::this_thread::get_id();
    std::atomic<bool> cancelled{false};
    std::optional<LabelAnalysisRequest> request;
    std::future<std::shared_ptr<const LabelAnalysisRun>> future;
    std::shared_ptr<const LabelAnalysisRun> result;
    LabelAnalysisStats stats;
    bool stale = false;
};
LabelAnalysisJob::LabelAnalysisJob() : impl_(std::make_unique<Impl>()) {}
LabelAnalysisJob::~LabelAnalysisJob() {
    impl_->cancelled.store(true);
    if (impl_->future.valid())
        impl_->future.wait();
}
bool LabelAnalysisJob::busy() const {
    workflow_require_owner(impl_->owner);
    return impl_->future.valid() &&
           impl_->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}
void LabelAnalysisJob::cancel() {
    impl_->cancelled.store(true);
}
void LabelAnalysisJob::await_completion() {
    workflow_require_owner(impl_->owner);
    if (impl_->future.valid())
        impl_->future.wait();
}
bool LabelAnalysisJob::request(const LabelAnalysisRequest& request) {
    workflow_require_owner(impl_->owner);
    workflow_require_owner(request.data_->owner);
    auto& s = *impl_;
    if (s.future.valid())
        return false;
    if (s.stats.generation == UINT64_MAX)
        throw std::overflow_error("Label job generation exhausted");
    s.request = request;
    s.cancelled.store(false);
    s.stale = false;
    s.stats = {"running", "", s.stats.generation + 1};
    try {
        s.future = std::async(std::launch::async,
                              [&s, request] { return run_label_analysis(request, &s.cancelled); });
    } catch (...) {
        s.stats.state = "failed";
        s.stats.error = workflow_failure_message(std::current_exception());
        throw;
    }
    return true;
}
LabelAnalysisStats LabelAnalysisJob::poll(const Document& doc) {
    workflow_require_owner(impl_->owner);
    auto& s = *impl_;
    if (s.request && !s.request->matches(doc)) {
        s.stale = true;
        s.cancelled.store(true);
        s.stats.state = busy() ? "cancelling" : "stale";
    }
    if (s.future.valid() &&
        s.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto value = s.future.get();
            if (s.stale)
                s.stats.state = "stale";
            else if (s.cancelled.load())
                s.stats.state = "cancelled";
            else {
                s.result = std::move(value);
                s.stats.state = "ready";
            }
        } catch (const WorkflowInputChanged& e) {
            s.stats.state = "stale";
            s.stats.error = e.what();
        } catch (const std::exception& e) {
            s.stats.state = s.stale ? "stale" : s.cancelled.load() ? "cancelled" : "failed";
            s.stats.error = e.what();
        }
    }
    return s.stats;
}
LabelAnalysisStats LabelAnalysisJob::wait(const Document& doc) {
    workflow_require_owner(impl_->owner);
    poll(doc);
    await_completion();
    return poll(doc);
}
std::shared_ptr<const LabelAnalysisRun> LabelAnalysisJob::result() const {
    workflow_require_owner(impl_->owner);
    return impl_->result;
}
} // namespace qpx
