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
        if(opening){ImGui::PushID("Source tree");editor.open(project,active);ImGui::PopID();opening=false;}
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
    auto activate_checkbox=[&](const char* label,const char* tab) {
        // Shared checkboxes scope their stable caption around the value item.
        ImGui::ActivateItemByID(ImHashStr("##value",0,id(label,tab)));frame();frame();
    };
    auto text=[&](const char* label,const char* value,const char* tab) {
        ImGui::ActivateItemByID(id(label,tab));GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;
        frame();frame();require(ImGui::GetActiveID()==id(label,tab),"Text field did not activate");
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddKeyEvent(ImGuiKey_Backspace,true);frame();io.AddKeyEvent(ImGuiKey_Backspace,false);frame();
        const std::string input=value;std::size_t start=0;
        for(;;) {
            const auto end=input.find('\t',start);
            io.AddInputCharactersUTF8(input.substr(start,end==std::string::npos?end:end-start).c_str());frame();
            if(end==std::string::npos)break;
            io.AddKeyEvent(ImGuiKey_Tab,true);frame();io.AddKeyEvent(ImGuiKey_Tab,false);frame();start=end+1;
        }
    };
    activate("Add metadata","Metadata");text("Key",u8"Research \u03b1","Metadata");text("Value","value\nsecond\tline","Metadata");
    require(project->image_metadata(image).empty(),"Draft leaked into live metadata");
    activate("Apply metadata","Metadata");
    require(project->image_metadata(image).at(u8"Research \u03b1")=="value\nsecond\tline","Metadata apply lost text");
    activate("Add metadata","Metadata");text("Key",u8"Research \u03b1","Metadata");text("Value","overwrite","Metadata");
    activate("Apply metadata","Metadata");require(project->image_metadata(image).size()==1 && project->image_metadata(image).begin()->second!="overwrite","Duplicate key overwrote live metadata");
    activate("Remove metadata","Metadata");
    // Activate tab using its actual tab-bar identity, then its controls.
    ImGui::ActivateItemByID(ImHashStr("Description and tags",0,ImHashStr("Entry fields",0,ImGui::FindWindowByName("Edit project entry")->ID)));frame();frame();
    activate_checkbox("Description present","Description and tags");text("Description",u8"Description \u03b1\nsecond line","Description and tags");
    activate("Add tag","Description and tags");text("Tag","z-last","Description and tags");
    activate("Add tag","Description and tags");text("Tag","a-first","Description and tags");
    activate("Add tag","Description and tags"); // Preserve an explicit empty tag.
    require(!project->image_description(image) && project->image_tags(image).empty(),"Details draft leaked");
    activate("Apply description and tags","Description and tags");
    std::cout<<"Details diagnostic: present="<<project->image_description(image).has_value()<<" description="<<project->image_description(image).value_or("<null>")<<" tags="<<project->image_tags(image).size()<<" popup scroll="<<ImGui::FindWindowByName("Edit project entry")->Scroll.y<<" max="<<ImGui::FindWindowByName("Edit project entry")->ScrollMax.y<<'\n';
    for(const auto& tag:project->image_tags(image))std::cout<<"Saved tag ["<<tag<<"]\n";
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
    activate_checkbox("Description present","Description and tags");activate("Clear tags","Description and tags");activate("Apply description and tags","Description and tags");
    require(!project->image_description(image) && project->image_tags(image).empty(),"Clear did not preserve absent description");
    activate_checkbox("Description present","Description and tags");text("Description","","Description and tags");activate("Apply description and tags","Description and tags");
    require(project->image_description(image)=="","Empty description became absent");
    // A different project with the same numeric image ID must not receive the old draft.
    auto original=project;project=std::make_shared<qpx::Project>();
    const auto other=project->add_document(std::make_shared<qpx::Document>(qpx::Image::demo()));
    active=other;frame();activate("Apply description and tags","Description and tags");
    require(!project->image_description(other),"Replacement project accepted old draft");project=original;active=image;frame();
    // Native refusal at the aggregate tag boundary must leave live values intact.
    std::vector<std::string> near_limit;
    for(int i=0;i<252;++i){auto value=std::to_string(i);value.append(4096-value.size(),'x');near_limit.push_back(std::move(value));}
    project->set_image_details(image,"boundary",near_limit);activate("Reload current entry");
    activate("Add tag","Description and tags");text("Tag",std::string(4096,'y').c_str(),"Description and tags");
    activate("Apply description and tags","Description and tags");
    require(project->image_tags(image)==near_limit && project->image_description(image)=="boundary","Native refusal partially applied draft");
    project->set_image_details(image,"",{});activate("Reload current entry");
    // Metadata has its own stale baseline and independent clear operation.
    ImGui::ActivateItemByID(ImHashStr("Metadata",0,ImHashStr("Entry fields",0,ImGui::FindWindowByName("Edit project entry")->ID)));frame();frame();
    project->set_image_metadata(image,{{"external","current"}});activate("Apply metadata","Metadata");
    require(project->image_metadata(image).at("external")=="current","Stale metadata overwrote newer map");
    activate("Reload current entry");activate("Clear metadata","Metadata");activate("Apply metadata","Metadata");
    require(project->image_metadata(image).empty() && project->image_description(image)=="","Metadata clear changed details");
    ImGui::ActivateItemByID(ImHashStr("Description and tags",0,ImHashStr("Entry fields",0,ImGui::FindWindowByName("Edit project entry")->ID)));frame();frame();
    // Unknown active image disables publication; removal is caught by Project.
    active=image+100;frame();activate("Apply description and tags","Description and tags");require(project->image_description(image)=="","Wrong image accepted draft");active=image;frame();
    project->remove_image(image);activate("Apply description and tags","Description and tags");require(project->images().empty(),"Removed entry was recreated");
    activate("Cancel / close");
    std::cout<<"Entry editor actual ImGui input: metadata add/edit/duplicate refusal, details/order/null/empty, clear/cancel/stale/image removal passed\n";
}
inline void project_metadata() {
    if(!qpx::has_project())return;
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,900};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsAlpha8(&pixels,&width,&height);
    auto project=std::make_shared<qpx::Project>();std::uint64_t active=0;
    qpx::detail::EntryEditor editor;bool opening=true;
    auto frame=[&] {ImGui::NewFrame();ImGui::Begin("Project host");if(opening){ImGui::PushID("Other tree");editor.open_project(project);ImGui::PopID();opening=false;}editor.draw(project,active);ImGui::End();ImGui::Render();};
    frame();frame();
    auto id=[&](const char* label,bool field=true) {
        auto* window=ImGui::FindWindowByName("Edit project metadata");require(window!=nullptr,"Global metadata popup missing");auto seed=window->ID;
        if(field)seed=ImHashStr("Metadata",0,ImHashStr("Entry fields",0,seed));return ImHashStr(label,0,seed);
    };
    auto activate=[&](const char* label,bool field=true){ImGui::ActivateItemByID(id(label,field));frame();frame();};
    auto text=[&](const char* label,const char* value) {
        ImGui::ActivateItemByID(id(label));GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;frame();frame();require(ImGui::GetActiveID()==id(label),"Global text field inactive");
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddKeyEvent(ImGuiKey_Backspace,true);frame();io.AddKeyEvent(ImGuiKey_Backspace,false);frame();io.AddInputCharactersUTF8(value);frame();
    };
    activate("Add metadata");text("Key",u8"Global \u03b1");text("Value","project scope");require(project->metadata().empty(),"Global draft leaked");
    activate("Apply metadata");require(project->metadata().at(u8"Global \u03b1")=="project scope" && project->images().empty(),"Zero-image global Apply failed");
    const auto path=std::filesystem::temp_directory_path()/(project->identity()+"-global-editor.qpxproj");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove(path,error);}} cleanup{path};project->save(path.u8string());
    activate("Apply metadata");require(!project->dirty(),"Global exact Apply dirtied project");
    auto restored=qpx::Project::open(path.u8string());require(restored->metadata()==project->metadata() && restored->images().empty(),"Global GUI save/reopen failed");
    // Image selection is independent of project-level editing; the new entry must remain untouched.
    active=project->add_document(std::make_shared<qpx::Document>(qpx::Image::demo()));project->set_image_metadata(active,{{"Global","entry only"}});frame();
    text("Value","global after image change");activate("Apply metadata");require(project->metadata().at(u8"Global \u03b1")=="global after image change" && project->image_metadata(active).at("Global")=="entry only","Project mode merged entry namespace or became stale on image change");
    project->set_metadata({{"external","current"}});activate("Apply metadata");require(project->metadata().count("external")==1,"Stale global Apply overwrote newer state");
    activate("Reload current project",false);activate("Clear metadata");activate("Apply metadata");require(project->metadata().empty(),"Global clear failed");
    activate("Add metadata");text("Value","cancel this");activate("Cancel / close",false);require(project->metadata().empty(),"Global cancel applied draft");
    std::cout<<"Project metadata actual ImGui: zero-image Apply/save/reopen/no-op, image-selection namespace independence, stale refusal, clear and cancel passed\n";
}

}
