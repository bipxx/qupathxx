#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "qupathxx/format_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "reader_registry.hpp"
#include "workflow_input.hpp"
#include "reader_file_guard.hpp"
#include "spatial_output_guard.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
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
constexpr const char* matrix_frame_id="qpx.format-import-cli-image/1";
std::uint64_t image_id(const std::string& value){
    std::uint64_t n=0;const auto p=std::from_chars(value.data(),value.data()+value.size(),n);
    if(value.empty() || p.ec!=std::errc{} || p.ptr!=value.data()+value.size() || !n)throw std::invalid_argument("IMAGE_ID must be a positive native image ID");return n;
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
struct ImageGuards {
    std::vector<std::unique_ptr<qpx::detail::ReaderFileGuard>> files;
    explicit ImageGuards(const std::shared_ptr<qpx::ImageServer>& server){
        if(!server || server->closed())throw std::invalid_argument("Import image is closed");auto source=server->reader_source();
        if(source && source->scope=="unopened"){server->read_region({0,0,0,1,1});source=server->reader_source();}
        if(!source)throw std::invalid_argument("Import image has no stable source identity");source->validate();
        if(source->scope=="directory_store")throw std::invalid_argument("Format-import CLI does not support directory-store image targets");
        if(source->primary){files.push_back(std::make_unique<qpx::detail::ReaderFileGuard>(source->primary->path));for(const auto& file:source->companions)files.push_back(std::make_unique<qpx::detail::ReaderFileGuard>(file.path));}
        else (void)qpx::dense_image(*server);
        verify();
    }
    void verify() const {for(const auto& file:files){qpx::workflow_checkpoint(&cancelled);file->verify();}}
    void protect(const std::string& output) const {for(const auto& file:files){const auto v=file->version();qpx::detail::protect_spatial_path(v->path,v->identity,output);}}
};
void help(){
    std::cout<<"Usage: qupathxx_format_import [--format-plugins ABS_MANIFEST] [--reader-plugins ABS_MANIFEST] catalog\n"
        "       qupathxx_format_import [bootstrap options] matrix-index PROVIDER COUNTS POSITIONS IMAGE NEW_QMX\n"
        "       qupathxx_format_import [bootstrap options] points-import PROVIDER INPUT_PROJECT IMAGE_ID POINTS NEW_PROJECT [--class-map-json JSON]\n"
        "Matrix positions use observation_id/x_px/y_px and native image level-zero pixel coordinates.\n"
        "Matrix frame ID: qpx.format-import-cli-image/1; source identity/calibration come from the selected image.\n"
        "IMAGE uses native automatic reader selection. Reader manifests configure linked project readers.\n"
        "Class-map JSON maps source labels to existing native class keys; empty source labels stay unclassified.\n"
        "Native target IDs and matrix feature names are nonempty UTF-8, at most256 bytes, with no TAB/CR/LF.\n"
        "Matrix observation names must be empty. Class labels/keys are at most4096 UTF-8 bytes.\n"
        "Outputs are new-only. Interpretation, validation, joins, annotation publication and persistence are native.\n";
}
int execute(std::vector<std::string> args){
    try {
        std::size_t start=0;std::set<std::string> bootstrap;
        while(start<args.size() && (args[start]=="--format-plugins" || args[start]=="--reader-plugins")){
            const auto flag=args[start++];if(!bootstrap.insert(flag).second)throw std::invalid_argument("Duplicate bootstrap option: "+flag);
            if(start==args.size())throw std::invalid_argument("Missing absolute manifest for "+flag);const auto path=args[start++];
            if(!std::filesystem::u8path(path).is_absolute())throw std::invalid_argument(flag+" requires an absolute manifest path");
            if(flag=="--format-plugins")qpx::configure_format_plugins(path);else qpx::configure_reader_plugins(path);
        }
        args.erase(args.begin(),args.begin()+start);
        if(args.empty() || (args.size()==1 && args[0]=="--help")){help();return 0;}
        qpx::workflow_checkpoint(&cancelled);
        if(args[0]=="catalog"){
            if(args.size()!=1)throw std::invalid_argument("catalog accepts no positional arguments");json providers=json::array();for(const auto& p:qpx::format_plugin_infos())providers.push_back(provider_json(p));
            std::cout<<json{{"schema","qpx.format-plugin-catalog/1"},{"providers",std::move(providers)},{"diagnostics",qpx::format_plugin_diagnostics()}}.dump()<<'\n';return 0;
        }
        if(args[0]=="matrix-index"){
            if(args.size()!=6)throw std::invalid_argument("matrix-index requires PROVIDER COUNTS POSITIONS IMAGE NEW_QMX");
            const auto output=qpx::detail::export_path(args[5]);
            if(std::filesystem::exists(std::filesystem::u8path(output)))throw qpx::WorkflowOutputExists("Matrix output already exists");
            const auto selected=qpx::detail::select_reader(args[4],"auto",qpx::detail::ReaderPolicy::Project,qpx::detail::reader_features());
            auto server=qpx::detail::open_reader(args[4],selected,{},&cancelled);ImageGuards image_files(server);image_files.protect(output);qpx::Document document(server);
            qpx::MatrixPluginImportOptions options;options.frame=qpx::image_coordinate_frame(document,matrix_frame_id,{},&cancelled);
            image_files.verify();const auto indexed=qpx::import_expression_matrix_plugin(args[1],args[2],args[3],output,options,std::nullopt,&cancelled);
            // Read the newly published typed receipt through the same verified
            // sidecar authority used by SDK consumers, not a CLI reconstruction.
            const auto source=qpx::open_expression_matrix(indexed.path,indexed.file_sha256,64u*1024*1024,options.limits.store_bytes,&cancelled);
            const auto receipt=qpx::external_expression_matrix_receipt(*source,64u*1024*1024,&cancelled);
            if(!receipt)throw std::logic_error("Published external matrix receipt is absent");
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","indexed"},{"command","matrix-index"},{"output",indexed.path},{"file_sha256",indexed.file_sha256},{"source_sha256",indexed.metadata.source_sha256},{"features",indexed.metadata.features},{"observations",indexed.metadata.observations},{"entries",indexed.metadata.entries},{"frame_sha256",options.frame.sha256()},{"receipt",json::parse(qpx::format_plugin_receipt_json(receipt->reader))},{"positions_sha256",receipt->positions_sha256},{"position_columns",receipt->position_columns}}.dump()<<'\n';return 0;
        }
        if(args[0]=="points-import"){
            if(args.size()!=6 && args.size()!=8)throw std::invalid_argument("points-import requires PROVIDER INPUT_PROJECT IMAGE_ID POINTS NEW_PROJECT [--class-map-json JSON]");
            qpx::AnnotationPluginImportOptions options;if(args.size()==8){if(args[6]!="--class-map-json")throw std::invalid_argument("Unknown points-import option: "+args[6]);options.class_map=class_map(args[7]);}options.validate();
            const auto output=qpx::detail::export_path(args[5]);if(std::filesystem::exists(std::filesystem::u8path(output)))throw qpx::WorkflowOutputExists("Project output already exists");
            qpx::detail::WorkflowInput primary(args[2],1024ull*1024*1024,true,&cancelled),points(args[4],options.file_bytes,false,&cancelled);
            qpx::detail::ReaderFileGuard project_file(primary.path()),point_file(points.path());primary.verify(true,&cancelled);points.verify(true,&cancelled);
            const auto project=qpx::Project::open_without_journal_recovery(primary.path());const auto document=project->document(image_id(args[3]));ImageGuards image_files(document->server());
            const auto images=project->images();const auto frame=qpx::annotation_plugin_target_frame(*document,&cancelled);std::uint64_t applied_revision=0;
            auto protect=[&]{qpx::workflow_checkpoint(&cancelled);project_file.verify();point_file.verify();primary.verify(false,&cancelled);points.verify(false,&cancelled);image_files.verify();
                qpx::detail::protect_spatial_path(primary.path(),project_file.version()->identity,output);qpx::detail::protect_spatial_path(points.path(),point_file.version()->identity,output);image_files.protect(output);
                for(const auto* suffix:{"-journal","-wal","-shm"})qpx::detail::protect_spatial_path(primary.path()+suffix,"",output);
                for(const auto& image:images)if(!image.embedded && !image.source_path.empty())qpx::detail::protect_spatial_path(qpx::detail::export_path(image.source_path),"",output);
                if(applied_revision && (document->revision()!=applied_revision || qpx::annotation_plugin_target_frame(*document,&cancelled).sha256()!=frame.sha256()))throw qpx::WorkflowInputChanged("Applied annotation document/frame changed before project save");
            };
            protect();auto preview=qpx::preview_annotation_plugin(*document,args[1],points.path(),options,std::nullopt,&cancelled);
            const auto result=qpx::apply_annotation_plugin(*document,preview,&cancelled);applied_revision=document->revision();protect();
            project->save_new(output,512ull*1024*1024,1024ull*1024*1024,&cancelled,protect);
            json objects=json::array();for(const auto& mapping:result.objects)objects.push_back({{"source_id",mapping.source_id},{"object_id",mapping.object_id}});
            std::cout<<json{{"schema","qpx.format-import-cli/1"},{"state","saved"},{"command","points-import"},{"output",output},{"image_id",image_id(args[3])},{"preview_sha256",result.preview_sha256},{"receipt",json::parse(qpx::format_plugin_receipt_json(result.receipt))},{"objects",std::move(objects)}}.dump()<<'\n';return 0;
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
