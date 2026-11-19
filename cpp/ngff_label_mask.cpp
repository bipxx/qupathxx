// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/ngff_label_mask.hpp"
#include "label_analysis_internal.hpp"
#include "reader_plugin.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>

namespace qpx {
namespace {
using Json = nlohmann::json;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
bool digest(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
void group(const std::string& s, bool empty) {
    detail::bounded_utf8(s, 1024);
    if (s.empty()) {
        require(empty, "Label source requires an explicit label group");
        return;
    }
    require(s.find_first_of("\\:") == std::string::npos && s.front() != '/',
            "Label groups require relative slash keys");
    std::size_t at = 0;
    while (at <= s.size()) {
        const auto end = s.find('/', at);
        const auto part = s.substr(at, end == std::string::npos ? end : end - at);
        require(!part.empty() && part != "." && part != "..", "Invalid label group component");
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
}
void keys(const Json& j, std::initializer_list<const char*> names) {
    require(j.is_object() && j.size() == names.size(), "Unsupported label description members");
    for (const auto* name : names)
        require(j.contains(name), "Missing label description member");
}
std::uint32_t index(const Json& j) {
    require(j.is_number_integer() && j >= 0 && j <= UINT32_MAX, "Invalid label source plane index");
    return j.get<std::uint32_t>();
}
std::uint64_t decimal_unsigned(const std::string& s) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), result);
    require(!s.empty() && parsed.ec == std::errc{} && parsed.ptr == s.data() + s.size() &&
                std::to_string(result) == s,
            "Noncanonical exact label-description integer");
    return result;
}
LabelCode code(const std::string& s) {
    return LabelCode::from_decimal(s);
}
SampleType integral_type(const std::string& s) {
    for (unsigned i = 0; i < 8; ++i) {
        const auto type = SampleType(i);
        if (sample_name(type) == s)
            return type;
    }
    throw std::invalid_argument("Label source requires an exact integral sample type");
}
struct DescriptionSax : detail::StrictJson {
    std::size_t nodes = 0;
    const std::atomic<bool>* cancelled;
    explicit DescriptionSax(const std::atomic<bool>* c) : StrictJson("label description"), cancelled(c) {}
    void tick() {
        workflow_checkpoint(cancelled);
        if (++nodes > 16384 || frames.size() > 16)
            throw WorkflowCaptureLimit("Label description exceeds structural limits");
    }
    bool null() override { tick(); return true; }
    bool boolean(bool) override { tick(); return true; }
    bool number_integer(number_integer_t) override { tick(); return true; }
    bool number_unsigned(number_unsigned_t) override { tick(); return true; }
    bool number_float(number_float_t n, const string_t&) override {
        tick(); require(std::isfinite(n), "Nonfinite label description number"); return true;
    }
    bool string(string_t& s) override {
        tick(); detail::bounded_utf8(s, label_mask_description_limit); return true;
    }
    bool key(string_t& s) override { tick(); return StrictJson::key(s); }
    bool start_object(std::size_t n) override { tick(); return StrictJson::start_object(n); }
    bool start_array(std::size_t n) override { tick(); return StrictJson::start_array(n); }
};
LabelMaskFrame frame(const Json& j) {
    keys(j, {"axes"});
    const auto& axes = j.at("axes");
    require(axes.is_array() && axes.size() >= 2 && axes.size() <= 5, "Invalid label frame rank");
    LabelMaskFrame result;
    for (const auto& a : axes) {
        keys(a, {"name", "unit", "extent", "scale", "translation"});
        LabelMaskAxisFrame item;
        item.name = a.at("name").get<std::string>();
        item.unit = a.at("unit").get<std::string>();
        detail::bounded_utf8(item.name, 1);
        detail::bounded_utf8(item.unit, 32);
        item.extent = decimal_unsigned(a.at("extent").get<std::string>());
        require(a.at("scale").is_number() && a.at("translation").is_number(),
                "Nonnumeric label frame transform");
        item.scale = a.at("scale").get<double>();
        item.translation = a.at("translation").get<double>();
        require(item.extent && std::isfinite(item.scale) && item.scale > 0 &&
                    std::isfinite(item.translation), "Invalid label frame transform");
        result.axes.push_back(std::move(item));
    }
    return result;
}
std::size_t frame_bytes(const LabelMaskFrame& value) {
    auto bytes = value.axes.capacity() * sizeof(LabelMaskAxisFrame);
    for (const auto& axis : value.axes)
        bytes += axis.name.capacity() + axis.unit.capacity();
    return bytes;
}
std::shared_ptr<const detail::AdmittedReaderPlugin> provider() {
    const auto& catalog = detail::reader_plugin_catalog();
    for (const auto& p : catalog.readers)
        if (p->info.provider_id == "org.qupathxx.ngff-labels") {
            require(p->info.contract == "ngff04-zarr2-integral-labels/1" &&
                        p->info.capabilities == (QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW |
                            QPX_READER_CAP_SOURCE_STORE | QPX_READER_CAP_LABEL_DESCRIPTION) &&
                        p->info.default_options.empty(),
                    "NGFF labels require the declared integral-label/store/description profile");
            return p;
        }
    std::string message = "NGFF labels require configured org.qupathxx.ngff-labels";
    for (const auto& diagnostic : catalog.diagnostics)
        if (message.size() < 4096)
            message += "; " + diagnostic.substr(0, 4096 - message.size());
    throw std::invalid_argument(message);
}
} // namespace

void NgffLabelMaskOptions::validate() const {
    detail::bounded_utf8(path, 2048);
    group(label_group, false);
    group(reference_group, true);
    for (const auto* s : {&author, &producer, &producer_version})
        detail::bounded_utf8(*s, 256);
    require(!path.empty() && std::filesystem::u8path(path).is_absolute() && level == 0 &&
                digest(input_sha256) && (!reader_sha256.empty() ? digest(reader_sha256) : true) &&
                max_regions && max_regions <= 1000000,
            "NGFF labels require an absolute store, level zero, target input SHA and bounded regions");
    if (origin == LabelMaskOrigin::Authored)
        require(!author.empty() && producer.empty() && producer_version.empty() && model_sha256.empty(),
                "Authored labels require an author and no producer/model identity");
    else {
        require(origin == LabelMaskOrigin::Producer && author.empty() && !producer.empty() &&
                    !producer_version.empty() && digest(model_sha256),
                "Producer labels require producer/version/model identity and no authored attribution");
    }
    if (replay_source) {
        detail::bounded_utf8(replay_source->description_json, label_mask_description_limit);
        detail::bounded_utf8(replay_source->codes_text, 1024u * 1024);
        require(!replay_source->description_json.empty() && digest(replay_source->observed_store_sha256) &&
                    digest(replay_source->content_sha256), "Invalid exact NGFF replay-source identity");
    }
}
std::size_t LabelMaskReplaySource::payload_bytes() const {
    return sizeof(*this) + description_json.capacity() + codes_text.capacity() +
        observed_store_sha256.capacity() + content_sha256.capacity();
}
std::size_t NgffLabelMaskOptions::payload_bytes() const {
    return sizeof(*this) + path.capacity() + label_group.capacity() + reference_group.capacity() +
        author.capacity() + producer.capacity() + producer_version.capacity() + model_sha256.capacity() +
        input_sha256.capacity() + reader_sha256.capacity() +
        (replay_source ? replay_source->payload_bytes() : 0);
}
std::string label_code_map_text(const LabelCodeMap& codes, std::size_t limit,
                                 const std::atomic<bool>* cancelled) {
    require(limit && limit <= 32u * 1024 * 1024, "Invalid source-code text limit");
    std::string result;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (!(i & 1023))
            workflow_checkpoint(cancelled);
        const auto value = codes.at(i).decimal();
        if (result.size() >= limit || value.size() > limit - result.size() - 1)
            throw WorkflowCaptureLimit("Exact source-code map exceeds text limit");
        result += value;
        result += '\n';
    }
    workflow_checkpoint(cancelled);
    return result;
}
std::string ngff_label_selection_json(const NgffLabelMaskOptions& options) {
    options.validate();
    return Json({{"schema", "qpx.ngff-label-selection/1"}, {"label_group", options.label_group},
                 {"reference_group", options.reference_group}, {"level", options.level},
                 {"z", options.z}, {"t", options.t}}).dump();
}
NgffLabelMaskOptions capture_ngff_label_mask_options(NgffLabelMaskOptions options) {
    options.validate();
    const auto identity = detail::reader_plugin_implementation(provider()->info,
        ngff_label_selection_json(options)).sha256();
    if (!options.reader_sha256.empty() && options.reader_sha256 != identity)
        throw WorkflowInputChanged("NGFF label decoder/options differ from captured identity");
    options.reader_sha256 = identity;
    return options;
}
std::size_t LabelMaskSourceDescription::payload_bytes() const {
    auto bytes = sizeof(*this) + format.capacity() + label_group.capacity() + reference_group.capacity() +
        source_reference.capacity() + label_metadata.capacity() + reference_metadata.capacity() +
        json.capacity() + sha256.capacity() + frame_bytes(label_frame) + frame_bytes(reference_frame) +
        label_properties.capacity() * sizeof(LabelMaskSourceProperty);
    for (const auto& p : label_properties)
        bytes += p.properties.capacity();
    return bytes;
}
LabelMaskSourceDescription decode_label_mask_description(std::string text,
                                                        const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    detail::bounded_utf8(text, label_mask_description_limit);
    require(!text.empty(), "Empty label description");
    DescriptionSax sax(cancelled);
    require(Json::sax_parse(text, &sax), "Invalid label description JSON");
    const auto j = Json::parse(text);
    keys(j, {"schema", "format", "label_group", "reference_group", "source_reference", "level",
             "z", "t", "sample_type", "label_frame", "reference_frame", "label_metadata",
             "reference_metadata", "label_properties"});
    require(j.at("schema") == "qpx.label-mask-description/1" &&
                j.at("format") == "ome-ngff-labels-0.4-zarr2/1", "Unsupported label description profile");
    LabelMaskSourceDescription result;
    result.format = j.at("format").get<std::string>();
    result.label_group = j.at("label_group").get<std::string>();
    result.reference_group = j.at("reference_group").get<std::string>();
    group(result.label_group, false);
    group(result.reference_group, true);
    const auto& source = j.at("source_reference");
    keys(source, {"path", "origin"});
    result.source_reference = source.at("path").get<std::string>();
    detail::bounded_utf8(result.source_reference, 1024);
    require(!result.source_reference.empty() &&
                (source.at("origin") == "default" || source.at("origin") == "explicit"),
            "Invalid declared label reference");
    result.default_reference = source.at("origin") == "default";
    result.level = index(j.at("level"));
    result.z = index(j.at("z"));
    result.t = index(j.at("t"));
    require(result.level == 0, "Only original level-zero label lattices are admitted");
    result.sample_type = integral_type(j.at("sample_type").get<std::string>());
    result.label_frame = frame(j.at("label_frame"));
    result.reference_frame = frame(j.at("reference_frame"));
    result.label_metadata = j.at("label_metadata").get<std::string>();
    result.reference_metadata = j.at("reference_metadata").get<std::string>();
    detail::bounded_utf8(result.label_metadata, 24576);
    detail::bounded_utf8(result.reference_metadata, 24576);
    require(!result.label_metadata.empty() && !result.reference_metadata.empty(), "Missing original label/reference metadata");
    const auto& properties = j.at("label_properties");
    require(properties.is_array() && properties.size() <= 4096, "Too many label metadata entries");
    for (const auto& p : properties) {
        workflow_checkpoint(cancelled);
        keys(p, {"code", "properties", "rgba"});
        LabelMaskSourceProperty item;
        item.code = code(p.at("code").get<std::string>());
        item.properties = p.at("properties").get<std::string>();
        detail::bounded_utf8(item.properties, 24576);
        if (!p.at("rgba").is_null()) {
            const auto& rgba = p.at("rgba");
            require(rgba.is_array() && rgba.size() == 4, "Invalid label RGBA metadata");
            std::array<std::uint8_t, 4> value{};
            for (unsigned i = 0; i < 4; ++i) {
                require(rgba[i].is_number_integer() && rgba[i] >= 0 && rgba[i] <= 255,
                        "Invalid label color component");
                value[i] = rgba[i].get<std::uint8_t>();
            }
            item.rgba = value;
        }
        require(result.label_properties.empty() || result.label_properties.back().code < item.code,
                "Label description codes are repeated or unsorted");
        result.label_properties.push_back(std::move(item));
    }
    result.json = std::move(text);
    Sha256 hash;
    detail::label_hash_text(hash, "qpx.label-mask-description/1");
    detail::label_hash_text(hash, result.json);
    result.sha256 = hash.finish();
    workflow_checkpoint(cancelled);
    return result;
}
ImportedNgffLabelMask import_ngff_label_mask(const NgffLabelMaskOptions& options,
    const LabelMaskTargetFrame& target, std::size_t work, const std::atomic<bool>* cancelled) {
    const auto captured = capture_ngff_label_mask_options(options);
    detail::label_work_limit(work);
    workflow_checkpoint(cancelled);
    const auto n = detail::label_pixels(target.crop.width, target.crop.height);
    std::size_t host = 4u * 1024 * 1024;
    detail::label_charge(host, n, 32, work);
    detail::label_charge(host, options.max_regions, 8, work);
    const auto decoder = std::min<std::size_t>(64u * 1024 * 1024, work - host);
    if (decoder < 8u * 1024 * 1024)
        throw WorkflowCaptureLimit("No bounded store-decoder work remains for NGFF labels");
    auto server = detail::open_bounded_plugin_image_server(options.path, provider(), decoder,
        cancelled, ngff_label_selection_json(captured));
    const auto description_text = detail::capture_native_reader_plugin_label_description(*server);
    require(bool(description_text), "NGFF provider omitted its label description");
    auto description = std::make_shared<const LabelMaskSourceDescription>(
        decode_label_mask_description(*description_text, cancelled));
    if (description->label_group != options.label_group ||
        description->reference_group != options.reference_group || description->level != options.level ||
        description->z != options.z || description->t != options.t)
        throw WorkflowInputChanged("NGFF provider described another label selection");
    const LabelMaskPlaneSelection plane{options.z, options.t, options.reference_z, options.reference_t};
    const auto linked = validate_link_label_mask_frames(description->label_frame,
        description->reference_frame, plane, target);
    const auto metadata = server->raw_metadata(cancelled);
    const auto x = std::find_if(description->label_frame.axes.begin(), description->label_frame.axes.end(),
                               [](const auto& a) { return a.name == "x"; });
    const auto y = std::find_if(description->label_frame.axes.begin(), description->label_frame.axes.end(),
                               [](const auto& a) { return a.name == "y"; });
    require(metadata && x != description->label_frame.axes.end() && y != description->label_frame.axes.end() &&
                std::uint64_t(metadata->width) == x->extent && std::uint64_t(metadata->height) == y->extent &&
                metadata->channels == 1 && metadata->series_count == 1 && metadata->series == 0 &&
                metadata->z_planes == 1 && metadata->timepoints == 1 && metadata->levels.size() == 1 &&
                metadata->levels[0].downsample == 1 && metadata->sample_type == description->sample_type,
            "NGFF raw/description lattice differs");
    RawRegionRequest request;
    request.region = linked.source_crop;
    auto raw = server->read_raw(request, cancelled);
    detail::require_raw_lattice(raw);
    if (raw.width() != target.crop.width || raw.height() != target.crop.height || raw.channels() != 1 ||
        raw.sample_type() != description->sample_type)
        throw WorkflowInputChanged("NGFF decoder returned another crop or raw type");
    const auto source = server->reader_source();
    const auto store = detail::capture_native_reader_plugin_store(*server);
    const auto link = detail::capture_native_reader_plugin_link(*server);
    require(source && store && link && source->scope == "directory_store" &&
                store->observed_sha256() == source->directory_sha256 &&
                link->implementation.sha256() == captured.reader_sha256 &&
                source->reader == detail::reader_plugin_binding(link->implementation),
            "NGFF decoder/source/store identity differs");
    server->close();
    server.reset();
    std::size_t extra = 65536;
    if (captured.replay_source)
        detail::label_charge(extra, 2u * 1024 * 1024, 1, work);
    detail::label_charge(extra, store->payload_bytes(), 1, work);
    detail::label_charge(extra, store->limits().inventory_bytes, 1, work);
    detail::label_charge(extra, source->payload_bytes() + description->payload_bytes(), 1, work);
    auto normalized = normalize_label_codes(raw, {options.max_regions, work - extra}, cancelled);
    Sha256 content;
    detail::label_hash_text(content, "qpx.ngff-label-content/1");
    detail::label_hash_text(content, description->sha256);
    detail::label_hash_text(content, detail::label_raw_input(raw, cancelled));
    for (const auto i : {linked.source_crop.x, linked.source_crop.y,
                         linked.source_crop.width, linked.source_crop.height})
        content.integer(std::uint64_t(i));
    const auto content_sha256 = content.finish();
    if (captured.replay_source) {
        const auto& expected = *captured.replay_source;
        if (expected.description_json != description->json ||
            expected.codes_text != label_code_map_text(normalized.source_codes, 1024u * 1024, cancelled) ||
            expected.observed_store_sha256 != source->directory_sha256 || expected.content_sha256 != content_sha256)
            throw WorkflowInputChanged("Saved NGFF metadata, original codes or store/content identity changed");
    }
    store->require_current(cancelled);
    return {std::move(normalized.raster), std::move(normalized.source_codes), source, store,
            std::move(description), linked, content_sha256, captured.reader_sha256};
}
} // namespace qpx
