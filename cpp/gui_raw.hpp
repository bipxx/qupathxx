#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_record_list.hpp"
#include "gui_path_field.hpp"
#include "gui_output_gate.hpp"
#include "gui_record_removal.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "ascii_case.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "reader_registry.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include "gui_raw_grid_controls.hpp"
#include "utf8_text.hpp"
#include <array>
#include <future>
#include <functional>
#include <chrono>

namespace qpx::detail {
class GuiRaw {
    ui::PathField path_{{"##raw NumPy path",ui::PathRole::NewFile,{".npy"},"","New NumPy file (.npy)"}};
    ui::PathField csv_path_{{"##raw CSV path",ui::PathRole::NewFile,{".csv"},"","New raw measurements CSV"}};
    friend struct OutputWriterTestAccess;
    enum class Kind {Metadata,View,Series,Export,Image};
    struct Result {
        Kind kind;std::string state="succeeded",error;std::shared_ptr<ImageServer> source,candidate;
        std::shared_ptr<const char> identity;std::shared_ptr<const RawImageMetadata> metadata;
        std::size_t bytes=0;std::string project_instance;
    };
    static constexpr auto first=GuiInputTarget::ApplyTiffView,last=GuiInputTarget::RawRecordsTab;
    static constexpr auto target_count=std::size_t(last)-std::size_t(first)+1;
    std::array<Rect,target_count> targets_{};std::array<bool,target_count> enabled_{};
    bool open_=false,initial_options_=false,initial_request_=false,initial_record_=false;
    bool use_selected_=true,follow_display_=true,reset_request_=true,nucleus_=false;
    bool raw_pending_=false,csv_pending_=false,verified_=false,require_current_=false,export_selected_=false;
    int initial_tab_=0;bool activate_tab_=true;
    TiffOpenOptions options_;RawRegionRequest request_;
    std::optional<RawGridSampling> grid_;std::size_t grid_work_=256u*1024*1024;
    std::shared_ptr<ImageServer> source_;std::weak_ptr<const char> identity_;std::weak_ptr<ImageServer> applied_;
    std::shared_ptr<const RawImageMetadata> metadata_;std::shared_ptr<const RawRegionStatistics> statistics_;
    std::shared_ptr<const RawAnalysisResult> analysis_;
    std::weak_ptr<const RawAnalysisResult> kept_result_;
    std::future<Result> future_;std::atomic<bool> cancel_{false};Kind running_=Kind::Metadata;
    RawAnalysisJob raw_job_;RawAnalysisStats raw_stats_;RawTableJob csv_job_;RawTableStats csv_stats_;
    std::string state_="waiting",task_="idle",error_;
    ui::RecordRows record_rows_;
    std::vector<std::shared_ptr<const RawAnalysisRecord>> record_page_;
    std::uint64_t selected_=0,kept_=0,verify_id_=0,cursor_=0;std::vector<std::uint64_t> previous_;
    std::size_t changes_=0,export_bytes_=0,catalog_max_=0;
    static std::size_t index(GuiInputTarget target) {return std::size_t(target)-std::size_t(first);}
    bool busy() const {return future_.valid() || raw_pending_ || csv_pending_;}
    void failure(const std::exception& error) {try {error_=error.what();bounded_utf8(error_,8192);}catch(...) {error_="Raw image operation failed";}}
    void refresh(Document& doc) {
        if(source_==doc.server() && identity_.lock()==doc.analysis_identity_.token) return;
        const bool different=identity_.lock()!=doc.analysis_identity_.token;reset_request_=different;
        if(future_.valid() && running_!=Kind::Image) cancel_.store(true);
        if(different && source_) {
            raw_job_.cancel();statistics_.reset();analysis_.reset();kept_result_.reset();verified_=false;kept_=0;
            if(!initial_record_) selected_=0;cursor_=0;previous_.clear();if(!raw_pending_) task_="idle";
        }
        source_=doc.server();identity_=doc.analysis_identity_.token;metadata_.reset();
        state_=supports(source_->reader_id())?"waiting":"unsupported";
        if(supports(source_->reader_id())) initial_record_=false;
        if(!raw_pending_ && !csv_pending_) error_.clear();
    }
    void metadata_job(Document& doc) {
        if(busy()) return;cancel_.store(false);state_="loading";running_=Kind::Metadata;
        const auto source=source_;const auto identity=doc.analysis_identity_.token;
        future_=std::async(std::launch::async,[this,source,identity] {
            Result result{Kind::Metadata};result.source=source;result.identity=identity;
            try {result.metadata=source->raw_metadata(&cancel_);export_checkpoint(&cancel_);if(!result.metadata)throw std::invalid_argument("image has no quantitative channels");if(!source->raw_view()) throw std::invalid_argument("Raw samples are available; this reader has no editable channel view or quantitative workflow.");}
            catch(const std::exception& error) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=error.what();bounded_utf8(result.error,8192);}catch(...) {result.error="Channel metadata opening failed";}}
            return result;
        });
    }
    void view_job(Document& doc,bool series) {
        options_.validate();if(!metadata_ || busy()) return;
        if(!series && options_.view.series!=metadata_->series) throw std::invalid_argument("open another series as a new image");
        cancel_.store(false);state_="changing";running_=series?Kind::Series:Kind::View;
        const auto source=source_;const auto options=options_;const auto identity=doc.analysis_identity_.token;const auto kind=running_;applied_.reset();
        future_=std::async(std::launch::async,[this,source,options,identity,kind] {
            Result result{kind};result.source=source;result.identity=identity;
            try {const auto previous=source->tiff_options();
                if(source->reader_id()=="ome-zarr") {if(kind==Kind::Series)throw std::invalid_argument("Zarr group has one image series");result.candidate=source->with_raw_view(options.view,&cancel_);}
                else
                if(kind==Kind::View && previous && previous->cache_bytes==options.cache_bytes && previous->decode_bytes==options.decode_bytes) result.candidate=source->with_tiff_view(options.view,&cancel_);
                else result.candidate=open_tiff(source->source_locator(),options,&cancel_);
                export_checkpoint(&cancel_);result.metadata=result.candidate->raw_metadata();
            }catch(const std::exception& error) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=error.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TIFF view opening failed";}}
            return result;
        });
    }
    void data_job(Document& doc,ObjectSelection& selection,const Project* project,bool save,OutputGate* gate=nullptr) {
        if(!metadata_ || busy()) return;auto request=request_;std::uint64_t object=0;
        if(follow_display_) {const auto view=source_->raw_view();request.z=view->z;request.t=view->t;}
        if(use_selected_ && selection.primary()) object=selection.primary();
        if(!save) {
            if(grid_ && !object) throw std::invalid_argument("select an area ROI for a grid measurement");
            ScientificRawOptions sampling;sampling.first_channel=request.first_channel;sampling.channels=request.channels;
            sampling.z=request.z;sampling.t=request.t;sampling.work_bytes=grid_work_;
            RawAnalysisRequest captured=grid_?RawAnalysisRequest::for_grid_roi(doc,object,*grid_,sampling,nucleus_):
                RawAnalysisRequest(doc,request,object,object?nucleus_:false);
            if(!raw_job_.request(captured)) throw std::runtime_error("raw analysis is already running");
            raw_pending_=true;raw_stats_.state="running";verify_id_=0;verified_=false;analysis_.reset();statistics_.reset();task_="reading";error_.clear();return;
        }
        if(grid_) throw std::invalid_argument("select natural sampling to export original raw samples");
        // Export retains the entire selected bounding rectangle, including OOB
        // padding and pixels outside a nonrectangular selected area.
        if(object) {
            const auto selected=doc.annotation(object);if(!selected.geometry.is_area()) throw std::invalid_argument("select an area ROI or use the rectangle fields");
            if(nucleus_ && !selected.nucleus) throw std::invalid_argument("selected object has no nucleus");
            const auto& roi=nucleus_?*selected.nucleus:selected.geometry;const auto bounds=roi.bounds();
            if(request.region.level<0 || std::size_t(request.region.level)>=metadata_->levels.size()) throw std::out_of_range("invalid raw pyramid level");
            const auto scale=metadata_->levels[request.region.level].downsample;
            const auto x=std::int64_t(std::floor(bounds.x/scale))*std::int64_t(scale),y=std::int64_t(std::floor(bounds.y/scale))*std::int64_t(scale);
            request.region=RegionRequest(x,y,request.region.level,std::int64_t(std::ceil((bounds.x+bounds.width-x)/scale)),std::int64_t(std::ceil((bounds.y+bounds.height-y)/scale)));
        }
        if(!path_.text()[0]) throw std::invalid_argument("enter a new NumPy file path");request.validate(*metadata_);
        if(!gate)throw std::logic_error("Output gate is unavailable");
        gate->submit(path_.state().resolved,output_scope("Raw measurement",false,ui::command(ui::CommandId::Tool_Channels).caption),
            [this,gate,source=source_,identity=doc.analysis_identity_.token,request](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>& paths) {
                cancel_.store(false);task_="reading";running_=Kind::Export;
                future_=std::async(std::launch::async,[this,source,identity,request,output,paths,policy] {
                    Result result{Kind::Export};result.source=source;result.identity=identity;
                    try {for(const auto& path:paths)guard_file_output(export_path(path),"",output);const auto raw=source->read_raw(request,&cancel_);
                        for(const auto& path:paths)guard_file_output(export_path(path),"",output);save_raw_numpy(raw,output,policy==ExportWritePolicy::ReplaceExisting,&cancel_);result.bytes=std::size_t(std::filesystem::file_size(std::filesystem::u8path(output)));result.state="saved";
                    }catch(const std::exception& e){result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="Raw export failed";}}return result;
                });
                gate->watch(output,[this]()->std::optional<bool>{if(future_.valid())return {};return task_=="saved";},[this]{return task_=="failed"?error_:std::string{};});
            },[this](const std::string& message){error_=message;},false,{}, {source_->source_locator()});
    }
    void verify_record(Document& doc) {
        if(busy() || !selected_) return;const auto request=RawAnalysisRequest::replay(doc,selected_);
        if(!raw_job_.request(request)) throw std::runtime_error("raw analysis is already running");
        raw_pending_=true;raw_stats_.state="running";verify_id_=selected_;verified_=false;analysis_.reset();statistics_.reset();task_="reading";error_.clear();
    }
    void export_records(const Document& doc,const Project*,OutputGate& gate) {
        if(busy())return;
        const std::vector<std::uint64_t> ids=export_selected_?std::vector<std::uint64_t>{selected_}:std::vector<std::uint64_t>{};
        gate.submit(csv_path_.state().resolved,output_scope("Raw measurement",false,ui::command(ui::CommandId::Tool_Channels).caption),
            [this,&gate,document=&doc,ids,verified=require_current_](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& paths) {
                RawTableRequest request(*document,ids,verified,128u*1024*1024,paths);
                if(!csv_job_.request(request,path,policy==ExportWritePolicy::ReplaceExisting))throw std::runtime_error("raw measurement export is already running");
                csv_pending_=true;csv_stats_.state="running";error_.clear();
                gate.watch(path,[this]()->std::optional<bool>{if(csv_pending_)return {};return csv_stats_.state=="saved";},[this]{return csv_stats_.state=="failed"?csv_stats_.error:std::string{};});
            },[this](const std::string& message){csv_stats_.state="failed";failure(std::runtime_error(message));});
    }
    void poll_jobs(Document& doc) {
        if(kept_) {try {doc.raw_analysis_record(kept_);}catch(const std::out_of_range&) {kept_=0;kept_result_.reset();}}
        if(raw_pending_ || raw_stats_.state=="ready") {
            raw_stats_=raw_job_.poll(doc);
            if(raw_pending_ && raw_stats_.state!="running") {
                raw_pending_=false;
                if(raw_stats_.state=="ready") {
                    analysis_=raw_job_.result();statistics_=std::shared_ptr<const RawRegionStatistics>(analysis_,&analysis_->statistics());task_="succeeded";error_.clear();
                    if(verify_id_) {
                        try {verified_=raw_job_.verify_result(doc,verify_id_);if(!verified_) error_="Saved raw inputs or results differ from the current data";}
                        catch(const std::exception& error) {failure(error);task_="failed";}
                        verify_id_=0;
                    }
                }else {analysis_.reset();statistics_.reset();task_=raw_stats_.state;error_=raw_stats_.error;if(task_=="stale" && error_.empty()) error_="The captured raw measurement no longer matches the image or ROI";}
            }else if(!raw_pending_ && raw_stats_.state=="stale") {analysis_.reset();statistics_.reset();task_="stale";}
        }
        if(csv_pending_) {csv_stats_=csv_job_.poll();if(csv_stats_.state!="running") {csv_pending_=false;if(csv_stats_.state=="failed") error_=csv_stats_.error;}}
    }
    static ScientificRawOptions selected_axes(const RawAnalysisResult& result) {
        if(result.grid()) return result.grid()->options;
        const auto& region=result.region();ScientificRawOptions options;
        options.first_channel=region.first_channel;options.channels=region.channels;options.z=region.z;options.t=region.t;
        return options;
    }
    static void result_table(const RawAnalysisResult& result,float scale,std::size_t& maximum) {
        const auto& stats=result.statistics();const auto region=selected_axes(result);const auto& context=result.object_context();
        ui::DataTextWrapped("Captured Z %u / time %u / channels %u..%u",region.z,region.t,region.first_channel,region.first_channel+region.channels-1);
        if(result.object_id()) ui::DataTextWrapped("ROI: %s (#%llu)",context.name.c_str(),static_cast<unsigned long long>(result.object_id()));
        if(context.tma) ui::DataTextWrapped("Case: %s / core %s",context.tma->case_id.c_str(),context.tma->core_name.c_str());
        if(result.grid()) {
            const auto& grid=result.grid()->grid;
            ui::DataTextf("Core grid: %lld x %lld",static_cast<long long>(grid.x().samples()),static_cast<long long>(grid.y().samples()));
            ui::DataTextf("Source: %s / measured samples: float64",sample_name(result.metadata().sample_type).c_str());
            ui::DataTextf("Eligible core samples: %llu",static_cast<unsigned long long>(stats.pixels));
            ui::DataTextf("Fully supported samples: %llu",static_cast<unsigned long long>(stats.valid_pixels));
        } else ui::DataTextf("Valid pixels: %llu",static_cast<unsigned long long>(stats.valid_pixels));
        if(ImGui::BeginTable("raw means",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,150.f*scale})) {
            ImGui::TableSetupColumn("Channel");ImGui::TableSetupColumn("Mean");ImGui::TableSetupColumn("Finite");ImGui::TableSetupColumn("Nonfinite");ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(int(stats.means.size()));std::size_t inspected=0;
            while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd && inspected<512;++i) {
                ++inspected;ImGui::TableNextRow();ImGui::TableNextColumn();ui::DataText(result.metadata().channel_names[i].c_str());ImGui::TableNextColumn();
                if(stats.means[i]) ui::DataTextf("%.10g",*stats.means[i]);else ui::DataText("No finite samples");
                ImGui::TableNextColumn();ui::DataTextf("%llu",static_cast<unsigned long long>(stats.finite_samples[i]));ImGui::TableNextColumn();ui::DataTextf("%llu",static_cast<unsigned long long>(stats.nonfinite_samples[i]));
            }
            maximum=std::max(maximum,inspected);ImGui::EndTable();
        }
    }
public:
    std::vector<ui::PathField*> paths(){return {&path_,&csv_path_};}
    static bool supports(std::string_view reader_id) {return reader_id=="tiff" || reader_id=="ome-zarr";}
    struct Change {bool refresh=false;std::uint64_t image=0;};
    explicit GuiRaw(const GuiOptions& options):open_(options.raw_open),initial_options_(bool(options.tiff_options)),initial_request_(bool(options.raw_request)),initial_record_(options.raw_record_id!=0),
        nucleus_(options.analysis_nucleus),require_current_(options.raw_require_current),initial_tab_(options.raw_tab),options_(options.tiff_options?*options.tiff_options:TiffOpenOptions{}),request_(options.raw_request?*options.raw_request:RawRegionRequest{}),selected_(options.raw_record_id) {
        if(initial_tab_<0 || initial_tab_>3) throw std::invalid_argument("invalid quantitative channels tab");
        if(options.raw_grid) {options.raw_grid->validate();grid_=*options.raw_grid;}
        grid_work_=options.raw_grid_work_bytes;
        ScientificRawOptions sampling;sampling.work_bytes=grid_work_;sampling.validate();
        follow_display_=!initial_request_;options_.validate();bounded_utf8(options.raw_numpy_path,32768);bounded_utf8(options.raw_csv_path,32768);
        path_.set_option(options.raw_numpy_path);csv_path_.set_option(options.raw_csv_path);
        if(!valid_reader_request(options.image_reader)) throw std::invalid_argument("invalid image reader selection");
    }
    ~GuiRaw() {try {finish(nullptr,false);}catch(...) {}}
    void open() {open_=true;if(state_=="cancelled" || state_=="failed") state_="waiting";}
    void open_image(Project& project,const std::string& path,TiffOpenOptions options,ReaderSelection selection) {
        if(busy()) throw std::runtime_error("finish or cancel the current raw task before adding an image");
        options.validate();cancel_.store(false);running_=Kind::Image;state_="changing";const auto instance=project.instance_identity_;
        future_=std::async(std::launch::async,[this,path,options,instance,selection=std::move(selection)] {
            Result result{Kind::Image};result.project_instance=instance;
            try {ReaderOpenOptions reader_options;reader_options.tiff=options;reader_options.zarr.view=options.view;result.candidate=open_reader(path,selection,reader_options,&cancel_);export_checkpoint(&cancel_);}
            catch(const std::exception& error) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=error.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TIFF image opening failed";}}return result;
        });
    }
    Rect bounds(GuiInputTarget target) const {return targets_.at(index(target));}
    bool enabled(GuiInputTarget target) const {return enabled_.at(index(target));}
    bool ready(const Document& doc) const {return metadata_ && source_==doc.server() && identity_.lock()==doc.analysis_identity_.token && state_=="ready";}
    bool applied(const Document& doc) const {return applied_.lock()==doc.server();}
    bool finished() const {return task_!="idle" && task_!="reading";}
    bool record_kept() const {return kept_!=0;}
    bool record_verified() const {return verified_;}
    std::uint64_t record_id() const {return selected_;}
    std::string record_status(const Document& doc) const {if(!selected_) return {};try {return doc.raw_analysis_status(selected_);}catch(const std::out_of_range&) {return {};}}
    const std::string& state() const {return state_;}const std::string& task_state() const {return task_;}const std::string& error() const {return error_;}
    const std::string& analysis_state() const {return raw_stats_.state;}const auto& analysis_result() const {return analysis_;}
    const std::string& csv_state() const {return csv_stats_.state;}std::uint64_t csv_rows() const {return csv_stats_.completed_rows;}std::size_t csv_bytes() const {return csv_stats_.bytes;}
    const auto& statistics() const {return statistics_;}std::size_t changes() const {return changes_;}std::size_t export_bytes() const {return export_bytes_;}std::size_t catalog_max_step() const {return catalog_max_;}
    Change poll(Document& doc,Project* project,bool allow_apply=true) {
        poll_jobs(doc);Change changed;if(!future_.valid() || future_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return changed;
        Result result{running_};try {result=future_.get();}catch(const std::exception& error) {failure(error);state_="failed";task_="failed";return changed;}
        const bool current=result.kind==Kind::Image?project && result.project_instance==project->instance_identity_:result.source==doc.server() && result.identity==doc.analysis_identity_.token;
        if(result.kind==Kind::Export && result.state=="saved") {task_="saved";export_bytes_=result.bytes;error_.clear();return changed;}
        if(!current) return changed;
        if(cancel_.load() || result.state!="succeeded") {if(result.kind==Kind::Export) task_=cancel_.load()?"cancelled":result.state;else state_=cancel_.load()?"cancelled":result.state;error_=result.error;return changed;}
        try {
            if(result.kind==Kind::Image) {
                if(!allow_apply || !project) {state_="cancelled";return changed;}
                applied_=result.candidate;if(result.candidate->reader_id()=="ome-zarr")changed.image=project->add_document(std::make_shared<Document>(std::move(result.candidate)));else changed.image=project->add_tiff_source(std::move(result.candidate));++changes_;state_="ready";error_.clear();
            }else if(result.kind==Kind::Metadata) {
                metadata_=std::move(result.metadata);const auto current_view=*source_->raw_view();
                if(!initial_options_) {if(auto current=source_->tiff_options()) options_=*current;options_.view=current_view;}
                else {for(std::size_t i=0;i<3;++i) {if(options_.view.channels[i]==-2) options_.view.channels[i]=current_view.channels[i];if(std::isnan(options_.view.lower[i])) {options_.view.lower[i]=current_view.lower[i];options_.view.upper[i]=current_view.upper[i];}}initial_options_=false;}
                if(reset_request_ && !initial_request_) {request_={};request_.region=RegionRequest(0,0,0,std::min<std::int64_t>(512,metadata_->width),std::min<std::int64_t>(512,metadata_->height));request_.z=current_view.z;request_.t=current_view.t;}
                initial_request_=false;reset_request_=false;state_="ready";error_.clear();
            }else if(result.kind==Kind::View || result.kind==Kind::Series) {
                if(!allow_apply) {state_="cancelled";return changed;}
                if(result.kind==Kind::Series) {if(!project) throw std::invalid_argument("a project is required to add TIFF series");applied_=result.candidate;changed.image=project->add_tiff_source(std::move(result.candidate));}
                else {const auto old=doc.server();doc.adopt_tiff_source(std::move(result.candidate));applied_=doc.server();changed.refresh=old!=doc.server();}
                ++changes_;state_="ready";error_.clear();
            }
        }catch(const std::exception& error) {failure(error);state_="failed";}
        return changed;
    }
    void finish(Document* doc,bool finite) {
        if(!finite) {raw_job_.cancel();csv_job_.cancel();}
        raw_job_.await_completion();csv_job_.await_completion();if(doc) poll_jobs(*doc);
        if(!future_.valid()) return;if(!finite || running_!=Kind::Export) cancel_.store(true);
        future_.wait();if(doc) poll(*doc,nullptr,false);else future_.get();
    }
    Change draw(Document& doc,ObjectSelection& selection,Project* project,bool blocked,const std::function<void(GuiInputTarget)>& mark,RecordRemovalDialog* removal=nullptr,std::uint64_t epoch=0,OutputGate* gate=nullptr,const ui::PathContext& context={},bool has_image=true) {
    ui::i18n::SurfaceTrace translation_surface("tool/Quantitative channels");
        enabled_.fill(false);refresh(doc);auto changed=poll(doc,project);refresh(doc);
        if(!blocked && ui::chord_pressed(ui::CommandId::Tool_Channels,ui::frame_commands())) {if(open_) open_=false;else open();}
        if(!open_) return changed;if(state_=="waiting" && !busy()) try {metadata_job(doc);}catch(const std::exception& error) {failure(error);state_="failed";}
        const auto* viewport=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(540.f*scale,viewport->WorkSize.x*.9f),std::min(750.f*scale,viewport->WorkSize.y*.9f)},ImGuiCond_FirstUseEver);
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::Channels),&open_)) {ImGui::End();return changed;}
        auto remember=[&](GuiInputTarget target,bool disabled) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(target)]=!disabled && ImGui::IsItemVisible();mark(target);};
        const auto availability=[&](GuiInputTarget target)->ui::Availability {
            using T=GuiInputTarget;
            if(target==T::CancelRawMeasurements || target==T::CancelRawTask)return {};
            if(!has_image)return {false,"Open an image first."};
            if(blocked)return ui::WorkflowBlocked();
            if(busy())return {false,"Wait for the current work to finish."};
            if((target==T::ApplyTiffView || target==T::AddTiffSeries || target==T::MeasureRawRegion || target==T::ExportRawRegion) && !metadata_)
                return {false,"Open a quantitative OME-TIFF or local OME-Zarr image."};
            switch(target) {
            case T::ApplyTiffView:if(options_.view.series!=metadata_->series)return {false,"Open another series as a new image."};break;
            case T::AddTiffSeries:
                if(!project)return {false,"Open a project first."};
                if(source_->reader_id()=="ome-zarr")return {false,"Open an OME-TIFF image to open another series."};break;
            case T::ExportRawRegion:
                if(grid_)return {false,"Keep a grid measurement in Results, or switch to natural sampling for original-sample NumPy export."};
                if(!path_.text()[0])return {false,"Choose a file path first."};break;
            case T::KeepRawAnalysis:
                if(!analysis_ || raw_stats_.state!="ready")return {false,"Run a preview first."};
                if(kept_result_.lock()==analysis_)return {false,"Run again for a new result."};break;
            case T::ExportRawMeasurements:
                if(!csv_path_.text()[0])return {false,"Choose a file path first."};
                if(!doc.raw_analysis_record_count())return {false,"Keep a result first."};
                if(export_selected_ && !selected_)return {false,"Select a kept result first."};break;
            case T::VerifyRawAnalysisRecord:case T::RemoveRawAnalysisRecord:
                if(!selected_)return {false,"Select a kept result first."};break;
            default:break;
            }
            return {};
        };
        const auto button=[&](const char* text,GuiInputTarget target) {
            const auto available=availability(target);const auto pressed=ui::AvailableButton(text,available,target==GuiInputTarget::MeasureRawRegion?ui::design::Action::Primary:ui::design::Action::Secondary);remember(target,!available.enabled);
            if(target==GuiInputTarget::MeasureRawRegion)ui::WorkflowReason(text,available);return pressed;
        };
        auto tab=[&](const char* title,int number,GuiInputTarget target) {const auto shown=ImGui::BeginTabItem(title,nullptr,activate_tab_ && number==initial_tab_?ImGuiTabItemFlags_SetSelected:0);remember(target,false);return shown;};
        auto perform=[&](auto&& action) {try {error_.clear();action();}catch(const std::exception& error) {failure(error);task_="failed";}};
        if(metadata_) ui::DataTextWrapped("%s / %u channels / %u Z planes / %u timepoints",sample_name(metadata_->sample_type).c_str(),metadata_->channels,metadata_->z_planes,metadata_->timepoints);
        else {ui::DataTextWrapped("Channels: %s",ui::caption(ui::parse_run_state(state_),state_).c_str());if(state_=="unsupported") ui::DataTextWrapped("Open a quantitative OME-TIFF or local OME-Zarr image.");else if(state_=="failed" || state_=="cancelled") {ui::CaptionedStrip retry("Channel metadata",1);retry.next();if(ui::IconButton("Retry channel metadata",ui::Icon::Refresh))state_="waiting";}}
        if(metadata_) {
            try {
                const auto z=doc.z_position_um(options_.view.z),t=doc.timepoint_seconds(options_.view.t);
                ui::DataTextWrapped("Physical axes: %s",doc.image_axis_calibration()?"saved image calibration":"reader calibration");
                if(z)ui::Text("Z: %.10g %s",*z,ui::unit::um);else ui::DataText("Z position: unknown");
                if(t)ui::DataTextf("Time: %.10g s",*t);else ui::DataText("Time: unknown");
            } catch(const std::exception& error) {ui::DataTextWrapped("Physical axes unavailable: %s",error.what());}
        }
        const bool disabled=blocked || busy();
        auto number=[&](const char* label,std::uint32_t& value,std::uint32_t maximum) {int n=static_cast<int>(value);if(ui::InputInt(label,&n)) value=std::uint32_t(std::clamp(n,0,int(maximum-1)));};
        if(ImGui::BeginTabBar("Quantitative views")) {
            if(tab("Display",0,GuiInputTarget::RawDisplayTab)) {
                if(metadata_) {
                    ImGui::BeginDisabled(disabled);number("Series",options_.view.series,metadata_->series_count);number("Z plane",options_.view.z,metadata_->z_planes);number("Timepoint",options_.view.t,metadata_->timepoints);
                    const char* colors[]={"Red","Green","Blue"};for(std::size_t i=0;i<3;++i) {ImGui::PushID(int(i));int channel=options_.view.channels[i];ui::InputInt(colors[i],&channel);options_.view.channels[i]=std::clamp(channel,-1,int(metadata_->channels)-1);ui::InputDouble("Raw lower",&options_.view.lower[i],0,0,"%.10g");ui::InputDouble("Raw upper",&options_.view.upper[i],0,0,"%.10g");ImGui::PopID();}ImGui::EndDisabled();
                    if(button("Apply display channels / Z / time",GuiInputTarget::ApplyTiffView)) perform([&]{view_job(doc,false);});
                    if(button("Open series as a new image",GuiInputTarget::AddTiffSeries)) perform([&]{view_job(doc,true);});
                    ui::DataTextWrapped("Display conversion leaves stored raw samples and saved raw measurements unchanged.");
                }else {
                    ui::TextWrapped("Channel metadata is not ready.");
                    button("Apply display channels / Z / time",GuiInputTarget::ApplyTiffView);
                    button("Open series as a new image",GuiInputTarget::AddTiffSeries);
                }
                ImGui::EndTabItem();
            }
            if(tab("Data",1,GuiInputTarget::RawDataTab)) {
                if(metadata_) {
                    ImGui::BeginDisabled(disabled);
                    bool grid=bool(grid_);if(ui::Checkbox("Explicit raw grid",&grid)) grid_=grid?std::optional<RawGridSampling>(RawGridSampling{}):std::nullopt;
                    if(grid_) {use_selected_=true;raw_grid_controls(*grid_,grid_work_);}
                    ImGui::BeginDisabled(bool(grid_));ui::Checkbox("Use selected area ROI",&use_selected_);ImGui::EndDisabled();
                    if(use_selected_) ui::Checkbox("Use selected nucleus",&nucleus_);
                    if(!grid_) {
                    ui::InputInt64("Level-zero X",&request_.region.x);ui::InputInt64("Level-zero Y",&request_.region.y);ui::InputInt64("Output width",&request_.region.width);ui::InputInt64("Output height",&request_.region.height);
                    int level=request_.region.level;if(ui::InputInt("Pyramid level",&level)) request_.region.level=std::clamp(level,0,int(metadata_->levels.size())-1);
                    }
                    number("First channel",request_.first_channel,metadata_->channels);int count=int(request_.channels);if(ui::InputInt("Channel count",&count)) request_.channels=std::uint32_t(std::clamp(count,1,int(metadata_->channels-request_.first_channel)));
                    ui::Checkbox("Follow displayed Z / time",&follow_display_);if(follow_display_) {const auto view=source_->raw_view();request_.z=view->z;request_.t=view->t;}
                    ImGui::BeginDisabled(follow_display_);number("Data Z",request_.z,metadata_->z_planes);number("Data time",request_.t,metadata_->timepoints);ImGui::EndDisabled();ImGui::EndDisabled();
                    if(button("Measure raw channels",GuiInputTarget::MeasureRawRegion)) perform([&]{data_job(doc,selection,project,false);});
                    path_.draw(context,false,{},{},"Export destination");
                    if(button("Export raw rectangle (.npy)",GuiInputTarget::ExportRawRegion)) perform([&]{data_job(doc,selection,project,true,gate);});
                    if(grid_) ui::DataTextWrapped("Keep a grid measurement in Results, or switch to natural sampling for original-sample NumPy export.");
                    ui::DataTextWrapped("NumPy export preserves the rectangle, including pixels outside a nonrectangular ROI. Existing files ask before replacement.");
                }else {
                    ui::TextWrapped("Channel metadata is not ready.");
                    button("Measure raw channels",GuiInputTarget::MeasureRawRegion);
                    button("Export raw rectangle (.npy)",GuiInputTarget::ExportRawRegion);
                }
                ImGui::EndTabItem();
            }
            if(tab("Results",2,GuiInputTarget::RawResultsTab)) {
                if(button("Keep raw measurement",GuiInputTarget::KeepRawAnalysis)) perform([&]{selected_=kept_=raw_job_.keep_result(doc);kept_result_=analysis_;});
                if(analysis_ && kept_result_.lock()==analysis_) ui::TextWrapped("Kept as record %llu.",static_cast<unsigned long long>(kept_));
                if(analysis_) result_table(*analysis_,scale,catalog_max_);else ui::DataTextWrapped("Measure a selected area or rectangle to inspect raw channel statistics.");
                if(raw_pending_) ui::DataTextf("Tiles: %llu / %llu",static_cast<unsigned long long>(raw_stats_.completed_tiles),static_cast<unsigned long long>(raw_stats_.total_tiles));
                ImGui::EndTabItem();
            }
            if(tab("Records",3,GuiInputTarget::RawRecordsTab)) {
                csv_path_.draw(context,false,{},{},"Export destination");ui::Checkbox("Only selected record",&export_selected_);ui::Checkbox("Require verified current records",&require_current_);
                if(button("Export kept raw measurements###Export saved raw measurements",GuiInputTarget::ExportRawMeasurements)) perform([&]{if(output_gate_available(gate,error_))export_records(doc,project,*gate);});
                if(csv_pending_ && button("Cancel raw CSV export",GuiInputTarget::CancelRawMeasurements)) csv_job_.cancel();
                record_rows_.refresh(&doc,doc.revision(),cursor_,[&](auto& rows){record_page_=doc.raw_analysis_page(cursor_,64);for(const auto& r:record_page_){auto row=ui::record_row(r->id,r->result->object_context().name,"Raw measurement",doc.raw_analysis_status(r->id));const auto region=selected_axes(*r->result);row.detail="Z "+std::to_string(region.z)+" / time "+std::to_string(region.t)+" / "+(r->result->grid()?"grid":"rectangle");rows.push_back(std::move(row));}},doc.server().get());
                const auto& page=record_page_;catalog_max_=std::max(catalog_max_,page.size());
                {ui::ActionRow pages(2);
                ui::Text("Kept measurements: %llu",static_cast<unsigned long long>(doc.raw_analysis_record_count()));
                pages.next();ImGui::BeginDisabled(previous_.empty());if(ui::IconButton("Previous records",ui::Icon::Left,ui::Availability{!previous_.empty(),"The first record page is displayed."})) {cursor_=previous_.back();previous_.pop_back();}ImGui::EndDisabled();
                pages.next();ImGui::BeginDisabled(page.size()<64);if(ui::IconButton("Next records",ui::Icon::Right,ui::Availability{page.size()>=64,"There are no further records on this page."})) {previous_.push_back(cursor_);cursor_=page.back()->id;}ImGui::EndDisabled();
                }
                ui::RecordListActions list_actions;list_actions.status=[&](std::uint64_t id){return doc.raw_analysis_status(id);};const auto previous_selected=selected_;selected_=ui::draw_record_list("##saved raw records",record_rows_,selected_,list_actions);if(previous_selected!=selected_)verified_=false;
                std::shared_ptr<const RawAnalysisRecord> selected;if(selected_) try {selected=doc.raw_analysis_record(selected_);}catch(const std::out_of_range&) {selected_=0;}
                if(button("Recompute and verify",GuiInputTarget::VerifyRawAnalysisRecord)) perform([&]{verify_record(doc);});
                if(button("Remove kept result###Remove saved measurement",GuiInputTarget::RemoveRawAnalysisRecord)) {if(removal)removal->request_record(doc,epoch,"Raw measurement",selected_,&Document::raw_analysis_record,&Document::remove_raw_analysis);}
                if(selected) {ui::DataTextWrapped("Status: %s",ui::caption(ui::parse_record_state(doc.raw_analysis_status(selected->id)),doc.raw_analysis_status(selected->id)).c_str());result_table(*selected->result,scale,catalog_max_);}
                ui::DataTextWrapped("CSV: %s",ui::caption(ui::parse_export_state(csv_stats_.state),csv_stats_.state).c_str());ImGui::EndTabItem();
            }
            ImGui::EndTabBar();activate_tab_=false;
        }
        if((future_.valid() || raw_pending_) && button("Cancel raw task",GuiInputTarget::CancelRawTask)) {cancel_.store(true);raw_job_.cancel();}
        ui::DataTextWrapped("Channels: %s / raw task: %s",ui::caption(ui::parse_run_state(state_),state_).c_str(),ui::caption(ui::parse_run_state(task_),task_).c_str());if(!error_.empty()) ui::DataTextWrapped("%s",ui::i18n::text(error_.c_str()));ImGui::End();return changed;
    }
};
}
