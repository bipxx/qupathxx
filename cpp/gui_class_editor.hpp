#pragma once
#include "qupathxx/project.hpp"
#include <imgui.h>
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include <imgui_internal.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace qpx::detail {
// Bounded field drafts only. Native Document/Project commands own class graphs,
// color resolution, validation, transactions and undo. Palette edits keep order.
class ClassEditor {
    bool project_mode_,bound_=false,color_present_=true;
    std::weak_ptr<Project> owner_;
    Document* document_{};
    std::uint64_t image_{};
    std::string selected_,default_key_,error_;
    bool error_defaults_=false;
    std::optional<ObjectClass> baseline_;
    std::vector<std::string> baseline_defaults_;
    std::array<char,4097> name_{},parent_{};
    float rgb_[3]={65/255.f,225/255.f,190/255.f};int alpha_=0,default_row_=-1;
    static void copy(std::array<char,4097>& target,const std::string& text) {
        target.fill(0);std::memcpy(target.data(),text.data(),text.size());
    }
    bool same(const std::shared_ptr<Project>& project,Document* document,std::uint64_t image) const {
        return bound_ && owner_.lock()==project && document_==document && image_==image;
    }
    static std::vector<ObjectClass> definitions(const std::map<std::string,ObjectClass>& registry) {
        std::vector<ObjectClass> result;result.reserve(registry.size());for(const auto& [key,c]:registry)result.push_back(c);return result;
    }
    void load(const std::shared_ptr<Project>& project,Document* document,std::uint64_t image,const std::string& key) {
        const auto& registry=project_mode_?project->classes():document->classes();
        std::optional<ObjectClass> value;
        if(!key.empty()) {const auto found=registry.find(key);if(found==registry.end())throw std::invalid_argument("Class was removed; choose New class or another definition");value=found->second;}
        ImGui::ClearActiveID();owner_=project;document_=document;image_=image;bound_=true;selected_=key;baseline_=value;
        copy(name_,key);copy(parent_,value?value->parent:std::string{});
        const auto color=value?value->color:0x41e1be;
        for(int i=0;i<3;++i)rgb_[i]=((color>>(16-8*i))&255)/255.f;
        color_present_=!value || value->color_present;alpha_=value?value->alpha:0;
        if(project_mode_)baseline_defaults_=project->class_defaults();
        if(default_row_>=static_cast<int>(baseline_defaults_.size()))default_row_=-1;
        default_key_=key;error_.clear();
    }
public:
    explicit ClassEditor(bool project_mode=false):project_mode_(project_mode){}
    // Caller provides a distinct enclosing panel; errors stay beside its fields.
    void draw(const std::shared_ptr<Project>& project,Document* document,std::uint64_t image) {
        if((project_mode_ && !project) || (!project_mode_ && !document))return;
        const auto& registry=project_mode_?project->classes():document->classes();
        if(!bound_)load(project,document,image,{});
        ImGui::PushID(project_mode_?"Project class editor":"Image class editor");
        bool defaults_action=false;
        const auto attempt=[&](auto action) {try {action();error_.clear();}catch(const std::exception& e){error_=e.what();error_defaults_=defaults_action;}};
        {ui::CaptionedStrip classes("Classes",1);classes.next();
        if(ui::IconButton("New class",ui::Icon::Plus))attempt([&]{load(project,document,image,{});});
        }
        if(ui::LegacyComboScope combo{"Registry","Registry",selected_.empty()?ui::i18n::text("New class"):selected_.c_str()};combo) {
            for(const auto& [key,c]:registry)if(ui::Selectable(ui::data_label(key,std::to_string(reinterpret_cast<std::uintptr_t>(&c))).c_str(),key==selected_))attempt([&]{load(project,document,image,key);});
        }
        const bool context=same(project,document,image);
        const auto found=registry.find(selected_);
        const bool unchanged=!baseline_?selected_.empty():(found!=registry.end() && found->second==*baseline_);
        if(!context || !unchanged)ui::DataTextWrapped("The project, image or class changed. Reload or choose a class before applying this draft.");
        ui::legacy_field("Class name","Class name",[&](const char* id){return ImGui::InputTextWithHint(id,ui::i18n::text("Full key, e.g. Tumor: Positive"),name_.data(),name_.size());});
        ui::legacy_field("Parent class","Parent class",[&](const char* id){return ImGui::InputTextWithHint(id,ui::i18n::text("Empty for a flat/root class"),parent_.data(),parent_.size());});
        ui::Checkbox("Color specified",&color_present_);
        ImGui::BeginDisabled(!color_present_);
        // ImGui193 scalar text otherwise commits on deactivation, after an
        // adjacent Apply button can already have consumed the previous value.
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar,true);
        ui::legacy_field("RGB","RGB",[&](const char* id){return ui::color_value3(id,rgb_);},true);ui::legacy_field("Alpha byte","Alpha byte",[&](const char* id){return ui::integer_value(id,&alpha_);});ImGui::PopItemFlag();
        ImGui::EndDisabled();
        if(!color_present_ && found!=registry.end())ui::Text("Resolved RGB: #%06x",found->second.color);
        if(!selected_.empty() && !project_mode_)ui::Text("Assigned: %zu",registry.count(selected_)?document->class_count(selected_):0);
        {ui::CaptionedStrip changes("Class changes",selected_.empty()?1:project_mode_?2:3,0,!selected_.empty());
        if(!selected_.empty() && !project_mode_) {
            changes.next();if(ui::IconButton("Rename class###Rename",ui::Icon::Check,ui::Availability{context && unchanged,"Reload the class before applying this draft."}))attempt([&]{const std::string next=name_.data();document->rename_class(selected_,next);load(project,document,image,next);});
        }
        changes.next();if(ui::IconButton("Reload class draft###Reload class",ui::Icon::Refresh))attempt([&]{load(project,document,image,selected_);});
        if(!selected_.empty()) {
            changes.destructive();if(ui::IconButton(project_mode_?"Remove class":"Remove unused class",ui::Icon::Trash,ui::Availability{context && unchanged,"Reload the class before applying this draft."}))attempt([&] {
                if(project_mode_) {
                    auto entries=definitions(registry);entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const ObjectClass& c){return c.name==selected_;}),entries.end());
                    project->set_class_registry(entries,project->class_defaults());
                } else document->remove_class(selected_);
                load(project,document,image,{});
            });
        }
        }
        if(ui::AvailableButton("Apply class###Save class",ui::Availability{context && unchanged,"Reload the class before applying this draft."},ui::design::Action::Primary,"Apply class"))attempt([&] {
            const std::string key=selected_.empty()?name_.data():selected_;
            if(selected_.empty() && registry.count(key))throw std::invalid_argument("Choose the existing class before replacing it");
            std::optional<std::uint32_t> raw;
            if(color_present_) {
                if(alpha_<0 || alpha_>255)throw std::invalid_argument("Alpha byte must be in0..255");
                const auto byte=[](float value){return static_cast<std::uint32_t>(std::lround(std::clamp(value,0.f,1.f)*255));};
                raw=(std::uint32_t(alpha_)<<24)|(byte(rgb_[0])<<16)|(byte(rgb_[1])<<8)|byte(rgb_[2]);
            }
            auto value=ObjectClass::definition(key,raw,parent_.data());
            if(project_mode_) {
                auto entries=definitions(registry);bool replaced=false;
                for(auto& entry:entries)if(entry.name==key){entry=value;replaced=true;break;}
                if(!replaced)entries.push_back(std::move(value));project->set_class_registry(entries,project->class_defaults());
            } else document->set_class_definition(std::move(value));
            load(project,document,image,key);
        });
        if(!error_.empty() && !error_defaults_)ui::DataTextWrapped("%s",error_.c_str());
        if(project_mode_) {
            defaults_action=true;
            const auto current=project->class_defaults();
            const bool palette_current=context && current==baseline_defaults_;
            if(!palette_current)ui::DataTextWrapped("Defaults or active image changed. Reload defaults before editing or applying them.");
            if(ui::LegacyComboScope combo{"Default class","Default class",default_key_.empty()?ui::i18n::text("Choose class"):default_key_.c_str()};combo) {
                for(const auto& [key,c]:registry)if(ui::Selectable(ui::data_label(key,std::to_string(reinterpret_cast<std::uintptr_t>(&c))).c_str(),key==default_key_))default_key_=key;
            }
            const bool row_valid=default_row_>=0 && default_row_<static_cast<int>(current.size());
            const auto update=[&](int direction,bool remove) {auto next=current;
                if(remove)next.erase(next.begin()+default_row_);else std::swap(next[default_row_],next[default_row_+direction]);
                project->set_class_registry(definitions(registry),next);baseline_defaults_=std::move(next);
                default_row_=remove?-1:default_row_+direction;
            };
            ui::LayoutRegion defaults("Ordered defaults");
            {ui::CaptionedStrip actions("Ordered defaults",5,0,true);actions.next();
            if(ui::IconButton("Append default",ui::Icon::Plus,ui::Availability{palette_current,"Reload defaults before editing them."}))attempt([&]{auto next=current;next.push_back(default_key_);project->set_class_registry(definitions(registry),next);baseline_defaults_=std::move(next);default_row_=static_cast<int>(baseline_defaults_.size())-1;});
            actions.next();
            if(ui::IconButton("Reload defaults",ui::Icon::Refresh)) {ImGui::ClearActiveID();baseline_defaults_=current;default_row_=-1;}
            actions.next();
            ImGui::BeginDisabled(!row_valid || default_row_==0);
            if(ui::IconButton("Move selected default up###Move default up",ui::Icon::Up,ui::Availability{palette_current && row_valid && default_row_>0,!palette_current?"Reload defaults before editing them.":"Select a default below the first position."}))attempt([&]{update(-1,false);});ImGui::EndDisabled();actions.next();
            ImGui::BeginDisabled(!row_valid || default_row_+1>=static_cast<int>(current.size()));
            if(ui::IconButton("Move selected default down###Move default down",ui::Icon::Down,ui::Availability{palette_current && row_valid && default_row_+1<static_cast<int>(current.size()),!palette_current?"Reload defaults before editing them.":"Select a default above the last position."}))attempt([&]{update(1,false);});ImGui::EndDisabled();
            actions.destructive();ImGui::BeginDisabled(!row_valid);
            if(ui::IconButton("Remove selected default###Remove default",ui::Icon::Trash,ui::Availability{palette_current && row_valid,!palette_current?"Reload defaults before editing them.":"Select a default first."}))attempt([&]{update(0,true);});ImGui::EndDisabled();
            }
            ImGui::BeginDisabled(!palette_current);
            if(ImGui::BeginChild("Default order",{0,100*ui::design::scale()},ImGuiChildFlags_Borders)) {
                if(current.empty())ui::TextDisabled("No defaults. Use %s.",ui::i18n::text("Append default"));
                ImGuiListClipper rows;rows.Begin(static_cast<int>(current.size()));
                while(rows.Step())for(int row=rows.DisplayStart;row<rows.DisplayEnd;++row) {
                    ImGui::PushID(row);const auto label=std::to_string(row+1)+". "+current[static_cast<std::size_t>(row)];
                    if(ui::Selectable(ui::data_label(label,"default").c_str(),default_row_==row))default_row_=row;ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::BeginDisabled(!image);
            if(ui::Button("Apply defaults to image"))attempt([&]{project->apply_class_defaults(image);});ImGui::EndDisabled();
            ImGui::EndDisabled();
        }
        if(!error_.empty() && error_defaults_)ui::DataTextWrapped("%s",error_.c_str());
        ImGui::PopID();
    }
};
}
