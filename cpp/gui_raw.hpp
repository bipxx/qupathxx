#pragma once
#include "qupathxx/raw_image.hpp"
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
    enum class Kind {Metadata,View,Series,Statistics,Export,Image};
    struct Result {
        Kind kind;std::string state="succeeded",error;std::shared_ptr<ImageServer> source,candidate;
        std::shared_ptr<const char> identity;std::shared_ptr<const RawImageMetadata> metadata;
        std::shared_ptr<const RawRegionStatistics> statistics;RawRegionRequest request;
        std::uint64_t object=0,object_revision=0;std::size_t bytes=0;std::string project_instance;
    };
    static constexpr auto first=GuiInputTarget::ApplyTiffView,last=GuiInputTarget::RawResultsTab;
    std::array<Rect,8> targets_{};std::array<bool,8> enabled_{};
    bool open_=false,initial_options_=false,initial_request_=false,use_selected_=true,follow_display_=true,reset_request_=true,nucleus_=false;
    int initial_tab_=0;bool activate_tab_=true;
    TiffOpenOptions options_;RawRegionRequest request_,result_request_;
    std::shared_ptr<ImageServer> source_;std::weak_ptr<const char> identity_;std::weak_ptr<ImageServer> applied_;
    std::shared_ptr<const RawImageMetadata> metadata_;std::shared_ptr<const RawRegionStatistics> statistics_;
    std::future<Result> future_;std::atomic<bool> cancel_{false};Kind running_=Kind::Metadata;
    std::string state_="waiting",task_="idle",error_;std::array<char,32769> path_{};
    std::size_t changes_=0,export_bytes_=0,catalog_max_=0;
    static std::size_t index(GuiInputTarget t) {return std::size_t(t)-std::size_t(first);}
    void failure(const std::exception& e) {try {error_=e.what();bounded_utf8(error_,8192);}catch(...) {error_="Raw image operation failed";}}
    void refresh(Document& doc) {
        if(source_==doc.server() && identity_.lock()==doc.analysis_identity_.token) return;
        reset_request_=identity_.lock()!=doc.analysis_identity_.token;if(future_.valid() && running_!=Kind::Image) cancel_.store(true);source_=doc.server();identity_=doc.analysis_identity_.token;metadata_.reset();statistics_.reset();task_="idle";error_.clear();state_=source_->reader_id()=="tiff"?"waiting":"unsupported";
    }
    void metadata_job(Document& doc) {
        if(future_.valid()) return;cancel_.store(false);state_="loading";running_=Kind::Metadata;
        auto source=source_;auto identity=doc.analysis_identity_.token;
        future_=std::async(std::launch::async,[this,source,identity] {Result result{Kind::Metadata};result.source=source;result.identity=identity;
            try {result.metadata=source->raw_metadata(&cancel_);export_checkpoint(&cancel_);if(!result.metadata || !source->tiff_view()) throw std::invalid_argument("image has no quantitative TIFF channels");}
            catch(const std::exception& e) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="Channel metadata opening failed";}}return result;});
    }
    void view_job(Document& doc,bool series) {
        options_.validate();if(!metadata_ || future_.valid()) return;if(!series && options_.view.series!=metadata_->series) throw std::invalid_argument("open another series as a new image");
        cancel_.store(false);state_="changing";running_=series?Kind::Series:Kind::View;auto source=source_;const auto options=options_;const auto identity=doc.analysis_identity_.token;const auto kind=running_;applied_.reset();
        future_=std::async(std::launch::async,[this,source,options,identity,kind] {Result result{kind};result.source=source;result.identity=identity;
            try {const auto previous=source->tiff_options();if(kind==Kind::View && previous && previous->cache_bytes==options.cache_bytes && previous->decode_bytes==options.decode_bytes) result.candidate=source->with_tiff_view(options.view,&cancel_);else result.candidate=open_tiff(source->source_locator(),options,&cancel_);export_checkpoint(&cancel_);result.metadata=result.candidate->raw_metadata();}
            catch(const std::exception& e) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TIFF view opening failed";}}return result;});
    }
    void data_job(Document& doc,ObjectSelection& selection,const Project* project,bool save) {
        if(!metadata_ || future_.valid()) return;auto request=request_;std::optional<Roi> mask;std::uint64_t object=0,revision=0;
        if(follow_display_) {const auto view=source_->tiff_view();request.z=view->z;request.t=view->t;}
        if(use_selected_ && selection.primary()) {const auto selected=doc.annotation(selection.primary());if(!selected.geometry.is_area()) throw std::invalid_argument("select an area ROI or use the rectangle fields");
            if(nucleus_ && !selected.nucleus) throw std::invalid_argument("selected object has no nucleus");mask=nucleus_?*selected.nucleus:selected.geometry;object=selected.id;revision=selected.revision;const auto b=mask->bounds();if(request.region.level<0 || std::size_t(request.region.level)>=metadata_->levels.size()) throw std::out_of_range("invalid raw pyramid level");const auto d=metadata_->levels[request.region.level].downsample;
            const auto x=std::int64_t(std::floor(b.x/d))*std::int64_t(d),y=std::int64_t(std::floor(b.y/d))*std::int64_t(d);request.region=RegionRequest(x,y,request.region.level,std::int64_t(std::ceil((b.x+b.width-x)/d)),std::int64_t(std::ceil((b.y+b.height-y)/d)));
        }
        std::vector<std::string> protected_paths;protected_paths.push_back(source_->source_locator());
        if(project) {if(!project->path().empty()) protected_paths.push_back(project->path());if(!project->recovery_source().empty()) protected_paths.push_back(project->recovery_source());}
        const auto output=save?export_path(path_.data()):std::string{};if(save && !path_[0]) throw std::invalid_argument("enter a new NumPy file path");
        if(save) {request.validate(*metadata_);for(const auto& p:protected_paths) guard_file_output(export_path(p),"",output);}
        cancel_.store(false);task_="reading";running_=save?Kind::Export:Kind::Statistics;if(!save) statistics_.reset();auto source=source_;const auto identity=doc.analysis_identity_.token;const auto kind=running_;
        future_=std::async(std::launch::async,[this,source,identity,kind,request,mask,object,revision,output,protected_paths] {
            Result result{kind};result.source=source;result.identity=identity;result.request=request;result.object=object;result.object_revision=revision;
            try {export_checkpoint(&cancel_);
                if(kind==Kind::Statistics) result.statistics=std::make_shared<const RawRegionStatistics>(raw_region_statistics(*source,request,256,mask?&*mask:nullptr,1000000000,&cancel_));
                else {const auto raw=source->read_raw(request,&cancel_);for(const auto& p:protected_paths) guard_file_output(export_path(p),"",output);save_raw_numpy(raw,output,false,&cancel_);result.bytes=std::size_t(std::filesystem::file_size(std::filesystem::u8path(output)));result.state="saved";}
            }catch(const std::exception& e) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="Raw image task failed";}}return result;
        });
    }
public:
    struct Change {bool refresh=false;std::uint64_t image=0;};
    explicit GuiRaw(const GuiOptions& options):open_(options.raw_open),initial_options_(bool(options.tiff_options)),initial_request_(bool(options.raw_request)),initial_tab_(options.raw_tab),options_(options.tiff_options?*options.tiff_options:TiffOpenOptions{}),request_(options.raw_request?*options.raw_request:RawRegionRequest{}) {
        if(initial_tab_<0 || initial_tab_>2) throw std::invalid_argument("invalid quantitative channels tab");
        follow_display_=!initial_request_;
        nucleus_=options.analysis_nucleus;
        options_.validate();bounded_utf8(options.raw_numpy_path,32768);std::copy(options.raw_numpy_path.begin(),options.raw_numpy_path.end(),path_.begin());
        if(options.image_reader!="auto" && options.image_reader!="ppm" && options.image_reader!="openslide" && options.image_reader!="tiff") throw std::invalid_argument("invalid image reader selection");
    }
    ~GuiRaw() {try {finish(nullptr,false);}catch(...) {}}
    void open() {open_=true;if(state_=="cancelled" || state_=="failed") state_="waiting";}
    void open_image(Project& project,const std::string& path,TiffOpenOptions options) {
        if(future_.valid()) throw std::runtime_error("finish or cancel the current raw task before adding an image");options.validate();cancel_.store(false);running_=Kind::Image;state_="changing";const auto instance=project.instance_identity_;
        future_=std::async(std::launch::async,[this,path,options,instance] {Result result{Kind::Image};result.project_instance=instance;
            try {result.candidate=open_tiff(path,options,&cancel_);export_checkpoint(&cancel_);}
            catch(const std::exception& e) {result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TIFF image opening failed";}}return result;});
    }
    Rect bounds(GuiInputTarget t) const {return targets_.at(index(t));}bool enabled(GuiInputTarget t) const {return enabled_.at(index(t));}
    bool ready(const Document& doc) const {return metadata_ && source_==doc.server() && identity_.lock()==doc.analysis_identity_.token && state_=="ready";}
    bool applied(const Document& doc) const {return applied_.lock()==doc.server();}
    bool finished() const {return task_!="idle" && task_!="reading";}
    const std::string& state() const {return state_;}const std::string& task_state() const {return task_;}const std::string& error() const {return error_;}
    const auto& statistics() const {return statistics_;}std::size_t changes() const {return changes_;}std::size_t export_bytes() const {return export_bytes_;}std::size_t catalog_max_step() const {return catalog_max_;}
    Change poll(Document& doc,Project* project,bool allow_apply=true) {
        Change changed;if(!future_.valid() || future_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return changed;
        Result result{running_};try {result=future_.get();}catch(const std::exception& e) {failure(e);state_="failed";task_="failed";return changed;}
        const bool current=result.kind==Kind::Image?project && result.project_instance==project->instance_identity_:result.source==doc.server() && result.identity==doc.analysis_identity_.token;
        if(result.kind==Kind::Export && result.state=="saved") {task_="saved";export_bytes_=result.bytes;error_.clear();return changed;}
        if(!current) return changed;
        if(cancel_.load() || result.state!="succeeded") {if(result.kind==Kind::Metadata || result.kind==Kind::View || result.kind==Kind::Series || result.kind==Kind::Image) state_=cancel_.load()?"cancelled":result.state;else task_=cancel_.load()?"cancelled":result.state;error_=result.error;return changed;}
        try {
            if(result.kind==Kind::Image) {
                if(!allow_apply || !project) {state_="cancelled";return changed;}applied_=result.candidate;changed.image=project->add_tiff_source(std::move(result.candidate));++changes_;state_="ready";error_.clear();
            }else if(result.kind==Kind::Metadata) {
                metadata_=std::move(result.metadata);const auto current_view=*source_->tiff_view();
                if(!initial_options_) {if(auto current=source_->tiff_options()) options_=*current;options_.view=current_view;}
                else {for(std::size_t i=0;i<3;++i) {if(options_.view.channels[i]==-2) options_.view.channels[i]=current_view.channels[i];if(std::isnan(options_.view.lower[i])) {options_.view.lower[i]=current_view.lower[i];options_.view.upper[i]=current_view.upper[i];}}initial_options_=false;}
                if(reset_request_ && !initial_request_) {request_={};request_.region=RegionRequest(0,0,0,std::min<std::int64_t>(512,metadata_->width),std::min<std::int64_t>(512,metadata_->height));request_.z=current_view.z;request_.t=current_view.t;}initial_request_=false;reset_request_=false;
                state_="ready";error_.clear();
            } else if(result.kind==Kind::View || result.kind==Kind::Series) {
                if(!allow_apply) {state_="cancelled";return changed;}
                if(result.kind==Kind::Series) {if(!project) throw std::invalid_argument("a project is required to add TIFF series");applied_=result.candidate;changed.image=project->add_tiff_source(std::move(result.candidate));}
                else {const auto old=doc.server();doc.adopt_tiff_source(std::move(result.candidate));applied_=doc.server();changed.refresh=old!=doc.server();}
                ++changes_;state_="ready";error_.clear();
            } else if(result.kind==Kind::Statistics) {
                if(result.object && doc.annotation(result.object).revision!=result.object_revision) throw std::runtime_error("selected ROI changed before raw statistics completed");statistics_=std::move(result.statistics);result_request_=result.request;task_="succeeded";error_.clear();
            }
        }catch(const std::exception& e) {failure(e);if(result.kind==Kind::Statistics) task_="stale";else state_="failed";}
        return changed;
    }
    void finish(Document* doc,bool finite) {
        if(!future_.valid()) return;const bool data=running_==Kind::Statistics || running_==Kind::Export;if(!finite || !data) cancel_.store(true);future_.wait();if(doc) poll(*doc,nullptr,false);else future_.get();
    }
    Change draw(Document& doc,ObjectSelection& selection,Project* project,bool blocked,const std::function<void(GuiInputTarget)>& mark) {
        enabled_.fill(false);refresh(doc);auto changed=poll(doc,project);refresh(doc);
        if(!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2,false)) {if(open_) open_=false;else open();}
        if(!open_) return changed;if(state_=="waiting" && !future_.valid()) try {metadata_job(doc);}catch(const std::exception& e) {failure(e);state_="failed";}
        const auto* viewport=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(540.f*scale,viewport->WorkSize.x*.9f),std::min(750.f*scale,viewport->WorkSize.y*.9f)},ImGuiCond_Appearing);
        if(!ImGui::Begin("Quantitative channels",&open_)) {ImGui::End();return changed;}
        auto remember=[&](GuiInputTarget target,bool disabled) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(target)]=!disabled && ImGui::IsItemVisible();mark(target);};
        auto button=[&](const char* text,GuiInputTarget target,bool disabled) {ImGui::BeginDisabled(disabled);const auto pressed=ui::Button(text);remember(target,disabled);ImGui::EndDisabled();return pressed;};
        auto tab=[&](const char* title,int number,GuiInputTarget target) {const auto shown=ImGui::BeginTabItem(title,nullptr,activate_tab_ && number==initial_tab_?ImGuiTabItemFlags_SetSelected:0);remember(target,false);return shown;};
        auto perform=[&](auto&& action) {try {action();error_.clear();}catch(const std::exception& e) {failure(e);task_="failed";}};
        if(!metadata_) {ImGui::TextWrapped("Channels: %s",state_.c_str());if(state_=="unsupported") ImGui::TextWrapped("Open a TIFF/OME-TIFF with the quantitative TIFF reader.");else if((state_=="failed" || state_=="cancelled") && ui::Button("Retry channel metadata")) state_="waiting";}
        else {
            ImGui::TextWrapped("%s / %u channels / %u Z planes / %u timepoints",sample_name(metadata_->sample_type).c_str(),metadata_->channels,metadata_->z_planes,metadata_->timepoints);
            const bool disabled=blocked || future_.valid();
            auto number=[&](const char* label,std::uint32_t& value,std::uint32_t maximum) {int n=static_cast<int>(value);if(ui::InputInt(label,&n)) value=std::uint32_t(std::clamp(n,0,int(maximum-1)));};
            if(ImGui::BeginTabBar("Quantitative views")) {
                if(tab("Display",0,GuiInputTarget::RawDisplayTab)) {
                    ImGui::BeginDisabled(disabled);number("Series",options_.view.series,metadata_->series_count);number("Z plane",options_.view.z,metadata_->z_planes);number("Timepoint",options_.view.t,metadata_->timepoints);
                    const char* colors[]={"Red","Green","Blue"};for(std::size_t i=0;i<3;++i) {ImGui::PushID(int(i));int channel=options_.view.channels[i];ui::InputInt(colors[i],&channel);options_.view.channels[i]=std::clamp(channel,-1,int(metadata_->channels)-1);ui::InputDouble("Raw lower",&options_.view.lower[i],0,0,"%.10g");ui::InputDouble("Raw upper",&options_.view.upper[i],0,0,"%.10g");ImGui::PopID();}ImGui::EndDisabled();
                    if(button("Apply display channels / Z / time",GuiInputTarget::ApplyTiffView,disabled || options_.view.series!=metadata_->series)) perform([&]{view_job(doc,false);});
                    if(button("Open series as a new image",GuiInputTarget::AddTiffSeries,disabled || !project)) perform([&]{view_job(doc,true);});
                    ImGui::TextWrapped("Display conversion leaves raw samples unchanged. Raw measurement and export use the Data tab selection.");ImGui::EndTabItem();
                }
                if(tab("Data",1,GuiInputTarget::RawDataTab)) {
                    ImGui::BeginDisabled(disabled);ui::Checkbox("Use selected area ROI",&use_selected_);if(use_selected_) ui::Checkbox("Use selected nucleus",&nucleus_);ui::InputInt64("Level-zero X",&request_.region.x);ui::InputInt64("Level-zero Y",&request_.region.y);ui::InputInt64("Output width",&request_.region.width);ui::InputInt64("Output height",&request_.region.height);
                    int level=request_.region.level;if(ui::InputInt("Pyramid level",&level)) request_.region.level=std::clamp(level,0,int(metadata_->levels.size())-1);
                    number("First channel",request_.first_channel,metadata_->channels);int count=int(request_.channels);if(ui::InputInt("Channel count",&count)) request_.channels=std::uint32_t(std::clamp(count,1,int(metadata_->channels-request_.first_channel)));
                    ui::Checkbox("Follow displayed Z / time",&follow_display_);if(follow_display_) {const auto view=source_->tiff_view();request_.z=view->z;request_.t=view->t;}
                    ImGui::BeginDisabled(follow_display_);number("Data Z",request_.z,metadata_->z_planes);number("Data time",request_.t,metadata_->timepoints);ImGui::EndDisabled();ImGui::EndDisabled();
                    if(button("Measure raw channels",GuiInputTarget::MeasureRawRegion,disabled)) perform([&]{data_job(doc,selection,project,false);});
                    ui::InputTextWithHint("##raw NumPy path","New NumPy file (.npy)",path_.data(),path_.size());
                    if(button("Export raw rectangle (.npy)",GuiInputTarget::ExportRawRegion,disabled || !path_[0])) perform([&]{data_job(doc,selection,project,true);});
                    ImGui::TextWrapped("Export preserves the rectangle, including pixels outside a selected nonrectangular ROI. Existing files are kept.");ImGui::EndTabItem();
                }
                if(tab("Results",2,GuiInputTarget::RawResultsTab)) {
                    if(statistics_) {ImGui::TextWrapped("Captured Z %u / time %u / channels %u..%u",result_request_.z,result_request_.t,result_request_.first_channel,result_request_.first_channel+result_request_.channels-1);ImGui::Text("Valid pixels: %llu",static_cast<unsigned long long>(statistics_->valid_pixels));
                        if(ImGui::BeginTable("raw means",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,180.f*scale})) {ImGui::TableSetupColumn("Channel");ImGui::TableSetupColumn("Mean");ImGui::TableSetupColumn("Finite samples");ImGui::TableHeadersRow();ImGuiListClipper clip;clip.Begin(int(statistics_->means.size()));std::size_t inspected=0;
                            while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd && inspected<512;++i) {++inspected;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(metadata_->channel_names[result_request_.first_channel+std::uint32_t(i)].c_str());ImGui::TableNextColumn();if(statistics_->means[i]) ImGui::Text("%.10g",*statistics_->means[i]);else ImGui::TextUnformatted("No finite samples");ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(statistics_->finite_samples[i]));}catalog_max_=std::max(catalog_max_,inspected);ImGui::EndTable();}
                    }else ImGui::TextWrapped("Measure a selected area or rectangle to inspect raw channel statistics.");ImGui::EndTabItem();
                }
                ImGui::EndTabBar();activate_tab_=false;
            }
        }
        if(button("Cancel raw task",GuiInputTarget::CancelRawTask,!future_.valid())) cancel_.store(true);
        ImGui::TextWrapped("Channels: %s / raw task: %s",state_.c_str(),task_.c_str());if(!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());ImGui::End();return changed;
    }
};
}
