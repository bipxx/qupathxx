#pragma once
#include "qupathxx/spatial_frontend.hpp"
#include "gui_layout.hpp"
#include <imgui.h>
#include <array>
#include <cstring>
#include <limits>
namespace qpx::detail {
class GuiSpatial {
    SpatialFrontend frontend_;
    GuiSpatialOptions options_;
    bool open_=false,initial_=true;
    std::vector<std::uint64_t> sources_,targets_;
    std::uint64_t record_=0,revision_=UINT64_MAX;
    std::vector<std::shared_ptr<const SpatialDistanceRecord>> records_;
    std::array<char,32769> path_{};
    std::string error_;
    static constexpr auto first=GuiInputTarget::SpatialOpen,last=GuiInputTarget::SpatialRecordId;
    static constexpr std::size_t count=std::size_t(last)-std::size_t(first)+1;
    std::array<Rect,count> bounds_{};std::array<bool,count> enabled_{};
    static std::size_t index(GuiInputTarget t) {return std::size_t(t)-std::size_t(first);}
    template<class Mark>void mark(GuiInputTarget t,bool enabled,Mark& callback) {
        callback(t);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds_[index(t)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(t)]=enabled;
    }
    template<class Mark,class Fn>void button(const char* label,GuiInputTarget target,bool enabled,Mark& mark_target,Fn action) {
        ImGui::BeginDisabled(!enabled);const bool clicked=ui::Button(label);mark(target,enabled,mark_target);ImGui::EndDisabled();
        if(clicked) {try {action();error_.clear();}catch(const std::exception& e){error_=e.what();}}
    }
public:
    explicit GuiSpatial(const GuiOptions& gui):options_(gui.spatial?*gui.spatial:GuiSpatialOptions{}) {
        options_.validate();open_=options_.open || options_.run || options_.record_id;sources_=options_.sources;targets_=options_.targets;record_=options_.record_id;
        std::memcpy(path_.data(),options_.csv_path.c_str(),options_.csv_path.size()+1);
    }
    static bool handles(GuiInputTarget t) {return t>=first && t<=last;}
    bool enabled(GuiInputTarget t) const {return enabled_.at(index(t));}
    Rect bounds(GuiInputTarget t) const {return bounds_.at(index(t));}
    template<class Mark>void open_button(Mark& mark_target) {button("Spatial...",GuiInputTarget::SpatialOpen,true,mark_target,[&]{open_=true;});}
    GuiSpatialReport report() const {auto report=frontend_.report();if(!error_.empty())report.error=error_;return report;}
    void finish(Document& doc,bool wait) {frontend_.finish(doc,wait);}
    template<class Mark>void draw(Document& doc,const ObjectSelection& selection,const Project* project,const std::vector<ProjectImage>* catalog,bool blocked,bool has_image,Mark& mark_target) {
        if(has_image || !initial_ || frontend_.busy()) {
            if(!frontend_.same_document(doc)) {sources_.clear();targets_.clear();record_=0;records_.clear();revision_=UINT64_MAX;initial_=false;}
            frontend_.poll(doc);
        }
        if(initial_ && has_image && !blocked) {
            initial_=false;try {if(record_)frontend_.inspect(doc,record_);else if(options_.run)frontend_.measure(doc,sources_,targets_,options_.analysis);}catch(const std::exception& e){error_=e.what();}
        }
        if(!open_)return;
        ImGui::SetNextWindowSize(ImVec2(650,530),ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Spatial distances",&open_)) {ImGui::End();return;}
        const auto state=frontend_.report();const bool idle=!state.busy && !blocked && has_image;
        button("Use selection as sources",GuiInputTarget::SpatialSources,idle,mark_target,[&]{sources_=selection.ids();});
        ImGui::SameLine();button("Use selection as targets",GuiInputTarget::SpatialTargets,idle,mark_target,[&]{targets_=selection.ids();});
        ImGui::Text("Sources: %zu    Targets: %zu",sources_.size(),targets_.size());
        ImGui::BeginDisabled(!idle);int units=options_.analysis.units==SpatialDistanceUnits::Pixels?0:1;
        if(ImGui::Combo("Distance units",&units,"Pixels\0Micrometres\0"))options_.analysis.units=units?SpatialDistanceUnits::Micrometres:SpatialDistanceUnits::Pixels;
        ImGui::Checkbox("Prefer cell nuclei",&options_.analysis.prefer_nucleus);ImGui::SameLine();ImGui::Checkbox("Exclude the same object",&options_.analysis.exclude_self);
        if(ImGui::CollapsingHeader("Resource limits")) {
            int snapshot=int(options_.analysis.snapshot_bytes/(1024*1024)),work=int(options_.analysis.work_bytes/(1024*1024));
            if(ImGui::InputInt("Capture MiB",&snapshot) && snapshot>=1 && snapshot<=256)options_.analysis.snapshot_bytes=std::size_t(snapshot)*1024*1024;
            if(ImGui::InputInt("Work MiB",&work) && work>=1 && work<=1024)options_.analysis.work_bytes=std::size_t(work)*1024*1024;
            ImU64 comparisons=options_.analysis.max_distance_tests;if(ImGui::InputScalar("Distance comparisons",ImGuiDataType_U64,&comparisons))options_.analysis.max_distance_tests=std::uint64_t(comparisons);
        }
        ImGui::EndDisabled();
        button("Run",GuiInputTarget::SpatialRun,idle && !sources_.empty(),mark_target,[&]{frontend_.measure(doc,sources_,targets_,options_.analysis);});ImGui::SameLine();
        button("Cancel",GuiInputTarget::SpatialCancel,state.busy,mark_target,[&]{frontend_.cancel();});ImGui::SameLine();
        button("Keep result",GuiInputTarget::SpatialKeep,idle && state.state=="ready" && bool(state.result),mark_target,[&]{record_=frontend_.keep(doc);});
        ImGui::Text("Analysis: %s  (%llu / %llu rows)",state.state.c_str(),static_cast<unsigned long long>(state.analysis.completed_rows),static_cast<unsigned long long>(state.analysis.total_rows));
        if(doc.revision()!=revision_) {records_=doc.spatial_records();revision_=doc.revision();}
        if(ImGui::BeginListBox("Saved results",ImVec2(-1,ImGui::GetTextLineHeightWithSpacing()*3.5f))) {
            ImGuiListClipper clip;clip.Begin(int(records_.size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto& saved=*records_[std::size_t(i)];const auto label=std::to_string(saved.id)+": "+saved.name;
                if(ImGui::Selectable(label.c_str(),record_==saved.id))record_=saved.id;
            }ImGui::EndListBox();
        }
        ImU64 selected=record_;ImGui::BeginDisabled(!idle);if(ImGui::InputScalar("Record ID",ImGuiDataType_U64,&selected))record_=std::uint64_t(selected);mark(GuiInputTarget::SpatialRecordId,idle,mark_target);ImGui::EndDisabled();
        button("Inspect",GuiInputTarget::SpatialInspect,idle && record_,mark_target,[&]{frontend_.inspect(doc,record_);});ImGui::SameLine();
        button("Verify",GuiInputTarget::SpatialVerify,idle && record_,mark_target,[&]{frontend_.verify(doc,record_);});ImGui::SameLine();
        button("Remove record",GuiInputTarget::SpatialRemove,idle && record_,mark_target,[&]{doc.remove_spatial(record_);});
        if(!state.record_status.empty())ImGui::Text("Saved status: %s",state.record_status.c_str());
        ImGui::BeginDisabled(!idle);ImGui::InputText("CSV path",path_.data(),path_.size());mark(GuiInputTarget::SpatialPath,idle,mark_target);ImGui::EndDisabled();
        button("Export CSV",GuiInputTarget::SpatialExport,idle && state.result && path_[0],mark_target,[&]{frontend_.export_csv(doc,path_.data(),options_.csv_bytes,spatial_project_paths(project,catalog,options_.analysis.snapshot_bytes),options_.analysis.snapshot_bytes);});
        ImGui::SameLine();ImGui::Text("Export: %s",state.export_state.c_str());
        if(!state.export_error.empty())ImGui::TextWrapped("%s",state.export_error.c_str());
        if(!state.error.empty())ImGui::TextWrapped("%s",state.error.c_str());
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        if(state.result) {
            const auto& result=*state.result;ImGui::Text("%zu rows; %s",result.rows().size(),result.options().units==SpatialDistanceUnits::Pixels?"pixels":"micrometres");
            if(ImGui::BeginTable("Distances",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                ImGui::TableSetupColumn("Source");ImGui::TableSetupColumn("Nearest target");ImGui::TableSetupColumn("Distance");ImGui::TableSetupColumn("Status");ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%llu",static_cast<unsigned long long>(row.source_id));
                    ImGui::TableSetColumnIndex(1);if(row.target_id)ImGui::Text("%llu",static_cast<unsigned long long>(*row.target_id));else ImGui::TextUnformatted("Absent");
                    ImGui::TableSetColumnIndex(2);if(row.distance)ImGui::Text("%.7g",*row.distance);else ImGui::TextUnformatted("--");
                    if(row.distance && ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",*row.distance);
                    ImGui::TableSetColumnIndex(3);ImGui::TextUnformatted(row.distance?"Matched":"No target");
                }ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};
}
