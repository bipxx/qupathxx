#pragma once
#include "qupathxx/project.hpp"
#include <imgui.h>
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
        const auto attempt=[&](auto action) {try {action();error_.clear();}catch(const std::exception& e){error_=e.what();}};
        if(ImGui::BeginCombo("Registry",selected_.empty()?"New class":selected_.c_str())) {
            for(const auto& [key,c]:registry)if(ImGui::Selectable(key.c_str(),key==selected_))attempt([&]{load(project,document,image,key);});
            ImGui::EndCombo();
        }
        if(ImGui::Button("New class"))attempt([&]{load(project,document,image,{});});
        ImGui::SameLine();if(ImGui::Button("Reload class"))attempt([&]{load(project,document,image,selected_);});
        const bool context=same(project,document,image);
        const auto found=registry.find(selected_);
        const bool unchanged=!baseline_?selected_.empty():(found!=registry.end() && found->second==*baseline_);
        if(!context || !unchanged)ImGui::TextWrapped("The project, image or class changed. Reload or choose a class before applying this draft.");
        ImGui::InputTextWithHint("Class name","Full key, e.g. Tumor: Positive",name_.data(),name_.size());
        ImGui::InputTextWithHint("Parent class","Empty for a flat/root class",parent_.data(),parent_.size());
        ImGui::Checkbox("Color specified",&color_present_);
        ImGui::BeginDisabled(!color_present_);
        ImGui::ColorEdit3("RGB",rgb_);ImGui::InputInt("Alpha byte",&alpha_);
        ImGui::EndDisabled();
        if(!color_present_ && found!=registry.end())ImGui::Text("Resolved RGB: #%06x",found->second.color);
        ImGui::BeginDisabled(!context || !unchanged);
        if(ImGui::Button("Save class"))attempt([&] {
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
        if(!selected_.empty()) {
            if(!project_mode_) {
                ImGui::SameLine();if(ImGui::Button("Rename"))attempt([&]{const std::string next=name_.data();document->rename_class(selected_,next);load(project,document,image,next);});
                ImGui::Text("Assigned: %zu",registry.count(selected_)?document->class_count(selected_):0);
            }
            if(ImGui::Button(project_mode_?"Remove class":"Remove unused class"))attempt([&] {
                if(project_mode_) {
                    auto entries=definitions(registry);entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const ObjectClass& c){return c.name==selected_;}),entries.end());
                    project->set_class_registry(entries,project->class_defaults());
                } else document->remove_class(selected_);
                load(project,document,image,{});
            });
        }
        ImGui::EndDisabled();
        if(project_mode_) {
            ImGui::SeparatorText("Ordered defaults");
            const auto current=project->class_defaults();
            const bool palette_current=context && current==baseline_defaults_;
            if(!palette_current)ImGui::TextWrapped("Defaults or active image changed. Reload defaults before editing or applying them.");
            if(ImGui::Button("Reload defaults")) {ImGui::ClearActiveID();baseline_defaults_=current;default_row_=-1;}
            if(ImGui::BeginCombo("Default class",default_key_.empty()?"Choose class":default_key_.c_str())) {
                for(const auto& [key,c]:registry)if(ImGui::Selectable(key.c_str(),key==default_key_))default_key_=key;
                ImGui::EndCombo();
            }
            ImGui::BeginDisabled(!palette_current);
            if(ImGui::Button("Append default"))attempt([&]{auto next=current;next.push_back(default_key_);project->set_class_registry(definitions(registry),next);baseline_defaults_=std::move(next);default_row_=static_cast<int>(baseline_defaults_.size())-1;});
            if(ImGui::BeginChild("Default order",{0,100},ImGuiChildFlags_Borders)) {
                ImGuiListClipper rows;rows.Begin(static_cast<int>(current.size()));
                while(rows.Step())for(int row=rows.DisplayStart;row<rows.DisplayEnd;++row) {
                    ImGui::PushID(row);const auto label=std::to_string(row+1)+". "+current[static_cast<std::size_t>(row)];
                    if(ImGui::Selectable(label.c_str(),default_row_==row))default_row_=row;ImGui::PopID();
                }
            }
            ImGui::EndChild();
            const bool row_valid=default_row_>=0 && default_row_<static_cast<int>(current.size());
            const auto update=[&](int direction,bool remove) {auto next=current;
                if(remove)next.erase(next.begin()+default_row_);else std::swap(next[default_row_],next[default_row_+direction]);
                project->set_class_registry(definitions(registry),next);baseline_defaults_=std::move(next);
                default_row_=remove?-1:default_row_+direction;
            };
            ImGui::BeginDisabled(!row_valid || default_row_==0);
            if(ImGui::Button("Move default up"))attempt([&]{update(-1,false);});ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(!row_valid || default_row_+1>=static_cast<int>(current.size()));
            if(ImGui::Button("Move default down"))attempt([&]{update(1,false);});ImGui::EndDisabled();
            ImGui::BeginDisabled(!row_valid);
            if(ImGui::Button("Remove default"))attempt([&]{update(0,true);});ImGui::EndDisabled();
            ImGui::BeginDisabled(!image);
            if(ImGui::Button("Apply defaults to image"))attempt([&]{project->apply_class_defaults(image);});ImGui::EndDisabled();
            ImGui::EndDisabled();
        }
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        ImGui::PopID();
    }
};
}
