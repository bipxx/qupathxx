#pragma once
#include "../gui_class_editor.hpp"
#include <filesystem>
#include <iostream>

namespace class_editor_checks {
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline void run() {
    if(!qpx::has_project())return;
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,950};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsAlpha8(&pixels,&width,&height);
    auto project=std::make_shared<qpx::Project>();std::uint64_t active=0;
    auto doc=std::make_shared<qpx::Document>(qpx::Image::demo());
    qpx::detail::ClassEditor palette(true),image;
    bool project_mode=true;
    auto frame=[&] {
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({900,900});ImGui::Begin("Class host");
        if(project_mode)palette.draw(project,nullptr,active);else image.draw(project,doc.get(),active);
        ImGui::End();ImGui::Render();
    };
    frame();frame();
    auto id=[&](const char* label) {const auto* window=ImGui::FindWindowByName("Class host");require(window!=nullptr,"Class window missing");return ImHashStr(label,0,ImHashStr(project_mode?"Project class editor":"Image class editor",0,window->ID));};
    auto activate=[&](const char* label) {ImGui::ActivateItemByID(id(label));frame();frame();};
    auto text=[&](const char* label,const char* value) {
        ImGui::ActivateItemByID(id(label));GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;frame();frame();
        require(ImGui::GetActiveID()==id(label),"Class input field inactive");
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddKeyEvent(ImGuiKey_Backspace,true);frame();io.AddKeyEvent(ImGuiKey_Backspace,false);frame();io.AddInputCharactersUTF8(value);frame();
    };
    // Actual controls author project defaults before there are any images.
    text("Class name","Parent");text("Alpha byte","128");activate("Save class");
    std::cout<<"Class alpha diagnostic: raw="<<project->classes().at("Parent").color_argb().value_or(0)<<" expected="<<0x8041e1beu<<" active="<<ImGui::GetActiveID()<<"\n";
    require(project->classes().at("Parent").color_argb()==0x8041e1be,"GUI raw alpha was lost");
    activate("Append default");
    activate("New class");text("Class name","Parent: Positive");text("Parent class","Parent");activate("Color specified");activate("Save class");
    require(project->classes().at("Parent: Positive").parent=="Parent" && !project->classes().at("Parent: Positive").color_argb(),"GUI derived/absent state lost");
    activate("Append default");activate("Append default");
    require(project->class_defaults()==std::vector<std::string>({"Parent","Parent: Positive","Parent: Positive"}),"GUI lost repeated defaults");
    activate("Move default up");activate("Move default up");
    require(project->class_defaults()==std::vector<std::string>({"Parent: Positive","Parent","Parent: Positive"}),"GUI default move up lost order");
    activate("Move default down");activate("Remove default");
    require(project->class_defaults()==std::vector<std::string>({"Parent","Parent: Positive"}),"GUI default remove lost order");
    const auto path=std::filesystem::temp_directory_path()/(project->identity()+"-class-editor.qpxproj");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{path};
    project->save(path.u8string());auto restored=qpx::Project::open(path.u8string());
    require(restored->images().empty() && restored->classes()==project->classes() && restored->class_defaults()==project->class_defaults(),"GUI empty class project failed reopen");
    activate("Save class");require(!project->dirty(),"No-op class save dirtied project");
    // A defaults refresh cannot authorize a stale class draft for another image.
    active=17;activate("Color specified");activate("Reload defaults");activate("Save class");
    require(!project->dirty() && !project->classes().at("Parent: Positive").color_argb(),"Stale context wrote through defaults refresh");
    active=project->add_document(doc);activate("Reload class");const auto revision=doc->revision();
    activate("Apply defaults to image");
    require(doc->class_path("Parent: Positive")==std::vector<std::string>({"Parent","Positive"}) && doc->revision()==revision+1,"GUI default application not one native step");
    require(doc->undo() && doc->classes().empty(),"GUI defaults not undoable");
    // External definition edits must not be overwritten by an old field draft.
    auto entries=std::vector<qpx::ObjectClass>{qpx::ObjectClass::definition("Parent",0x123456),qpx::ObjectClass::definition("Parent: Positive",0xabcdef,"Parent")};
    project->set_class_registry(entries,project->class_defaults());activate("Save class");
    require(project->classes().at("Parent: Positive").color_argb()==0xabcdef,"Stale class draft overwrote external definition");
    activate("Reload class");activate("Remove class");
    require(project->classes().count("Parent: Positive") && project->class_defaults().size()==2,"Removing a default class partially mutated project");
    // Image controls call the same definitions and native rename/history path.
    project_mode=false;frame();frame();
    text("Class name","Root");activate("Save class");
    activate("New class");text("Class name","Root: Child");text("Parent class","Root");activate("Color specified");activate("Save class");
    const auto object=doc->add_detection(qpx::Roi::rectangle({1,2,3,4}),"Classed",0,"Root: Child");
    text("Class name","Root: Renamed");activate("Rename");
    require(doc->annotation(object).class_name=="Root: Renamed" && doc->class_path("Root: Renamed")==std::vector<std::string>({"Root","Renamed"}),"GUI rename lost explicit class or object association");
    require(doc->undo() && doc->annotation(object).class_name=="Root: Child","GUI class rename not undoable");
    std::cout<<"Class editor actual ImGui controls: parent/raw alpha/absence, repeated ordered defaults, zero-image save/reopen, stale/refusal guards, explicit application and image rename/undo passed\n";
}
}
