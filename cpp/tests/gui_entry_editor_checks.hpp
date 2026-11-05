#pragma once
#include "../gui_entry_editor.hpp"
#include <imgui_internal.h>
#include <filesystem>
#include <iostream>

namespace entry_editor_checks {
inline void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
inline void run() {
    if(!qpx::has_project()) {std::cout<<"Entry authoring unavailable without project storage\n";return;}
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.DisplaySize={1000,900};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsAlpha8(&pixels,&width,&height);
    auto project=std::make_shared<qpx::Project>();
    auto doc=std::make_shared<qpx::Document>(qpx::Image::demo());
    const auto image=project->add_document(doc,"Entry editor");auto active=image;
    qpx::detail::EntryEditor editor;bool opening=true;
    auto frame=[&] {
        ImGui::NewFrame();ImGui::Begin("Host");
        if(opening){editor.open(project,active);opening=false;}
        editor.draw(project,active);ImGui::End();ImGui::Render();
    };
    frame();frame();
    auto id=[&](const char* label,const char* tab=nullptr) {
        auto* window=ImGui::FindWindowByName("Edit project entry");require(window!=nullptr,"Editor popup missing");
        auto seed=window->ID;
        if(tab){seed=ImHashStr("Entry fields",0,seed);seed=ImHashStr(tab,0,seed);}
        return ImHashStr(label,0,seed);
    };
    auto activate=[&](const char* label,const char* tab=nullptr) {
        ImGui::ActivateItemByID(id(label,tab));frame();frame();
    };
    auto text=[&](const char* label,const char* value,const char* tab) {
        ImGui::ActivateItemByID(id(label,tab));GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;
        frame();frame();require(ImGui::GetActiveID()==id(label,tab),"Text field did not activate");
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddKeyEvent(ImGuiKey_Backspace,true);frame();io.AddKeyEvent(ImGuiKey_Backspace,false);frame();
        io.AddInputCharactersUTF8(value);frame();
    };
    activate("Add metadata","Metadata");text("Key",u8"Research \u03b1","Metadata");text("Value","value\nsecond line","Metadata");
    require(project->image_metadata(image).empty(),"Draft leaked into live metadata");
    activate("Apply metadata","Metadata");
    require(project->image_metadata(image).at(u8"Research \u03b1")=="value\nsecond line","Metadata apply lost text");
    activate("Add metadata","Metadata");text("Key",u8"Research \u03b1","Metadata");text("Value","overwrite","Metadata");
    activate("Apply metadata","Metadata");require(project->image_metadata(image).size()==1 && project->image_metadata(image).begin()->second!="overwrite","Duplicate key overwrote live metadata");
    activate("Remove metadata","Metadata");
    // Activate tab using its actual tab-bar identity, then its controls.
    ImGui::ActivateItemByID(ImHashStr("Description and tags",0,ImHashStr("Entry fields",0,ImGui::FindWindowByName("Edit project entry")->ID)));frame();frame();
    activate("Description present","Description and tags");text("Description",u8"Description \u03b1\nsecond line","Description and tags");
    activate("Add tag","Description and tags");text("Tag","z-last","Description and tags");
    activate("Add tag","Description and tags");text("Tag","a-first","Description and tags");
    activate("Add tag","Description and tags"); // Preserve an explicit empty tag.
    require(!project->image_description(image) && project->image_tags(image).empty(),"Details draft leaked");
    activate("Apply description and tags","Description and tags");
    require(project->image_description(image)==u8"Description \u03b1\nsecond line" && project->image_tags(image)==std::vector<std::string>({"z-last","a-first",""}),"Details apply lost nullable/order state");
    const auto path=std::filesystem::temp_directory_path()/(project->identity()+"-entry-editor.qpxproj");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};
    project->save(path.u8string());auto restored=qpx::Project::open(path.u8string());
    require(restored->image_metadata(image)==project->image_metadata(image) && restored->image_description(image)==project->image_description(image) && restored->image_tags(image)==project->image_tags(image),"GUI-applied values changed on save/reopen");
    activate("Apply description and tags","Description and tags");require(!project->dirty(),"Exact UI apply dirtied saved project");
    // Same-field stale draft must refuse; independent metadata stays untouched.
    text("Description","stale overwrite","Description and tags");project->set_image_details(image,"newer",{"external"});
    activate("Apply description and tags","Description and tags");require(project->image_description(image)=="newer","Stale details overwrote newer values");
    activate("Reload current entry");
    // Cancel unsaved draft edits.
    text("Description","cancel this","Description and tags");activate("Cancel / close");
    require(project->image_description(image)=="newer","Cancel committed draft");
    opening=true;frame();frame();
    activate("Description present","Description and tags");activate("Clear tags","Description and tags");activate("Apply description and tags","Description and tags");
    require(!project->image_description(image) && project->image_tags(image).empty(),"Clear did not preserve absent description");
    activate("Description present","Description and tags");text("Description","","Description and tags");activate("Apply description and tags","Description and tags");
    require(project->image_description(image)=="","Empty description became absent");
    // A different project with the same numeric image ID must not receive the old draft.
    auto original=project;project=std::make_shared<qpx::Project>();
    const auto other=project->add_document(std::make_shared<qpx::Document>(qpx::Image::demo()));
    active=other;frame();activate("Apply description and tags","Description and tags");
    require(!project->image_description(other),"Replacement project accepted old draft");project=original;active=image;frame();
    // Unknown active image disables publication; removal is caught by Project.
    active=image+100;frame();activate("Apply description and tags","Description and tags");require(project->image_description(image)=="","Wrong image accepted draft");active=image;frame();
    project->remove_image(image);activate("Apply description and tags","Description and tags");require(project->images().empty(),"Removed entry was recreated");
    activate("Cancel / close");
    std::cout<<"Entry editor actual ImGui input: metadata add/edit/duplicate refusal, details/order/null/empty, clear/cancel/stale/image removal passed\n";
}
}
