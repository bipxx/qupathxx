#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
static int launch(int argc, char** argv) {
    try {
        bool smoke=false,empty=false,compact=false,reset_layout=false; std::string path,project_path,recovery_path,recovery_base,layout_path,reader="auto"; std::uint64_t image_id=0; double ui_scale=1;std::optional<std::uint32_t> tiff_series;
        for (int i=1;i<argc;++i) {
            auto arg=std::string(argv[i]);
            if(arg=="--smoke") smoke=true;
            else if(arg=="--reader" && i+1<argc) reader=argv[++i];
            else if(arg=="--tiff-series" && i+1<argc) {std::string value=argv[++i];std::size_t used=0;const auto n=std::stoul(value,&used);if(value.empty() || value[0]=='-' || used!=value.size() || n>65535) throw std::invalid_argument("invalid TIFF series index");tiff_series=std::uint32_t(n);}
            else if(arg=="--ui-scale" && i+1<argc) {std::string value=argv[++i];std::size_t used=0;ui_scale=std::stod(value,&used);if(used!=value.size()) throw std::invalid_argument("invalid UI scale");}
            else if(arg=="--compact") compact=true;
            else if(arg=="--reset-layout") reset_layout=true;
            else if(arg=="--layout" && i+1<argc) layout_path=argv[++i];
            else if(arg=="--empty") empty=true;
            else if(arg=="--project" && i+1<argc) project_path=argv[++i];
            else if(arg=="--recover" && i+1<argc) recovery_path=argv[++i];
            else if(arg=="--recovery-base" && i+1<argc) recovery_base=argv[++i];
            else if(arg=="--image-id" && i+1<argc) { std::string value=argv[++i]; std::size_t used=0; image_id=std::stoull(value,&used); if(used!=value.size() || value.empty() || value[0]=='-') throw std::invalid_argument("invalid image ID"); }
            else if(path.empty() && arg.rfind("--",0)!=0) path=arg;
            else throw std::invalid_argument("Usage: qupathxx_app [image | --project file | --recover snapshot | --empty] [--recovery-base primary] [--image-id id] [--compact | --layout preferences.ini] [--reset-layout] [--ui-scale multiplier] [--smoke]");
        }
        if(!recovery_base.empty() && recovery_path.empty()) throw std::invalid_argument("--recovery-base requires --recover");
        if(tiff_series && path.empty()) throw std::invalid_argument("--tiff-series requires an image path");
        auto workspace_options=[&] {
            qpx::GuiOptions options;options.ui_scale=ui_scale;options.docked_workspace=!compact;options.reset_layout=reset_layout;
            options.image_reader=reader;if(tiff_series) {qpx::TiffOpenOptions tiff;tiff.view.series=*tiff_series;options.tiff_options=std::make_shared<const qpx::TiffOpenOptions>(tiff);options.image_reader="tiff";}
            if(compact && (reset_layout || !layout_path.empty())) throw std::invalid_argument("--layout and --reset-layout require the docked workspace");
            if(!compact) options.layout_path=layout_path.empty()?(smoke?std::string{}:qpx::default_workspace_layout_path()):layout_path;
            return options;
        };
        if(!project_path.empty() || !recovery_path.empty() || empty) {
            if(!path.empty() || (!project_path.empty() && !recovery_path.empty()) || (empty && (!project_path.empty() || !recovery_path.empty() || image_id))) throw std::invalid_argument("choose image, --project, --recover or --empty; --empty does not accept --image-id");
            auto project=std::make_shared<qpx::Project>();
            auto options=workspace_options(); options.project=project; options.recovery_path=recovery_path; options.recovery_base_path=recovery_base;
            if(!empty) {
                options.startup_kind=recovery_path.empty()?"project":"recovery";
                options.startup_path=recovery_path.empty()?project_path:recovery_path;
                options.startup_image_id=image_id;
            }
            auto report=qpx::run_project_gui(project,smoke?90:0,"",options);
            if(!report.layout_error.empty()) std::cerr<<"Workspace layout: "<<report.layout_error<<'\n';
            if(smoke && !empty && !report.startup_succeeded) throw std::runtime_error(report.project_error.empty()?"initial opening cancelled":report.project_error);
            if(smoke && project->active_image() && !report.uploaded_tiles) throw std::runtime_error("project smoke run uploaded no tiles");
            return 0;
        }
        if(image_id) throw std::invalid_argument("--image-id requires --project or --recover");
        if(!path.empty()) {
            auto project=std::make_shared<qpx::Project>(); auto options=workspace_options();
            options.startup_kind="image"; options.startup_path=path;
            auto report=qpx::run_project_gui(project,smoke?90:0,"",options);
            if(!report.layout_error.empty()) std::cerr<<"Workspace layout: "<<report.layout_error<<'\n';
            if(smoke && !report.startup_succeeded) throw std::runtime_error(report.project_error.empty()?"initial opening cancelled":report.project_error);
            if(smoke && !report.uploaded_tiles) throw std::runtime_error("smoke run uploaded no image tiles");
            return 0;
        }
        auto server=qpx::dense_image_server(qpx::Image::demo());
        auto document=std::make_shared<qpx::Document>(server); auto options=workspace_options();
        if(qpx::has_project()) { options.project=std::make_shared<qpx::Project>(); options.project->add_document(document,"",path); }
        auto report=qpx::run_gui(*document, smoke ? 90 : 0,"",options);
        if(!report.layout_error.empty()) std::cerr<<"Workspace layout: "<<report.layout_error<<'\n';
        if (smoke && !report.uploaded_tiles) throw std::runtime_error("smoke run did not upload any image tiles");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {std::vector<std::string> text;for(int i=0;i<argc;++i) text.push_back(std::filesystem::path(argv[i]).u8string());std::vector<char*> pointers;for(auto& item:text) pointers.push_back(item.data());pointers.push_back(nullptr);return launch(argc,pointers.data());}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv) {return launch(argc,argv);}
#endif
