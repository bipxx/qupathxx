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
    std::vector<std::uint64_t> sources_,targets_,tissue_,excluded_;
    std::uint64_t record_=0,revision_=UINT64_MAX;
    std::vector<std::pair<std::uint64_t,std::string>> records_;
    std::array<char,32769> path_{};
    std::string error_;
    static constexpr auto first=GuiInputTarget::SpatialOpen,last=GuiInputTarget::SpatialRecordId;
    static constexpr std::size_t old_count=std::size_t(last)-std::size_t(first)+1,count=old_count+2;
    std::array<Rect,count> bounds_{};std::array<bool,count> enabled_{};
    static std::size_t index(GuiInputTarget t) {if(t==GuiInputTarget::SpatialTissue)return old_count;if(t==GuiInputTarget::SpatialExclusions)return old_count+1;return std::size_t(t)-std::size_t(first);}
    template<class Mark>void mark(GuiInputTarget t,bool enabled,Mark& callback) {
        callback(t);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds_[index(t)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(t)]=enabled;
    }
    template<class Mark,class Fn>void button(const char* label,GuiInputTarget target,bool enabled,Mark& mark_target,Fn action) {
        ImGui::BeginDisabled(!enabled);const bool clicked=ui::Button(label);mark(target,enabled,mark_target);ImGui::EndDisabled();
        if(clicked) {try {action();error_.clear();}catch(const std::exception& e){error_=e.what();}}
    }
    bool density() const {return options_.operation==SpatialOperation::TissueDensity;}
    bool neighborhood() const {return options_.operation==SpatialOperation::RadiusCount;}
    void measure(const Document& doc) {
        if(density()) frontend_.measure_density(doc,sources_,tissue_,excluded_,targets_,options_.density);
        else if(neighborhood()) {SpatialNeighborhoodOptions p;static_cast<SpatialDistanceOptions&>(p)=options_.analysis;p.radius=options_.radius;frontend_.measure_neighborhood(doc,sources_,targets_,p);}
        else frontend_.measure(doc,sources_,targets_,options_.analysis);
    }
    void inspect(const Document& doc) {if(density())frontend_.inspect_density(doc,record_);else if(neighborhood())frontend_.inspect_neighborhood(doc,record_);else frontend_.inspect(doc,record_);}
    void verify(const Document& doc) {if(density())frontend_.verify_density(doc,record_);else if(neighborhood())frontend_.verify_neighborhood(doc,record_);else frontend_.verify(doc,record_);}
public:
    explicit GuiSpatial(const GuiOptions& gui):options_(gui.spatial?*gui.spatial:GuiSpatialOptions{}) {
        options_.validate();open_=options_.open || options_.run || options_.record_id;sources_=options_.sources;targets_=options_.targets;tissue_=options_.tissue;excluded_=options_.excluded;record_=options_.record_id;
        std::memcpy(path_.data(),options_.csv_path.c_str(),options_.csv_path.size()+1);
    }
    static bool handles(GuiInputTarget t) {return (t>=first && t<=last) || t==GuiInputTarget::SpatialTissue || t==GuiInputTarget::SpatialExclusions;}
    bool enabled(GuiInputTarget t) const {return enabled_.at(index(t));}
    Rect bounds(GuiInputTarget t) const {return bounds_.at(index(t));}
    template<class Mark>void open_button(Mark& mark_target) {button("Spatial...",GuiInputTarget::SpatialOpen,true,mark_target,[&]{open_=true;});}
    GuiSpatialReport report() const {auto report=frontend_.report();if(!error_.empty())report.error=error_;return report;}
    void finish(Document& doc,bool wait) {frontend_.finish(doc,wait);}
    template<class Mark>void draw(Document& doc,const ObjectSelection& selection,const Project* project,const std::vector<ProjectImage>* catalog,bool blocked,bool has_image,Mark& mark_target) {
        enabled_[index(GuiInputTarget::SpatialOperation)]=false;
        enabled_[index(GuiInputTarget::SpatialRadius)]=false;
        enabled_[index(GuiInputTarget::SpatialTissue)]=false;enabled_[index(GuiInputTarget::SpatialExclusions)]=false;
        if(has_image || !initial_ || frontend_.busy()) {
            if(!frontend_.same_document(doc)) {sources_.clear();targets_.clear();tissue_.clear();excluded_.clear();record_=0;records_.clear();revision_=UINT64_MAX;initial_=false;}
            frontend_.poll(doc);
        }
        if(initial_ && has_image && !blocked) {
            initial_=false;try {if(record_)inspect(doc);else if(options_.run)measure(doc);}catch(const std::exception& e){error_=e.what();}
        }
        if(!open_)return;
        ImGui::SetNextWindowSize(ImVec2(650,density()?std::min(850.f,ImGui::GetMainViewport()->WorkSize.y-60):630.f),ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Spatial analysis",&open_)) {ImGui::End();return;}
        const auto state=frontend_.report();const bool idle=!state.busy && !blocked && has_image;
        button(density()?"Selection as ROI windows":"Use selection as sources",GuiInputTarget::SpatialSources,idle,mark_target,[&]{sources_=selection.ids();});
        ImGui::SameLine();button(density()?"Selection as cells":"Use selection as targets",GuiInputTarget::SpatialTargets,idle,mark_target,[&]{targets_=selection.ids();});
        ImGui::Text(density()?"ROI windows: %zu    Cells: %zu":"Sources: %zu    Targets: %zu",sources_.size(),targets_.size());
        if(density()) {
            button("Selection as tissue",GuiInputTarget::SpatialTissue,idle,mark_target,[&]{tissue_=selection.ids();});ImGui::SameLine();
            button("Selection as exclusions",GuiInputTarget::SpatialExclusions,idle,mark_target,[&]{excluded_=selection.ids();});
            ImGui::Text("Tissue ROIs: %zu    Exclusions: %zu",tissue_.size(),excluded_.size());
        }
        ImGui::BeginDisabled(!idle);int operation=int(options_.operation);
        if(ImGui::Combo("Operation",&operation,"Nearest distance\0Radius count\0Tissue cell density\0")) {options_.operation=SpatialOperation(operation);if(density())ImGui::SetWindowSize(ImVec2(650,std::min(850.f,ImGui::GetMainViewport()->WorkSize.y-60)));record_=0;revision_=UINT64_MAX;}
        mark(GuiInputTarget::SpatialOperation,idle,mark_target);
        if(neighborhood()) {ImGui::InputDouble("Radius",&options_.radius,0,0,"%.7g");mark(GuiInputTarget::SpatialRadius,idle,mark_target);ImGui::TextWrapped("Count selected target centroids at distance <= radius. Distinct coincident objects count.");}
        if(density()) {
            ImGui::TextWrapped("Cells/mm2 of valid tissue per ROI. Tissue overlaps count once; explicit exclusions remove their boundaries. Positive X/Y calibration is required. Empty area gives undefined density.");
            ImGui::Checkbox("Accept 512-segment curved ROI approximation",&options_.density.polygonize_curves);
            ImGui::Checkbox("Prefer cell nuclei",&options_.density.prefer_nucleus);
        }else {
            int units=options_.analysis.units==SpatialDistanceUnits::Pixels?0:1;
            if(ImGui::Combo("Distance units",&units,"Pixels\0Micrometres\0"))options_.analysis.units=units?SpatialDistanceUnits::Micrometres:SpatialDistanceUnits::Pixels;
            ImGui::Checkbox("Prefer cell nuclei",&options_.analysis.prefer_nucleus);ImGui::SameLine();ImGui::Checkbox("Exclude the same object",&options_.analysis.exclude_self);
        }
        if(ImGui::CollapsingHeader("Resource limits")) {
            auto& capture_limit=density()?options_.density.snapshot_bytes:options_.analysis.snapshot_bytes;
            auto& work_limit=density()?options_.density.work_bytes:options_.analysis.work_bytes;
            int snapshot=int(capture_limit/(1024*1024)),work=int(work_limit/(1024*1024));
            if(ImGui::InputInt("Capture MiB",&snapshot) && snapshot>=1 && snapshot<=256)capture_limit=std::size_t(snapshot)*1024*1024;
            if(ImGui::InputInt("Work MiB",&work) && work>=1 && work<=1024)work_limit=std::size_t(work)*1024*1024;
            if(density()) {
                ImU64 predicates=options_.density.max_membership_tests,operations=options_.density.max_geometry_operations;
                if(ImGui::InputScalar("Membership predicates",ImGuiDataType_U64,&predicates))options_.density.max_membership_tests=std::uint64_t(predicates);
                if(ImGui::InputScalar("Geometry operations",ImGuiDataType_U64,&operations))options_.density.max_geometry_operations=std::uint64_t(operations);
                int vertices=int(options_.density.max_geometry_vertices);if(ImGui::InputInt("Geometry vertices",&vertices) && vertices>0 && vertices<=1000000)options_.density.max_geometry_vertices=std::size_t(vertices);
                ImGui::TextWrapped("Planned native payload allowance; GEOS temporary allocations are not a process memory cap. Cancellation waits for the current geometry operation.");
            }else {
                ImU64 comparisons=options_.analysis.max_distance_tests;if(ImGui::InputScalar("Distance comparisons",ImGuiDataType_U64,&comparisons))options_.analysis.max_distance_tests=std::uint64_t(comparisons);
            }
        }
        ImGui::EndDisabled();
        const bool current_mode=state.operation==options_.operation;
        button("Run",GuiInputTarget::SpatialRun,idle && !sources_.empty() && (!density() || !tissue_.empty()),mark_target,[&]{measure(doc);});ImGui::SameLine();
        button("Cancel",GuiInputTarget::SpatialCancel,state.busy,mark_target,[&]{frontend_.cancel();});ImGui::SameLine();
        button("Keep result",GuiInputTarget::SpatialKeep,idle && current_mode && state.state=="ready" && (bool(state.result) || bool(state.neighborhood_result) || bool(state.density_result)),mark_target,[&]{record_=frontend_.keep(doc);});
        ImGui::Text("Analysis: %s  (%llu / %llu rows)",state.state.c_str(),static_cast<unsigned long long>(state.operation==SpatialOperation::TissueDensity?state.density_analysis.completed_rows:state.analysis.completed_rows),static_cast<unsigned long long>(state.operation==SpatialOperation::TissueDensity?state.density_analysis.total_rows:state.analysis.total_rows));
        if(doc.revision()!=revision_) {
            records_.clear();if(density()) {for(const auto& r:doc.density_records()) records_.emplace_back(r->id,r->name);}
            else if(neighborhood()) {for(const auto& r:doc.neighborhood_records()) records_.emplace_back(r->id,r->name);}
            else {for(const auto& r:doc.spatial_records()) records_.emplace_back(r->id,r->name);}revision_=doc.revision();
        }
        if(ImGui::BeginListBox("Saved results",ImVec2(-1,ImGui::GetTextLineHeightWithSpacing()*3.5f))) {
            ImGuiListClipper clip;clip.Begin(int(records_.size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                const auto& saved=records_[std::size_t(i)];const auto label=std::to_string(saved.first)+": "+saved.second;
                if(ImGui::Selectable(label.c_str(),record_==saved.first))record_=saved.first;
            }ImGui::EndListBox();
        }
        ImU64 selected=record_;ImGui::BeginDisabled(!idle);if(ImGui::InputScalar("Record ID",ImGuiDataType_U64,&selected))record_=std::uint64_t(selected);mark(GuiInputTarget::SpatialRecordId,idle,mark_target);ImGui::EndDisabled();
        button("Inspect",GuiInputTarget::SpatialInspect,idle && record_,mark_target,[&]{inspect(doc);});ImGui::SameLine();
        button("Verify",GuiInputTarget::SpatialVerify,idle && record_,mark_target,[&]{verify(doc);});ImGui::SameLine();
        button("Remove record",GuiInputTarget::SpatialRemove,idle && record_,mark_target,[&]{if(density())doc.remove_density(record_);else if(neighborhood())doc.remove_neighborhood(record_);else doc.remove_spatial(record_);});
        if(current_mode && !state.record_status.empty())ImGui::Text("Saved status: %s",state.record_status.c_str());
        ImGui::BeginDisabled(!idle);ImGui::InputText("CSV path",path_.data(),path_.size());mark(GuiInputTarget::SpatialPath,idle,mark_target);ImGui::EndDisabled();
        button("Export CSV",GuiInputTarget::SpatialExport,idle && current_mode && (state.result || state.neighborhood_result || state.density_result) && path_[0],mark_target,[&]{frontend_.export_csv(doc,path_.data(),options_.csv_bytes,spatial_project_paths(project,catalog,density()?options_.density.snapshot_bytes:options_.analysis.snapshot_bytes),density()?options_.density.snapshot_bytes:options_.analysis.snapshot_bytes);});
        ImGui::SameLine();ImGui::Text("Export: %s",state.export_state.c_str());
        if(!state.export_error.empty())ImGui::TextWrapped("%s",state.export_error.c_str());
        if(!state.error.empty())ImGui::TextWrapped("%s",state.error.c_str());
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        if(current_mode && state.density_result) {
            const auto& result=*state.density_result;
            ImGui::Text("Tissue density: %zu ROI rows; cells/mm2",result.rows().size());
            if(ImGui::BeginTable("Tissue density",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                for(const auto* label:{"ROI","Cells","Tissue mm2","Cells/mm2","Status"})ImGui::TableSetupColumn(label);ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%llu",static_cast<unsigned long long>(row.window_id));
                    ImGui::TableSetColumnIndex(1);ImGui::Text("%llu",static_cast<unsigned long long>(row.count));
                    ImGui::TableSetColumnIndex(2);ImGui::Text("%.7g",row.area_mm2);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",row.area_mm2);
                    ImGui::TableSetColumnIndex(3);if(row.density_per_mm2) {ImGui::Text("%.7g",*row.density_per_mm2);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",*row.density_per_mm2);}else ImGui::TextUnformatted("Undefined");
                    ImGui::TableSetColumnIndex(4);ImGui::TextUnformatted(row.density_per_mm2?"Valid":"Empty area");
                }ImGui::EndTable();
            }
        }
        if(current_mode && state.neighborhood_result) {
            const auto& result=*state.neighborhood_result;const auto p=result.options();
            ImGui::Text("Radius counts: %zu rows; radius %.7g %s",result.rows().size(),p.radius,p.units==SpatialDistanceUnits::Pixels?"px":"um");
            if(ImGui::BeginTable("Neighborhood counts",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                ImGui::TableSetupColumn("Source");ImGui::TableSetupColumn("Target count");ImGui::TableSetupColumn("Radius");ImGui::TableSetupColumn("Units");ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%llu",static_cast<unsigned long long>(row.source_id));
                    ImGui::TableSetColumnIndex(1);ImGui::Text("%llu",static_cast<unsigned long long>(row.count));
                    ImGui::TableSetColumnIndex(2);ImGui::Text("%.7g",p.radius);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",p.radius);
                    ImGui::TableSetColumnIndex(3);ImGui::TextUnformatted(p.units==SpatialDistanceUnits::Pixels?"px":"um");
                }ImGui::EndTable();
            }
        }
        if(current_mode && state.result) {
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
