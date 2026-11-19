#include "qupathxx/region_export.hpp"
#include "qupathxx/workflow.hpp"
#include "qupathxx/project.hpp"
#include "export_output.hpp"
#include "format_import_internal.hpp"
#include "qupathxx/directory_store.hpp"
#include "raw_numpy_write.hpp"
#include "ome_zarr_internal.hpp"
#include "project_database_output.hpp"
#include "project_export_guard.hpp"
#include "project_source_internal.hpp"
#include "reader_plugin.hpp"
#include "reader_registry.hpp"
#include "sha256.hpp"
#include "stored_zip_read.hpp"
#include "strict_json.hpp"
#include "workflow_input.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace qpx {
namespace {
using Json = nlohmann::json;
constexpr std::size_t bundle_array_limit=256u*1024*1024;
constexpr std::size_t bundle_receipt_limit=4u*1024*1024;
constexpr std::size_t bundle_framing_limit=4096;
void guard_project_region_output(const detail::WorkflowInput& input,const Project& project,
                                 const std::vector<ProjectImage>& catalog,const std::string& output,
                                 const std::atomic<bool>* cancelled) {
    input.verify(false,cancelled);
    detail::guard_project_database_output(input.path(),"",output);
    detail::guard_project_export_output(project,catalog,output,cancelled);
}
class SourceCapture {
    ReaderSource source_;
    std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards_;
    std::shared_ptr<const DirectoryStoreSnapshot> store_;
    std::shared_ptr<const OmeZarrSource> native_zarr_;

  public:
    SourceCapture(const ImageServer& server, const ReaderSource& source, const std::atomic<bool>* cancelled)
        : source_(source) {
        source_.validate();
        if (source_.scope == "directory_store") {
            if (server.reader_id() == "ome-zarr") {
                native_zarr_ = detail::capture_native_ome_zarr_source(server);
                if (!native_zarr_ || native_zarr_->directory() != source_.locator ||
                    native_zarr_->sha256() != source_.directory_sha256)
                    throw std::invalid_argument("Raw directory export requires an identified OME-Zarr source");
            } else {
                store_ = detail::capture_native_reader_plugin_store(server);
                if (!store_ || store_->directory() != source_.locator ||
                    store_->observed_sha256() != source_.directory_sha256)
                    throw std::invalid_argument("Raw directory export requires an identified native store");
            }
            verify(cancelled);
            return;
        }
        if (!source_.primary ||
            (source_.scope != "single_file" && source_.scope != "compound_files"))
            throw std::invalid_argument("Raw file export requires an observed file or companion set");
        guards_.reserve(source_.companions.size() + 1);
        guards_.push_back(std::make_unique<detail::ReaderFileGuard>(source_.primary->path));
        for (const auto& file : source_.companions) {
            workflow_checkpoint(cancelled);
            guards_.push_back(std::make_unique<detail::ReaderFileGuard>(file.path));
        }
        auto current = source_;
        current.primary = guards_[0]->version();
        for (std::size_t i = 1; i < guards_.size(); ++i)
            current.companions[i - 1] = *guards_[i]->version();
        if (!detail::same_reader_files(source_, current))
            throw WorkflowInputChanged("Raw export source changed before capture");
        verify(cancelled);
    }
    void verify(const std::atomic<bool>* cancelled) const {
        if (native_zarr_)
            native_zarr_->require_current(cancelled);
        if (store_)
            store_->require_current(cancelled);
        for (const auto& guard : guards_) {
            workflow_checkpoint(cancelled);
            try {
                guard->verify();
            } catch (const std::exception& e) {
                throw WorkflowInputChanged(std::string("Raw export source changed: ") + e.what());
            }
        }
    }
    const OmeZarrSource* native_zarr() const { return native_zarr_.get(); }
    void protect(const std::string& output) const {
        std::vector<std::string> paths{source_.primary->path};
        if (source_.scope == "directory_store")
            paths.push_back(source_.locator);
        for (const auto& file : source_.companions)
            paths.push_back(file.path);
        detail::guard_export_output(output, paths);
    }
};
bool default_tiff(const TiffOpenOptions& t) {
    return !t.view.series && !t.view.z && !t.view.t &&
           t.view.channels == std::array<std::int32_t, 3>{-2, -2, -2} &&
           std::all_of(t.view.lower.begin(), t.view.lower.end(),
                       [](double v) { return std::isnan(v); }) &&
           std::all_of(t.view.upper.begin(), t.view.upper.end(),
                       [](double v) { return std::isnan(v); }) &&
           t.decode_bytes == 64u * 1024 * 1024 && t.cache_bytes == 64u * 1024 * 1024;
}
std::optional<double> output_spacing(std::optional<double> base, double downsample) {
    if (!base)
        return {};
    const double spacing = *base * downsample;
    if (!std::isfinite(spacing) || spacing <= 0)
        throw std::invalid_argument("OME-Zarr selected level pixel spacing is outside the physical calibration range");
    return spacing;
}
Json project_source_json(const RawRegionProjectSource& source) {
    return {{"project_id",source.project_id},{"project_sha256",source.project_sha256},
            {"generation",source.generation},{"image_id",source.image_id}};
}
Json project_axes_json(const RawRegionProjectAxes& axes) {
    const auto quantity=[](std::optional<double> value) {return value?Json(*value):Json(nullptr);};
    return {{"z_spacing_um",quantity(axes.z_spacing_um)},{"z_position_um",quantity(axes.z_position_um)},
            {"timepoint_seconds",quantity(axes.timepoint_seconds)}};
}
Json bundle_receipt(const RawRegionExportReport& report) {
    const auto& meta=*report.metadata;
    const auto& level=meta.levels.at(std::size_t(report.request.region.level));
    Json calibration{{"level_downsample", level.downsample}};
    if(meta.pixel_width_um)calibration["base_pixel_width_um"]=*meta.pixel_width_um;
    if(meta.pixel_height_um)calibration["base_pixel_height_um"]=*meta.pixel_height_um;
    if(meta.z_spacing_um)calibration["z_spacing_um"]=*meta.z_spacing_um;
    if(meta.time_interval_seconds)calibration["time_interval_seconds"]=*meta.time_interval_seconds;
    return Json{{"schema", report.project?"qpx.raw-region-bundle/3":"qpx.raw-region-bundle/2"}, {"array", "raw.npy"},
                {"region", Json::parse(raw_region_export_report_json(report))},
                {"reader_source", Json::parse(reader_source_json(*report.source))},
                {"calibration", std::move(calibration)}};
}
std::string sha256(std::string_view bytes) {
    Sha256 digest;
    digest.update(bytes.data(),bytes.size());
    return digest.finish();
}
[[noreturn]] void invalid_bundle(const char* reason) {
    throw std::invalid_argument(std::string("Invalid raw region bundle: ")+reason);
}
void verify_bundle_project(const Json& project) {
    const auto hex=[&](const char* key,std::size_t length) {
        const auto* value=project.at(key).get_ptr<const std::string*>();
        return value && value->size()==length && value->find_first_not_of("0123456789abcdef")==std::string::npos;
    };
    const auto positive=[&](const char* key) {
        const auto& value=project.at(key);
        return value.is_number_unsigned() && value.get<std::uint64_t>()>0;
    };
    if(!project.is_object() || project.size()!=4 || !project.contains("project_id") ||
       !project.contains("project_sha256") || !project.contains("generation") || !project.contains("image_id") ||
       !hex("project_id",32) || !hex("project_sha256",64) || !positive("generation") || !positive("image_id") ||
       project.at("generation").get<std::uint64_t>()>std::uint64_t(std::numeric_limits<std::int64_t>::max()))
        invalid_bundle("invalid project source");
}
void verify_bundle_project_axes(const Json& region) {
    const auto& axes=region.at("project_axes");
    if(!axes.is_object() || axes.size()!=3 || !axes.contains("z_spacing_um") ||
       !axes.contains("z_position_um") || !axes.contains("timepoint_seconds"))
        invalid_bundle("invalid project axis calibration");
    for(const auto& value:axes)
        if(!value.is_null() && (!value.is_number() || !std::isfinite(value.get<double>())))
            invalid_bundle("invalid project axis calibration");
    const auto& spacing=axes.at("z_spacing_um");
    const auto& position=axes.at("z_position_um");
    const auto& z=region.at("z");
    if(!z.is_number_unsigned() || z.get<std::uint64_t>()>std::numeric_limits<std::uint32_t>::max() ||
       spacing.is_null()!=position.is_null() ||
       (!spacing.is_null() && (spacing.get<double>()<=0 ||
         position.get<double>()!=spacing.get<double>()*z.get<std::uint32_t>())))
        invalid_bundle("inconsistent project Z calibration");
}
std::uint64_t bundle_integer(const Json& region,const char* key) {
    const auto& number=region.at(key);
    if(!number.is_number_unsigned())invalid_bundle("invalid array dimensions or count");
    return number.get<std::uint64_t>();
}
SampleType bundle_type(const Json& region) {
    const auto& name=region.at("dtype");
    for(const auto type:{SampleType::UInt8,SampleType::Int8,SampleType::UInt16,SampleType::Int16,
                         SampleType::UInt32,SampleType::Int32,SampleType::UInt64,SampleType::Int64,
                         SampleType::Float32,SampleType::Float64})
        if(name==sample_name(type))return type;
    invalid_bundle("unknown sample type");
}
struct NumpyHeader {
    std::string bytes;
    void write(const char* data,std::size_t size) { bytes.append(data,size); }
};
void verify_bundle_array(detail::stored_zip::Input& input,const detail::stored_zip::Member& member,
                         const Json& receipt,bool bound,const std::atomic<bool>* cancelled,
                         detail::AtomicExportFile* output) {
    const auto& region=receipt.at("region");
    const auto width=bundle_integer(region,"width"),height=bundle_integer(region,"height");
    const auto channels=bundle_integer(region,"channels"),valid=bundle_integer(region,"valid_pixels");
    const auto sample=bundle_type(region);
    const auto schema=region.at("schema");
    const bool structured=schema=="qpx.raw-region/3";
    if((structured && region.value("array_layout",std::string{})!="values-valid") ||
       (!structured && (region.contains("array_layout") || (schema!="qpx.raw-region/1" && schema!="qpx.raw-region/2"))))
        invalid_bundle("unsupported raw region layout");
    if(!width || !height || !channels || width>bundle_array_limit/height ||
       channels>bundle_array_limit/sample_bytes(sample))
        invalid_bundle("array dimensions exceed bounds");
    const auto pixels=width*height;
    const auto stride=static_cast<std::size_t>(channels)*sample_bytes(sample)+(structured?1:0);
    if(pixels>bundle_array_limit/stride || valid>pixels || member.size<10)
        invalid_bundle("array extent or validity count exceeds bounds");
    const auto prefix=input.record<10>();
    const auto header_size=detail::stored_zip::read_little(prefix.data()+8,2);
    if(header_size>bundle_framing_limit || header_size>member.size-prefix.size())
        invalid_bundle("invalid NPY header extent");
    std::string header(prefix.data(),prefix.size());
    header.resize(prefix.size()+header_size);
    input.read(header.data()+prefix.size(),header_size);
    auto type=detail::numpy_sample_descriptor(sample);
    const auto xy=std::to_string(height)+", "+std::to_string(width);
    const auto c=std::to_string(channels);
    bool matches=false;
    for(const char order:{'<','>'}) {
        if(sample_bytes(sample)>1)type[0]=order;
        NumpyHeader expected;
        detail::write_numpy_header(expected,structured?"[('values', '"+type+"', ("+c+",)), ('valid', '|b1')]":"'"+type+"'",
                                   structured?xy:xy+", "+c);
        matches=matches || expected.bytes==header;
    }
    auto remaining=member.size-header.size();
    if(!matches || remaining!=pixels*stride)
        invalid_bundle("NPY type, shape or extent differs from the receipt");
    Sha256 digest;
    detail::stored_zip::Checksum checksum{cancelled};
    if(bound)checksum.digest=&digest;
    checksum.write(header.data(),header.size());
    if(output)output->write(header.data(),header.size());
    std::array<char,65536> bytes{};
    std::size_t next_valid=stride-1;
    std::uint64_t observed_valid=0;
    while(remaining) {
        const auto count=std::min(remaining,bytes.size());
        input.read(bytes.data(),count);
        checksum.write(bytes.data(),count);
        if(structured) {
            for(;next_valid<count;next_valid+=stride) {
                const auto bit=static_cast<unsigned char>(bytes[next_valid]);
                if(bit>1)invalid_bundle("NPY validity contains a non-Boolean byte");
                observed_valid+=bit;
            }
            next_valid-=count;
        }
        if(output)output->write(bytes.data(),count);
        remaining-=count;
    }
    if(~checksum.crc!=member.crc)invalid_bundle("NPY CRC differs");
    if(bound && digest.finish()!=receipt.at("array_sha256"))invalid_bundle("NPY digest differs");
    if(structured && observed_valid!=valid)invalid_bundle("NPY validity count differs from the receipt");
}
} // namespace
void RawRegionExportOptions::validate() const {
    if (reader.empty() || reader.size() > 256 || reader.find('\0') != std::string::npos ||
        reader == "auto")
        throw std::invalid_argument("Raw export requires an explicit reader ID (default tiff)");
    tiff.validate();
    if (reader != "tiff" && !default_tiff(tiff))
        throw std::invalid_argument(
            "TIFF view/decode/cache options cannot be applied to another raw reader");
    if (reader_plugin_options &&
        (reader_plugin_options->empty() || !detail::valid_reader_plugin_id(reader)))
        throw std::invalid_argument("Reader options require an explicit native reader plugin and a JSON object");
    if (level_coordinates && reader != "ome-zarr")
        throw std::invalid_argument("Level coordinates require the built-in OME-Zarr reader");
    if (!request.channels || request.channels > 65536 || request.region.level < 0 ||
        (!full_width && request.region.width <= 0) || (!full_height && request.region.height <= 0))
        throw std::invalid_argument("Raw export dimensions/channels/level are invalid");
}
namespace {
RawRegionExportReport export_opened_region(const std::shared_ptr<ImageServer>& server,
                                           const std::string& destination,
                                           const RawRegionExportOptions& options,
                                           const std::atomic<bool>* cancelled,
                                           const std::function<void()>& protect_project = {},
                                           std::optional<RawRegionProjectSource> project = {},
                                           const SourceFileAssociation* verified_source = nullptr) {
    const auto metadata = server->raw_metadata(cancelled);
    const auto source = server->reader_source();
    if (!metadata || !source)
        throw std::invalid_argument(
            "Selected reader does not expose identified native raw samples");
    SourceCapture capture(*server, *source, cancelled);
    capture.protect(destination);
    if(verified_source)
        detail::require_verified_project_source(*verified_source,*source,cancelled);
    auto request = options.request;
    if (request.region.level < 0 || std::size_t(request.region.level) >= metadata->levels.size())
        throw std::out_of_range("Invalid raw pyramid level");
    const auto& level = metadata->levels[request.region.level];
    if (options.full_width)
        request.region.width = level.width;
    if (options.full_height)
        request.region.height = level.height;
    if (!options.level_coordinates)
        request.validate(*metadata);
    std::optional<RawRegionExportGrid> output_grid;
    if (const auto* zarr = capture.native_zarr()) {
        const auto [level_x, level_y] = options.level_coordinates
            ? std::array<std::int64_t, 2>{request.region.x, request.region.y}
            : zarr->region_level_origin(request);
        const auto& dataset = zarr->levels().at(std::size_t(request.region.level));
        output_grid = RawRegionExportGrid{level.downsample,
                                         output_spacing(metadata->pixel_width_um, level.downsample),
                                         output_spacing(metadata->pixel_height_um, level.downsample),
                                         level_x, level_y, dataset.path, zarr->axes(), zarr->units(),
                                         dataset.scale, dataset.translation, options.level_coordinates};
    }
    const auto raw = options.level_coordinates
        ? detail::read_native_ome_zarr_level(*server, request, cancelled)
        : server->read_raw(request, cancelled);
    server->require_raw_current(cancelled);
    capture.verify(cancelled);
    const auto valid =
        std::uint64_t(std::count(raw.valid().begin(), raw.valid().end(), std::uint8_t(1)));
    if (!options.with_validity && options.reader != "tiff" &&
        valid != std::uint64_t(raw.width()) * std::uint64_t(raw.height())) {
        if (options.reader == "ome-zarr")
            throw std::invalid_argument("OME-Zarr NumPy array export requires complete valid samples; "
                                        "choose an in-bounds region or use --with-validity");
        throw std::invalid_argument("Plugin NumPy array export requires complete valid samples; "
                                    "choose an in-bounds region without missing pixels");
    }
    RawRegionExportReport report{server->reader_id(),
                                 request,
                                 metadata,
                                 std::make_shared<const ReaderSource>(*source),
                                 {},
                                 valid,
                                 raw.payload_bytes(),
                                 options.with_validity,
                                 output_grid,
                                 std::move(project)};
    if (detail::valid_reader_plugin_id(options.reader)) {
        const auto link = detail::capture_native_reader_plugin_link(*server);
        if (!link)
            throw std::invalid_argument("Raw file export requires a native plugin reader snapshot");
        report.reader_snapshot_json = detail::encode_reader_plugin_link(*link);
    }
    capture.protect(destination);
    detail::require_new_export(destination);
    capture.verify(cancelled);
    if(options.bundle && raw.payload_bytes()>bundle_array_limit)
        throw std::length_error("Raw array exceeds its bundle size limit");
    const auto receipt_limit=options.bundle?bundle_receipt_limit:0;
    detail::AtomicExportFile file(destination, raw.payload_bytes()+receipt_limit+bundle_framing_limit, cancelled);
    const auto write_raw=[&](auto& output) {
        if(options.with_validity)
            detail::write_raw_numpy_with_validity(output,raw);
        else
            detail::write_raw_numpy(output,raw);
    };
    if(options.bundle) {
        Sha256 array_digest;
        const auto array=detail::stored_zip::write_member(file,"raw.npy",cancelled,write_raw,&array_digest);
        auto receipt=bundle_receipt(report);
        receipt["array_sha256"]=array_digest.finish();
        receipt["receipt_sha256"]=sha256(receipt.dump());
        const auto text=receipt.dump()+"\n";
        if(text.size()>receipt_limit)
            throw std::length_error("Raw region receipt exceeds its bundle size limit");
        const auto provenance=detail::stored_zip::write_member(file,"receipt.json",cancelled,
            [&](auto& output){output.write(text.data(),text.size());});
        detail::stored_zip::finish(file,std::array{array,provenance});
    } else {
        write_raw(file);
    }
    capture.verify(cancelled);
    capture.protect(destination);
    if(protect_project)
        protect_project();
    file.commit(false);
    return report;
}
} // namespace
RawRegionExportReport export_raw_region(const std::string& input, const std::string& output,
                                        const RawRegionExportOptions& options,
                                        const std::atomic<bool>* cancelled) {
    options.validate();
    workflow_checkpoint(cancelled);
    const auto destination = detail::export_path(output);
    detail::require_new_export(destination);
    detail::ReaderOpenOptions opening;
    opening.tiff = options.tiff;
    const auto selection =
        detail::select_reader(input, options.reader, detail::ReaderPolicy::RegionCli);
    auto server = options.reader_plugin_options
                      ? detail::open_reader_with_plugin_options(input, selection,
                                                                *options.reader_plugin_options, cancelled)
                      : detail::open_reader(input, selection, opening, cancelled);
    return export_opened_region(server,destination,options,cancelled);
}
RawRegionExportReport export_project_raw_region(const std::string& path,std::uint64_t image,
                                                const std::string& output,
                                                const ProjectRawRegionExportOptions& requested,
                                                const std::atomic<bool>* cancelled) {
    auto options=requested.region;
    if(options.reader!="tiff" || options.reader_plugin_options || !default_tiff(options.tiff))
        throw std::invalid_argument("Project raw export uses the saved reader and options");
    const detail::WorkflowInput input(path,1024ull*1024*1024,true,cancelled);
    const auto project=Project::open_without_journal_recovery(input.path());
    const auto document=project->document(image);
    options.reader=document->server()->reader_id();
    if(const auto view=document->server()->raw_view()) {
        if(requested.follow_saved_z)options.request.z=view->z;
        if(requested.follow_saved_time)options.request.t=view->t;
    }
    options.validate();
    const auto destination=detail::export_path(output);
    const auto catalog=project->images();
    const auto protect=[&] {guard_project_region_output(input,*project,catalog,destination,cancelled);};
    protect();
    detail::require_new_export(destination);
    const auto server=detail::open_project_image_reader(*document,cancelled);
    const auto verified_source=project->source_verification(image);
    RawRegionProjectSource project_source{project->identity(),input.sha256(),project->generation(),image};
    if(document->image_axis_calibration())
        project_source.axes=RawRegionProjectAxes{document->z_spacing_um(),
            document->z_position_um(options.request.z),document->timepoint_seconds(options.request.t)};
    return export_opened_region(server,destination,options,cancelled,protect,
        std::move(project_source),verified_source.get());
}
std::string raw_region_export_report_json(const RawRegionExportReport& r) {
    if (!r.metadata || !r.source)
        throw std::invalid_argument("Raw export report is incomplete");
    const auto& p = r.request;
    // Preserve legacy TIFF output and schema exactly; new plugins are additive.
    if (r.reader == "tiff" && !r.with_validity) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << "{\"schema\":\"qpx.raw-region/1\",\"dtype\":\""
            << sample_name(r.metadata->sample_type) << "\",\"height\":" << p.region.height
            << ",\"width\":" << p.region.width << ",\"channels\":" << p.channels
            << ",\"series\":" << r.metadata->series << ",\"z\":" << p.z << ",\"time\":" << p.t
            << ",\"first_channel\":" << p.first_channel << ",\"x\":" << p.region.x
            << ",\"y\":" << p.region.y << ",\"level\":" << p.region.level
            << ",\"valid_pixels\":" << r.valid_pixels << ",\"raw_bytes\":" << r.raw_bytes;
        if(r.project) {
            out << ",\"project\":" << project_source_json(*r.project).dump();
            if(r.project->axes)
                out << ",\"project_axes\":" << project_axes_json(*r.project->axes).dump();
        }
        out << "}\n";
        return out.str();
    }
    Json result{{"schema", r.with_validity ? "qpx.raw-region/3" : "qpx.raw-region/2"},
                {"reader", r.reader},
                {"dtype", sample_name(r.metadata->sample_type)},
                {"width", p.region.width},
                {"height", p.region.height},
                {"channels", p.channels},
                {"series", r.metadata->series},
                {"z", p.z},
                {"time", p.t},
                {"first_channel", p.first_channel},
                {"x", p.region.x},
                {"y", p.region.y},
                {"level", p.region.level},
                {"valid_pixels", r.valid_pixels},
                {"raw_bytes", r.raw_bytes}};
    if (r.with_validity)
        result["array_layout"] = "values-valid";
    if(r.project) {
        result["project"]=project_source_json(*r.project);
        if(r.project->axes)
            result["project_axes"]=project_axes_json(*r.project->axes);
    }
    if (r.reader == "ome-zarr") {
        if (!r.output_grid)
            throw std::invalid_argument("OME-Zarr raw export report is missing the selected output grid");
        result["reader_source"] = Json::parse(reader_source_json(*r.source));
        result["level_downsample"] = r.output_grid->downsample;
        result["coordinate_space"] = r.output_grid->level_coordinates ? "level-array" : "level-zero";
        result["source_grid"] = {{"dataset", r.output_grid->dataset},
                                 {"level_x", r.output_grid->level_x}, {"level_y", r.output_grid->level_y},
                                 {"axes", r.output_grid->axes}, {"units", r.output_grid->units},
                                 {"scale", r.output_grid->scale}, {"translation", r.output_grid->translation}};
        if (r.output_grid->pixel_width_um)
            result["pixel_width_um"] = *r.output_grid->pixel_width_um;
        if (r.output_grid->pixel_height_um)
            result["pixel_height_um"] = *r.output_grid->pixel_height_um;
        if (r.metadata->z_spacing_um)
            result["z_spacing_um"] = *r.metadata->z_spacing_um;
        if (r.metadata->time_interval_seconds)
            result["time_interval_seconds"] = *r.metadata->time_interval_seconds;
    } else if (!r.with_validity || !r.reader_snapshot_json.empty())
        result["reader_snapshot"] = Json::parse(r.reader_snapshot_json);
    return result.dump() + '\n';
}
namespace {
std::string inspect_raw_region_bundle(const std::string& zip,const std::string* output,
                                     const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto path=detail::export_path(zip);
    const std::vector<std::string> protected_paths{path};
    std::string destination;
    if(output) {
        destination=detail::export_path(*output);
        detail::require_new_export(destination);
        detail::guard_export_output(destination,protected_paths);
    }
    const detail::ReaderFileGuard guard(path);
    const auto size=guard.version()->bytes;
    if(size>bundle_array_limit+bundle_receipt_limit+2*bundle_framing_limit)
        invalid_bundle("archive exceeds its size limit");
    detail::stored_zip::Input input(std::filesystem::u8path(path),size,cancelled,"raw region bundle");
    const auto array=input.member("raw.npy",bundle_array_limit+bundle_framing_limit);
    const auto array_offset=input.offset();
    input.seek(array_offset+array.size);
    const auto provenance=input.member("receipt.json",bundle_receipt_limit);
    std::string text(provenance.size,'\0');
    input.read(text.data(),text.size());
    detail::stored_zip::Checksum receipt_crc{cancelled};
    receipt_crc.write(text.data(),text.size());
    if(~receipt_crc.crc!=provenance.crc)invalid_bundle("receipt CRC differs");
    input.finish(std::array{array,provenance});

    detail::StrictJson strict("raw region bundle receipt");
    Json::sax_parse(text,&strict);
    auto receipt=Json::parse(text);
    const auto version=receipt.at("schema");
    const bool project_bound=version=="qpx.raw-region-bundle/3";
    const bool bound=project_bound || version=="qpx.raw-region-bundle/2";
    if((!bound && version!="qpx.raw-region-bundle/1") || receipt.at("array")!="raw.npy" ||
       !receipt.at("reader_source").is_object() || !receipt.at("calibration").is_object() || receipt.dump()+"\n"!=text)
        invalid_bundle("unsupported or noncanonical receipt");
    const auto& region=receipt.at("region");
    if(project_bound!=region.contains("project"))invalid_bundle("inconsistent project source");
    if(project_bound)verify_bundle_project(region.at("project"));
    if(region.contains("project_axes")) {
        if(!project_bound)invalid_bundle("unexpected project axis calibration");
        verify_bundle_project_axes(region);
    }
    const auto& source=receipt.at("reader_source");
    if((region.contains("reader_source") && region.at("reader_source")!=source) ||
       (region.contains("reader_snapshot") && region.at("reader_snapshot").at("observed")!=source))
        invalid_bundle("inconsistent observed source");
    if(bound) {
        const auto digest=receipt.at("receipt_sha256").get<std::string>();
        receipt.erase("receipt_sha256");
        if(sha256(receipt.dump())!=digest)invalid_bundle("receipt digest differs");
    }
    input.seek(array_offset);
    std::optional<detail::AtomicExportFile> extracted;
    if(output)extracted.emplace(destination,array.size,cancelled);
    verify_bundle_array(input,array,receipt,bound,cancelled,extracted?&*extracted:nullptr);
    guard.verify();
    workflow_checkpoint(cancelled);
    if(extracted)
        detail::commit_export_output(*extracted,destination,ExportWritePolicy::CreateNew,protected_paths);
    return text;
}
} // namespace
std::string verify_raw_region_bundle(const std::string& zip,const std::atomic<bool>* cancelled) {
    return inspect_raw_region_bundle(zip,nullptr,cancelled);
}
std::string extract_raw_region_bundle(const std::string& zip,const std::string& output,
                                      const std::atomic<bool>* cancelled) {
    return inspect_raw_region_bundle(zip,&output,cancelled);
}
namespace {
void publish_region_pam(const PixelBuffer& pixels, const std::string& destination,
                        const std::shared_ptr<const ReaderSource>& source,
                        const std::atomic<bool>* cancelled,
                        const std::function<void(const std::string&)>& before_commit = {}) {
    workflow_checkpoint(cancelled);
    constexpr std::size_t maximum = 256u * 1024 * 1024;
    if (pixels.width() <= 0 || pixels.height() <= 0 ||
        std::uint64_t(pixels.width()) > maximum / 4 ||
        std::uint64_t(pixels.height()) > maximum / (std::uint64_t(pixels.width()) * 4) ||
        pixels.bytes().size() != std::uint64_t(pixels.width()) * pixels.height() * 4)
        throw std::invalid_argument("PAM export requires a complete bounded RGBA8 display buffer");
    std::vector<std::string> protected_paths;
    if (source) {
        source->validate();
        if (!source->locator.empty())
            protected_paths.push_back(source->locator);
        if (source->primary)
            protected_paths.push_back(source->primary->path);
        for (const auto& file : source->companions)
            protected_paths.push_back(file.path);
    }
    detail::guard_export_output(destination, protected_paths);
    detail::require_new_export(destination);
    const auto header = "P7\nWIDTH " + std::to_string(pixels.width()) + "\nHEIGHT " +
                        std::to_string(pixels.height()) +
                        "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    detail::AtomicExportFile file(destination, pixels.bytes().size() + header.size(), cancelled);
    file.write(header.data(), header.size());
    file.write(reinterpret_cast<const char*>(pixels.bytes().data()), pixels.bytes().size());
    workflow_checkpoint(cancelled);
    if(before_commit)before_commit(header);
    detail::commit_export_output(file, destination, ExportWritePolicy::CreateNew, protected_paths);
}
std::string project_display_region(const std::string& path,std::uint64_t image,
                                  const RegionRequest& request,const std::string& output,
                                  bool with_receipt,const std::atomic<bool>* cancelled) {
    const detail::WorkflowInput input(path,1024ull*1024*1024,true,cancelled);
    const auto project=Project::open_without_journal_recovery(input.path());
    const auto server=project->document(image)->server();
    const auto catalog=project->images();
    const auto destination=detail::export_path(output);
    guard_project_region_output(input,*project,catalog,destination,cancelled);
    detail::require_new_export(destination);
    const auto verified=project->source_verification(image);
    const detail::FormatImportImageGuard observed(server,cancelled,verified.get());
    observed.protect(destination);
    const auto pixels=server->read_region(request);
    const auto source=server->reader_source();
    std::string receipt;
    publish_region_pam(pixels,destination,source,cancelled,[&](const std::string& header) {
        if(with_receipt) {
            Sha256 digest;
            digest.update(header.data(),header.size());
            const auto& bytes=pixels.bytes();
            for(std::size_t at=0;at<bytes.size();) {
                workflow_checkpoint(cancelled);
                const auto count=std::min<std::size_t>(bytes.size()-at,65536);
                digest.update(bytes.data()+at,count);
                at+=count;
            }
            Json report{{"schema","qpx.project-display-region/1"},{"format","pam-rgba8"},
                        {"output",destination},{"sha256",digest.finish()},
                        {"bytes",header.size()+bytes.size()},
                        {"project",project_source_json({project->identity(),input.sha256(),
                                                       project->generation(),image})},
                        {"reader",server->reader_id()},
                        {"reader_source",Json::parse(reader_source_json(*source))},
                        {"region",{{"x",request.x},{"y",request.y},{"level",request.level},
                                   {"width",pixels.width()},{"height",pixels.height()}}}};
            if(const auto view=server->raw_view())
                report["view"]={{"series",view->series},{"z",view->z},{"t",view->t},
                                {"channels",view->channels},{"lower",view->lower},{"upper",view->upper}};
            receipt=report.dump()+'\n';
        }
        guard_project_region_output(input,*project,catalog,destination,cancelled);
        observed.verify(cancelled);
        observed.protect(destination);
    });
    return receipt;
}
} // namespace
void save_region_pam(const PixelBuffer& pixels, const std::string& output,
                     const std::shared_ptr<const ReaderSource>& source,
                     const std::atomic<bool>* cancelled) {
    publish_region_pam(pixels,detail::export_path(output),source,cancelled);
}
void export_project_display_region(const std::string& path,std::uint64_t image,
                                   const RegionRequest& request,const std::string& output,
                                   const std::atomic<bool>* cancelled) {
    project_display_region(path,image,request,output,false,cancelled);
}
std::string export_project_display_region_with_receipt(const std::string& path,std::uint64_t image,
    const RegionRequest& request,const std::string& output,const std::atomic<bool>* cancelled) {
    return project_display_region(path,image,request,output,true,cancelled);
}
} // namespace qpx
