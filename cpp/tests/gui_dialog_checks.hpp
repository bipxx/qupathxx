#pragma once
#include "../gui_entry_editor.hpp"
#include "../gui_fonts.hpp"
#include "../gui_import_picker.hpp"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace dialog_checks {
inline void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
inline void capture(const std::filesystem::path& path,GLFWwindow* window) {
    int w=0,h=0;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)file.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);
    require(bool(file),"write native dialog capture");
}
inline void metadata_views(const std::filesystem::path& directory) {
    require(glfwInit()!=0,"dialog capture GLFW initialization");
    const auto old_theme=qpx::ui::design::state.choice;
    const auto old_language=qpx::ui::i18n::state;
    for(const std::string mode:{"metadata","details","project","metadata-light","details-light","metadata-narrow","details-narrow","metadata-german","metadata-errors","metadata-fr-resized","metadata-disabled","picker"}) {
        const bool stress=mode=="metadata-errors" || mode=="metadata-fr-resized",picker_mode=mode=="picker",resized=mode=="metadata-fr-resized";
        qpx::ui::i18n::choose(mode=="metadata-german"?3:resized?2:0);
        const bool narrow=mode.find("narrow")!=std::string::npos || stress,light=mode.find("light")!=std::string::npos,details=mode.find("details")==0,global=mode=="project";
        const float scale=narrow?2:light?1.5f:1;const int width=narrow?900:light?1440:1100,height=narrow?700:light?1000:800;
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        auto* window=glfwCreateWindow(width,height,"Native dialog checks",nullptr,nullptr);require(window!=nullptr,"dialog capture window");glfwMakeContextCurrent(window);
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigInputTrickleEventQueue=false;
        int maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);qpx::detail::configure_gui_fonts(*io.Fonts,maximum);
        qpx::ui::design::state.choice=light?qpx::ui::design::Theme::Light:qpx::ui::design::Theme::Dark;
        qpx::ui::design::apply(ImGui::GetStyle(),qpx::ui::design::state.choice);ImGui::GetStyle().ScaleAllSizes(scale);ImGui::GetStyle().FontSizeBase=qpx::ui::design::metrics.font_size*scale;
        ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
        auto project=std::make_shared<qpx::Project>();const auto image=project->add_document(std::make_shared<qpx::Document>(qpx::Image::demo()),"Breast biopsy - Case 024");
        project->set_image_metadata(image,{{"Case","BR-2026-024"},{"Stain","H&E"},{"Block","A"},{"Cohort","Discovery cohort"}});
        project->set_image_details(image,"H&E section. Review the selected tissue regions before running analysis.",{"Breast","H&E","Discovery cohort"});project->set_metadata({{"Study","Histology review"},{"Cohort","Discovery cohort"}});
        const auto generation=project->generation();qpx::detail::EntryEditor editor;qpx::detail::GuiImportPicker picker;
        const auto chosen_file=directory/"picker-input.txt";if(picker_mode){std::ofstream file(chosen_file);file<<"Read-only picker capture";picker.open(chosen_file.u8string(),{".txt"});}
        std::string first_error,second_error;if(stress)for(int row=0;row<30;++row){first_error+="Validation context retained without truncation.\n";second_error+="Recovery detail retained without truncation.\n";}
        const char* popup=picker_mode?"Choose import file":global?"Edit project metadata":"Edit project entry";
        for(int frame=0;frame<12;++frame) {
            if(frame==4 && resized){auto* modal=ImGui::FindWindowByName(popup);require(modal!=nullptr,"resized contract dialog missing");ImGui::SetWindowSize(modal,{560,636},ImGuiCond_Always);}
            if(frame==4 && details) {const auto* modal=ImGui::FindWindowByName(popup);require(modal!=nullptr,"metadata dialog missing");ImGui::ActivateItemByID(ImHashStr("Description and tags",0,ImHashStr("Entry fields",0,modal->ID)));}
            glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();qpx::ui::begin_frame();ImGui::Begin("Dialog host");
            if(frame==0 && !picker_mode) {if(stress)qpx::ui::OpenDialog(qpx::ui::DialogKind::ImageMetadata);else if(global)editor.open_project(project);else editor.open(project,image);}
            if(stress) {
                qpx::ui::DialogState state(true,false,qpx::ui::DialogVariant::Metadata);
                qpx::ui::RenderDialog(qpx::ui::DialogKind::ImageMetadata,state,"Long notices stay separate from the footer",{&first_error,&second_error},[](qpx::ui::DialogControls&){for(int row=0;row<60;++row)qpx::ui::Text("Draft row %d",row);},[](qpx::ui::DialogAction){return qpx::ui::DialogOutcome::Refused;});
            } else if(picker_mode)require(!picker.draw(),"picker capture selected a path without input");else {ImGui::BeginDisabled(mode=="metadata-disabled");editor.draw(project,image);ImGui::EndDisabled();}
            ImGui::End();ImGui::Render();
            if(frame>=5) {
                const auto* modal=ImGui::FindWindowByName(popup);require(modal!=nullptr,"native metadata dialog disappeared");
                require(modal->Pos.x>=0 && modal->Pos.y>=0 && modal->Pos.x+modal->Size.x<=io.DisplaySize.x+1 && modal->Pos.y+modal->Size.y<=io.DisplaySize.y+1,"dialog escaped viewport");
                require(modal->DC.CursorMaxPos.y<=modal->Pos.y+modal->Size.y-modal->WindowPadding.y+2 && modal->ScrollMax.y<2,"dialog footer scrolled below viewport");
                require(qpx::ui::layout_stats.overflow_items==0,"dialog field/action overflow");
                require(modal->DC.CursorMaxPos.x<=modal->Pos.x+modal->Size.x-modal->WindowPadding.x+2,"dialog header/actions overlap or escape horizontally");
                if(narrow) {bool scrollable=false;for(auto* child:ImGui::GetCurrentContext()->Windows)if(child->ParentWindow==modal && child->ScrollMax.y>0)scrollable=true;require(scrollable,"narrow form must scroll independently of its footer");}
            }
            int w=0,h=0;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);if(mode=="metadata-disabled")glClearColor(0,.4f,.7f,1);else glClearColor(.07f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(frame==11 && mode=="metadata-disabled") {
                const auto* modal=ImGui::FindWindowByName(popup);unsigned char pixel[3];glReadPixels(int(modal->Pos.x+8*scale),h-1-int(modal->Pos.y+8*scale),1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);
                const auto expected=qpx::ui::design::color(qpx::ui::design::colors().bg_secondary);
                require(std::abs(pixel[0]-expected.x*255)<2 && std::abs(pixel[1]-expected.y*255)<2 && std::abs(pixel[2]-expected.z*255)<2,"Disabled workbench leaked through modal background and reduced form contrast");
            }
            if(frame==11)capture(directory/("dialog-"+mode+".ppm"),window);glfwSwapBuffers(window);
        }
        require(project->generation()==generation,"dialog browsing changed project data");
        ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);
    }
    qpx::ui::i18n::state=old_language;qpx::ui::design::state.choice=old_theme;glfwTerminate();std::cout<<"Native metadata/project/details dialogs fit normal/light/narrow views with independently scrolling bodies and visible footers\n";
    std::cout<<"Registered frame stress: long joint notices, resized French header and actual file-picker bounds passed\n";
    std::cout<<"Modal contrast: disabled workbench cannot leak through the dialog background passed\n";
}
}
