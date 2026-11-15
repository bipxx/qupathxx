#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_record_list.hpp"
#include "gui_path_field.hpp"
#include "gui_output_gate.hpp"
#include "gui_record_removal.hpp"
#include "qupathxx/spatial_frontend.hpp"
#include "gui_layout.hpp"
#include "gui_theme.hpp"
#include <imgui.h>
#include <array>
#include <cstring>
#include <limits>
namespace qpx::detail {
class GuiSpatial {
    ui::PathField path_{{"CSV path",ui::PathRole::NewFile,{".csv"},"","",false,true}};
    friend struct OutputWriterTestAccess;
    SpatialFrontend frontend_;
    GuiSpatialOptions options_;
    bool open_=false,initial_=true;
    std::vector<std::uint64_t> sources_,targets_,tissue_,excluded_;
    std::uint64_t record_=0,revision_=UINT64_MAX;
    ui::RecordRows record_rows_;
    std::string error_;
    inline static constexpr auto input_targets=std::array{
        GuiInputTarget::SpatialOpen,
        GuiInputTarget::SpatialSources,
        GuiInputTarget::SpatialTargets,
        GuiInputTarget::SpatialRun,
        GuiInputTarget::SpatialCancel,
        GuiInputTarget::SpatialKeep,
        GuiInputTarget::SpatialInspect,
        GuiInputTarget::SpatialVerify,
        GuiInputTarget::SpatialExport,
        GuiInputTarget::SpatialRemove,
        GuiInputTarget::SpatialPath,
        GuiInputTarget::SpatialOperation,
        GuiInputTarget::SpatialRadius,
        GuiInputTarget::SpatialRecordId,
        GuiInputTarget::SpatialTissue,
        GuiInputTarget::SpatialExclusions};
    std::array<Rect,input_targets.size()> bounds_{};std::array<bool,input_targets.size()> enabled_{};
    static constexpr std::size_t index(GuiInputTarget t) {
        for(std::size_t i=0;i<input_targets.size();++i) if(input_targets[i]==t) return i;
        return input_targets.size();
    }
    template<class Mark>void mark(GuiInputTarget t,bool enabled,Mark& callback) {
        callback(t);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds_[index(t)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(t)]=enabled;
    }
    template<class Mark,class Fn>void button(const char* label,GuiInputTarget target,bool enabled,Mark& mark_target,Fn action) {
        ImGui::BeginDisabled(!enabled);const bool clicked=ui::Button(label);mark(target,enabled,mark_target);ImGui::EndDisabled();
        if(clicked) {try {error_.clear();action();}catch(const std::exception& e){error_=e.what();}}
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
    std::vector<ui::PathField*> paths(){return {&path_};}
    explicit GuiSpatial(const GuiOptions& gui):options_(gui.spatial?*gui.spatial:GuiSpatialOptions{}) {
        options_.validate();open_=options_.open || options_.run || options_.record_id;sources_=options_.sources;targets_=options_.targets;tissue_=options_.tissue;excluded_=options_.excluded;record_=options_.record_id;
        path_.set_option(options_.csv_path);
    }
    static constexpr bool handles(GuiInputTarget t) {return index(t)<input_targets.size();}
    bool enabled(GuiInputTarget t) const {return enabled_.at(index(t));}
    Rect bounds(GuiInputTarget t) const {return bounds_.at(index(t));}
    void open() {open_=true;}
    void shell_button(Rect bounds,bool enabled) {bounds_[index(GuiInputTarget::SpatialOpen)]=bounds;enabled_[index(GuiInputTarget::SpatialOpen)]=enabled;}
    template<class Mark>void open_button(Mark& mark_target) {button(ui::command(ui::CommandId::Tool_Spatial).caption,GuiInputTarget::SpatialOpen,true,mark_target,[&]{open_=true;});}
    template<class Mark>void open_icon(Mark& mark_target) {
        if(ui::icon_action("##Spatial",ui::ActionIcon::Spatial,ui::CommandId::Shell_Spatial)) open_=true;
        mark(GuiInputTarget::SpatialOpen,true,mark_target);
    }
    GuiSpatialReport report() const {auto report=frontend_.report();if(!error_.empty())report.error=error_;return report;}
    void finish(Document& doc,bool wait) {frontend_.finish(doc,wait);}
    template<class Mark>void draw(Document& doc,const ObjectSelection& selection,const Project* project,const std::vector<ProjectImage>* catalog,bool blocked,bool has_image,Mark& mark_target,RecordRemovalDialog* removal=nullptr,std::uint64_t epoch=0,OutputGate* gate=nullptr,const ui::PathContext& context={}) {
    ui::i18n::SurfaceTrace translation_surface("tool/Spatial");
        enabled_[index(GuiInputTarget::SpatialOperation)]=false;
        enabled_[index(GuiInputTarget::SpatialRadius)]=false;
        enabled_[index(GuiInputTarget::SpatialTissue)]=false;enabled_[index(GuiInputTarget::SpatialExclusions)]=false;
        if(has_image || !initial_ || frontend_.busy()) {
            if(!frontend_.same_document(doc)) {sources_.clear();targets_.clear();tissue_.clear();excluded_.clear();record_=0;record_rows_.rows.clear();revision_=UINT64_MAX;initial_=false;}
            frontend_.poll(doc);
        }
        if(initial_ && has_image && !blocked) {
            initial_=false;try {if(record_)inspect(doc);else if(options_.run)measure(doc);}catch(const std::exception& e){error_=e.what();}
        }
        if(!open_)return;
        ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Spatial),{650,density()?850.f:630.f});
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::Spatial),&open_)) {ImGui::End();return;}
        auto state=frontend_.report();const bool idle=!state.busy && !blocked && has_image;
        button(density()?"Selection as ROI windows":"Use selection as sources",GuiInputTarget::SpatialSources,idle,mark_target,[&]{sources_=selection.ids();});
        ImGui::SameLine();button(density()?"Selection as cells":"Use selection as targets",GuiInputTarget::SpatialTargets,idle,mark_target,[&]{targets_=selection.ids();});
        ui::DataTextf(density()?"ROI windows: %zu    Cells: %zu":"Sources: %zu    Targets: %zu",sources_.size(),targets_.size());
        if(density()) {
            button("Selection as tissue",GuiInputTarget::SpatialTissue,idle,mark_target,[&]{tissue_=selection.ids();});ImGui::SameLine();
            button("Selection as exclusions",GuiInputTarget::SpatialExclusions,idle,mark_target,[&]{excluded_=selection.ids();});
            ui::DataTextf("Tissue ROIs: %zu    Exclusions: %zu",tissue_.size(),excluded_.size());
        }
        ImGui::BeginDisabled(!idle);int operation=int(options_.operation);
        if(ImGui::Combo("Operation",&operation,"Nearest distance\0Radius count\0Tissue cell density\0")) {options_.operation=SpatialOperation(operation);if(density())ImGui::SetWindowSize(ImVec2(650,std::min(850.f,ImGui::GetMainViewport()->WorkSize.y-60)));record_=0;revision_=UINT64_MAX;}
        mark(GuiInputTarget::SpatialOperation,idle,mark_target);
        if(neighborhood()) {ui::double_value("Radius",&options_.radius,0,0,"%.7g");mark(GuiInputTarget::SpatialRadius,idle,mark_target);ui::DataTextWrapped("Count selected target centroids at distance <= radius. Distinct coincident objects count.");}
        if(density()) {
            ui::TextWrapped("Cells/%s of valid tissue per ROI. Tissue overlaps count once; explicit exclusions remove their boundaries. Positive X/Y calibration is required. Empty area gives undefined density.",ui::unit::mm2);
            ui::Checkbox("Accept 512-segment curved ROI approximation",&options_.density.polygonize_curves);
            ui::Checkbox("Prefer cell nuclei",&options_.density.prefer_nucleus);
        }else {
            if(!neighborhood()) {
                int metric=int(options_.analysis.metric);
                if(ImGui::Combo("Distance metric",&metric,"Centroid to centroid\0Signed Cell to annotation boundary\0")) {
                    options_.analysis.metric=SpatialDistanceMetric(metric);options_.analysis.polygonize_curves=false;
                }
                if(options_.analysis.metric==SpatialDistanceMetric::SignedBoundary) {
                    ui::DataTextWrapped("Sources: Cells. Targets: area Annotations. Negative inside, positive outside, zero on edge. Nearest individual boundary (including holes), not a union; ties use lower target ID.");
                    ui::Checkbox("Accept 512-segment curved boundary approximation",&options_.analysis.polygonize_curves);
                }
            }else {options_.analysis.metric=SpatialDistanceMetric::Centroid;options_.analysis.polygonize_curves=false;}
            int units=options_.analysis.units==SpatialDistanceUnits::Pixels?0:1;
            if(ImGui::Combo("Distance units",&units,"Pixels\0Micrometres\0"))options_.analysis.units=units?SpatialDistanceUnits::Micrometres:SpatialDistanceUnits::Pixels;
            ui::Checkbox("Prefer cell nuclei",&options_.analysis.prefer_nucleus);ImGui::SameLine();ui::Checkbox("Exclude the same object",&options_.analysis.exclude_self);
        }
        if(ui::CollapsingHeader("Resource limits")) {
            auto& capture_limit=density()?options_.density.snapshot_bytes:options_.analysis.snapshot_bytes;
            auto& work_limit=density()?options_.density.work_bytes:options_.analysis.work_bytes;
            int snapshot=int(capture_limit/(1024*1024)),work=int(work_limit/(1024*1024));
            if(ui::integer_value("Capture MiB",&snapshot) && snapshot>=1 && snapshot<=256)capture_limit=std::size_t(snapshot)*1024*1024;
            if(ui::integer_value("Work MiB",&work) && work>=1 && work<=1024)work_limit=std::size_t(work)*1024*1024;
            if(density()) {
                ImU64 predicates=options_.density.max_membership_tests,operations=options_.density.max_geometry_operations;
                if(ui::number_value("Membership predicates",ImGuiDataType_U64,&predicates))options_.density.max_membership_tests=std::uint64_t(predicates);
                if(ui::number_value("Geometry operations",ImGuiDataType_U64,&operations))options_.density.max_geometry_operations=std::uint64_t(operations);
                int vertices=int(options_.density.max_geometry_vertices);if(ui::integer_value("Geometry vertices",&vertices) && vertices>0 && vertices<=1000000)options_.density.max_geometry_vertices=std::size_t(vertices);
                ui::DataTextWrapped("Planned native payload allowance; GEOS temporary allocations are not a process memory cap. Cancellation waits for the current geometry operation.");
            }else {
                ImU64 comparisons=options_.analysis.max_distance_tests;if(ui::number_value("Distance comparisons",ImGuiDataType_U64,&comparisons))options_.analysis.max_distance_tests=std::uint64_t(comparisons);
            }
        }
        ImGui::EndDisabled();
        const bool current_mode=state.operation==options_.operation;
        button("Run",GuiInputTarget::SpatialRun,idle && !sources_.empty() && (!density() || !tissue_.empty()),mark_target,[&]{measure(doc);});ImGui::SameLine();
        button("Cancel",GuiInputTarget::SpatialCancel,state.busy,mark_target,[&]{frontend_.cancel();});ImGui::SameLine();
        button("Keep result",GuiInputTarget::SpatialKeep,idle && current_mode && state.state=="ready" && (bool(state.result) || bool(state.neighborhood_result) || bool(state.density_result)),mark_target,[&]{record_=frontend_.keep(doc);});
        ui::DataTextf("Analysis: %s  (%llu / %llu rows)",ui::caption(ui::parse_run_state(state.state),state.state).c_str(),static_cast<unsigned long long>(state.operation==SpatialOperation::TissueDensity?state.density_analysis.completed_rows:state.analysis.completed_rows),static_cast<unsigned long long>(state.operation==SpatialOperation::TissueDensity?state.density_analysis.total_rows:state.analysis.total_rows));
        record_rows_.refresh(&doc,doc.revision(),static_cast<std::uint64_t>(options_.operation),[&](auto& rows){
            if(density())for(const auto& r:doc.density_records())rows.push_back(ui::record_row(r->id,r->name,"Density",doc.density_status(r->id)));
            else if(neighborhood())for(const auto& r:doc.neighborhood_records())rows.push_back(ui::record_row(r->id,r->name,"Neighborhood",doc.neighborhood_status(r->id)));
            else for(const auto& r:doc.spatial_records())rows.push_back(ui::record_row(r->id,r->name,"Spatial distance",doc.spatial_status(r->id)));
        },doc.server().get());
        ImGui::BeginDisabled(!idle);ui::RecordListActions list_actions;list_actions.status=[&](std::uint64_t id){return density()?doc.density_status(id):neighborhood()?doc.neighborhood_status(id):doc.spatial_status(id);};list_actions.mark=[&]{mark(GuiInputTarget::SpatialRecordId,idle,mark_target);};
        const auto highlighted=ui::draw_record_list("Kept results###Record ID",record_rows_,record_,list_actions);ImGui::EndDisabled();
        if(highlighted!=record_) {record_=highlighted;try {inspect(doc);}catch(const std::exception& e){error_=e.what();}}
        button("Inspect",GuiInputTarget::SpatialInspect,idle && record_,mark_target,[&]{inspect(doc);});ImGui::SameLine();
        button("Verify",GuiInputTarget::SpatialVerify,idle && record_,mark_target,[&]{verify(doc);});
        button("Remove kept result###Remove record",GuiInputTarget::SpatialRemove,idle && record_,mark_target,[&]{if(!removal)return;if(density())removal->request_record(doc,epoch,"Density",record_,&Document::density_record,&Document::remove_density);else if(neighborhood())removal->request_record(doc,epoch,"Neighborhood",record_,&Document::neighborhood_record,&Document::remove_neighborhood);else removal->request_record(doc,epoch,"Spatial distance",record_,&Document::spatial_record,&Document::remove_spatial);});
        state=frontend_.report();
        if(current_mode && !state.record_status.empty())ui::Text("Kept status: %s",ui::caption(ui::parse_record_state(state.record_status),state.record_status).c_str());
        ImGui::BeginDisabled(!idle);path_.draw(context,false,[&]{mark(GuiInputTarget::SpatialPath,idle,mark_target);});ImGui::EndDisabled();
        button("Export CSV",GuiInputTarget::SpatialExport,idle && current_mode && (state.result || state.neighborhood_result || state.density_result) && path_.text()[0],mark_target,[&]{if(output_gate_available(gate,error_))gate->submit(path_.state().resolved,output_scope("Spatial distance",false,ui::command(ui::CommandId::Tool_Spatial).caption),
            [this,gate,document=&doc,limit=options_.csv_bytes,snapshot=density()?options_.density.snapshot_bytes:options_.analysis.snapshot_bytes](const std::string& path,ExportWritePolicy,const std::vector<std::string>& paths) {
                frontend_.export_csv(*document,path,limit,paths,snapshot);
                gate->watch(path,[this]()->std::optional<bool>{const auto value=frontend_.report();if(value.busy)return {};return value.export_committed;},[this]{return frontend_.report().export_error;});
            },[this](const std::string& message){error_=message;},false,ui::i18n::text("This file already exists and this export never replaces a file. Choose a new file name."));});
        ImGui::SameLine();ui::DataTextf("Export: %s",ui::caption(ui::parse_export_state(state.export_state),state.export_state).c_str());
        if(state.export_state=="saved")ui::TextWrapped("Exported %llu rows to %s",static_cast<unsigned long long>(state.export_rows),state.path.c_str());
        if(!state.export_error.empty())ui::DataTextWrapped("%s",state.export_error.c_str());
        if(!state.error.empty())ui::DataTextWrapped("%s",state.error.c_str());
        if(!error_.empty())ui::DataTextWrapped("%s",error_.c_str());
        if(current_mode && state.density_result) {
            const auto& result=*state.density_result;
            ui::Text("Tissue density: %zu ROI rows; cells/%s",result.rows().size(),ui::unit::mm2);
            if(ImGui::BeginTable("Tissue density",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                ImGui::TableSetupColumn("ROI");ImGui::TableSetupColumn("Cells");ImGui::TableSetupColumn(ui::unit_text("Tissue %s",ui::unit::mm2).c_str());ImGui::TableSetupColumn(ui::unit_text("Cells/%s",ui::unit::mm2).c_str());ImGui::TableSetupColumn("Status");ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ui::DataTextf("%llu",static_cast<unsigned long long>(row.window_id));
                    ImGui::TableSetColumnIndex(1);ui::DataTextf("%llu",static_cast<unsigned long long>(row.count));
                    ImGui::TableSetColumnIndex(2);ui::DataTextf("%.7g",row.area_mm2);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",row.area_mm2);
                    ImGui::TableSetColumnIndex(3);if(row.density_per_mm2) {ui::DataTextf("%.7g",*row.density_per_mm2);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",*row.density_per_mm2);}else ui::DataText("Undefined");
                    ImGui::TableSetColumnIndex(4);ui::DataText(row.density_per_mm2?"Valid":"Empty area");
                }ImGui::EndTable();
            }
        }
        if(current_mode && state.neighborhood_result) {
            const auto& result=*state.neighborhood_result;const auto p=result.options();
            ui::DataTextf("Radius counts: %zu rows; radius %.7g %s",result.rows().size(),p.radius,p.units==SpatialDistanceUnits::Pixels?"px":ui::unit::um);
            if(ImGui::BeginTable("Neighborhood counts",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                ImGui::TableSetupColumn("Source");ImGui::TableSetupColumn("Target count");ImGui::TableSetupColumn("Radius");ImGui::TableSetupColumn("Units");ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ui::DataTextf("%llu",static_cast<unsigned long long>(row.source_id));
                    ImGui::TableSetColumnIndex(1);ui::DataTextf("%llu",static_cast<unsigned long long>(row.count));
                    ImGui::TableSetColumnIndex(2);ui::DataTextf("%.7g",p.radius);if(ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",p.radius);
                    ImGui::TableSetColumnIndex(3);ui::DataText(p.units==SpatialDistanceUnits::Pixels?"px":ui::unit::um);
                }ImGui::EndTable();
            }
        }
        if(current_mode && state.result) {
            const auto& result=*state.result;ui::DataTextf("%zu rows; %s",result.rows().size(),result.options().units==SpatialDistanceUnits::Pixels?"pixels":"micrometres");
            ui::DataText(result.algorithm());
            if(result.options().metric==SpatialDistanceMetric::SignedBoundary) ui::DataTextWrapped("Signed boundary result: negative inside selected target; positive outside; zero on boundary.");
            if(ImGui::BeginTable("Distances",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,ImVec2(0,180))) {
                ImGui::TableSetupColumn("Source");ImGui::TableSetupColumn("Nearest target");ImGui::TableSetupColumn("Distance");ImGui::TableSetupColumn("Status");ImGui::TableHeadersRow();
                ImGuiListClipper clip;clip.Begin(int(result.rows().size()));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i) {
                    const auto& row=result.rows()[std::size_t(i)];ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ui::DataTextf("%llu",static_cast<unsigned long long>(row.source_id));
                    ImGui::TableSetColumnIndex(1);if(row.target_id)ui::DataTextf("%llu",static_cast<unsigned long long>(*row.target_id));else ui::DataText("Absent");
                    ImGui::TableSetColumnIndex(2);if(row.distance)ui::DataTextf("%.7g",*row.distance);else ui::DataText("--");
                    if(row.distance && ImGui::IsItemHovered())ImGui::SetTooltip("%.17g",*row.distance);
                    ImGui::TableSetColumnIndex(3);ui::DataText(row.distance?"Matched":"No target");
                }ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};
}
