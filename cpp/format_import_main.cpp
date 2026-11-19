#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "qupathxx/format_import.hpp"
#include "qupathxx/backed_spatial_import.hpp"
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/provenance.hpp"
#include "qupathxx/qupath_project.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "format_import_internal.hpp"
#include "matrix_format_registry.hpp"
#include "expression_matrix_export.hpp"
#include "expression_matrix_project_internal.hpp"
#include "expression_matrix_external_save.hpp"
#include "project_plugin.hpp"
#include "project_export_guard.hpp"
#include "project_source_internal.hpp"
#include "reader_plugin.hpp"
#include "reader_plugin_link.hpp"
#include "reader_registry.hpp"
#include "workflow_input.hpp"
#include "reader_file_guard.hpp"
#include "spatial_output_guard.hpp"
#include "sha256.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <charconv>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <set>

namespace {
using nlohmann::json;
std::atomic<bool> cancelled{false};
void interrupt(int){cancelled.store(true);}
#ifdef _WIN32
BOOL WINAPI console_interrupt(DWORD event){if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT){cancelled.store(true);return TRUE;}return FALSE;}
#endif
std::uint64_t native_id(const std::string& value,const char* error){
    std::uint64_t n=0;
    const auto p=std::from_chars(value.data(),value.data()+value.size(),n);
    if(p.ec!=std::errc{} || p.ptr!=value.data()+value.size() || !n)throw std::invalid_argument(error);
    return n;
}
std::uint64_t image_id(const std::string& value){
    return native_id(value,"IMAGE_ID must be a positive native image ID");
}
json provider_json(const qpx::FormatPluginInfo& p){
    json dependencies=json::array();for(const auto& d:p.dependencies)dependencies.push_back({{"name",d.name},{"sha256",d.sha256}});
    return {{"domain",p.domain==qpx::FormatPluginDomain::Matrix?"matrix":"annotation"},{"provider_id",p.provider_id},{"format_id",p.format_id},{"display_name",p.display_name},{"version",p.version},{"contract",p.contract},{"profile",p.profile},{"abi",p.abi},{"module_sha256",p.module_sha256},{"implementation_sha256",p.implementation_sha256},{"dependencies",std::move(dependencies)},{"default_options",p.default_options}};
}
std::map<std::string,std::string> class_map(const std::string& value){
    if(value.size()>65536)throw std::length_error("Class-map JSON exceeds64KiB");std::set<std::string> keys;
    auto parsed=json::parse(value,[&](int depth,json::parse_event_t event,json& item){if(depth>2)throw std::invalid_argument("Class-map JSON must be flat");if(event==json::parse_event_t::key){const auto key=item.get<std::string>();if(!keys.insert(key).second)throw std::invalid_argument("Duplicate class-map JSON key");if(keys.size()>4096)throw std::length_error("Class-map JSON exceeds4096 labels");}return true;});
    if(!parsed.is_object())throw std::invalid_argument("Class-map JSON must be a flat label-to-native-class-key object");
    std::map<std::string,std::string> result;for(auto it=parsed.begin();it!=parsed.end();++it){if(!it.value().is_string())throw std::invalid_argument("Class-map values must be existing native class-key strings");result.emplace(it.key(),it.value().get<std::string>());}return result;
}
enum class SourceOptionsFor {ProjectImage, MatrixImage, MatrixProject};
struct SourceCliOptions {
    std::optional<std::string> format_options;
    std::optional<std::string> reader;
    std::optional<std::string> reader_options;
    std::optional<std::uint32_t> tiff_series;
    qpx::ObjectPlane matrix_plane;
};
SourceCliOptions source_options(const std::vector<std::string>& args,std::size_t start,SourceOptionsFor target){
    SourceCliOptions options;
    std::set<std::string> plane_flags;
    const char* command=target==SourceOptionsFor::ProjectImage?args[0].c_str():"matrix import";
    for(auto at=start;at<args.size();at+=2){
        const auto& name=args[at];
        if(target!=SourceOptionsFor::ProjectImage &&
           (name=="--frame-channel" || name=="--frame-z" || name=="--frame-time")) {
            if(!plane_flags.insert(name).second)throw std::invalid_argument("Duplicate matrix import option: "+name);
            const auto& text=args[at+1];
            std::int32_t index=0;
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),index);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || index>65535 ||
               index<(name=="--frame-channel"?-1:0))
                throw std::invalid_argument("Invalid matrix frame plane index for "+name);
            if(name=="--frame-channel")options.matrix_plane.c=index;
            else if(name=="--frame-z")options.matrix_plane.z=std::uint32_t(index);
            else options.matrix_plane.t=std::uint32_t(index);
            continue;
        }
        if(target!=SourceOptionsFor::MatrixProject && name=="--series") {
            if(options.tiff_series)throw std::invalid_argument(std::string("Duplicate ")+command+" option: "+name);
            const auto& text=args[at+1];
            std::uint32_t series=0;
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),series);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || series>65535)
                throw std::invalid_argument("--series requires a TIFF series index from 0 to 65535");
            options.tiff_series=series;
            continue;
        }
        auto* value=&options.format_options;
        if(target!=SourceOptionsFor::MatrixProject && name=="--reader")value=&options.reader;
        else if(target!=SourceOptionsFor::MatrixProject && name=="--reader-options")value=&options.reader_options;
        else if(target==SourceOptionsFor::ProjectImage || name!="--plugin-options-json")
            throw std::invalid_argument(std::string("Unknown ")+command+" option: "+name);
        if(*value)throw std::invalid_argument(std::string("Duplicate ")+command+" option: "+name);
        *value=args[at+1];
    }
    if(options.reader && options.reader->empty())
        throw std::invalid_argument("--reader requires a reader ID");
    if(options.reader_options && (!options.reader || !qpx::detail::valid_reader_plugin_id(*options.reader)))
        throw std::invalid_argument("--reader-options requires an explicit native reader plugin");
    return options;
}
std::shared_ptr<qpx::ImageServer> open_source_image(const std::string& path,const SourceCliOptions& options){
    const auto selected=qpx::detail::select_reader(path,options.reader.value_or("auto"),
        qpx::detail::ReaderPolicy::Project,qpx::detail::reader_features());
    qpx::detail::ReaderOpenOptions opening;
    if(options.tiff_series) {
        if(selected.provider_id!="tiff")throw std::invalid_argument("--series requires the native TIFF reader");
        opening.tiff.view.series=*options.tiff_series;
    }
    if(options.reader_options)
        return qpx::detail::open_reader_with_plugin_options(path,selected,*options.reader_options,&cancelled);
    return qpx::detail::open_reader(path,selected,opening,&cancelled);
}
json observed_file(const qpx::FileVersion& file){
    return {{"path",file.path},{"bytes",file.bytes},{"identity_scheme",file.identity_scheme},
        {"identity",file.identity},{"stamp_scheme",file.stamp_scheme},{"modified_token",file.modified_token},
        {"change_token",file.change_token}};
}
json source_review_report(const qpx::detail::WorkflowInput& project,const qpx::ProjectSourceReview& review){
    const auto association=review.association();
    const auto& source=association->source;
    json companions=json::array();
    for(const auto& file:source.companions)companions.push_back(observed_file(file));
    json report={{"schema","qpx.project-source-review-cli/1"},{"input_project",project.path()},
        {"project_sha256",project.sha256()},{"image_id",review.image_id()},{"source",review.path()},
        {"comparison",review.comparison()},{"coverage",review.coverage()},
        {"requires_acceptance",review.requires_acceptance()},{"reader",source.reader},{"scope",source.scope},
        {"primary",observed_file(*source.primary)},{"primary_sha256",association->fingerprint.sha256},
        {"companions",std::move(companions)},{"directory_sha256",source.directory_sha256}};
    const auto text=report.dump();
    qpx::Sha256 digest;
    digest.update(text.data(),text.size());
    report["review_sha256"]=digest.finish();
    return report;
}
json interchange_issues(const std::vector<qpx::InterchangeIssue>& list) {
    auto issues=json::array();
    for(const auto& item:list)issues.push_back({{"code",item.code},{"message",item.message},{"count",item.count}});
    return issues;
}
int qupath_project_command(const std::vector<std::string>& args) {
    const bool importing=args[0]=="qupath-project-import";
    const std::size_t positional=importing?3:2;
    if(args.size()<positional)
        throw std::invalid_argument(importing?"qupath-project-import requires PROJECT.qpproj NEW_PROJECT":
                                              "qupath-project-preview requires PROJECT.qpproj");
    qpx::QuPathProjectImportOptions options;
    std::map<std::string,std::string> reader_options;
    std::string expected_review;
    bool losses=false;
    for(std::size_t at=positional;at<args.size();++at) {
        const auto& flag=args[at];
        if(flag=="--source" && at+2<args.size()) {
            const auto id=std::to_string(image_id(args[++at]));
            const auto& path=args[++at];
            if(path.empty() || !options.source_overrides.emplace(id,path).second)
                throw std::invalid_argument("--source requires a unique entry ID and nonempty path");
        } else if(flag=="--source-reader" && at+2<args.size()) {
            const auto id=std::to_string(image_id(args[++at]));
            if(!options.reader_overrides.emplace(id,qpx::QuPathProjectReaderOverride{args[++at],{}}).second)
                throw std::invalid_argument("--source-reader requires a unique entry ID");
        } else if(flag=="--source-reader-options" && at+2<args.size()) {
            const auto id=std::to_string(image_id(args[++at]));
            if(!reader_options.emplace(id,args[++at]).second)
                throw std::invalid_argument("--source-reader-options requires a unique entry ID");
        } else if(flag=="--exchange" && options.exchange_directory.empty() && at+1<args.size() && !args[at+1].empty()) {
            options.exchange_directory=args[++at];
        } else if(importing && flag=="--expect-review" && expected_review.empty() && at+1<args.size()) {
            expected_review=args[++at];
            if(expected_review.size()!=64 || expected_review.find_first_not_of("0123456789abcdef")!=std::string::npos)
                throw std::invalid_argument("--expect-review requires a lowercase SHA-256 digest");
        } else if(importing && flag=="--allow-losses" && !losses) losses=true;
        else throw std::invalid_argument("Unknown, duplicate or incomplete QuPath project option: "+flag);
    }
    for(auto& [id,value]:reader_options) {
        const auto reader=options.reader_overrides.find(id);
        if(reader==options.reader_overrides.end())
            throw std::invalid_argument("--source-reader-options requires --source-reader for the same entry ID");
        reader->second.options=std::move(value);
    }
    const auto preview=qpx::preview_qupath_project_with_options(args[1],options,&cancelled);
    json images=json::array();
    for(const auto& item:preview->images())
        images.push_back({{"source_id",item.source_id},{"native_id",item.native_id},{"name",item.name},
                          {"source_path",item.source_path},{"reader",item.reader},{"available",item.available},
                          {"objects",item.objects},{"source_object_ids",item.source_object_ids},
                          {"imported_object_ids",item.imported_object_ids}});
    json report={{"schema","qpx.qupath-project-review-cli/1"},{"source",preview->source_path()},
                 {"observed_inputs_sha256",preview->observed_inputs_sha256()},
                 {"source_tree_sha256",qpx::build_environment()->source_tree_sha256},
                 {"images",std::move(images)},{"issues",interchange_issues(preview->issues())}};
    const auto description=report.dump();
    qpx::Sha256 hash;
    hash.update(description.data(),description.size());
    const auto review=hash.finish();
    if(!expected_review.empty() && expected_review!=review)
        throw qpx::WorkflowInputChanged("QuPath project review changed; preview again");
    report["review_sha256"]=review;
    report["state"]=importing?"saved":"reviewed";
    if(importing) {
        const auto output=qpx::detail::export_path(args[2]);
        preview->import_new(output,losses,&cancelled);
        report["output"]=output;
    }
    std::cout<<report.dump()<<'\n';
    return 0;
}
json project_raw_view_json(const qpx::TiffView& view) {
    return {{"series",view.series},{"z",view.z},{"t",view.t},{"channels",view.channels},
            {"lower",view.lower},{"upper",view.upper}};
}
template<class T> std::array<T,3> project_display_triplet(std::string_view value,const std::string& flag) {
    std::array<T,3> result{};
    for(std::size_t i=0;i<result.size();++i) {
        const auto comma=value.find(',');
        const auto component=value.substr(0,comma);
        const auto parsed=std::from_chars(component.data(),component.data()+component.size(),result[i]);
        if(parsed.ec!=std::errc{} || parsed.ptr!=component.data()+component.size() ||
           !std::isfinite(result[i]) || (comma!=std::string_view::npos)!=(i+1<result.size()))
            throw std::invalid_argument(flag+" requires three comma-separated finite values in RGB order");
        if(comma!=std::string_view::npos)value.remove_prefix(comma+1);
    }
    return result;
}
void protect_project_output(const qpx::detail::WorkflowInput& input,const qpx::Project& project,
                            const std::vector<qpx::ProjectImage>& images,const std::string& output) {
    using namespace qpx::detail;
    input.verify(false,&cancelled);
    for(const auto* suffix:{"","-journal","-wal","-shm"})
        guard_file_output(input.path()+suffix,"",output);
    guard_project_export_output(project,images,output,&cancelled);
}
int project_set_view(const std::vector<std::string>& args) {
    using namespace qpx::detail;
    if(args.size()<6 || (args.size()-4)%2)
        throw std::invalid_argument("project-set-view requires INPUT_PROJECT IMAGE_ID NEW_PROJECT and view options");
    std::optional<std::uint32_t> z,time;
    std::optional<std::array<std::int32_t,3>> channels;
    std::optional<std::array<double,3>> lower,upper;
    std::set<std::string> seen;
    for(std::size_t at=4;at<args.size();at+=2) {
        const auto& flag=args[at];
        const auto& value=args[at+1];
        if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate project view option: "+flag);
        if(flag=="--display-channels") {channels=project_display_triplet<std::int32_t>(value,flag);continue;}
        if(flag=="--display-lower") {lower=project_display_triplet<double>(value,flag);continue;}
        if(flag=="--display-upper") {upper=project_display_triplet<double>(value,flag);continue;}
        if(flag!="--z" && flag!="--time")
            throw std::invalid_argument("Unknown project view option: "+flag);
        auto& axis=flag=="--z"?z:time;
        std::uint32_t index=0;
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),index);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || index>=65536)
            throw std::invalid_argument("Project view index must be 0..65535");
        axis=index;
    }
    const auto id=image_id(args[2]);
    const auto output=export_path(args[3]);
    const WorkflowInput input(args[1],1024ull*1024*1024,true,&cancelled);
    const auto project=qpx::Project::open_without_journal_recovery(input.path());
    const auto document=project->document(id);
    const auto current=document->server()->raw_view();
    if(!current)throw std::invalid_argument("Project image has no saved TIFF or OME-Zarr view");
    const auto images=project->images();
    protect_project_output(input,*project,images,output);
    if(std::filesystem::exists(std::filesystem::u8path(output)))
        throw qpx::WorkflowOutputExists("Project output already exists");
    const auto verified=project->source_verification(id);
    const FormatImportImageGuard observed(document->server(),&cancelled,verified.get());
    observed.protect(output);
    auto view=*current;
    if(z)view.z=*z;
    if(time)view.t=*time;
    if(channels)view.channels=*channels;
    if(lower)view.lower=*lower;
    if(upper)view.upper=*upper;
    view.validate(true);
    document->set_raw_view(view);
    const auto saved=document->server()->raw_view();
    project->save_new(output,512ull*1024*1024,1024ull*1024*1024,&cancelled,[&]{
        protect_project_output(input,*project,images,output);
        observed.verify(&cancelled);
        observed.protect(output);
    });
    std::cout<<json{{"schema","qpx.format-import-cli/1"},{"command",args[0]},{"state","saved"},
                    {"output",output},{"image_id",id},{"view",project_raw_view_json(*saved)}}.dump()<<'\n';
    return 0;
}
int project_images(const std::vector<std::string>& args,bool has_reader_manifests) {
    using qpx::detail::WorkflowInput;
    if(args.size()!=2)throw std::invalid_argument("project-images requires INPUT_PROJECT");
    const WorkflowInput input(args[1],1024ull*1024*1024,true,&cancelled);
    const auto project=qpx::Project::open_without_journal_recovery(input.path());
    json diagnostics=json::array();
    if(has_reader_manifests)diagnostics=qpx::detail::reader_plugin_catalog().diagnostics;
    json images=json::array();
    for(const auto& image:project->images()) {
        qpx::workflow_checkpoint(&cancelled);
        const auto document=project->document(image.id);
        json row={{"image_id",image.id},{"name",image.name},{"reader",image.reader},
                  {"width",image.width},{"height",image.height},{"source",image.source_path},
                  {"embedded",image.embedded},{"available",image.available},{"objects",document->object_count()}};
        if(const auto view=document->server()->raw_view())
            row["view"]=project_raw_view_json(*view);
        if(const auto link=qpx::detail::plugin_link_of(*document->server())) {
            const auto& options=link->implementation.options;
            row["reader_plugin"]={{"options",options.empty()?json::object():json::parse(options)},
                                  {"version",link->implementation.version},
                                  {"binding",link->observed.reader}};
        }
        images.push_back(std::move(row));
    }
    input.verify(false,&cancelled);
    std::cout<<json{{"schema","qpx.format-import-cli/1"},{"command",args[0]},{"state","listed"},
                    {"input_project",input.path()},{"active_image_id",project->active_image()},
                    {"images",std::move(images)},{"reader_diagnostics",std::move(diagnostics)}}.dump()<<'\n';
    return 0;
}
json matrix_metadata(const qpx::ExpressionMatrixSource& source,const std::string& command) {
    constexpr auto work_bytes = 64u*1024*1024;
    const auto& metadata=source.metadata();
    json report{{"schema","qpx.format-import-cli/1"},{"state","metadata"},{"command",command},
        {"input",source.path()},{"file_sha256",source.file_sha256()},{"source_sha256",metadata.source_sha256},
        {"features",metadata.features},{"observations",metadata.observations},{"entries",metadata.entries},
        {"domain",metadata.domain==qpx::ExpressionValueDomain::Counts?"counts":"continuous"},
        {"value_domain",metadata.value_domain},{"input_profile",metadata.input_profile},{"receipt",nullptr}};
    if(const auto& frame=metadata.spatial_frame) {
        report["frame_sha256"]=frame->sha256();
        json description{{"id",frame->id},{"source_id",frame->source_id},
            {"unit",frame->unit==qpx::CoordinateUnit::Pixel?"pixel":"micrometre"},
            {"plane",{{"channel",frame->plane.c},{"z",frame->plane.z},{"time",frame->plane.t}}},
            {"image_level_zero",frame->image_level_zero},{"width",frame->width},{"height",frame->height}};
        if(frame->micrometres_per_pixel)
            description["micrometres_per_pixel"]=*frame->micrometres_per_pixel;
        report["spatial_frame"]=std::move(description);
    }
    if(const auto receipt=qpx::external_expression_matrix_receipt(source,work_bytes,&cancelled)) {
        report["receipt"]=json::parse(qpx::format_plugin_receipt_json(receipt->reader));
        report["positions_sha256"]=receipt->positions_sha256;
        report["position_columns"]=receipt->position_columns;
    }
    if(const auto store=qpx::external_expression_matrix_store_receipt(source,work_bytes,&cancelled)) {
        report["source_scope"]="directory_store";
        report["primary_key"]=store->primary_key;
        report["store_observed_sha256"]=store->observed_sha256;
        report["store_content_sha256"]=store->content_sha256;
        report["store_files"]=store->files;
        report["store_bytes"]=store->bytes;
    }
    return report;
}
int matrix_market_index(const std::vector<std::string>& args) {
    using namespace qpx::detail;
    if(args.size()!=7 || args[5]!="--catalogue" || (args[6]!="qpx" && args[6]!="tenx"))
        throw std::invalid_argument("matrix-market-index requires MATRIX_MTX FEATURES_TSV OBSERVATIONS_TSV NEW_QMX --catalogue qpx|tenx");
    MatrixMarketImportRequest request{args[1],args[2],args[3],args[4],{}};
    request.options.format=args[6]=="tenx"?qpx::ExpressionCatalogueFormat::Tenx:qpx::ExpressionCatalogueFormat::Qpx;
    const auto indexed=import_matrix_format(select_matrix_format(MatrixFormat::MatrixMarket),request,&cancelled);
    const auto& metadata=indexed.metadata;
    std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","indexed"},{"command",args[0]},
        {"output",indexed.path},{"file_sha256",indexed.file_sha256},{"source_sha256",metadata.source_sha256},
        {"features",metadata.features},{"observations",metadata.observations},{"entries",metadata.entries},
        {"domain","counts"},{"value_domain",metadata.value_domain},{"input_profile",metadata.input_profile}}.dump()<<'\n';
    return 0;
}
int project_matrix_import_positions(const std::vector<std::string>& args) {
    if(args.size()<6 || (args.size()-6)%2)
        throw std::invalid_argument("project-matrix-import-positions requires INPUT_PROJECT IMAGE_ID QMX POSITIONS_TSV NEW_PROJECT");
    const auto selection=source_options(args,6,SourceOptionsFor::MatrixProject);
    if(selection.format_options)
        throw std::invalid_argument("Position import accepts frame options only");
    qpx::BackedSpatialImportRequest request;
    request.base.input_project=args[1];
    request.base.image_id=image_id(args[2]);
    request.store=args[3];
    request.positions=args[4];
    request.base.output_project=args[5];
    request.base.plane=selection.matrix_plane;
    const auto preview=qpx::preview_backed_spatial_import(request,&cancelled);
    std::cout<<qpx::publish_backed_spatial_import(*preview,&cancelled)<<'\n';
    return 0;
}
int project_matrices(const std::vector<std::string>& args) {
    const bool inspect=args[0]=="project-matrix-info";
    if(args.size()!=(inspect?4u:3u))
        throw std::invalid_argument(inspect?"project-matrix-info requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID":
                                             "project-matrices requires INPUT_PROJECT IMAGE_ID");
    const auto id=image_id(args[2]);
    const qpx::detail::WorkflowInput input(args[1],1024ull*1024*1024,true,&cancelled);
    const auto project=qpx::Project::open_without_journal_recovery(input.path());
    json report;
    if(inspect) {
        const auto request=qpx::expression_matrix_open_request(*project,id,args[3]);
        const auto source=request.open(64u*1024*1024,&cancelled);
        report=matrix_metadata(*source,args[0]);
        report["association_id"]=request.association.source_sha256;
        report["owned"]=request.association.owned;
        source->require_current(&cancelled);
    } else {
        json matrices=json::array();
        for(const auto& association_id:qpx::expression_matrix_ids(*project,id)) {
            qpx::workflow_checkpoint(&cancelled);
            const auto request=qpx::expression_matrix_open_request(*project,id,association_id);
            const auto& association=request.association;
            matrices.push_back({{"association_id",association_id},{"path",request.path},
                {"owned",association.owned},{"file_sha256",association.file_sha256},
                {"source_sha256",association.source_sha256},{"features",association.features},
                {"observations",association.observations},{"entries",association.entries}});
        }
        report={{"schema","qpx.format-import-cli/1"},{"command",args[0]},{"state","listed"},
                {"matrices",std::move(matrices)}};
    }
    input.verify(false,&cancelled);
    report["input_project"]=input.path();
    report["image_id"]=id;
    std::cout<<report.dump()<<'\n';
    return 0;
}
int project_matrix_export(const std::vector<std::string>& args) {
    using namespace qpx::detail;
    const bool positions=args[0]=="project-matrix-export-positions";
    const bool backup=args[0]=="project-matrix-backup";
    const bool market=args[0]=="project-matrix-export-mtx";
    const bool saved_frame=positions && args.size()>5;
    qpx::ObjectPlane plane;
    if(positions && (args.size()<5 || (saved_frame && (args.size()<7 || (args.size()-7)%2 ||
        args[5]!="--frame-id" || args[6].empty()))))
        throw std::invalid_argument("project-matrix-export-positions requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_TSV [--frame-id ID and optional plane]");
    if(saved_frame) {
        const auto selection=source_options(args,7,SourceOptionsFor::MatrixProject);
        if(selection.format_options)throw std::invalid_argument("Position export accepts frame options only");
        plane=selection.matrix_plane;
    }
    if(backup && args.size()!=5)
        throw std::invalid_argument("project-matrix-backup requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_QMX");
    if(market && args.size()!=5)
        throw std::invalid_argument("project-matrix-export-mtx requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_ZIP");
    if(!positions && !backup && !market && (args.size()<7 || (args.size()-5)%2))
        throw std::invalid_argument("project-matrix-export-csv requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_CSV --feature ID [...]");
    std::vector<std::string> features;
    for(std::size_t at=5;!positions && at<args.size();at+=2) {
        if(args[at]!="--feature")throw std::invalid_argument("Expected --feature ID");
        features.push_back(args[at+1]);
    }
    const auto id=image_id(args[2]);
    const WorkflowInput input(args[1],1024ull*1024*1024,true,&cancelled);
    const auto project=qpx::Project::open_without_journal_recovery(input.path());
    const auto source=qpx::reopen_expression_matrix(*project,id,args[3],64u*1024*1024,&cancelled);
    const auto output=export_path(args[4]);
    const auto images=project->images();
    const auto protect=[&] {
        protect_project_output(input,*project,images,output);
    };
    std::size_t bytes;
    std::optional<qpx::CoordinateFrame> position_frame;
    if(backup)bytes=backup_expression_matrix(*source,output,&cancelled,protect);
    else if(saved_frame) {
        if(source->metadata().observations>qpx::SpatialPositionOptions{}.max_observations)
            throw qpx::WorkflowCaptureLimit("Position TSV export exceeds 100000 observations");
        const auto document=project->document(id);
        const auto verified=project->source_verification(id);
        const FormatImportImageGuard image(document->server(),&cancelled,verified.get());
        const auto snapshot=qpx::ExpressionMatrixSpatialSnapshot::capture(*document,source,args[6],plane,
            128u*1024*1024,&cancelled);
        bytes=export_expression_matrix_positions(snapshot,output,&cancelled,[&]{
            protect();image.protect(output);image.verify(&cancelled);
        });
        position_frame=snapshot.frame();
    } else if(positions) {
        bytes=export_expression_matrix_positions(*source,output,&cancelled,protect);
        position_frame=source->metadata().spatial_frame;
    }
    else if(market)bytes=export_expression_matrix_market(*source,output,&cancelled,protect);
    else bytes=export_expression_matrix_csv(*source,output,features,&cancelled,protect);
    const auto& metadata=source->metadata();
    json report{{"schema","qpx.format-import-cli/1"},{"state","exported"},{"command",args[0]},
        {"input_project",input.path()},{"image_id",id},{"association_id",metadata.source_sha256},
        {"output",output},{"bytes",bytes},{"observations",metadata.observations},
        {"matrix_file_sha256",source->file_sha256()}};
    if(positions) {
        const auto& frame=*position_frame;
        report["frame_sha256"]=frame.sha256();
        if(saved_frame)report["coordinate_frame"]=frame.id;
        report["plane"]={{"channel",frame.plane.c},{"z",frame.plane.z},{"time",frame.plane.t}};
    } else if(market) {
        report["features"]=metadata.features;
        report["entries"]=metadata.entries;
        report["value_domain"]=metadata.value_domain;
    } else if(!backup) {
        report["feature_ids"]=features;
        report["value_domain"]=metadata.value_domain;
    }
    std::cout<<report.dump()<<'\n';
    return 0;
}
int project_objects_command(const std::vector<std::string>& args) {
    using namespace qpx::detail;
    const bool importing=args[0]=="geojson-import";
    const bool measurements=args[0]=="measurements-export";
    const std::size_t positional=importing?5:4;
    if(args.size()<positional)
        throw std::invalid_argument(importing?"geojson-import requires INPUT_PROJECT IMAGE_ID GEOJSON NEW_PROJECT":
                                              args[0]+(measurements?" requires INPUT_PROJECT IMAGE_ID NEW_CSV":
                                                                    " requires INPUT_PROJECT IMAGE_ID NEW_GEOJSON"));
    const auto id=image_id(args[2]);
    std::set<std::string> flags;
    std::set<std::uint64_t> object_ids;
    for(std::size_t at=positional;at<args.size();++at) {
        const auto& flag=args[at];
        if(measurements) {
            if(flag!="--object")throw std::invalid_argument("Unknown measurements export option: "+flag);
            if(++at==args.size())throw std::invalid_argument("--object requires an object ID");
            object_ids.insert(native_id(args[at],"--object must be a positive native object ID"));
            continue;
        }
        if(flag!="--allow-losses" && !(importing?(flag=="--assume-pixels" || flag=="--qupath-precision"):
                                                 flag=="--nested-hierarchy"))
            throw std::invalid_argument("Unknown GeoJSON option: "+flag);
        if(!flags.insert(flag).second)throw std::invalid_argument("Duplicate GeoJSON option: "+flag);
    }
    constexpr std::size_t text_limit=256ull*1024*1024, snapshot_limit=512ull*1024*1024;
    constexpr std::uint64_t project_limit=1024ull*1024*1024;
    const auto output=export_path(args[positional-1]);
    const WorkflowInput primary(args[1],project_limit,true,&cancelled);
    const auto project=qpx::Project::open_without_journal_recovery(primary.path());
    const auto document=project->document(id);
    const auto catalog=project->images();
    const auto protect=[&] {protect_project_output(primary,*project,catalog,output);};
    protect();
    if(std::filesystem::exists(std::filesystem::u8path(output)))
        throw qpx::WorkflowOutputExists(measurements?"Measurements CSV output already exists":
                                                     "GeoJSON command output already exists");

    const auto publish=[&](const std::string& bytes) {
        AtomicExportFile file(output,text_limit,&cancelled);
        file.write(bytes.data(),bytes.size());
        protect();
        file.commit(false);
    };
    if(measurements) {
        const std::vector<std::uint64_t> ids(object_ids.begin(),object_ids.end());
        const auto csv=qpx::measurements_csv(*document,ids);
        publish(csv);
        std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command",args[0]},
                        {"output",output},{"image_id",id},{"rows",ids.empty()?document->objects().size():ids.size()},
                        {"bytes",csv.size()}}.dump()<<'\n';
        return 0;
    }

    qpx::InterchangeReport result;
    if(importing) {
        const WorkflowInput input(args[3],text_limit,false,&cancelled);
        const auto verify=[&] {
            protect();
            input.verify(false,&cancelled);
            guard_file_output(input.path(),"",output);
        };
        verify();
        result=qpx::import_geojson(*document,read_workflow_input(input,&cancelled),flags.count("--assume-pixels"),
                                  flags.count("--allow-losses"),flags.count("--qupath-precision"));
        project->save_new(output,snapshot_limit,project_limit,&cancelled,verify);
    } else {
        result=qpx::export_geojson(*document,{},flags.count("--allow-losses"),flags.count("--nested-hierarchy"));
        publish(result.text);
    }
    json objects=json::array();
    for(std::size_t at=0;at<result.imported_ids.size();++at)
        objects.push_back({{"source_id",result.source_ids.at(at)},{"object_id",result.imported_ids[at]}});
    std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command",args[0]},
                    {"output",output},{"image_id",id},{"features",result.features},
                    {"issues",interchange_issues(result.issues)},{"objects",std::move(objects)}}.dump()<<'\n';
    return 0;
}
void help(){
    std::cout<<"Usage: qupathxx_format_import [--format-plugins ABS_MANIFEST] [--reader-plugins ABS_MANIFEST] catalog\n"
        "       qupathxx_format_import [bootstrap options] project-create IMAGE NEW_PROJECT [--reader ID] [--series N] [--reader-options JSON]\n"
        "       qupathxx_format_import [bootstrap options] project-add-image INPUT_PROJECT IMAGE NEW_PROJECT [--reader ID] [--series N] [--reader-options JSON]\n"
        "       qupathxx_format_import [bootstrap options] project-images INPUT_PROJECT\n"
        "       qupathxx_format_import [bootstrap options] project-set-view INPUT_PROJECT IMAGE_ID NEW_PROJECT [--z N] [--time N]\n"
        "           [--display-channels R,G,B] [--display-lower R,G,B] [--display-upper R,G,B]\n"
        "       qupathxx_format_import [bootstrap options] project-review-source INPUT_PROJECT IMAGE_ID PRIMARY_FILE [--directory ROOT]\n"
        "       qupathxx_format_import [bootstrap options] project-adopt-source INPUT_PROJECT IMAGE_ID PRIMARY_FILE REVIEW_SHA256 NEW_PROJECT [--directory ROOT] [--accept-changed]\n"
        "       qupathxx_format_import [bootstrap options] matrix-index PROVIDER INPUT POSITIONS IMAGE NEW_QMX [--plugin-options-json JSON]\n"
        "       qupathxx_format_import [bootstrap options] matrix-project-index PROVIDER INPUT POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX [--plugin-options-json JSON]\n"
        "       qupathxx_format_import [bootstrap options] matrix-store-index PROVIDER STORE PRIMARY_KEY POSITIONS IMAGE NEW_QMX [--plugin-options-json JSON]\n"
        "       qupathxx_format_import [bootstrap options] matrix-project-store-index PROVIDER STORE PRIMARY_KEY POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX [--plugin-options-json JSON]\n"
        "       qupathxx_format_import matrix-info QMX\n"
        "       qupathxx_format_import matrix-market-index MATRIX_MTX FEATURES_TSV OBSERVATIONS_TSV NEW_QMX --catalogue qpx|tenx\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-import-positions INPUT_PROJECT IMAGE_ID QMX POSITIONS_TSV NEW_PROJECT [--frame-z N] [--frame-time N]\n"
        "       qupathxx_format_import [bootstrap options] project-matrices INPUT_PROJECT IMAGE_ID\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-info INPUT_PROJECT IMAGE_ID ASSOCIATION_ID\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-export-csv INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_CSV --feature ID [...]\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-export-positions INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_TSV [--frame-id ID [--frame-channel N] [--frame-z N] [--frame-time N]]\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-export-mtx INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_ZIP\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-backup INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_QMX\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-restore INPUT_PROJECT IMAGE_ID ASSOCIATION_ID BACKUP_QMX\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-relink INPUT_PROJECT IMAGE_ID ASSOCIATION_ID EXACT_QMX NEW_PROJECT\n"
        "       qupathxx_format_import [bootstrap options] project-matrix-own INPUT_PROJECT IMAGE_ID ASSOCIATION_ID EXACT_QMX NEW_PROJECT\n"
        "       qupathxx_format_import [bootstrap options] matrix-attach INPUT_PROJECT IMAGE_ID QMX FILE_SHA256 NEW_PROJECT\n"
        "       qupathxx_format_import [bootstrap options] annotations-import PROVIDER INPUT_PROJECT IMAGE_ID ANNOTATIONS NEW_PROJECT [--class-map-json JSON]\n"
        "       qupathxx_format_import [bootstrap options] points-import PROVIDER INPUT_PROJECT IMAGE_ID POINTS NEW_PROJECT [--class-map-json JSON]\n"
        "       qupathxx_format_import [bootstrap options] geojson-import INPUT_PROJECT IMAGE_ID GEOJSON NEW_PROJECT [--assume-pixels] [--allow-losses] [--qupath-precision]\n"
        "       qupathxx_format_import [bootstrap options] geojson-export INPUT_PROJECT IMAGE_ID NEW_GEOJSON [--nested-hierarchy] [--allow-losses]\n"
        "       qupathxx_format_import [bootstrap options] measurements-export INPUT_PROJECT IMAGE_ID NEW_CSV [--object ID ...]\n"
        "       qupathxx_format_import [bootstrap options] qupath-project-preview PROJECT.qpproj [--source ENTRY_ID IMAGE] [--exchange DIRECTORY]\n"
        "       qupathxx_format_import [bootstrap options] qupath-project-import PROJECT.qpproj NEW_PROJECT [--source ENTRY_ID IMAGE] [--exchange DIRECTORY] [--expect-review SHA256] [--allow-losses]\n"
        "         both QuPath commands accept --source-reader ENTRY_ID PROVIDER_ID and --source-reader-options ENTRY_ID JSON\n"
        "Annotation ABI3/4 planes: --unpositioned-z N --unpositioned-time N --legacy-position-axis channel|z|time.\n"
        "Repeat --format-plugins or --reader-plugins to configure up to 32 manifests per catalog.\n"
        "Provider interpretation options: --plugin-options-json JSON.\n"
        "Matrix positions use observation_id/x_px/y_px and native image level-zero pixel coordinates.\n"
        "Use --frame-channel N --frame-z N --frame-time N to set the matrix plane (default -1/0/0).\n"
        "Matrix frames use the selected image's source and calibration. Optional reader plugins use reader-bound frames.\n"
        "Use --reader ID to select an image reader for project-create, project-add-image, matrix-index and matrix-store-index.\n"
        "Use --series N with these commands to select a zero-based series from the native TIFF reader.\n"
        "With an explicit plugin, --reader-options JSON replaces its manifest defaults for this open.\n"
        "Project-based commands reopen the saved reader and options.\n"
        "Class-map JSON maps source labels to existing native class keys; empty source labels stay unclassified.\n"
        "Native target IDs and matrix feature names are nonempty UTF-8, at most256 bytes, with no TAB/CR/LF.\n"
        "Matrix observation names must be empty. Class labels/keys are at most4096 UTF-8 bytes.\n"
        "Outputs are new-only. Interpretation, validation, joins, annotation publication and persistence are native.\n";
}
int execute(std::vector<std::string> args){
    try {
        std::size_t start=0;
        std::vector<std::string> format_manifests;
        std::vector<std::string> reader_manifests;
        while(start<args.size() && (args[start]=="--format-plugins" || args[start]=="--reader-plugins")){
            const auto flag=args[start++];
            if(start==args.size())throw std::invalid_argument("Missing absolute manifest for "+flag);const auto path=args[start++];
            if(!std::filesystem::u8path(path).is_absolute())throw std::invalid_argument(flag+" requires an absolute manifest path");
            if(flag=="--format-plugins")format_manifests.push_back(path);
            else reader_manifests.push_back(path);
        }
        if(!format_manifests.empty())qpx::configure_format_plugin_manifests(format_manifests);
        if(!reader_manifests.empty())qpx::configure_reader_plugin_manifests(reader_manifests);
        args.erase(args.begin(),args.begin()+start);
        if(args.empty() || (args.size()==1 && args[0]=="--help")){help();return 0;}
        qpx::workflow_checkpoint(&cancelled);
        if(args[0]=="catalog"){
            if(args.size()!=1)throw std::invalid_argument("catalog accepts no positional arguments");json providers=json::array();for(const auto& p:qpx::format_plugin_infos())providers.push_back(provider_json(p));
            std::cout<<json{{"schema","qpx.format-plugin-catalog/1"},{"providers",std::move(providers)},{"diagnostics",qpx::format_plugin_diagnostics()}}.dump()<<'\n';return 0;
        }
        if(args[0]=="geojson-import" || args[0]=="geojson-export" || args[0]=="measurements-export")
            return project_objects_command(args);
        if(args[0]=="project-images")return project_images(args,!reader_manifests.empty());
        if(args[0]=="project-matrices" || args[0]=="project-matrix-info")return project_matrices(args);
        if(args[0]=="matrix-market-index")return matrix_market_index(args);
        if(args[0]=="project-matrix-import-positions")return project_matrix_import_positions(args);
        if(args[0]=="project-matrix-export-csv" || args[0]=="project-matrix-export-positions" || args[0]=="project-matrix-export-mtx" ||
           args[0]=="project-matrix-backup")
            return project_matrix_export(args);
        if(args[0]=="project-matrix-restore") {
            if(args.size()!=5)
                throw std::invalid_argument("project-matrix-restore requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID BACKUP_QMX");
            const auto id=image_id(args[2]);
            const auto restored=qpx::detail::restore_owned_expression_matrix(args[1],id,args[3],args[4],&cancelled);
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"command",args[0]},
                {"state",restored.copied?"restored":"already-present"},{"image_id",id},{"association_id",args[3]},
                {"path",restored.path},{"file_sha256",restored.file_sha256}}.dump()<<'\n';
            return 0;
        }
        if(args[0]=="project-matrix-relink" || args[0]=="project-matrix-own") {
            if(args.size()!=6)
                throw std::invalid_argument(args[0]+" requires INPUT_PROJECT IMAGE_ID ASSOCIATION_ID EXACT_QMX NEW_PROJECT");
            const auto id=image_id(args[2]);
            const auto mode=args[0]=="project-matrix-own"?qpx::detail::ExternalMatrixSave::Own:
                                                        qpx::detail::ExternalMatrixSave::Relink;
            const auto saved=qpx::detail::save_external_expression_matrix(args[1],id,args[3],args[4],args[5],mode,&cancelled);
            const auto& association=saved.association;
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"command",args[0]},{"state","saved"},
                {"output",saved.project_path},{"image_id",id},{"association_id",association.source_sha256},
                {"path",saved.matrix_path},{"file_sha256",association.file_sha256},{"owned",association.owned}}.dump()<<'\n';
            return 0;
        }
        if(args[0]=="project-set-view")return project_set_view(args);
        if(args[0]=="qupath-project-preview" || args[0]=="qupath-project-import")return qupath_project_command(args);
        if(args[0]=="project-review-source" || args[0]=="project-adopt-source"){
            const bool adopt=args[0]=="project-adopt-source";
            const std::size_t positional=adopt?6:4;
            if(args.size()<positional)
                throw std::invalid_argument(adopt?
                    "project-adopt-source requires INPUT_PROJECT IMAGE_ID PRIMARY_FILE REVIEW_SHA256 NEW_PROJECT":
                    "project-review-source requires INPUT_PROJECT IMAGE_ID PRIMARY_FILE");
            std::optional<std::string> directory;
            bool accept=false;
            for(auto at=positional;at<args.size();++at){
                const auto& option=args[at];
                if(option=="--directory" && !directory && at+1<args.size() &&
                   !args[at+1].empty() && args[at+1].rfind("--",0)!=0)
                    directory=args[++at];
                else if(option=="--accept-changed" && adopt && !accept)
                    accept=true;
                else throw std::invalid_argument("Invalid or duplicate project source review option: "+option);
            }
            const auto id=image_id(args[2]);
            const qpx::detail::WorkflowInput input(args[1],1024ull*1024*1024,true,&cancelled);
            const auto project=qpx::Project::open_without_journal_recovery(input.path());
            input.verify(false,&cancelled);
            const auto previous_server=project->document(id)->server();
            const auto fingerprint=qpx::fingerprint_file(args[3],{},&cancelled);
            const auto review=directory?project->review_directory_source_verification(id,*directory,*fingerprint):
                                        project->review_source_verification(id,*fingerprint);
            input.verify(false,&cancelled);
            auto report=source_review_report(input,*review);
            if(adopt){
                if(report["review_sha256"]!=args[4])
                    throw qpx::WorkflowInputChanged("Source review changed; preview again");
                const auto output=qpx::detail::export_path(args[5]);
                const auto previous_source=project->source_verification(id);
                const auto previous_path=previous_server->source_locator();
                const auto protect_original=[&] {
                    input.verify(false,&cancelled);
                    for(const auto* suffix:{"","-journal","-wal","-shm"})
                        qpx::detail::guard_linked_project_image_output(*previous_server,previous_path,
                                                                      previous_source.get(),output+suffix);
                    for(const auto* suffix:{"-journal","-wal","-shm"})
                        qpx::detail::guard_file_output(input.path()+suffix,"",output);
                };
                protect_original();
                project->apply_source_review(review,accept);
                const auto server=project->document(id)->server();
                qpx::detail::FormatImportImageGuard guard(server,&cancelled);
                project->save_new(output,512ull*1024*1024,1024ull*1024*1024,&cancelled,[&]{
                    protect_original();
                    guard.verify(&cancelled);
                    if(qpx::detail::valid_reader_plugin_id(server->reader_id()) &&
                       review->association()->source.scope!="directory_store")
                        server->require_raw_current(&cancelled);
                });
                report["output"]=output;
            }
            report["state"]=adopt?"saved":"reviewed";
            std::cout<<report.dump()<<'\n';
            return 0;
        }
        if(args[0]=="project-create" || args[0]=="project-add-image"){
            const bool append=args[0]=="project-add-image";
            const std::size_t positional=append?4:3;
            if(args.size()<positional || (args.size()-positional)%2)
                throw std::invalid_argument(append?
                    "project-add-image requires INPUT_PROJECT IMAGE NEW_PROJECT [--reader ID] [--series N] [--reader-options JSON]":
                    "project-create requires IMAGE NEW_PROJECT [--reader ID] [--series N] [--reader-options JSON]");
            const auto selection=source_options(args,positional,SourceOptionsFor::ProjectImage);
            const auto path=qpx::detail::export_path(args[positional-2]);
            const auto output=qpx::detail::export_path(args[positional-1]);
            std::unique_ptr<qpx::detail::WorkflowInput> input_project;
            std::shared_ptr<qpx::Project> project;
            if(append){
                input_project=std::make_unique<qpx::detail::WorkflowInput>(args[1],1024ull*1024*1024,true,&cancelled);
                project=qpx::Project::open_without_journal_recovery(input_project->path());
                input_project->verify(false,&cancelled);
            } else project=std::make_shared<qpx::Project>();
            const auto server=open_source_image(path,selection);
            qpx::detail::FormatImportImageGuard guard(server,&cancelled);
            const auto verify=[&]{
                if(input_project)input_project->verify(false,&cancelled);
                guard.verify(&cancelled);
                guard.protect(output);
                if(qpx::detail::valid_reader_plugin_id(server->reader_id()) &&
                   server->reader_source()->scope!="directory_store")
                    server->require_raw_current(&cancelled);
            };
            verify();
            const auto id=project->add_document(std::make_shared<qpx::Document>(server),"",server->source_locator(),server->reader_id());
            const auto image=project->images().back();
            auto report=json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command",args[0]},
                {"output",output},{"image_id",id},{"reader",image.reader},{"source",image.source_path},
                {"width",image.width},{"height",image.height}};
            if(const auto view=server->tiff_view())report["series"]=view->series;
            project->save_new(output,512ull*1024*1024,1024ull*1024*1024,&cancelled,verify);
            std::cout<<report.dump()<<'\n';
            return 0;
        }
        if(args[0]=="matrix-index" || args[0]=="matrix-project-index"){
            const bool from_project=args[0]=="matrix-project-index";
            const std::size_t positional=from_project?7:6;
            if(args.size()<positional || (args.size()-positional)%2)
                throw std::invalid_argument(from_project?
                    "matrix-project-index requires PROVIDER INPUT POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX [--plugin-options-json JSON]":
                    "matrix-index requires PROVIDER INPUT POSITIONS IMAGE NEW_QMX [--plugin-options-json JSON]");
            const auto selection=source_options(args,positional,from_project?SourceOptionsFor::MatrixProject:SourceOptionsFor::MatrixImage);
            const auto output=qpx::detail::export_path(args[positional-1]);
            qpx::MatrixPluginImportOptions options;
            options.frame.plane=selection.matrix_plane;
            const auto indexed=[&]{
                if(from_project)
                    return qpx::index_expression_matrix_file_for_project(args[1],args[2],args[3],output,
                        args[4],image_id(args[5]),options,selection.format_options,&cancelled);
                if(std::filesystem::exists(std::filesystem::u8path(output)))throw qpx::WorkflowOutputExists("Matrix output already exists");
                auto server=open_source_image(args[4],selection);
                qpx::detail::FormatImportImageGuard target(server,&cancelled);qpx::Document document(server);
                const auto protect=[&]{target.verify(&cancelled);target.protect(output);};
                protect();
                options.frame=qpx::image_coordinate_frame(document,qpx::detail::indexed_matrix_frame_id(document),selection.matrix_plane,&cancelled);
                return qpx::detail::import_matrix_file_guarded(args[1],args[2],args[3],output,options,selection.format_options,protect,&cancelled);
            }();
            // Read the newly published typed receipt through the same verified
            // sidecar authority used by SDK consumers, not a CLI reconstruction.
            const auto source=qpx::open_expression_matrix(indexed.path,indexed.file_sha256,64u*1024*1024,options.limits.store_bytes,&cancelled);
            const auto receipt=qpx::external_expression_matrix_receipt(*source,64u*1024*1024,&cancelled);
            if(!receipt)throw std::logic_error("Published external matrix receipt is absent");
            json report{{"schema","qpx.format-import-cli/1"},{"state","indexed"},{"command",args[0]},{"output",indexed.path},{"file_sha256",indexed.file_sha256},{"source_sha256",indexed.metadata.source_sha256},{"features",indexed.metadata.features},{"observations",indexed.metadata.observations},{"entries",indexed.metadata.entries},{"frame_sha256",indexed.metadata.spatial_frame.value().sha256()},{"receipt",json::parse(qpx::format_plugin_receipt_json(receipt->reader))},{"positions_sha256",receipt->positions_sha256},{"position_columns",receipt->position_columns}};
            if(receipt->reader.provider.abi>=2){
                report["domain"]=indexed.metadata.domain==qpx::ExpressionValueDomain::Counts?"counts":"continuous";
                report["value_domain"]=indexed.metadata.value_domain;
                report["input_profile"]=indexed.metadata.input_profile;
            }
            std::cout<<report.dump()<<'\n';return 0;
        }
        if(args[0]=="matrix-store-index" || args[0]=="matrix-project-store-index"){
            const bool from_project=args[0]=="matrix-project-store-index";
            const std::size_t positional=from_project?8:7;
            if(args.size()<positional || (args.size()-positional)%2)
                throw std::invalid_argument(from_project?
                    "matrix-project-store-index requires PROVIDER STORE PRIMARY_KEY POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX [--plugin-options-json JSON]":
                    "matrix-store-index requires PROVIDER STORE PRIMARY_KEY POSITIONS IMAGE NEW_QMX [--plugin-options-json JSON]");
            const auto selection=source_options(args,positional,from_project?SourceOptionsFor::MatrixProject:SourceOptionsFor::MatrixImage);
            const auto output=qpx::detail::export_path(args[positional-1]);
            qpx::MatrixStorePluginImportOptions options;options.primary_key=args[3];
            options.matrix.frame.plane=selection.matrix_plane;
            const auto indexed=[&]{
                if(from_project)
                    return qpx::index_expression_matrix_store_for_project(args[1],args[2],args[4],output,
                        args[5],image_id(args[6]),options,selection.format_options,&cancelled);
                if(std::filesystem::exists(std::filesystem::u8path(output)))throw qpx::WorkflowOutputExists("Matrix output already exists");
                auto server=open_source_image(args[5],selection);
                qpx::detail::FormatImportImageGuard target(server,&cancelled);qpx::Document document(server);
                const auto protect=[&]{target.verify(&cancelled);target.protect(output);};
                protect();
                options.matrix.frame=qpx::image_coordinate_frame(document,qpx::detail::indexed_matrix_frame_id(document),selection.matrix_plane,&cancelled);
                return qpx::detail::import_matrix_store_guarded(args[1],args[2],args[4],output,options,selection.format_options,protect,&cancelled);
            }();
            const auto source=qpx::open_expression_matrix(indexed.path,indexed.file_sha256,64u*1024*1024,options.matrix.limits.store_bytes,&cancelled);
            const auto receipt=qpx::external_expression_matrix_store_receipt(*source,64u*1024*1024,&cancelled);
            if(!receipt)throw std::logic_error("Published matrix store receipt is absent");
            const auto& matrix=receipt->matrix;
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","indexed"},{"command",args[0]},{"output",indexed.path},
                {"file_sha256",indexed.file_sha256},{"source_sha256",indexed.metadata.source_sha256},{"features",indexed.metadata.features},
                {"observations",indexed.metadata.observations},{"entries",indexed.metadata.entries},{"frame_sha256",indexed.metadata.spatial_frame.value().sha256()},
                {"domain",indexed.metadata.domain==qpx::ExpressionValueDomain::Counts?"counts":"continuous"},{"value_domain",indexed.metadata.value_domain},
                {"input_profile",indexed.metadata.input_profile},{"receipt",json::parse(qpx::format_plugin_receipt_json(matrix.reader))},
                {"positions_sha256",matrix.positions_sha256},{"position_columns",matrix.position_columns},{"source_scope","directory_store"},
                {"primary_key",receipt->primary_key},{"store_observed_sha256",receipt->observed_sha256},{"store_content_sha256",receipt->content_sha256},
                {"store_files",receipt->files},{"store_bytes",receipt->bytes}}.dump()<<'\n';return 0;
        }
        if(args[0]=="matrix-attach"){
            if(args.size()!=6)
                throw std::invalid_argument("matrix-attach requires INPUT_PROJECT IMAGE_ID QMX FILE_SHA256 NEW_PROJECT");
            const auto saved=qpx::attach_indexed_expression_matrix(args[1],image_id(args[2]),args[3],args[4],args[5],&cancelled);
            const auto& association=saved.association;
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command","matrix-attach"},
                {"output",saved.project_path},{"file_sha256",association.file_sha256},
                {"source_sha256",association.source_sha256},{"association_id",association.source_sha256},
                {"frame_sha256",saved.frame_sha256},{"owned",association.owned}}.dump()<<'\n';
            return 0;
        }
        if(args[0]=="matrix-info"){
            if(args.size()!=2)throw std::invalid_argument("matrix-info requires one indexed QMX input");
            const auto source=qpx::open_expression_matrix(args[1],"",64u*1024*1024,2ull*1024*1024*1024,&cancelled);
            std::cout<<matrix_metadata(*source,args[0]).dump()<<'\n';return 0;
        }
        if(args[0]=="points-import" || args[0]=="annotations-import"){
            if(args.size()<6 || (args.size()-6)%2)throw std::invalid_argument("Annotation import requires PROVIDER INPUT_PROJECT IMAGE_ID ANNOTATIONS NEW_PROJECT [OPTIONS]");
            qpx::AnnotationPluginImportOptions options;std::optional<std::string> plugin_options;std::set<std::string> seen;
            for(std::size_t at=6;at<args.size();at+=2) {
                const auto& flag=args[at];const auto& value=args[at+1];
                if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate annotation import option: "+flag);
                if(flag=="--plugin-options-json")plugin_options=value;
                else if(flag=="--class-map-json")options.class_map=class_map(value);
                else if(flag=="--unpositioned-z" || flag=="--unpositioned-time") {
                    std::uint32_t plane=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),plane);
                    if(value.empty() || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || plane>=65536)throw std::invalid_argument("Unpositioned plane must be an integer0..65535");
                    if(flag=="--unpositioned-z")options.unpositioned_z=plane;else options.unpositioned_time=plane;
                } else if(flag=="--legacy-position-axis") {
                    if(value=="channel")options.legacy_position_axis=qpx::AnnotationLinearAxis::Channel;
                    else if(value=="z")options.legacy_position_axis=qpx::AnnotationLinearAxis::Z;
                    else if(value=="time")options.legacy_position_axis=qpx::AnnotationLinearAxis::Time;
                    else throw std::invalid_argument("Legacy position axis must be channel, z or time");
                } else throw std::invalid_argument("Unknown annotation import option: "+flag);
            }
            options.validate();
            const auto output=qpx::detail::export_path(args[5]);if(std::filesystem::exists(std::filesystem::u8path(output)))throw qpx::WorkflowOutputExists("Project output already exists");
            qpx::detail::WorkflowInput primary(args[2],1024ull*1024*1024,true,&cancelled),points(args[4],options.file_bytes,false,&cancelled);
            qpx::detail::ReaderFileGuard project_file(primary.path()),point_file(points.path());primary.verify(true,&cancelled);points.verify(true,&cancelled);
            const auto project=qpx::Project::open_without_journal_recovery(primary.path());
            const auto id=image_id(args[3]);
            const auto document=project->document(id);
            const auto verified_source=project->source_verification(id);
            qpx::detail::FormatImportImageGuard image_files(document->server(),&cancelled,verified_source.get());
            const auto images=project->images();const auto frame=qpx::annotation_plugin_target_frame(*document,&cancelled);std::uint64_t applied_revision=0;
            auto protect=[&]{qpx::workflow_checkpoint(&cancelled);project_file.verify();point_file.verify();primary.verify(false,&cancelled);points.verify(false,&cancelled);image_files.verify(&cancelled);
                qpx::detail::protect_spatial_path(primary.path(),project_file.version()->identity,output);qpx::detail::protect_spatial_path(points.path(),point_file.version()->identity,output);image_files.protect(output);
                for(const auto* suffix:{"-journal","-wal","-shm"})qpx::detail::protect_spatial_path(primary.path()+suffix,"",output);
                qpx::detail::guard_project_inputs_output(*project,images,output,&cancelled);
                if(applied_revision && (document->revision()!=applied_revision || qpx::annotation_plugin_target_frame(*document,&cancelled).sha256()!=frame.sha256()))throw qpx::WorkflowInputChanged("Applied annotation document/frame changed before project save");
            };
            protect();auto preview=qpx::preview_annotation_plugin(*document,args[1],points.path(),options,plugin_options,&cancelled);
            const auto result=qpx::apply_annotation_plugin(*document,preview,&cancelled);applied_revision=document->revision();protect();
            project->save_new(output,512ull*1024*1024,1024ull*1024*1024,&cancelled,protect);
            json objects=json::array();for(const auto& mapping:result.objects)objects.push_back({{"source_id",mapping.source_id},{"object_id",mapping.object_id}});
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command",args[0]},{"output",output},{"image_id",id},{"preview_sha256",result.preview_sha256},{"receipt",json::parse(qpx::format_plugin_receipt_json(result.receipt))},{"objects",std::move(objects)}}.dump()<<'\n';return 0;
        }
        throw std::invalid_argument("Unknown format-import command; use --help");
    }catch(const qpx::WorkflowCancelled& error){std::cerr<<json{{"schema","qpx.format-import-error/1"},{"error",error.what()}}.dump()<<'\n';return 130;}
    catch(const std::exception& error){std::vector<std::string> diagnostics;try{diagnostics=qpx::format_plugin_diagnostics();}catch(...){}std::cerr<<json{{"schema","qpx.format-import-error/1"},{"error",error.what()},{"diagnostics",diagnostics}}.dump()<<'\n';return cancelled.load()?130:2;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){SetConsoleCtrlHandler(console_interrupt,TRUE);std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);std::vector<std::string> args;for(int i=1;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).u8string());return execute(std::move(args));}
#else
int main(int argc,char** argv){std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);return execute({argv+1,argv+argc});}
#endif
