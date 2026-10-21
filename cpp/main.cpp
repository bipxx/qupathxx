#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    try {
        bool smoke=false,empty=false; std::string path,project_path,recovery_path,recovery_base; std::uint64_t image_id=0;
        for (int i=1;i<argc;++i) {
            auto arg=std::string(argv[i]);
            if(arg=="--smoke") smoke=true;
            else if(arg=="--empty") empty=true;
            else if(arg=="--project" && i+1<argc) project_path=argv[++i];
            else if(arg=="--recover" && i+1<argc) recovery_path=argv[++i];
            else if(arg=="--recovery-base" && i+1<argc) recovery_base=argv[++i];
            else if(arg=="--image-id" && i+1<argc) { std::string value=argv[++i]; std::size_t used=0; image_id=std::stoull(value,&used); if(used!=value.size() || value.empty() || value[0]=='-') throw std::invalid_argument("invalid image ID"); }
            else if(path.empty() && arg.rfind("--",0)!=0) path=arg;
            else throw std::invalid_argument("Usage: qupathxx_app [image | --project file | --recover snapshot | --empty] [--recovery-base primary] [--image-id id] [--smoke]");
        }
        if(!recovery_base.empty() && recovery_path.empty()) throw std::invalid_argument("--recovery-base requires --recover");
        if(!project_path.empty() || !recovery_path.empty() || empty) {
            if(!path.empty() || (!project_path.empty() && !recovery_path.empty()) || (empty && (!project_path.empty() || !recovery_path.empty() || image_id))) throw std::invalid_argument("choose image, --project, --recover or --empty; --empty does not accept --image-id");
            auto project=empty?std::make_shared<qpx::Project>():(recovery_path.empty()?qpx::Project::open(project_path):qpx::Project::recover(recovery_path,recovery_base));
            if(image_id) project->set_active_image(image_id);
            qpx::GuiOptions options; options.project=project; options.recovery_path=recovery_path; options.recovery_base_path=recovery_base;
            auto report=qpx::run_project_gui(project,smoke?90:0,"",options);
            if(smoke && project->active_image() && !report.uploaded_tiles) throw std::runtime_error("project smoke run uploaded no tiles");
            return 0;
        }
        if(image_id) throw std::invalid_argument("--image-id requires --project or --recover");
        auto server=path.empty() ? qpx::dense_image_server(qpx::Image::demo()) :
            (path.size()>=4 && path.substr(path.size()-4)==".ppm" ? qpx::dense_image_server(qpx::Image::read_ppm(path)) : qpx::open_slide(path));
        auto document=std::make_shared<qpx::Document>(server); qpx::GuiOptions options;
        if(qpx::has_project()) { options.project=std::make_shared<qpx::Project>(); options.project->add_document(document,"",path); }
        auto report=qpx::run_gui(*document, smoke ? 90 : 0,"",options);
        if (smoke && !report.uploaded_tiles) throw std::runtime_error("smoke run did not upload any image tiles");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
