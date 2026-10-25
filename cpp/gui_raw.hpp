#pragma once
#include "qupathxx/raw_image.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "gui_layout.hpp"
#include "utf8_text.hpp"
#include <array>
#include <future>
#include <functional>
#include <chrono>

namespace qpx::detail {
class GuiRaw {
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
    std::shared_ptr<ImageServer> source_;std::weak_ptr<const char> identity_;std::weak_ptr<ImageServer> applied_;
    std::shared_ptr<const RawImageMetadata> metadata_;std::shared_ptr<const RawRegionStatistics> statistics_;
    std::shared_ptr<const RawAnalysisResult> analysis_;
    std::future<Result> future_;std::atomic<bool> cancel_{false};Kind running_=Kind::Metadata;
    RawAnalysisJob raw_job_;RawAnalysisStats raw_stats_;RawTableJob csv_job_;RawTableStats csv_stats_;
    std::string state_="waiting",task_="idle",error_;
    std::array<char,32769> path_{},csv_path_{};
    std::uint64_t selected_=0,kept_=0,verify_id_=0,cursor_=0;std::vector<std::uint64_t> previous_;
    std::size_t changes_=0,export_bytes_=0,catalog_max_=0;
    static std::size_t index(GuiInputTarget target) {return std::size_t(target)-std::size_t(first);}
    bool busy() const {return future_.valid() || raw_pending_ || csv_pending_;}
    void failure(const std::exception& error) {try {error_=error.what();bounded_utf8(error_,8192);}catch(...) {error_="Raw image operation failed";}}
    static std::vector<std::string> protected_paths(const Project* project) {
        std::vector<std::string> paths;if(project) {if(!project->path().empty()) paths.push_back(project->path());if(!project->recovery_source().empty()) paths.push_back(project->recovery_source());}return paths;
    }
    void refresh(Document& doc) {
        if(source_==doc.server() && identity_.lock()==doc.analysis_identity_.token) return;
        const bool different=identity_.lock()!=doc.analysis_identity_.token;reset_request_=different;
        if(future_.valid() && running_!=Kind::Image) cancel_.store(true);
        if(different && source_) {
            raw_job_.cancel();statistics_.reset();analysis_.reset();verified_=false;kept_=0;
            if(!initial_record_) selected_=0;cursor_=0;previous_.clear();if(!raw_pending_) task_="idle";
        }
        source_=doc.server();identity_=doc.analysis_identity_.token;metadata_.reset();
        state_=source_->reader_id()=="tiff"?"waiting":"unsupported";
        if(source_->reader_id()=="tiff") initial_record_=false;
        if(!raw_pending_ && !csv_pending_) error_.clear();
    }
    void metadata_job(Document& doc) {
        if(busy()) return;cancel_.store(false);state_="loading";running_=Kind::Metadata;
        const auto source=source_;const auto identity=doc.analysis_identity_.token;
        future_=std::async(std::launch::async,[this,source,identity] {
            Result result{Kind::Metadata};result.source=source;result.identity=identity;
            try {result.metadata=source->raw_metadata(&cancel_);export_checkpoint(&cancel_);if(!result.metadata || !source->tiff_view()) throw std::invalid_argument("image has no quantitative TIFF channels");}
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
                if(kind==Kind::View && previous && previous->cache_bytes==options.cache_bytes && previous->decode_bytes==options.decode_bytes) result.candidate=source->with_tiff_view(options.view,&cancel_);
                else result.candidate=open_tiff(source->source_locator(),options,&cancel_);
                export_checkpoint(&cancel_);result.metadata=result.candidate->raw_metadata();
            }catch(const std::exception& error) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=error.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TIFF view opening failed";}}
            return result;
        });
    }
    void data_job(Document& doc,ObjectSelection& selection,const Project* project,bool save) {
        if(!metadata_ || busy()) return;auto request=request_;std::uint64_t object=0;
        if(follow_display_) {const auto view=source_->tiff_view();request.z=view->z;request.t=view->t;}
        if(use_selected_ && selection.primary()) object=selection.primary();
        if(!save) {
            RawAnalysisRequest captured(doc,request,object,object?nucleus_:false);
            if(!raw_job_.request(captured)) throw std::runtime_error("raw analysis is already running");
            raw_pending_=true;raw_stats_.state="running";verify_id_=0;verified_=false;analysis_.reset();statistics_.reset();task_="reading";error_.clear();return;
        }
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
        if(!path_[0]) throw std::invalid_argument("enter a new NumPy file path");request.validate(*metadata_);
        const auto output=export_path(path_.data());auto paths=protected_paths(project);paths.push_back(source_->source_locator());
        cancel_.store(false);task_="reading";running_=Kind::Export;const auto source=source_;const auto identity=doc.analysis_identity_.token;
        future_=std::async(std::launch::async,[this,source,identity,request,output,paths] {
            Result result{Kind::Export};result.source=source;result.identity=identity;
            try {for(const auto& path:paths) guard_file_output(export_path(path),"",output);const auto raw=source->read_raw(request,&cancel_);
                for(const auto& path:paths) guard_file_output(export_path(path),"",output);save_raw_numpy(raw,output,false,&cancel_);result.bytes=std::size_t(std::filesystem::file_size(std::filesystem::u8path(output)));result.state="saved";
            }catch(const std::exception& error) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=error.what();bounded_utf8(result.error,8192);}catch(...) {result.error="Raw export failed";}}return result;
        });
    }
    void verify_record(Document& doc) {
        if(busy() || !selected_) return;const auto request=RawAnalysisRequest::replay(doc,selected_);
        if(!raw_job_.request(request)) throw std::runtime_error("raw analysis is already running");
        raw_pending_=true;raw_stats_.state="running";verify_id_=selected_;verified_=false;analysis_.reset();statistics_.reset();task_="reading";error_.clear();
    }
    void export_records(const Document& doc,const Project* project) {
        if(busy()) return;
        try {const std::vector<std::uint64_t> ids=export_selected_?std::vector<std::uint64_t>{selected_}:std::vector<std::uint64_t>{};
            RawTableRequest request(doc,ids,require_current_,128u*1024*1024,protected_paths(project));
            if(!csv_job_.request(request,csv_path_.data())) throw std::runtime_error("raw measurement export is already running");
            csv_pending_=true;csv_stats_.state="running";error_.clear();
        }catch(const std::exception& error) {csv_stats_.state="failed";failure(error);throw;}
    }
    void poll_jobs(Document& doc) {
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
        ImGui::TextWrapped("Captured Z %u / time %u / channels %u..%u",region.z,region.t,region.first_channel,region.first_channel+region.channels-1);
        if(result.object_id()) ImGui::TextWrapped("ROI: %s (#%llu)",context.name.c_str(),static_cast<unsigned long long>(result.object_id()));
        if(context.tma) ImGui::TextWrapped("Case: %s / core %s",context.tma->case_id.c_str(),context.tma->core_name.c_str());
        if(result.grid()) {
            const auto& grid=result.grid()->grid;
            ImGui::Text("Core grid: %lld x %lld",static_cast<long long>(grid.x().samples()),static_cast<long long>(grid.y().samples()));
            ImGui::Text("Source: %s / measured samples: float64",sample_name(result.metadata().sample_type).c_str());
            ImGui::Text("Eligible core samples: %llu",static_cast<unsigned long long>(stats.pixels));
            ImGui::Text("Fully supported samples: %llu",static_cast<unsigned long long>(stats.valid_pixels));
        } else ImGui::Text("Valid pixels: %llu",static_cast<unsigned long long>(stats.valid_pixels));
        if(ImGui::BeginTable("raw means",4,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,150.f*scale})) {
            ImGui::TableSetupColumn("Channel");ImGui::TableSetupColumn("Mean");ImGui::TableSetupColumn("Finite");ImGui::TableSetupColumn("Nonfinite");ImGui::TableHeadersRow();
            ImGuiListClipper clip;clip.Begin(int(stats.means.size()));std::size_t inspected=0;
            while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd && inspected<512;++i) {
                ++inspected;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(result.metadata().channel_names[i].c_str());ImGui::TableNextColumn();
                if(stats.means[i]) ImGui::Text("%.10g",*stats.means[i]);else ImGui::TextUnformatted("No finite samples");
                ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(stats.finite_samples[i]));ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(stats.nonfinite_samples[i]));
            }
            maximum=std::max(maximum,inspected);ImGui::EndTable();
        }
    }
public:
    struct Change {bool refresh=false;std::uint64_t image=0;};
    explicit GuiRaw(const GuiOptions& options):open_(options.raw_open),initial_options_(bool(options.tiff_options)),initial_request_(bool(options.raw_request)),initial_record_(options.raw_record_id!=0),
        nucleus_(options.analysis_nucleus),require_current_(options.raw_require_current),initial_tab_(options.raw_tab),options_(options.tiff_options?*options.tiff_options:TiffOpenOptions{}),request_(options.raw_request?*options.raw_request:RawRegionRequest{}),selected_(options.raw_record_id) {
        if(initial_tab_<0 || initial_tab_>3) throw std::invalid_argument("invalid quantitative channels tab");
        follow_display_=!initial_request_;options_.validate();bounded_utf8(options.raw_numpy_path,32768);bounded_utf8(options.raw_csv_path,32768);
        std::copy(options.raw_numpy_path.begin(),options.raw_numpy_path.end(),path_.begin());std::copy(options.raw_csv_path.begin(),options.raw_csv_path.end(),csv_path_.begin());
        if(options.image_reader!="auto" && options.image_reader!="ppm" && options.image_reader!="openslide" && options.image_reader!="tiff") throw std::invalid_argument("invalid image reader selection");
    }
    ~GuiRaw() {try {finish(nullptr,false);}catch(...) {}}
    void open() {open_=true;if(state_=="cancelled" || state_=="failed") state_="waiting";}
    void open_image(Project& project,const std::string& path,TiffOpenOptions options) {
        if(busy()) throw std::runtime_error("finish or cancel the current raw task before adding an image");
        options.validate();cancel_.store(false);running_=Kind::Image;state_="changing";const auto instance=project.instance_identity_;
        future_=std::async(std::launch::async,[this,path,options,instance] {
            Result result{Kind::Image};result.project_instance=instance;
            try {result.candidate=open_tiff(path,options,&cancel_);export_checkpoint(&cancel_);}
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
                applied_=result.candidate;changed.image=project->add_tiff_source(std::move(result.candidate));++changes_;state_="ready";error_.clear();
            }else if(result.kind==Kind::Metadata) {
                metadata_=std::move(result.metadata);const auto current_view=*source_->tiff_view();
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
    Change draw(Document& doc,ObjectSelection& selection,Project* project,bool blocked,const std::function<void(GuiInputTarget)>& mark) {
        enabled_.fill(false);refresh(doc);auto changed=poll(doc,project);refresh(doc);
        if(!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2,false)) {if(open_) open_=false;else open();}
        if(!open_) return changed;if(state_=="waiting" && !busy()) try {metadata_job(doc);}catch(const std::exception& error) {failure(error);state_="failed";}
        const auto* viewport=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(540.f*scale,viewport->WorkSize.x*.9f),std::min(750.f*scale,viewport->WorkSize.y*.9f)},ImGuiCond_Appearing);
        if(!ImGui::Begin("Quantitative channels",&open_)) {ImGui::End();return changed;}
        auto remember=[&](GuiInputTarget target,bool disabled) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(target)]=!disabled && ImGui::IsItemVisible();mark(target);};
        auto button=[&](const char* text,GuiInputTarget target,bool disabled) {ImGui::BeginDisabled(disabled);const auto pressed=ui::Button(text);remember(target,disabled);ImGui::EndDisabled();return pressed;};
        auto tab=[&](const char* title,int number,GuiInputTarget target) {const auto shown=ImGui::BeginTabItem(title,nullptr,activate_tab_ && number==initial_tab_?ImGuiTabItemFlags_SetSelected:0);remember(target,false);return shown;};
        auto perform=[&](auto&& action) {try {action();error_.clear();}catch(const std::exception& error) {failure(error);task_="failed";}};
        if(metadata_) ImGui::TextWrapped("%s / %u channels / %u Z planes / %u timepoints",sample_name(metadata_->sample_type).c_str(),metadata_->channels,metadata_->z_planes,metadata_->timepoints);
        else {ImGui::TextWrapped("Channels: %s",state_.c_str());if(state_=="unsupported") ImGui::TextWrapped("Open a TIFF/OME-TIFF with the quantitative TIFF reader.");else if((state_=="failed" || state_=="cancelled") && ui::Button("Retry channel metadata")) state_="waiting";}
        const bool disabled=blocked || busy();
        auto number=[&](const char* label,std::uint32_t& value,std::uint32_t maximum) {int n=static_cast<int>(value);if(ui::InputInt(label,&n)) value=std::uint32_t(std::clamp(n,0,int(maximum-1)));};
        if(ImGui::BeginTabBar("Quantitative views")) {
            if(tab("Display",0,GuiInputTarget::RawDisplayTab)) {
                if(metadata_) {
                    ImGui::BeginDisabled(disabled);number("Series",options_.view.series,metadata_->series_count);number("Z plane",options_.view.z,metadata_->z_planes);number("Timepoint",options_.view.t,metadata_->timepoints);
                    const char* colors[]={"Red","Green","Blue"};for(std::size_t i=0;i<3;++i) {ImGui::PushID(int(i));int channel=options_.view.channels[i];ui::InputInt(colors[i],&channel);options_.view.channels[i]=std::clamp(channel,-1,int(metadata_->channels)-1);ui::InputDouble("Raw lower",&options_.view.lower[i],0,0,"%.10g");ui::InputDouble("Raw upper",&options_.view.upper[i],0,0,"%.10g");ImGui::PopID();}ImGui::EndDisabled();
                    if(button("Apply display channels / Z / time",GuiInputTarget::ApplyTiffView,disabled || options_.view.series!=metadata_->series)) perform([&]{view_job(doc,false);});
                    if(button("Open series as a new image",GuiInputTarget::AddTiffSeries,disabled || !project)) perform([&]{view_job(doc,true);});
                    ImGui::TextWrapped("Display conversion leaves stored raw samples and saved raw measurements unchanged.");
                }else ImGui::TextWrapped("Channel metadata is not ready.");
                ImGui::EndTabItem();
            }
            if(tab("Data",1,GuiInputTarget::RawDataTab)) {
                if(metadata_) {
                    ImGui::BeginDisabled(disabled);ui::Checkbox("Use selected area ROI",&use_selected_);if(use_selected_) ui::Checkbox("Use selected nucleus",&nucleus_);
                    ui::InputInt64("Level-zero X",&request_.region.x);ui::InputInt64("Level-zero Y",&request_.region.y);ui::InputInt64("Output width",&request_.region.width);ui::InputInt64("Output height",&request_.region.height);
                    int level=request_.region.level;if(ui::InputInt("Pyramid level",&level)) request_.region.level=std::clamp(level,0,int(metadata_->levels.size())-1);
                    number("First channel",request_.first_channel,metadata_->channels);int count=int(request_.channels);if(ui::InputInt("Channel count",&count)) request_.channels=std::uint32_t(std::clamp(count,1,int(metadata_->channels-request_.first_channel)));
                    ui::Checkbox("Follow displayed Z / time",&follow_display_);if(follow_display_) {const auto view=source_->tiff_view();request_.z=view->z;request_.t=view->t;}
                    ImGui::BeginDisabled(follow_display_);number("Data Z",request_.z,metadata_->z_planes);number("Data time",request_.t,metadata_->timepoints);ImGui::EndDisabled();ImGui::EndDisabled();
                    if(button("Measure raw channels",GuiInputTarget::MeasureRawRegion,disabled)) perform([&]{data_job(doc,selection,project,false);});
                    ui::InputTextWithHint("##raw NumPy path","New NumPy file (.npy)",path_.data(),path_.size());
                    if(button("Export raw rectangle (.npy)",GuiInputTarget::ExportRawRegion,disabled || !path_[0])) perform([&]{data_job(doc,selection,project,true);});
                    ImGui::TextWrapped("NumPy export preserves the rectangle, including pixels outside a nonrectangular ROI. Existing files are kept.");
                }else ImGui::TextWrapped("Channel metadata is not ready.");
                ImGui::EndTabItem();
            }
            if(tab("Results",2,GuiInputTarget::RawResultsTab)) {
                if(button("Keep raw measurement",GuiInputTarget::KeepRawAnalysis,disabled || !analysis_ || raw_stats_.state!="ready")) perform([&]{selected_=kept_=raw_job_.keep_result(doc);});
                if(analysis_) result_table(*analysis_,scale,catalog_max_);else ImGui::TextWrapped("Measure a selected area or rectangle to inspect raw channel statistics.");
                if(raw_pending_) ImGui::Text("Tiles: %llu / %llu",static_cast<unsigned long long>(raw_stats_.completed_tiles),static_cast<unsigned long long>(raw_stats_.total_tiles));
                ImGui::EndTabItem();
            }
            if(tab("Records",3,GuiInputTarget::RawRecordsTab)) {
                ui::InputTextWithHint("##raw CSV path","New raw measurements CSV",csv_path_.data(),csv_path_.size());ui::Checkbox("Only selected record",&export_selected_);ui::Checkbox("Require verified current records",&require_current_);
                if(button("Export saved raw measurements",GuiInputTarget::ExportRawMeasurements,disabled || !csv_path_[0] || !doc.raw_analysis_record_count() || (export_selected_ && !selected_))) perform([&]{export_records(doc,project);});
                if(button("Cancel raw CSV export",GuiInputTarget::CancelRawMeasurements,!csv_pending_)) csv_job_.cancel();
                auto page=doc.raw_analysis_page(cursor_,64);catalog_max_=std::max(catalog_max_,page.size());
                ImGui::Text("Saved measurements: %llu",static_cast<unsigned long long>(doc.raw_analysis_record_count()));
                if(ImGui::BeginListBox("##saved raw records",{-1,110.f*scale})) {
                    for(const auto& record:page) {const auto axes=selected_axes(*record->result);const auto label="#"+std::to_string(record->id)+"  "+record->result->object_context().name+"  Z "+std::to_string(axes.z)+" / time "+std::to_string(axes.t)+(record->result->grid()?" / grid":"");
                        ImGui::PushID(std::to_string(record->id).c_str());if(ImGui::Selectable(label.c_str(),selected_==record->id)) {selected_=record->id;verified_=false;}ImGui::PopID();}
                    ImGui::EndListBox();
                }
                ImGui::BeginDisabled(previous_.empty());if(ui::Button("Previous records")) {cursor_=previous_.back();previous_.pop_back();}ImGui::EndDisabled();
                ImGui::SameLine();ImGui::BeginDisabled(page.size()<64);if(ui::Button("Next records")) {previous_.push_back(cursor_);cursor_=page.back()->id;}ImGui::EndDisabled();
                std::shared_ptr<const RawAnalysisRecord> selected;if(selected_) try {selected=doc.raw_analysis_record(selected_);}catch(const std::out_of_range&) {selected_=0;}
                if(button("Recompute and verify",GuiInputTarget::VerifyRawAnalysisRecord,disabled || !selected)) perform([&]{verify_record(doc);});
                if(button("Remove saved measurement",GuiInputTarget::RemoveRawAnalysisRecord,disabled || !selected)) perform([&]{doc.remove_raw_analysis(selected_);selected_=0;selected.reset();verified_=false;});
                if(selected) {ImGui::TextWrapped("Status: %s",doc.raw_analysis_status(selected->id).c_str());result_table(*selected->result,scale,catalog_max_);}
                ImGui::TextWrapped("CSV: %s",csv_stats_.state.c_str());ImGui::EndTabItem();
            }
            ImGui::EndTabBar();activate_tab_=false;
        }
        if(button("Cancel raw task",GuiInputTarget::CancelRawTask,!future_.valid() && !raw_pending_)) {cancel_.store(true);raw_job_.cancel();}
        ImGui::TextWrapped("Channels: %s / raw task: %s",state_.c_str(),task_.c_str());if(!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());ImGui::End();return changed;
    }
};
}
