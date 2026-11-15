#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_record_list.hpp"
#include "gui_result_state.hpp"
#include "gui_path_field.hpp"
#include "gui_output_gate.hpp"
#include "gui_class_palette.hpp"
#include <set>
#include "gui_record_removal.hpp"
#include "gui_workflow.hpp"
#include "gui_pixel_preview.hpp"
#include "gui_pixel_archives.hpp"
#include "gui_pixel_stream_browser.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "qupathxx/pixel_contours.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include "utf8_text.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <future>
#include <functional>
#include <iomanip>
#include <locale>
#include <sstream>
namespace qpx::detail {
class GuiPixelClassifier {
    ui::PathField path_{{"Pixel model file",ui::PathRole::OpenFile,{".qpc"},"",""}};
    ui::PathField save_path_{{"Export pixel model file",ui::PathRole::NewFile,{".qpc"},"",""}};
    ui::PathField stream_path_{{"Pixel stream file",ui::PathRole::NewFile,{".qps"},"",""}};
    ui::RecordRows record_rows_;
    friend struct OutputWriterTestAccess;
    friend struct GuiPixelClassifierTestAccess;
    friend struct ModelSafetyTestAccess;
    inline static constexpr auto input_targets=std::array{
        GuiInputTarget::TrainPixelClassifier,
        GuiInputTarget::PredictPixelClasses,
        GuiInputTarget::CancelPixelClassifier,
        GuiInputTarget::SavePixelModel,
        GuiInputTarget::LoadPixelModel,
        GuiInputTarget::TogglePixelOverlay,
        GuiInputTarget::TracePixelContours,
        GuiInputTarget::PublishPixelContours,
        GuiInputTarget::KeepPixelTraining,
        GuiInputTarget::KeepPixelPrediction,
        GuiInputTarget::UsePixelRecordModel,
        GuiInputTarget::PreviewPixelRecord,
        GuiInputTarget::RemovePixelRecord,
        GuiInputTarget::ExportPixelStream,
        GuiInputTarget::OpenPixelStreamBrowser};
    GuiPixelArchives archives_;GuiPixelStreamBrowser browser_;
    PixelStreamExportJob stream_;PixelStreamExportStats stream_stats_;PixelStreamOptions stream_options_;
    bool stream_replace_=false;std::uint64_t stream_max_bytes_=1ULL<<40;
    std::optional<PixelTrainingRequest> training_request_;
    std::shared_ptr<const PixelPredictionResult> historical_;
    std::uint64_t historical_record_=0,kept_prediction_=0;bool record_loaded_=false,keeping_prediction_=false;
    std::shared_ptr<const PixelPredictionResult> preview_source() const {return historical_?historical_:prediction_.result();}
    PixelContourJob contours_;TissueContourOptions contour_options_;PixelTrainingStats contour_stats_;
    std::optional<PixelPredictionRequest> prediction_request_;
    std::shared_ptr<const PixelContours> published_contours_;
    std::set<std::uint64_t> session_published_ids_;
    std::vector<std::uint64_t> published_ids_;bool published_=false;
    void stop_busy_work() {if(stream_.busy())stream_.cancel();if(archives_.busy())archives_.cancel();if(training_.busy())cancel_training();if(prediction_.busy())cancel_prediction();if(contours_.busy())contours_.cancel();if(io_.valid())io_cancelled_.store(true);}
    void clear_contours() {contours_.cancel();published_=false;published_contours_.reset();published_ids_.clear();}
    void cancel_prediction() {prediction_.cancel();prediction_request_.reset();historical_.reset();historical_record_=0;clear_contours();}
    PixelTrainingOptions options_;PixelPredictionOptions prediction_options_;ForestParameters parameters_;
    PixelTrainingJob training_;PixelPredictionJob prediction_;PixelTrainingStats training_stats_,prediction_stats_;
    std::shared_ptr<const PixelClassifier> model_;GuiPixelPreview preview_;
    bool model_unsaved_=false,model_saved_=true;
    std::set<std::string> saved_models_;
    std::uint64_t model_record_=0,retention_revision_=std::numeric_limits<std::uint64_t>::max();
    std::weak_ptr<const char> retention_identity_,holder_identity_;
    void refresh_model_retention(const Document& doc) {
        if(!model_) {model_unsaved_=false;return;}
        model_saved_=saved_models_.count(model_->sha256())!=0;
        // A holder in another open document stays a session fact. Never keep
        // that document alive, and never query its ID in a different document.
        if(retention_identity_.lock()!=doc.analysis_identity_.token || retention_revision_!=doc.revision() || (model_record_ && holder_identity_.expired())) {
            retention_identity_=doc.analysis_identity_.token;retention_revision_=doc.revision();
            if(holder_identity_.expired())model_record_=0;
            if(model_record_ && holder_identity_.lock()==doc.analysis_identity_.token)
                try {if(doc.pixel_classifier_record(model_record_)->archive->model()->sha256()!=model_->sha256())model_record_=0;}catch(const std::out_of_range&) {model_record_=0;}
            if(!model_saved_ && !model_record_)for(const auto& record:doc.pixel_classifier_records())
                if(record->archive->model()->sha256()==model_->sha256()) {model_record_=record->id;holder_identity_=doc.analysis_identity_.token;break;}
        }
        model_unsaved_=!model_saved_ && !model_record_;
    }
    std::vector<std::uint32_t> historical_palette_;
    const std::vector<std::uint32_t>& preview_palette() const {return historical_?historical_palette_:palette_;}
    bool open_=false,selected_only_=false,replace_=false,show_=true,viewport_only_=false,trained_=false,preview_reset_=false;
    std::optional<GuiInputTarget> reveal_target_;
    std::uint64_t target_=0,adopted_=0;std::size_t preview_limit_=0;double capture_ms_=0;float opacity_=.45f;
    std::vector<std::array<float,3>> colors_;std::vector<std::uint32_t> palette_;
    std::array<char,256> scales_{};std::array<Rect,input_targets.size()> targets_{};
    std::string error_,io_state_="idle",io_error_;
    struct IoResult {std::shared_ptr<const PixelClassifier> model;std::string error;bool save=false;};
    std::future<IoResult> io_;std::atomic<bool> io_cancelled_{false};
    std::shared_ptr<const char> catalog_identity_;std::uint64_t catalog_revision_=0;
    std::map<std::uint64_t,Annotation>::const_iterator catalog_cursor_;
    std::vector<std::uint64_t> labelled_,areas_;bool catalog_complete_=false;
    std::size_t catalog_inspected_=0,catalog_max_step_=0;
    std::vector<std::uint64_t> captured_selection_;
    static constexpr std::size_t index(GuiInputTarget t) {
        for(std::size_t i=0;i<input_targets.size();++i) if(input_targets[i]==t) return i;
        return input_targets.size();
    }
    void scales_text() {std::ostringstream text;text.imbue(std::locale::classic());text<<std::setprecision(17);for(auto v:options_.features.sigmas) {if(text.tellp()>0) text<<", ";text<<v;}const auto s=text.str();scales_.fill(0);std::memcpy(scales_.data(),s.data(),std::min(s.size(),scales_.size()-1));}
    void parse_scales() {std::istringstream all(scales_.data());std::string field;std::vector<double> values;while(std::getline(all,field,',')) {std::istringstream part(field);part.imbue(std::locale::classic());double value;if(!(part>>value)) throw std::invalid_argument("Enter comma-separated feature scales");part>>std::ws;if(!part.eof()) throw std::invalid_argument("Invalid feature scale");values.push_back(value);}if(!all.eof() || (!std::string(scales_.data()).empty() && std::string(scales_.data()).back()==',')) throw std::invalid_argument("Invalid feature scale list");auto f=options_.features;f.sigmas=std::move(values);f.validate();options_.features=std::move(f);}
    void make_palette() {if(!std::isfinite(opacity_)) opacity_=.45f;opacity_=std::clamp(opacity_,0.f,1.f);palette_.clear();for(const auto& c:colors_) {std::uint32_t rgb=0;for(unsigned i=0;i<3;++i) rgb|=std::uint32_t(std::lround((std::isfinite(c[i])?std::clamp(c[i],0.f,1.f):0.f)*255))<<(8*i);palette_.push_back(rgb|(std::uint32_t(std::lround(opacity_*255))<<24));}for(auto& color:historical_palette_)color=(color&0xffffffu)|(std::uint32_t(std::lround(opacity_*255))<<24);}
    Rect opacity_bounds_;
    bool draw_opacity() {const bool changed=ui::SliderFloat("Opacity",&opacity_,0,1);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();opacity_bounds_={a.x,a.y,b.x-a.x,b.y-a.y};if(changed)make_palette();return changed;}
    void model_colors(const Document& doc) {
        colors_.clear();
        for(const auto color:class_palette(doc,model_->classes())) colors_.push_back({float(color&255)/255,float((color>>8)&255)/255,float((color>>16)&255)/255});make_palette();
    }
    void adopt(std::shared_ptr<const PixelClassifier> model,const Document& doc,bool saved=true,std::uint64_t record=0) {
        cancel_prediction();model_=std::move(model);if(saved)saved_models_.insert(model_->sha256());model_saved_=saved_models_.count(model_->sha256())!=0;model_record_=record;holder_identity_=record?std::weak_ptr<const char>(doc.analysis_identity_.token):std::weak_ptr<const char>{};retention_identity_.reset();refresh_model_retention(doc);options_.features=model_->options();scales_text();model_colors(doc);
        try {options_.level=pixel_classifier_level(doc,*model_);}catch(const std::exception&) {}error_.clear();
    }
    void cancel_training() {training_.cancel();training_request_.reset();captured_selection_.clear();trained_=false;}
    void train(const Document& doc,const ObjectSelection& selection,const StainTransform& transform) {
        try {parse_scales();options_.validate();parameters_.validate();std::vector<std::uint64_t> ids;
            if(selected_only_) {if(selection.size()>4096) throw std::length_error("Select at most 4096 training areas");for(auto id:selection.ids()) {const auto a=doc.annotation(id);if(a.kind==ObjectKind::Annotation && a.geometry.is_area() && !a.class_name.empty()) ids.push_back(id);}}
            else {if(!catalog_complete_) throw std::logic_error("Wait for the annotation list to finish");ids=labelled_;}
            if(ids.size()>4096) throw std::length_error("Too many labelled annotations; select up to 4096 training areas");
            PixelTrainingRequest request(doc,ids,transform,options_);capture_ms_=request.capture_ms();if(!training_.train(request,parameters_)) return;training_request_=request;cancel_prediction();trained_=false;captured_selection_=selected_only_?selection.ids():std::vector<std::uint64_t>{};error_.clear();
        }catch(const std::exception& e) {error_=e.what();}
    }
    void predict(const Document& doc,const ObjectSelection& selection,Rect visible) {
        try {if(!model_) throw std::logic_error("Train or load a pixel model first");const auto id=target_?target_:selection.primary();if(!id) throw std::invalid_argument("Select an area for prediction");const auto start=std::chrono::steady_clock::now();PixelPredictionRequest request(doc,id,*model_,prediction_options_,viewport_only_?std::optional<Rect>(visible):std::nullopt);capture_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(prediction_.request(request)) {kept_prediction_=0;historical_.reset();historical_record_=0;clear_contours();prediction_request_=request;error_.clear();}}catch(const std::exception& e) {error_=e.what();}
    }
    void export_stream(const Document& doc,const ObjectSelection& selection,Rect visible,OutputGate& gate) {
        try {if(!model_)throw std::logic_error("Train or load a pixel model first");
            const auto id=target_?target_:selection.primary();if(!id)throw std::invalid_argument("Select an area for export");
            auto options=stream_options_;options.prediction.min_vote_fraction=prediction_options_.min_vote_fraction;
            gate.submit(stream_path_.state().resolved,output_scope("Pixel stream",false,ui::command(ui::CommandId::Tool_PixelClassifier).caption),
                [this,&gate,document=&doc,id,model=model_,options,visible,viewport=viewport_only_,limit=stream_max_bytes_](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>&) {
                    if(!stream_.request(*document,id,*model,path,options,viewport?std::optional<Rect>(visible):std::nullopt,policy==ExportWritePolicy::ReplaceExisting,limit))throw std::runtime_error("Wait for the current export to finish.");
                    stream_stats_=stream_.poll(*document);error_.clear();
                    gate.watch(path,[this]()->std::optional<bool>{if(stream_.busy())return {};return stream_stats_.state=="saved";},[this]{return stream_stats_.state=="failed"?stream_stats_.error:std::string{};});
                },[this](const std::string& message){error_=message;},stream_replace_);
        }catch(const std::exception& e){error_=e.what();}
    }
    void trace(const Document& doc) {
        try {
            if(historical_)return;
            if(!prediction_request_ || !prediction_.result() || !prediction_request_->matches(doc)) throw std::logic_error("Predict an area before tracing contours");
            if(contours_.request(*prediction_request_,prediction_.result(),contour_options_)) {published_=false;published_contours_.reset();published_ids_.clear();error_.clear();}
        }catch(const std::exception& e) {error_=e.what();}
    }
    void publish(Document& doc) {
        try {
            if(historical_)return;
            if(published_ || !prediction_request_ || contours_.poll(doc).state!="ready" || !contours_.result()) throw std::logic_error(ui::i18n::text("Trace current pixel contours before adding."));
            auto traced=contours_.result();auto ids=publish_pixel_contours(doc,*prediction_request_,*traced);
            contours_.cancel();published_contours_=std::move(traced);published_ids_=std::move(ids);session_published_ids_.insert(published_ids_.begin(),published_ids_.end());published_=true;error_.clear();
        }catch(const std::exception& e) {error_=e.what();}
    }
    void file(bool save,std::string output={},bool replace=false) {
        if(io_.valid()) return;if(save && !model_) {io_error_="Train or load a pixel model first";return;}const std::string path=output.empty()?(save?save_path_.state().resolved:path_.state().resolved):std::move(output);auto model=model_;io_cancelled_.store(false);io_state_=save?"saving":"loading";io_error_.clear();
        try {io_=std::async(std::launch::async,[this,path,model=std::move(model),save,replace] {IoResult out;out.save=save;try {if(save) {save_pixel_classifier(*model,path,replace,&io_cancelled_);out.model=model;}else {auto loaded=load_pixel_classifier(path,&io_cancelled_);if(!loaded.resolution()) throw std::invalid_argument("Pixel preview requires a resolution-bound model; this file is an unbound buffer recipe");out.model=std::make_shared<const PixelClassifier>(std::move(loaded));}}catch(const std::exception& e) {out.error=e.what();}return out;});}catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}
    }
public:
    std::vector<ui::PathField*> paths(){auto fields=browser_.paths();for(auto* field:std::vector<ui::PathField*>{&path_,&save_path_,&stream_path_})fields.push_back(field);return fields;}
    explicit GuiPixelClassifier(const GuiOptions& o):browser_(o),contour_options_(o.pixel_contour_options?*o.pixel_contour_options:TissueContourOptions{}),options_(o.pixel_training_options?*o.pixel_training_options:PixelTrainingOptions{}),prediction_options_(o.pixel_prediction_options?*o.pixel_prediction_options:PixelPredictionOptions{}),parameters_(o.pixel_parameters?*o.pixel_parameters:pixel_forest_parameters()),open_(o.pixel_open),selected_only_(o.pixel_selected_only),replace_(o.pixel_replace_model),show_(o.pixel_show_overlay),viewport_only_(o.pixel_viewport_only),target_(o.pixel_object_id),preview_limit_(o.pixel_preview_budget) {
        stream_options_=o.pixel_stream_options?*o.pixel_stream_options:PixelStreamOptions{};if(!o.pixel_stream_options) stream_options_.prediction=prediction_options_;stream_options_.validate();stream_replace_=o.pixel_stream_replace;stream_max_bytes_=o.pixel_stream_max_bytes;if(stream_max_bytes_<128 || stream_max_bytes_>(1ULL<<46)) throw std::invalid_argument("invalid pixel stream file byte limit");bounded_utf8(o.pixel_stream_path,4095);stream_path_.set_option(o.pixel_stream_path);
        contour_options_.validate();options_.validate();prediction_options_.validate();parameters_.validate();if(preview_limit_>128u*1024*1024) throw std::invalid_argument("pixel preview budget exceeds 128 MiB");bounded_utf8(o.pixel_model_path,4095);path_.set_option(o.pixel_model_path);save_path_.set_option(o.pixel_model_path);scales_text();if(o.pixel_model) model_=std::make_shared<const PixelClassifier>(*o.pixel_model);
    }
    ~GuiPixelClassifier() {contours_.cancel();io_cancelled_.store(true);if(io_.valid()) io_.wait();}
    void open() {open_=true;}
    void open_saved_streams() {browser_.open();}
    void reveal_target(GuiInputTarget target) {if(open_ && handles(target))reveal_target_=target;}
    void invalidate_training() {if(training_.busy())cancel_training();}
    PixelTrainingStats input_training_status(const Document& doc) {return training_.poll(doc);}
    void update(Document& doc,const ObjectSelection& selection) {
        browser_.update();browser_.catalog(doc,doc.streams_,doc.analysis_identity_.token);
        if(preview_reset_) {preview_.clear();preview_reset_=false;}
        const bool changed_document=catalog_identity_ && catalog_identity_!=doc.analysis_identity_.token;
        if(changed_document) {stream_.cancel();cancel_training();cancel_prediction();target_=0;record_loaded_=false;kept_prediction_=0;keeping_prediction_=false;}
        archives_.update(doc,doc.pixels_,doc.analysis_identity_.token,open_);
        if(keeping_prediction_ && !archives_.busy()) {if(archives_.state()=="kept")kept_prediction_=archives_.kept();keeping_prediction_=false;}
        if(historical_record_ && !doc.pixels_.count(historical_record_)) {historical_.reset();historical_record_=0;}
        if(catalog_identity_!=doc.analysis_identity_.token || catalog_revision_!=doc.revision()) {if(catalog_identity_!=doc.analysis_identity_.token)session_published_ids_.clear();if(target_ && !doc.objects().count(target_)) target_=0;catalog_identity_=doc.analysis_identity_.token;catalog_revision_=doc.revision();catalog_cursor_=doc.objects().cbegin();catalog_complete_=false;catalog_inspected_=0;labelled_.clear();areas_.clear();}
        std::size_t step=0;while(open_ && !catalog_complete_ && step<1024) {if(catalog_cursor_==doc.objects().cend()) {catalog_complete_=true;break;}const auto& [id,a]=*catalog_cursor_++;if(a.kind==ObjectKind::Annotation && a.geometry.is_area()) {if(areas_.size()<10000) areas_.push_back(id);if(!a.class_name.empty() && labelled_.size()<4097) labelled_.push_back(id);}++step;++catalog_inspected_;}catalog_max_step_=std::max(catalog_max_step_,step);
        if(selected_only_ && !captured_selection_.empty() && (selection.size()!=captured_selection_.size() || selection.ids()!=captured_selection_)) cancel_training();
        if(io_.valid() && io_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto out=io_.get();if(io_cancelled_.load()) io_state_="cancelled";else if(!out.error.empty()) {io_state_="failed";io_error_=std::move(out.error);}else if(out.save) {io_state_="saved";saved_models_.insert(out.model->sha256());if(out.model==model_)model_saved_=true;}else {cancel_training();adopt(std::move(out.model),doc);io_state_="loaded";}}catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}}
        stream_stats_=stream_.poll(doc);
        training_stats_=training_.poll(doc);prediction_stats_=prediction_.poll(doc);contour_stats_=contours_.poll(doc);
        if(prediction_request_ && !prediction_request_->matches(doc)) prediction_request_.reset();
        if(training_request_ && !training_request_->matches(doc)) training_request_.reset();
        if(training_stats_.state=="ready" && training_.model() && adopted_!=training_stats_.generation) {adopt(training_.model(),doc,false);adopted_=training_stats_.generation;trained_=true;}
        if(model_ && colors_.size()!=model_->classes().size()) {auto m=model_;adopt(std::move(m),doc);}
        refresh_model_retention(doc);
        preview_.update(preview_source(),preview_palette(),false,preview_limit_);
    }
    void update_preview() {preview_.update(preview_source(),preview_palette(),show_,preview_limit_);}
    void draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {if(show_) preview_.draw(draw,view,offset,size);}
    bool trained() const {return trained_ && !training_.busy();}
    bool predicted() const {return bool(prediction_.result()) && !prediction_.busy();}
    bool preview_ready() const {return preview_.ready();}
    bool show_overlay() const {return show_;}
    bool overlay_visible() const {return show_ && opacity_>0 && preview_.ready() && preview_source() && preview_source()->classified_pixels()>0;}
    bool settled() const {return browser_.settled() && !stream_.busy() && !archives_.busy() && !contours_.busy() && !training_.busy() && !prediction_.busy() && !io_.valid() && (!show_ || !preview_source() || preview_.ready() || !preview_.error().empty());}
    const GuiPixelStreamBrowser& browser() const {return browser_;}
    const PixelStreamExportStats& stream_stats() const {return stream_stats_;}
    const GuiPixelArchives& archives() const {return archives_;}
    bool record_loaded() const {return record_loaded_;}
    std::shared_ptr<const PixelPredictionResult> historical_result() const {return historical_;}
    const GuiPixelPreview& preview() const {return preview_;}
    const PixelTrainingStats& training_stats() const {return training_stats_;}
    const PixelTrainingStats& prediction_stats() const {return prediction_stats_;}
    const std::string& io_state() const {return io_state_;}
    const std::string& io_error() const {return io_error_;}
    std::string error() const {if(!error_.empty()) return error_;if(!browser_.error().empty()) return browser_.error();if(!stream_stats_.error.empty()) return stream_stats_.error;if(!archives_.error().empty()) return archives_.error();if(!training_stats_.error.empty()) return training_stats_.error;if(!prediction_stats_.error.empty()) return prediction_stats_.error;if(!contour_stats_.error.empty()) return contour_stats_.error;return preview_.error();}
    std::shared_ptr<const PixelClassifier> model() const {return model_;}
    std::shared_ptr<const PixelPredictionResult> result() const {return prediction_.result();}
    std::shared_ptr<const PixelTrainingTable> table() const {return training_.table();}
    bool contours_ready() const {return !contours_.busy() && bool(contours_.result());}
    bool contours_published() const {return published_;}
    std::string contour_state() const {return published_?"published":contour_stats_.state;}
    std::shared_ptr<const PixelContours> traced() const {return historical_?nullptr:published_contours_?published_contours_:contours_.result();}
    const std::vector<std::uint64_t>& published_ids() const {return published_ids_;}
    double capture_ms() const {return capture_ms_;}
    std::size_t catalog_inspected() const {return catalog_inspected_;}
    std::size_t catalog_max_step() const {return catalog_max_step_;}
    bool catalog_complete() const {return catalog_complete_;}
    static constexpr bool handles(GuiInputTarget target) {return index(target)<input_targets.size() || GuiPixelStreamBrowser::handles(target);}
    Rect bounds(GuiInputTarget t) const {return GuiPixelStreamBrowser::handles(t)?browser_.bounds(t):targets_.at(index(t));}
    void finish(Document& doc,const ObjectSelection& selection,bool finite) {if(!finite) {stream_.cancel();archives_.cancel();training_.cancel();cancel_prediction();io_cancelled_.store(true);}if(training_.busy()) training_.wait(doc);if(prediction_.busy()) prediction_.wait(doc);if(contours_.busy()) contours_.wait(doc);if(io_.valid()) io_.wait();archives_.wait();if(stream_.busy()) stream_.wait(doc);browser_.finish(finite);update(doc,selection);}
    void draw(Document& doc,const ObjectSelection& selection,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark,const std::function<StainTransform(bool)>& transform,Rect visible,RecordRemovalDialog* removal=nullptr,std::uint64_t epoch=0,OutputGate* gate=nullptr,const ui::PathContext& context={},const std::string& input_cancellation={}) {
    ui::i18n::SurfaceTrace translation_surface("tool/Pixel classifier");
        if(!blocked && ui::chord_pressed(ui::CommandId::Tool_PixelClassifier,ui::frame_commands())) open_=!open_;if(!open_) {browser_.draw(doc,blocked,mark,removal,epoch,context);return;}
        ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::PixelClassifier),{620,720},ui::ToolHeight::Content);
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::PixelClassifier),&open_)) {ImGui::End();browser_.draw(doc,blocked,mark,removal,epoch,context);return;}
        const bool busy=stream_.busy() || archives_.busy() || training_.busy() || prediction_.busy() || contours_.busy() || io_.valid();const bool disabled=blocked || !has_image || busy;
        std::optional<GuiInputTarget> primary_action;
        if(!disabled && !historical_) {
            if(contours_ready() && !published_)primary_action=GuiInputTarget::PublishPixelContours;
            else if(has_geos() && prediction_.result() && prediction_request_ && !published_)primary_action=GuiInputTarget::TracePixelContours;
            else if(model_)primary_action=GuiInputTarget::PredictPixelClasses;
            else if(has_classifier_training() && (selected_only_ || catalog_complete_))primary_action=GuiInputTarget::TrainPixelClassifier;
        }
        const auto availability=[&](ui::CommandId target)->ui::Availability {
            if(target==ui::CommandId::Action_Close_kept_mask_Close_kept_pixel_mask)return {};
            std::shared_ptr<const PixelClassifierRecord> record;
            if(archives_.selected()) {const auto found=doc.pixels_.find(archives_.selected());if(found!=doc.pixels_.end())record=found->second;}
            switch(target) {
            case ui::CommandId::Action_Cancel_pixel_work:
                if(blocked)return ui::WorkflowBlocked();
                if(!busy)return {false,"Wait for the current work to finish."};
                break;
            case ui::CommandId::Action_Load_pixel_model:
                if(blocked)return ui::WorkflowBlocked();
                if(busy)return {false,"Wait for the current work to finish."};
                if(!path_.text()[0])return {false,"Choose a file path first."};
                break;
            case ui::CommandId::Action_Save_pixel_model:
                if(blocked)return ui::WorkflowBlocked();
                if(busy)return {false,"Wait for the current work to finish."};
                if(!model_)return {false,"Train or load a model first."};
                if(!save_path_.text()[0])return {false,"Choose a file path first."};
                break;
            case ui::CommandId::Action_Train_labelled_areas:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!has_classifier_training())return {false,"Use a build with this feature enabled."};
                if((!selected_only_ && !catalog_complete_))return {false,"Wait for the current work to finish."};
                break;
            case ui::CommandId::Action_Keep_training_cohort:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!training_request_)return {false,"Train a model first."};
                if(!training_.table())return {false,"Train a model first."};
                if(!training_.model())return {false,"Train a model first."};
                break;
            case ui::CommandId::Action_Predict_area:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!model_)return {false,"Train or load a model first."};
                break;
            case ui::CommandId::Action_Trace_pixel_contours:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(historical_)return {false,"Close the kept mask first."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                if(!prediction_.result())return {false,"Run a preview first."};
                if(!prediction_request_)return {false,"Run a preview first."};
                if(published_)return {false,"Run again for a new result."};
                break;
            case ui::CommandId::Action_Publish_class_annotations:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(historical_)return {false,"Close the kept mask first."};
                if(!contours_ready())return {false,"Trace contours"};
                if(published_)return {false,"Run again for a new result."};
                break;
            case ui::CommandId::Action_Hide_pixel_overlay:case ui::CommandId::Action_Show_pixel_overlay:
                if(blocked)return ui::WorkflowBlocked();
                break;
            case ui::CommandId::Action_Keep_prediction_mask:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(historical_)return {false,"Close the kept mask first."};
                if(!prediction_request_)return {false,"Run a preview first."};
                if(!prediction_.result())return {false,"Run a preview first."};
                break;
            case ui::CommandId::Action_Export_pixel_stream:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!model_)return {false,"Train or load a model first."};
                if(!stream_path_.text()[0])return {false,"Choose a file path first."};
                break;
            case ui::CommandId::Action_Saved_stream_files:
                if(blocked)return ui::WorkflowBlocked();
                break;
            case ui::CommandId::Action_Use_saved_model:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!record)return {false,"Select a kept result first."};
                break;
            case ui::CommandId::Action_Preview_saved_mask:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!record)return {false,"Select a kept result first."};
                if(!record->archive->result())return {false,"Select a kept result first."};
                break;
            case ui::CommandId::Action_Remove_record:
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(busy)return {false,"Wait for the current work to finish."};
                if(!record)return {false,"Select a kept result first."};
                break;
            default:break;
            }
            return {};
        };
        auto button=[&](const char* label,GuiInputTarget target,std::optional<ui::Icon> icon={}){
            const auto action=primary_action==target?ui::design::Action::Primary:ui::design::Action::Secondary;
            const auto available=availability(ui::workflow_command(label).id);const bool clicked=icon?ui::WorkflowIcon(label,*icon,available):ui::WorkflowAction(label,action,available);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};mark(target);
            if(reveal_target_ && *reveal_target_==target) {if(!ImGui::IsItemVisible())ImGui::SetScrollHereY(.5f);else reveal_target_.reset();}
            if(target==(model_?GuiInputTarget::PredictPixelClasses:GuiInputTarget::TrainPixelClassifier))ui::WorkflowReason(label,available);
            return clicked;};
        if(!input_cancellation.empty())ui::TextWrapped("%s",input_cancellation.c_str());
        else if(training_stats_.state!="idle") ui::DataTextWrapped("Training: %s / %s (%llu/%llu tiles)",ui::caption(ui::parse_run_state(training_stats_.state),training_stats_.state).c_str(),training_stats_.phase.c_str(),static_cast<unsigned long long>(training_stats_.completed_tiles),static_cast<unsigned long long>(training_stats_.total_tiles));
        if(!historical_ && contour_state()!="idle") ui::DataTextWrapped("Contours: %s%s",ui::caption(ui::parse_run_state(contour_state()),contour_state()).c_str(),published_?" (use Undo/Redo for this batch)":"");
        if(!historical_ && prediction_stats_.state!="idle") ui::DataTextWrapped("Prediction: %s (%llu/%llu tiles)",ui::caption(ui::parse_run_state(prediction_stats_.state),prediction_stats_.state).c_str(),static_cast<unsigned long long>(prediction_stats_.completed_tiles),static_cast<unsigned long long>(prediction_stats_.total_tiles));
        if(!error().empty()) ui::DataTextWrapped("%s",error().c_str());
        if(stream_stats_.state!="idle") ui::DataTextWrapped("Stream: %s (%llu/%llu blocks)%s",ui::caption(ui::parse_export_state(stream_stats_.state),stream_stats_.state).c_str(),static_cast<unsigned long long>(stream_stats_.completed_blocks),static_cast<unsigned long long>(stream_stats_.total_blocks),stream_stats_.current?"":" / captured historical input");
        if(busy && button("Cancel pixel work",GuiInputTarget::CancelPixelClassifier,ui::Icon::Stop)) {stop_busy_work();}
        // Model I/O remains accessible without scrolling past feature settings.
        if(model_unsaved_)ui::TextWrapped(ui::session_model_warning());
        path_.draw(context,blocked || busy,{},[&](bool){if(button("Load pixel model",GuiInputTarget::LoadPixelModel,ui::Icon::Open)) file(false);});
        save_path_.draw(context,blocked || busy,{},[&](bool){if(button("Save pixel model",GuiInputTarget::SavePixelModel,ui::Icon::Save)) {if(output_gate_available(gate,io_error_))gate->submit(save_path_.state().resolved,output_scope("Pixel classifier",false,ui::command(ui::CommandId::Tool_PixelClassifier).caption),[this,gate](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>&) {file(true,path,policy==ExportWritePolicy::ReplaceExisting);gate->watch(path,[this]()->std::optional<bool>{if(io_.valid())return {};return io_state_=="saved";},[this]{return io_state_=="failed"?io_error_:std::string{};});},[this](const std::string& message){io_error_=message;},replace_);}});
        if(io_state_!="idle") ui::DataTextf("Model file: %s",(io_state_=="saving" || io_state_=="saved"?ui::caption(ui::parse_export_state(io_state_),io_state_):ui::caption(ui::parse_run_state(io_state_),io_state_)).c_str());if(!io_error_.empty()) ui::DataTextWrapped("%s",io_error_.c_str());
        ui::SeparatorText("Training");
        ui::DataTextWrapped("Label area annotations with at least two classes, then train. Stain/white values come from the image analysis controls.");
        ImGui::BeginDisabled(disabled);bool changed=ui::Checkbox("Train selected labelled areas only",&selected_only_);if(!catalog_complete_) ui::DataTextWrapped("Indexing annotations: %zu objects inspected",catalog_inspected_);
        if(ui::CollapsingHeader("Training features and sampling")) {
            changed=ui::Checkbox("RGB channels",&options_.features.rgb) || changed;changed=ui::Checkbox("Optical density channels",&options_.features.optical_density) || changed;changed=ui::Checkbox("Stain channels",&options_.features.stains) || changed;
            changed=ui::Checkbox("Gradient magnitude",&options_.features.gradient_magnitude) || changed;changed=ui::Checkbox("Laplacian",&options_.features.laplacian) || changed;changed=ui::InputText("Scales (grid pixels)",scales_.data(),scales_.size()) || changed;
            changed=ui::Checkbox("Use calibration when available",&options_.calibrated) || changed;changed=ui::Checkbox("Balance classes",&options_.balanced) || changed;changed=ui::InputScalar("Pyramid level",ImGuiDataType_S32,&options_.level) || changed;
            std::uint64_t samples=options_.max_samples,per_class=options_.max_samples_per_class;if(ui::InputScalar("Total sample cap",ImGuiDataType_U64,&samples)) {options_.max_samples=static_cast<std::size_t>(samples);changed=true;}if(ui::InputScalar("Per-class sample cap",ImGuiDataType_U64,&per_class)) {options_.max_samples_per_class=static_cast<std::size_t>(per_class);changed=true;}
            changed=ui::InputScalar("Sample seed",ImGuiDataType_U64,&options_.seed) || changed;changed=ui::InputScalar("Maximum scanned pixels",ImGuiDataType_U64,&options_.max_scanned_pixels) || changed;
        }
        if(ui::CollapsingHeader("Forest and memory settings")) {changed=ui::InputScalar("Trees",ImGuiDataType_U32,&parameters_.trees) || changed;changed=ui::InputScalar("Maximum depth",ImGuiDataType_U32,&parameters_.max_depth) || changed;changed=ui::InputScalar("Minimum samples",ImGuiDataType_U32,&parameters_.min_samples) || changed;changed=ui::InputScalar("Forest seed",ImGuiDataType_U64,&parameters_.seed) || changed;std::uint64_t mib=options_.work_bytes/(1024*1024);if(ui::InputScalar("Work limit (MiB)",ImGuiDataType_U64,&mib)) {options_.work_bytes=std::clamp<std::uint64_t>(mib,1,1024)*1024*1024;prediction_options_.work_bytes=options_.work_bytes;parameters_.work_bytes=options_.work_bytes;changed=true;}}
        if(changed) cancel_training();ImGui::EndDisabled();
        std::size_t training_areas=0,classifier_areas=0;
        auto labelled_area=[&](std::uint64_t id) {const auto found=doc.objects().find(id);return found!=doc.objects().end() && found->second.kind==ObjectKind::Annotation && found->second.geometry.is_area() && !found->second.class_name.empty();};
        if(selected_only_) {for(auto id:selection.ids())if(labelled_area(id)){++training_areas;if(session_published_ids_.count(id))++classifier_areas;}}
        else {training_areas=labelled_.size();for(auto id:session_published_ids_)if(labelled_area(id))++classifier_areas;}
        ui::TextWrapped("Training uses %zu labelled areas (%zu added by a classifier in this session).",training_areas,classifier_areas);
        if(button("Train labelled areas",GuiInputTarget::TrainPixelClassifier)) {try {train(doc,selection,transform(options_.features.stains));}catch(const std::exception& e) {error_=e.what();}}
        if(auto table=training_.table();table && ui::CollapsingHeader("Training data###Training cohort")) {ui::DataTextf("%zu rows; %llu conflicting pixels excluded",table->row_count(),static_cast<unsigned long long>(table->ambiguous_pixels()));if(ImGui::BeginTable("Pixel cohort",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)) {ImGui::TableSetupColumn("Class");ImGui::TableSetupColumn("Eligible");ImGui::TableSetupColumn("Used");ImGui::TableHeadersRow();for(std::size_t i=0;i<table->classes().size();++i) {ImGui::TableNextRow();ImGui::TableNextColumn();ui::DataText(table->classes()[i].c_str());ImGui::TableNextColumn();ui::DataTextf("%llu",static_cast<unsigned long long>(table->eligible_class_pixels()[i]));ImGui::TableNextColumn();ui::DataTextf("%llu",static_cast<unsigned long long>(table->sampled_class_counts()[i]));}ImGui::EndTable();}}
        if(button("Keep training cohort",GuiInputTarget::KeepPixelTraining)) archives_.keep(*training_request_,training_.table(),training_.model());
        ui::SeparatorText("Predict and review");
        ImGui::BeginDisabled(disabled);
        const auto preview=target_?"Area #"+std::to_string(target_):std::string("Current selection");
        if(ui::ComboScope combo{"Prediction area",preview.c_str()};combo) {if(ui::Selectable("Current selection",target_==0)) {target_=0;cancel_prediction();}ImGuiListClipper clipper;clipper.Begin(static_cast<int>(areas_.size()));while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {const auto id=areas_[i];const auto label="Area #"+std::to_string(id);if(ui::Selectable(ui::data_label(label,std::to_string(id)).c_str(),target_==id)) {target_=id;cancel_prediction();}if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",doc.annotation(id).name.c_str());}}
        std::uint64_t preview_mib=preview_limit_/(1024*1024);
        if(ui::InputScalar("Preview limit (MiB)",ImGuiDataType_U64,&preview_mib)) {preview_limit_=std::clamp<std::uint64_t>(preview_mib,1,128)*1024*1024;preview_reset_=true;}
        if(ui::InputDouble("Minimum tree vote",&prediction_options_.min_vote_fraction,0,0,"%.3f"))cancel_prediction();ImGui::EndDisabled();
        ImGui::BeginDisabled(disabled);if(ui::Checkbox("Predict visible part of area only",&viewport_only_)) cancel_prediction();ImGui::EndDisabled();
        if(!historical_ && model_) {const auto resolution=model_->resolution();ui::DataTextWrapped("Model: %zu classes, %zu features, %u trees",model_->classes().size(),model_->features().size(),model_->forest().tree_count());if(resolution) {if(resolution->physical()) ui::TextWrapped("Sampling: %.6g x %.6g %s",*resolution->pixel_width_um,*resolution->pixel_height_um,ui::unit::um);else ui::DataTextWrapped("Sampling: %.6g source pixels",resolution->downsample);}ui::DataTextWrapped("Prediction uses the model's saved feature recipe.");}
        if(button("Predict area",GuiInputTarget::PredictPixelClasses)) predict(doc,selection,visible);
        if(button("Trace pixel contours",GuiInputTarget::TracePixelContours)) trace(doc);
        if(button("Publish class annotations",GuiInputTarget::PublishPixelContours)) publish(doc);
        if(ui::CollapsingHeader("Contour settings")) {
            ImGui::BeginDisabled(disabled);bool contour_changed=ui::Checkbox("Clip contours to prediction area",&contour_options_.clip_to_roi);
            auto limit=[&](const char* label,std::size_t& value,std::uint64_t maximum) {std::uint64_t v=value;if(ui::InputScalar(label,ImGuiDataType_U64,&v)) {value=static_cast<std::size_t>(std::clamp<std::uint64_t>(v,1,maximum));contour_changed=true;}};
            limit("Maximum contour classes",contour_options_.max_components,1000000);limit("Maximum contour edges",contour_options_.max_edges,1000000);limit("Maximum contour vertices",contour_options_.max_vertices,1000000);
            std::uint64_t mib=contour_options_.work_bytes/(1024*1024);if(ui::InputScalar("Contour work limit (MiB)",ImGuiDataType_U64,&mib)) {contour_options_.work_bytes=std::clamp<std::uint64_t>(mib,1,512)*1024*1024;contour_changed=true;}
            if(contour_changed) clear_contours();ImGui::EndDisabled();
            ui::DataTextWrapped("One multipolygon annotation per class. Clipped output is parented to the prediction area. Raw footprints are root annotations.");
        }
        if(button(show_?"Hide pixel overlay":"Show pixel overlay",GuiInputTarget::TogglePixelOverlay)) show_=!show_;
        if(auto geometry=historical_?nullptr:this->traced()) {const auto& c=*geometry->contours();ui::DataTextWrapped("%zu classes / %zu edges / %zu vertices / %.1f ms",c.components().size(),c.edge_count(),c.vertex_count(),c.elapsed_ms());}
        if(!historical_ && published_) ui::TextWrapped("Added %zu class annotations",published_ids_.size());
        if(!historical_ && model_ && ui::CollapsingHeader("Overlay colors",colors_.size()<=8?ImGuiTreeNodeFlags_DefaultOpen:0)) {bool palette=draw_opacity();for(std::size_t i=0;i<colors_.size();++i) {ImGui::PushID(static_cast<int>(i));const auto label=ui::data_label("Class "+std::to_string(i+1)+" / "+model_->classes()[i],"class-color");palette=ui::ColorEdit3(label.c_str(),colors_[i].data(),0,true) || palette;ImGui::PopID();}if(palette)make_palette();}
        if(auto result=historical_?nullptr:prediction_.result()) {if(result->windowed()) ui::DataTextWrapped("Windowed prediction: captured visible grid; pan and predict again to update.");ui::DataTextWrapped("%lld x %lld / level %d / %.1f ms",static_cast<long long>(result->width()),static_cast<long long>(result->height()),result->level(),result->elapsed_ms());ui::DataTextWrapped("%llu classified pixels; preview %s",static_cast<unsigned long long>(result->classified_pixels()),preview_.ready()?"ready":"preparing");}
        if(!historical_ && kept_prediction_) ui::TextWrapped("Kept as record %llu. Predict again to trace contours or add annotations.",static_cast<unsigned long long>(kept_prediction_));
        if(button("Keep prediction mask",GuiInputTarget::KeepPixelPrediction)) {archives_.keep(*prediction_request_,prediction_.result());keeping_prediction_=true;}
        ui::SeparatorText("Save and export");
        ImGui::BeginDisabled(blocked || busy);stream_path_.draw(context);ImGui::EndDisabled();
        if(ui::CollapsingHeader("Stream resource limits")) {
            ImGui::BeginDisabled(blocked || busy);
            std::uint64_t block=stream_options_.block_size;if(ui::InputScalar("Stream block size",ImGuiDataType_U64,&block)) stream_options_.block_size=static_cast<std::size_t>(std::clamp<std::uint64_t>(block,16,512));
            std::uint64_t mib=stream_options_.prediction.work_bytes/(1024*1024);if(ui::InputScalar("Stream work limit (MiB)",ImGuiDataType_U64,&mib)) stream_options_.prediction.work_bytes=std::clamp<std::uint64_t>(mib,1,1024)*1024*1024;
            std::uint64_t gib=stream_max_bytes_/(1ULL<<30);if(ui::InputScalar("Stream file limit (GiB)",ImGuiDataType_U64,&gib)) stream_max_bytes_=std::clamp<std::uint64_t>(gib,1,65536)*(1ULL<<30);ImGui::EndDisabled();
            ui::TextWrapped("Export captures the selected area and saved model recipe. Visible-only uses the captured viewport. No full mask is kept; completed files are historical values.");
            if(stream_stats_.summary) ui::DataTextWrapped("Saved %llu bytes / content SHA-256: %s",static_cast<unsigned long long>(stream_stats_.summary->bytes),stream_stats_.summary->sha256.c_str());
        }
        if(button("Export pixel stream",GuiInputTarget::ExportPixelStream)) {if(output_gate_available(gate,error_))export_stream(doc,selection,visible,*gate);}
        if(button("Saved stream files",GuiInputTarget::OpenPixelStreamBrowser)) browser_.open();
        std::shared_ptr<const PixelClassifierRecord> record;
        if(archives_.selected()) {const auto found=doc.pixels_.find(archives_.selected());if(found!=doc.pixels_.end()) record=found->second;}
        ImGui::BeginDisabled(blocked || busy);
        record_rows_.refresh(&doc,doc.revision(),archives_.records().size(),[&](auto& rows){for(const auto& r:archives_.records()){auto row=ui::record_row(r->id,r->name,"Pixel classifier");row.kind=r->archive->purpose()=="training"?"Training":"Prediction";rows.push_back(std::move(row));}},doc.server().get());
        archives_.select(ui::draw_record_list("Kept pixel records###Saved pixel records",record_rows_,archives_.selected(),{}));
        record.reset();if(archives_.selected())record=doc.pixel_classifier_record(archives_.selected());
        ImGui::EndDisabled();
        if(model_unsaved_)ui::TextWrapped(ui::session_model_warning());
        if(button("Use saved model",GuiInputTarget::UsePixelRecordModel)) {cancel_training();adopt(record->archive->model(),doc,false,record->id);record_loaded_=true;}
        ImGui::SameLine();
        if(button("Preview saved mask",GuiInputTarget::PreviewPixelRecord)) {historical_=record->archive->result();historical_record_=record->id;historical_palette_=class_palette(doc,record->archive->model()->classes());for(auto& color:historical_palette_)color=(color&0xffffffu)|(std::uint32_t(std::lround(opacity_*255))<<24);}
        ImGui::SameLine();
        if(button("Remove record",GuiInputTarget::RemovePixelRecord)) {if(removal)removal->request_record(doc,epoch,"Pixel classifier",record->id,&Document::pixel_classifier_record,&Document::remove_pixel_classifier_record);}
        if(historical_ && ui::WorkflowAction("Close kept mask###Close kept pixel mask",ui::design::Action::Secondary,availability(ui::CommandId::Action_Close_kept_mask_Close_kept_pixel_mask))) {historical_.reset();historical_record_=0;}
        if(historical_) {
            if(historical_)draw_opacity();
            if(historical_)for(std::size_t i=0;i<std::min(historical_->model().classes().size(),historical_palette_.size());++i) {
                const auto color=historical_palette_[i];ImGui::PushID(int(i));
                ImGui::ColorButton("##kept-class-color",ImVec4(float(color&255)/255,float((color>>8)&255)/255,float((color>>16)&255)/255,1),ImGuiColorEditFlags_NoTooltip,{ImGui::GetFontSize(),ImGui::GetFontSize()});ImGui::SameLine();
                ui::DataTextWrapped("%s",historical_->model().classes()[i].c_str());ImGui::PopID();
            }
        }
        if(historical_) ui::TextWrapped("%s - run again before adding annotations.",historical_->windowed()?"Historical windowed mask":"Historical mask");
        if(archives_.state()!="idle") ui::DataTextf("Record: %s",ui::caption(ui::parse_run_state(archives_.state()),archives_.state()).c_str());
        if(record && ui::CollapsingHeader("Kept record details###Saved record details")) {
            const auto& archive=*record->archive;ui::TextWrapped("%s / %zu original areas / %zu kept bytes",archive.purpose().c_str(),archive.annotations().size(),archive.payload_bytes());
            ui::DataTextWrapped("Artifact SHA-256: %s",archive.sha256().c_str());
            ui::DataTextWrapped("Captured document revision: %llu",static_cast<unsigned long long>(archive.provenance()->document_revision));
            if(archive.table()) ui::DataTextf("%zu original training rows",archive.table()->row_count());
            if(archive.result()) {const auto& r=*archive.result();ui::DataTextf("%llu original classified pixels",static_cast<unsigned long long>(r.classified_pixels()));ui::DataTextWrapped("%lld x %lld / level %d / %s",static_cast<long long>(r.width()),static_cast<long long>(r.height()),r.level(),r.windowed()?"windowed grid":"full target grid");}
        }
        ImGui::End();browser_.draw(doc,blocked,mark,removal,epoch,context);
    }
};
}
