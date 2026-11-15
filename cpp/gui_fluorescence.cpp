#include "gui_tool_catalog.hpp"
#include "gui_record_list.hpp"
#include "gui_tool_scope.hpp"
#include "gui_fluorescence.hpp"
#include "gui_marker_rules.hpp"
#include "gui_dark_flat.hpp"
#include "gui_linear_unmixing.hpp"
#include "gui_layout.hpp"
#include "gui_notice_message.hpp"
#include "gui_workflow_ui.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include <GLFW/glfw3.h>
#include <future>
#include <chrono>
#include <algorithm>
namespace qpx::detail {
struct GuiFluorescence::Impl {
    ui::StringRecordRows record_rows;

    GuiMarkerRules markers;GuiDarkFlat correction;std::optional<DarkFlatRequest> chosen_correction;std::string chosen_correction_id;bool use_correction=false,correction_dropped=false;
    GuiLinearUnmixing unmixing,replay_source;std::optional<LinearUnmixRequest> chosen_unmixing;std::string chosen_unmixing_id;bool use_unmixing=false,load_settings_only=false;
    bool open=false,use_selected=true,use_crop=false,shown=true,body=false,pending=false,verify=false,published=false,records_dirty=true;
    RawRegionRequest region;FluorescenceCellOptions options;std::array<std::uint32_t,2> context{32,32};
    int method=0,units=0,row=0,channel=0;
    FluorescenceCellJob job;FluorescenceCellStats stats;
    std::optional<FluorescenceCellRequest> request;
    bool tiling=false;FluorescenceTileOptions tiles;FluorescenceCheckpointOptions checkpoint;
    ui::PathField tile_directory{{"Tiled results folder",ui::PathRole::NewFolder,{}}};std::uint64_t checkpoint_mib=2048;
    std::optional<FluorescenceTiledRequest> tiled_request;
    std::shared_ptr<const FluorescenceTiledReport> tiled_report;
    std::shared_ptr<const FluorescenceCellRun> run;
    std::future<std::shared_ptr<const FluorescenceCellContours>> contours;
    std::atomic<bool> cancel{false};bool discard_contours=false;
    ToolScope scope;int scope_frame=-1;
    std::shared_ptr<const ImageDataProperties> record_properties;
    std::vector<std::string> records;std::string record,kept,last_kept,error,state="idle",pending_replay,pending_unmix;
    const char* error_key=nullptr;
    std::size_t count=0,boundary_count=0;std::vector<std::uint64_t> published_ids;std::uint64_t published_structure=0;std::array<Rect,11> bounds{};
    GLuint texture=0;std::shared_ptr<const FluorescenceCellRun> texture_run;bool texture_body=false;
    int texture_width=0,texture_height=0;bool sampled=false;std::uint32_t display_z=0,display_t=0;
    ~Impl(){job.cancel();cancel.store(true);if(contours.valid())contours.wait();clear_texture();}
    bool busy() const{return pending || job.busy() || contours.valid() || !pending_replay.empty();}
    void clear_texture(){if(texture)glDeleteTextures(1,&texture);texture=0;texture_run.reset();}
    void invalidate(){if(!pending_replay.empty())replay_source.cancel();pending_replay.clear();pending_unmix.clear();job.cancel();cancel.store(true);discard_contours=true;pending=false;verify=false;request.reset();tiled_request.reset();tiled_report.reset();run.reset();kept.clear();last_kept.clear();published=false;count=0;published_ids.clear();row=channel=0;clear_texture();}
    void refresh(Document& doc){
        if(ImGui::GetCurrentContext() && scope_frame==ImGui::GetFrameCount())return;
        scope_frame=ImGui::GetCurrentContext()?ImGui::GetFrameCount():-1;const auto change=detail::update(scope,doc,0);
        if(change.document)records_dirty=true;
        if(!change.image && !change.source)return;
        const bool retain=chosen_correction && chosen_correction->matches(doc),retain_unmix=chosen_unmixing && chosen_unmixing->matches(doc);
        invalidate();if(!retain){if(chosen_correction)correction_dropped=true;chosen_correction.reset();chosen_correction_id.clear();}if(!retain_unmix){chosen_unmixing.reset();chosen_unmixing_id.clear();use_unmixing=false;}
        record_properties.reset();records_dirty=true;records.clear();record.clear();error.clear();error_key=nullptr;state="idle";}
    void refresh_records(Document& doc){if(!records_dirty && record_properties==doc.image_data_properties())return;records_dirty=false;
        record_properties=doc.image_data_properties();records=fluorescence_cell_record_ids(doc);
        if(std::find(records.begin(),records.end(),record)==records.end())record=records.empty()?std::string{}:records.back();
        kept=!last_kept.empty() && std::find(records.begin(),records.end(),last_kept)!=records.end()?last_kept:std::string{};}
    void capture_error(const std::exception& e){error=e.what();const auto* message=dynamic_cast<const ui::NoticeMessageKey*>(&e);error_key=message?message->key:nullptr;}
    template<class F>void attempt(F f){error_key=nullptr;try{f();error.clear();}catch(const std::exception& e){capture_error(e);}}
    void mark(Action a){const auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();bounds[std::size_t(a)]={p.x,p.y,q.x-p.x,q.y-p.y};}
    void start(Document& doc,std::uint64_t selected){
        auto selection=region;const auto object=use_selected?selected:0;
        if(use_selected && !object)throw std::invalid_argument("Select an area annotation or turn off Use selected ROI");
        if(!object && !use_crop){selection.region={0,0,0,doc.metadata().width,doc.metadata().height};}
        if(use_correction && !chosen_correction)throw std::invalid_argument(ui::i18n::text("Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction."));
        if(use_unmixing && !chosen_unmixing)throw std::invalid_argument(ui::i18n::text("Choose an unmixing recipe in Linear unmixing, or turn off Use selected unmixing."));
        if(tiling){
            if(!tile_directory.text()[0])throw std::invalid_argument("Choose a dedicated tiled results folder");
            if(checkpoint_mib>65536)throw std::invalid_argument("Checkpoint disk limit must be at most65536 MiB");checkpoint.total_bytes=checkpoint_mib*1024*1024;
            auto candidate=use_unmixing?FluorescenceTiledRequest(doc,*chosen_unmixing,selection,object,options,tiles):use_correction?FluorescenceTiledRequest(doc,*chosen_correction,selection,object,options,tiles):FluorescenceTiledRequest(doc,selection,object,options,tiles);
            if(!job.request(candidate,tile_directory.state().resolved,checkpoint))throw std::runtime_error("Fluorescence job is still running");
            tiled_request=std::move(candidate);request.reset();
        }else{
            auto candidate=use_unmixing?FluorescenceCellRequest(doc,*chosen_unmixing,selection,object,options,context):use_correction?FluorescenceCellRequest(doc,*chosen_correction,selection,object,options,context):FluorescenceCellRequest(doc,selection,object,options,context);
            if(!job.request(candidate))throw std::runtime_error("Fluorescence job is still running");request=std::move(candidate);tiled_request.reset();
        }
        tiled_report.reset();run.reset();clear_texture();pending=true;verify=false;published=false;published_ids.clear();kept.clear();last_kept.clear();count=0;state="running";row=channel=0;
    }
    void submit_replay(FluorescenceCellRequest candidate,const std::string& id){
        if(!job.request(candidate))throw std::runtime_error("Fluorescence job is still running");
        tiled_request.reset();tiled_report.reset();request=std::move(candidate);run.reset();clear_texture();pending=true;verify=true;kept=id;last_kept=id;published=false;published_ids.clear();count=0;state="running";
    }
    void load_settings(const FluorescenceCellRequest& candidate){region=candidate.core_region();options=candidate.options();context=candidate.context();use_selected=candidate.object_id()!=0;use_crop=!use_selected;method=int(options.threshold_method);units=int(options.units);}
    void replay(Document& doc,bool settings_only=false){const auto id=fluorescence_cell_record_unmix(doc,record);if(id.empty()){auto candidate=replay_fluorescence_cell_record(doc,record);if(settings_only)load_settings(candidate);else submit_replay(std::move(candidate),record);return;}
        replay_source.restore(doc,id);pending_replay=record;pending_unmix=id;load_settings_only=settings_only;
    }
    void poll(Document& doc,bool blocked){correction.poll(doc,blocked || busy());unmixing.poll(doc,blocked || pending || job.busy() || contours.valid());replay_source.poll(doc,blocked);markers.poll(doc);refresh(doc);if(chosen_unmixing && !chosen_unmixing->matches(doc)){chosen_unmixing.reset();chosen_unmixing_id.clear();}if(chosen_correction && !chosen_correction->matches(doc)){correction_dropped=true;chosen_correction.reset();chosen_correction_id.clear();}display_z=doc.active_plane().z;display_t=doc.active_plane().t;stats=job.poll(doc);
        if(!pending_replay.empty() && !blocked && !replay_source.busy()){
            try{const auto prepared=replay_source.request();if(!prepared || prepared->recipe().sha256()!=pending_unmix)throw WorkflowInputChanged(replay_source.error().empty()?ui::i18n::text("Kept unmixing source is unavailable."):replay_source.error());auto candidate=replay_fluorescence_cell_record(doc,pending_replay,*prepared);const auto id=pending_replay;pending_replay.clear();pending_unmix.clear();if(load_settings_only)load_settings(candidate);else submit_replay(std::move(candidate),id);stats=job.poll(doc);}
            catch(const std::exception& e){pending_replay.clear();pending_unmix.clear();state="failed";capture_error(e);}
        }
        if(pending && stats.state!="running"){
            pending=false;state=stats.state;
            if(tiled_request && (stats.state=="ready" || stats.state=="unresolved")){tiled_report=job.tiled_result();run.reset();}
            else if(stats.state=="ready" && request){run=job.result();boundary_count=0;for(const auto& b:run->raster().boundaries)if(b.nucleus || b.cell)++boundary_count;if(verify){attempt([&]{if(!verify_fluorescence_cell_record(doc,kept,*request,*run))throw ui::NoticeException<std::runtime_error>("The kept fluorescence result differs from the recheck.");state="verified";});verify=false;}}
            else {run.reset();if(!stats.error.empty()){error=stats.error;error_key=nullptr;}}
        }
        if(run && request && !request->matches(doc)){run.reset();request.reset();kept.clear();published=false;count=0;state="stale";clear_texture();cancel.store(true);discard_contours=true;}
        if(tiled_report && tiled_request && !tiled_request->matches(doc)){tiled_report.reset();tiled_request.reset();state="stale";}
        if(contours.valid() && (!blocked || discard_contours) && contours.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            try{auto traced=contours.get();if(!discard_contours && run && request){published_ids=publish_fluorescence_cells(doc,*request,*traced,"Fluorescence cell",request->options().work_bytes,&cancel);count=published_ids.size();published_structure=doc.structure_revision();published=true;state="published";error.clear();error_key=nullptr;}}
            catch(const std::exception& e){if(!discard_contours){state="failed";capture_error(e);}}
            if(discard_contours && state=="cancelling")state="cancelled";
        }
        if(!published_ids.empty() && published_structure!=doc.structure_revision()){
            count=std::count_if(published_ids.begin(),published_ids.end(),[&](auto id){return doc.objects().count(id)!=0;});published=count!=0;published_structure=doc.structure_revision();
        }
        if(state=="cancelling" && !busy() && !unmixing.busy() && !correction.busy())state="cancelled";
        attempt_records(doc);
    }
    void attempt_records(Document& doc){try{refresh_records(doc);}catch(const std::exception& e){capture_error(e);records.clear();record.clear();}}
    void build_texture(){
        if(texture_run==run && texture_body==body)return;clear_texture();if(!run)return;
        const auto& r=run->raster();GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);const auto edge=std::min(1024,maximum);if(edge<=0)return;
        texture_width=int(std::min<std::int64_t>(r.width,edge));texture_height=int(std::min<std::int64_t>(r.height,edge));sampled=texture_width!=r.width || texture_height!=r.height;
        std::vector<unsigned char> rgba(std::size_t(texture_width)*texture_height*4,0);const auto& labels=body?r.bodies:r.nuclei;
        for(int y=0;y<texture_height;++y)for(int x=0;x<texture_width;++x){const auto sx=(2*std::int64_t(x)+1)*r.width/(2*texture_width),sy=(2*std::int64_t(y)+1)*r.height/(2*texture_height);
            const auto label=labels[std::size_t(sy)*std::size_t(r.width)+std::size_t(sx)];if(!label)continue;const auto at=(std::size_t(y)*texture_width+x)*4;rgba[at]=body?40:210;rgba[at+1]=210;rgba[at+2]=body?180:70;rgba[at+3]=115;}
        GLint old=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture_width,texture_height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glBindTexture(GL_TEXTURE_2D,GLuint(old));
        if(glGetError()!=GL_NO_ERROR){clear_texture();error="Fluorescence preview texture allocation failed";}
        texture_run=run;texture_body=body;
    }
    ui::Availability availability(Action action,const Document& doc,std::uint64_t selected,bool blocked,bool has_image) const {
        if(action==Action::Cancel)return {};
        if(!has_image)return {false,"Open an image first."};
        if(blocked)return ui::WorkflowBlocked();
        if(busy() || ((action==Action::Run || action==Action::Replay || action==Action::Remove) && (correction.busy() || unmixing.busy())))return {false,"Wait for the current work to finish."};
        if(action==Action::Run) {
            if(doc.server()->reader_id()!="tiff")return {false,"Fluorescence analysis needs a quantitative TIFF image."};
            if(use_selected && !selected)return {false,"Select an area annotation or turn off Use selected ROI"};
            if(use_correction && !chosen_correction)return {false,correction_dropped?"The selected correction no longer matches this image. Choose it again in Dark/flat correction.":"Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction."};
            if(use_unmixing && !chosen_unmixing)return {false,"Choose an unmixing recipe in Linear unmixing, or turn off Use selected unmixing."};
        }
        if(action==Action::KeepCells || action==Action::KeepRecord) {
            if(tiled_report)return {false,"Run without tiles to keep results in the project."};
            if(!run)return {false,"Run a preview first."};
            if(!request || !request->publication_matches(doc))return {false,"Run again for this image view before keeping results."};
            if(action==Action::KeepRecord && !kept.empty())return {false,"Run again for a new result."};
            if(action==Action::KeepCells) {
                if(published)return {false,"Run again for a new result."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                if(run->raster().measurements.empty())return {false,"Run again for a new result."};
            }
        }
        if((action==Action::Replay || action==Action::Remove) && record.empty())return {false,"Select a kept result first."};
        return {};
    }
    void controls(Document& doc,std::uint64_t selected,bool blocked,const ui::PathContext& path_context,bool has_image){
        if(ui::Button("Marker rules..."))markers.open();ImGui::SameLine();
        if(ui::Button("Dark/flat correction..."))correction.open();mark(Action::Correction);
        ImGui::BeginDisabled(blocked || busy());if(ui::Checkbox("Use selected dark/flat correction",&use_correction)){use_unmixing=false;invalidate();}mark(Action::Domain);ImGui::EndDisabled();
        if(ui::Button("Linear unmixing..."))unmixing.open();mark(Action::Unmixing);ImGui::SameLine();ImGui::BeginDisabled(blocked || busy());if(ui::Checkbox("Use selected unmixing",&use_unmixing)){use_correction=false;invalidate();}mark(Action::UnmixDomain);ImGui::EndDisabled();
        if(use_unmixing){ui::TextWrapped("Unmixed components/residuals. Set thresholds in the chosen output's units; ROI plane must be channel-independent.");if(chosen_unmixing)ui::TextWrapped("Selected unmixing: %s",ui::short_record_id(chosen_unmixing_id).c_str());if(chosen_unmixing)ui::record_identifier_details(chosen_unmixing_id);else ui::TextWrapped("Before a new run, choose a kept recipe in Linear unmixing. You can still Recheck a kept run.");}
        else ui::TextWrapped(use_correction?"Corrected-domain analysis. Set thresholds in corrected sample units.":"Original-channel analysis. RGB display colors do not select measurement channels.");
        if(use_correction && chosen_correction){ui::TextWrapped("Selected correction: %s",ui::short_record_id(chosen_correction_id).c_str());ui::record_identifier_details(chosen_correction_id);}
        if(run)ui::TextWrapped("Measured domain: %s",run->measurement_domain().c_str());
        if(const auto view=doc.server()->tiff_view())ui::Text("Source series: %u",view->series);
        ImGui::BeginDisabled(busy() || blocked);if(ui::Checkbox("Process by tiles",&tiling)){invalidate();state="idle";}mark(Action::Tiling);
        if(tiling){if(tile_directory.draw(path_context)){invalidate();state="idle";}ui::TextWrapped("Run again with matching inputs to verify and resume saved tiles.");}ImGui::EndDisabled();
        const auto run_original=tiling?"Run / resume tiles":"Run fluorescence";
        const auto run_available=availability(Action::Run,doc,selected,blocked,has_image);
        if(ui::WorkflowAction(tiling?"Run / resume tiles":"Run fluorescence",!run?ui::design::Action::Primary:ui::design::Action::Secondary,run_available))attempt([&]{start(doc,selected);});mark(Action::Run);
        ui::WorkflowReason(run_original,run_available);
        bounds[std::size_t(Action::Cancel)]={};
        if(busy()) {if(ui::WorkflowIcon("Cancel fluorescence",ui::Icon::Stop)){job.cancel();cancel.store(true);discard_contours=true;if(!pending_replay.empty()){replay_source.cancel();pending_replay.clear();pending_unmix.clear();}state="cancelling";}mark(Action::Cancel);}
        ui::Text("Fluorescence state: %s",ui::caption(ui::parse_run_state(state),state).c_str());
        if(tiled_report){const auto& report=*tiled_report;
            ui::Text("Complete cells: %llu / tiles: %zu",(unsigned long long)report.cells,report.tiles);ui::Text("Reused tiles: %zu / segmented: %zu",report.reused_tiles,report.segmented_tiles);
            ui::Text("Unresolved boundary tiles: %zu",report.unresolved_tiles);ui::Text("Resolved threshold: %.8g",report.resolved_threshold);
            ui::TextWrapped("Measured domain: %s",report.measurement_domain.c_str());ui::TextWrapped("Masks and raw measurements: %s",report.checkpoint_path.c_str());
            if(report.unresolved_tiles)ui::TextWrapped("Boundary-dependent tiles remain unresolved. Review the halo and run changed settings into a new results folder.");
        }
        if(run){ui::Text("Detected cells: %zu",run->raster().measurements.size());
            ui::Text("Boundary cells: %zu",boundary_count);ui::Text("Resolved threshold: %.8g",run->raster().resolved_threshold);ui::Checkbox("Show fluorescence preview",&shown);ui::Checkbox("Preview cell bodies",&body);
            if(run->region().z!=display_z || run->region().t!=display_t)ui::TextWrapped("Preview hidden: it was made for another Z or time point.");
            if(sampled)ui::TextWrapped("Preview is sampled to at most 1024 pixels per axis; analysis keeps the original lattice.");
        }
        if(tiled_report)ui::TextWrapped("Tiled runs write masks and measurements to the folder above. They are not added to the project.");

        const auto keep_cells_role=run && !published && has_geos() && !run->raster().measurements.empty()?ui::design::Action::Primary:ui::design::Action::Secondary;
        const auto keep_cells_available=availability(Action::KeepCells,doc,selected,blocked,has_image);
        if(ui::WorkflowAction("Keep fluorescence cells",keep_cells_role,keep_cells_available))attempt([&]{require_current_fluorescence_cells(doc,*request,*run);cancel.store(false);discard_contours=false;const auto snapshot=run;TissueContourOptions limit;limit.work_bytes=request->options().work_bytes;
            contours=std::async(std::launch::async,[this,snapshot,limit]{return trace_fluorescence_cells(snapshot,limit,&cancel);});state="tracing";});mark(Action::KeepCells);
        if(keep_cells_role==ui::design::Action::Primary)ui::WorkflowReason("Keep fluorescence cells",keep_cells_available);
        if(published)ui::Text("Added cell objects: %zu (Undo available)",count);
        if(!has_geos())ui::TextWrapped("Cell objects require GEOS. Run records remain available.");
        const auto keep_record_role=run && (published || !has_geos() || run->raster().measurements.empty()) && kept.empty()?ui::design::Action::Primary:ui::design::Action::Secondary;
        const auto keep_record_available=availability(Action::KeepRecord,doc,selected,blocked,has_image);
        if(ui::WorkflowAction("Keep fluorescence run",keep_record_role,keep_record_available))attempt([&]{kept=keep_fluorescence_cell_run(doc,*request,*run);last_kept=kept;record=kept;records_dirty=true;});mark(Action::KeepRecord);
        if(keep_record_role==ui::design::Action::Primary)ui::WorkflowReason("Keep fluorescence run",keep_record_available);
        if(!kept.empty())ui::TextWrapped("Kept record: %s",ui::short_record_id(kept).c_str());
        if(run && !run->raster().measurements.empty() && ui::CollapsingHeader("Inspect compartment measurements"))measurements();
        ImGui::BeginDisabled(busy() || blocked || unmixing.busy() || correction.busy());record=record_rows.draw("Kept fluorescence record###Saved fluorescence record",records,record,doc.image_data_properties().get(),doc.revision(),"Fluorescence");
        ImGui::EndDisabled();if(ui::WorkflowAction("Replay fluorescence record",ui::design::Action::Secondary,availability(Action::Replay,doc,selected,blocked,has_image)))attempt([&]{replay(doc);});mark(Action::Replay);
        if(ui::WorkflowAction("Load settings from this result###Fluorescence load result settings",ui::design::Action::Secondary,availability(Action::Replay,doc,selected,blocked,has_image)))attempt([&]{replay(doc,true);});
        if(ui::WorkflowAction("Remove fluorescence record",ui::design::Action::Secondary,availability(Action::Remove,doc,selected,blocked,has_image)))attempt([&]{const auto id=record;remove_fluorescence_cell_record(doc,id);if(kept==id)kept.clear();records_dirty=true;});mark(Action::Remove);
        ui::TextWrapped("Recheck verifies kept analysis identity. Later edits to added cells do not recompute observed intensities.");
        if(!error.empty())ui::TextWrapped("Fluorescence: %s",error.c_str());
        if(ui::CollapsingHeader("Fluorescence settings"))settings(blocked,selected);

    }
    void settings(bool blocked,std::uint64_t selected){
        ImGui::BeginDisabled(busy() || blocked);bool changed=false;
        changed=ui::Checkbox("Use selected ROI",&use_selected)||changed;
        if(use_selected)ui::Text("Selected ROI: %llu",static_cast<unsigned long long>(selected));
        else {changed=ui::Checkbox("Use explicit crop",&use_crop)||changed;if(use_crop){
            changed=ui::InputScalar("Crop X",ImGuiDataType_S64,&region.region.x)||changed;changed=ui::InputScalar("Crop Y",ImGuiDataType_S64,&region.region.y)||changed;
            changed=ui::InputScalar("Crop width",ImGuiDataType_S64,&region.region.width)||changed;changed=ui::InputScalar("Crop height",ImGuiDataType_S64,&region.region.height)||changed;}}
        changed=ui::InputScalar(use_unmixing?"First derived output":"First original channel",ImGuiDataType_U32,&region.first_channel)||changed;changed=ui::InputScalar("Channel count",ImGuiDataType_U32,&region.channels)||changed;
        changed=ui::InputScalar("Segmentation channel in block",ImGuiDataType_U32,&options.segmentation_channel)||changed;
        changed=ui::InputScalar("Analysis Z",ImGuiDataType_U32,&region.z)||changed;changed=ui::InputScalar("Analysis time",ImGuiDataType_U32,&region.t)||changed;
        changed=ui::Combo("Threshold method",&method,"Explicit\0Otsu 256\0")||changed;options.threshold_method=FluorescenceThreshold(method);
        if(!method)changed=ui::InputDouble("Raw threshold",&options.threshold,0,0,"%.8g")||changed;
        changed=ui::Combo("Analysis units",&units,"Pixels\0Micrometres\0")||changed;options.units=FluorescenceCellUnits(units);
        changed=ui::InputDouble("Gaussian sigma",&options.sigma,0,0,"%.5g")||changed;changed=ui::InputDouble("Peak prominence",&options.peak_prominence,0,0,"%.5g")||changed;
        changed=ui::InputDouble("Minimum nucleus area",&options.min_nucleus_area,0,0,"%.5g")||changed;
        bool max=bool(options.max_nucleus_area);if(ui::Checkbox("Limit nucleus area",&max)){options.max_nucleus_area=max?std::optional<double>(1000):std::nullopt;changed=true;}
        if(max)changed=ui::InputDouble("Maximum nucleus area",&*options.max_nucleus_area,0,0,"%.5g")||changed;
        changed=ui::InputDouble("Raw cell expansion",&options.cell_expansion,0,0,"%.5g")||changed;
        if(tiling){changed=ui::InputScalar("Tile width",ImGuiDataType_U32,&tiles.width)||changed;changed=ui::InputScalar("Tile height",ImGuiDataType_U32,&tiles.height)||changed;
            changed=ui::InputScalar("Segmentation halo",ImGuiDataType_U32,&tiles.halo)||changed;changed=ui::InputScalar("Checkpoint disk limit (MiB)",ImGuiDataType_U64,&checkpoint_mib)||changed;}
        changed=ui::InputScalar("Context X",ImGuiDataType_U32,&context[0])||changed;changed=ui::InputScalar("Context Y",ImGuiDataType_U32,&context[1])||changed;
        changed=ui::InputScalar("Maximum cells",ImGuiDataType_U64,&options.max_cells)||changed;changed=ui::InputScalar("Work bytes",ImGuiDataType_U64,&options.work_bytes)||changed;
        if(changed){invalidate();state="idle";}
        ImGui::EndDisabled();
    }
    void measurements(){const auto& all=run->raster().measurements;ui::InputInt("Inspect cell",&row);row=std::clamp(row,0,int(all.size()-1));const auto& m=all[std::size_t(row)];
        ui::InputInt("Inspect channel in block",&channel);channel=std::clamp(channel,0,int(m.nucleus.channels.size()-1));const auto original=run->region().first_channel+std::uint32_t(channel);
        const auto& names=run->source_metadata().channel_names;ui::Text(run->unmix_record().empty()?"Original channel: %u / %s":"Derived output: %u / %s",original,original<names.size()?names[original].c_str():"unnamed");
        const auto compartment=[&](const char* title,const RawCellCompartment& c){ui::Text("%s: %llu pixels / %llu valid",title,(unsigned long long)c.pixels,(unsigned long long)c.valid_pixels);
            const auto& v=c.channels[std::size_t(channel)];ui::Text("Finite: %llu / nonfinite: %llu",(unsigned long long)v.finite_samples,(unsigned long long)v.nonfinite_samples);
            if(v.mean)ui::Text("Mean: %.8g",*v.mean);else ui::TextUnformatted("Mean: absent");if(v.sum)ui::Text("Sum: %.8g",*v.sum);if(v.sample_standard_deviation)ui::Text("Sample SD: %.8g",*v.sample_standard_deviation);};
        compartment("Nucleus",m.nucleus);compartment("Cell body",m.cell);if(m.cytoplasm)compartment("Cytoplasm",*m.cytoplasm);else ui::TextUnformatted("Cytoplasm: absent");
    }
};
GuiFluorescence::GuiFluorescence():impl_(std::make_unique<Impl>()){}
GuiFluorescence::~GuiFluorescence()=default;
void GuiFluorescence::open(){impl_->open=true;}
void GuiFluorescence::open_marker_rules(){impl_->markers.open();}
void GuiFluorescence::poll(Document& doc,bool blocked){impl_->poll(doc,blocked);}
std::vector<ui::PathField*> GuiFluorescence::paths(){auto fields=impl_->correction.paths();for(auto* field:impl_->unmixing.paths())fields.push_back(field);for(auto* field:impl_->markers.paths())fields.push_back(field);fields.push_back(&impl_->tile_directory);return fields;}
void GuiFluorescence::draw(Document& doc,std::uint64_t selected,bool blocked,const ui::PathContext& context,bool has_image){
    ui::i18n::SurfaceTrace translation_surface("tool/Fluorescence");auto& s=*impl_;s.tile_directory.sync(context);s.poll(doc,blocked);s.correction.draw(doc,blocked || s.busy(),context,has_image);if(auto chosen=s.correction.take_selection()){try{set_correction(*chosen);s.open=true;}catch(const std::exception& e){s.capture_error(e);}}s.unmixing.draw(doc,blocked || s.busy(),context,has_image);if(auto chosen=s.unmixing.take_selection()){try{set_unmixing(*chosen);s.open=true;}catch(const std::exception& e){s.capture_error(e);}}s.markers.draw(doc,selected,blocked,context,has_image);s.bounds={};if(!s.open)return;
    ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Fluorescence),{560,860});if(ImGui::Begin(ui::tool_window_label(ui::ToolId::Fluorescence),&s.open)){s.controls(doc,selected,blocked,context,has_image);}ImGui::End();}
void GuiFluorescence::draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size){auto& s=*impl_;if(!s.shown || !s.run || s.run->region().z!=s.display_z || s.run->region().t!=s.display_t)return;s.build_texture();if(!s.texture)return;
    const auto& r=s.run->region().region;const auto a=view.image_to_screen({double(r.x),double(r.y)}),b=view.image_to_screen({double(r.x+r.width),double(r.y+r.height)});
    draw->PushClipRect(offset,{offset.x+size.x,offset.y+size.y},true);draw->AddImage(ImTextureID(s.texture),{offset.x+float(a.x),offset.y+float(a.y)},{offset.x+float(b.x),offset.y+float(b.y)});draw->PopClipRect();}
void GuiFluorescence::set_request(RawRegionRequest r,FluorescenceCellOptions o,std::array<std::uint32_t,2> c,bool selected){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");o.validate();s.invalidate();s.region=r;s.options=o;s.context=c;s.use_selected=selected;s.use_crop=!selected;s.method=int(o.threshold_method);s.units=int(o.units);}
void GuiFluorescence::set_tiling(bool enabled,FluorescenceTileOptions options,const std::string& directory){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");options.validate();if(directory.size()>=4096)throw std::invalid_argument("Tiled results folder path is too long");s.invalidate();s.tiling=enabled;s.tiles=options;s.tile_directory.set_option(directory);}
void GuiFluorescence::set_correction(const DarkFlatRequest& request){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");s.invalidate();s.chosen_correction=request;s.correction_dropped=false;s.chosen_correction_id=request.recipe().sha256();s.use_correction=true;s.use_unmixing=false;}
void GuiFluorescence::set_unmixing(const LinearUnmixRequest& request){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");s.invalidate();s.chosen_unmixing=request;s.chosen_unmixing_id=request.recipe().sha256();s.use_unmixing=true;s.use_correction=false;}
bool GuiFluorescence::uses_unmixing()const{return impl_->use_unmixing;}
GuiLinearUnmixing& GuiFluorescence::unmixing_tool(){return impl_->unmixing;}
void GuiFluorescence::use_original(){auto& s=*impl_;if(s.busy())throw std::runtime_error("Fluorescence is busy");s.invalidate();s.use_correction=false;s.use_unmixing=false;}
bool GuiFluorescence::uses_correction() const{return impl_->use_correction;}
GuiDarkFlat& GuiFluorescence::correction_tool(){return impl_->correction;}
Rect GuiFluorescence::bounds(Action a) const{return impl_->bounds[std::size_t(a)];}
std::shared_ptr<const FluorescenceCellRun> GuiFluorescence::result() const{return impl_->run;}
std::shared_ptr<const FluorescenceTiledReport> GuiFluorescence::tiled_result() const{return impl_->tiled_report;}
const std::string& GuiFluorescence::error() const{return impl_->error;}
const char* GuiFluorescence::notification_error_key() const{return impl_->state=="ready"?impl_->error_key:nullptr;}
const std::string& GuiFluorescence::state() const{return impl_->state;}
const std::string& GuiFluorescence::kept_record() const{return impl_->kept;}
std::size_t GuiFluorescence::published_cells() const{return impl_->count;}
bool GuiFluorescence::busy() const{return impl_->busy();}
GuiFluorescence::Form GuiFluorescence::form() const {const auto& s=*impl_;return {s.region,s.options,s.context,s.use_selected,s.use_crop,s.tiling,s.use_correction,s.use_unmixing,s.chosen_correction_id,s.chosen_unmixing_id};}
std::optional<FluorescenceCellRequest> GuiFluorescence::request() const{return impl_->request;}
}
