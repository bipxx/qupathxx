#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include "../gui_analysis_inputs.hpp"
#include "../gui_project_forms.hpp"
#include "gui_path_field_checks.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <climits>
#include <fstream>
#include <stdexcept>

namespace layout_scenes {
using namespace qpx;
inline void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
inline void capture(const std::filesystem::path& path) {
    auto* window=glfwGetCurrentContext();int width=0,height=0;glfwGetFramebufferSize(window,&width,&height);
    std::vector<unsigned char> rgb(std::size_t(width)*height*3);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;--y)file.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*width*3),width*3);
    require(bool(file),"Layout capture write failed");
}
inline void run(const std::filesystem::path& dir,bool probe=false) {
    std::filesystem::create_directories(dir);
    for(int language:{0,2,10})for(const std::string surface:{"recovery-empty-none","recovery-empty-chosen","recovery-listed-none","recovery-listed-chosen","classes","metadata","project","clipboard"}) {
        if(probe && (language!=0 || surface!="recovery-empty-none"))continue;
        const auto name="l1-"+surface+"-"+ui::i18n::languages[language].code+"200";const auto folder=dir/name;std::filesystem::create_directories(folder);
        auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(128,96,std::vector<std::uint8_t>(128*96*3,100)));
        project->add_document(doc,"Research image");project->save_new((folder/"source.qpxproj").u8string());
        project->set_metadata({{"Cohort","Research"}});project->set_image_metadata(1,{{"Block","A"}});
        project->set_class_registry({ObjectClass::definition("Tumor",0x00ff00,{})},{"Tumor"});
        doc->add_rectangle({5,5,20,20});
        const bool recovery=surface.rfind("recovery-",0)==0,listed=surface.find("listed")!=std::string::npos,chosen=surface.find("chosen")!=std::string::npos;
        std::string checkpoint;
        if(listed || chosen) {ProjectAutosave writer;checkpoint=listed?writer.default_path(*project):(folder/"manual.qpxrecover").u8string();require(writer.request(*project,checkpoint) && writer.wait().state=="ready","Layout checkpoint preparation failed");}
        GuiOptions options;options.width=2200;options.height=1600;options.ui_scale=2;options.docked_workspace=true;options.autosave_interval_seconds=0;
        options.layout_path=(folder/"workspace.ini").u8string();
        {std::ofstream file(options.layout_path);file<<"; qupathxx workspace layout v1\n; qupathxx language="<<ui::i18n::languages[language].code<<"\n; qupathxx theme=dark\n; qupathxx sidebar="<<(surface=="clipboard"?"annotations":"project")<<"\n; qupathxx workflow=region\n";}
        if(surface=="clipboard")options.interchange_path=(folder/"annotations.geojson").u8string();
        ui::LayoutTrace local;auto* old_layout=ui::layout_trace;if(!old_layout)ui::layout_trace=&local;
        auto& trace=*ui::layout_trace;const auto old_frame=trace.frame;const auto old_settings=trace.project_settings;const auto old_after=trace.after_settings;
        ui::CommandTrace commands;auto* old_commands=ui::command_trace;ui::command_trace=&commands;
        detail::AnalysisInputsTrace inputs;auto* old_inputs=detail::analysis_inputs_trace;detail::analysis_inputs_trace=&inputs;
        detail::GuiProjectFormsTrace paths;auto* old_paths=detail::project_forms_trace;detail::project_forms_trace=&paths;
        bool saw_candidate=false,saw_surface=false,scanning=false,activated=false,captured=false,probe_detected=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        paths.before_frame=[&](auto&,const auto& fields,auto&){
            if(recovery)for(auto* field:fields)if(std::string(ui::PathFieldTestAccess::spec(*field).label)=="##Restore recovery")field->set(chosen?checkpoint:std::string{});
        };
        trace.project_settings=[&](bool& visible,int& section){visible=recovery || surface=="classes";section=recovery?2:1;
            if(visible)if(auto* window=ImGui::FindWindowByName(ui::i18n::label("Project settings").c_str())) {ImGui::SetWindowPos(window,{300,60});ImGui::SetWindowSize(window,{1500,1450});}};
        trace.frame=[&](const auto& items){if(old_frame)old_frame(items);
            if(probe)for(const auto& error:ui::lint_layout(items,{}))if(error.message=="orphan-icon" && error.item.command=="Browse")probe_detected=true;
            for(const auto& item:items){if(item.kind==ui::LayoutKind::Row && item.caption.find("###candidate")!=std::string::npos)saw_candidate=true;
                if((recovery || surface=="classes") && item.window.find("Project settings")!=std::string::npos)saw_surface=true;
                if(surface=="metadata" && item.window.find("Edit project entry")!=std::string::npos)saw_surface=true;
                if(surface=="project" && item.caption=="Image metadata")saw_surface=true;
                if(surface=="clipboard" && item.caption.find("Paste")!=std::string::npos)saw_surface=true;}
        };
        trace.after_settings=[&]{if(probe)ui::IconButton("Browse",ui::Icon::Open);};
        commands.replay_input=[&](const auto& emit){if(listed && !scanning){GuiInput input;input.kind=GuiInputKind::FindRecoveriesKey;emit(input);scanning=true;}};
        commands.before_context=[&](int){require(std::chrono::steady_clock::now()<deadline,"Layout scene state deadline: "+name);
            if(surface=="metadata" && !activated)if(auto* window=ImGui::FindWindowByName(ui::i18n::label("Project###Project and objects").c_str());window && window->WasActive){ImGui::ActivateItemByID(ImHashStr("Edit image metadata",0,window->ID));activated=true;}
        };
        inputs.after_render=[&]{
            if(!saw_surface || (listed && !saw_candidate))return;
            if(recovery || surface=="classes") {auto* window=ImGui::FindWindowByName(ui::i18n::label("Project settings").c_str());if(!window || window->Pos.x!=300 || window->Size.x!=1500)return;}
            require(ui::layout_stats.overflow_items==0,"Layout capture overflow: "+ui::layout_stats.first_overflow);
            capture(dir/(name+".ppm"));captured=true;commands.end_session=true;
        };
        try {const auto report=run_project_gui(project,INT_MAX,"",options);require(captured && report.ui_overflow_items==0,"Layout scene did not capture: "+name);}
        catch(...) {trace.frame=old_frame;trace.project_settings=old_settings;trace.after_settings=old_after;ui::layout_trace=old_layout;ui::command_trace=old_commands;detail::analysis_inputs_trace=old_inputs;detail::project_forms_trace=old_paths;throw;}
        trace.frame=old_frame;trace.project_settings=old_settings;trace.after_settings=old_after;ui::layout_trace=old_layout;ui::command_trace=old_commands;detail::analysis_inputs_trace=old_inputs;detail::project_forms_trace=old_paths;
        if(probe){require(probe_detected,"Deliberate scratch orphan was not detected");throw std::runtime_error("orphan-icon: deliberate scratch Recovery control");}
        std::cout<<"Layout scene captured: "<<name<<" overflow0\n";
    }
}
}
