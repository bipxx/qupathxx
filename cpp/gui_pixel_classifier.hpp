#pragma once
#include "gui_pixel_preview.hpp"
#include "gui_pixel_archives.hpp"
#include "qupathxx/pixel_contours.hpp"
#include "gui_layout.hpp"
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
    GuiPixelArchives archives_;
    std::optional<PixelTrainingRequest> training_request_;
    std::shared_ptr<const PixelPredictionResult> historical_;
    std::uint64_t historical_record_=0;bool record_loaded_=false;
    std::shared_ptr<const PixelPredictionResult> preview_source() const {return historical_?historical_:prediction_.result();}
    PixelContourJob contours_;TissueContourOptions contour_options_;PixelTrainingStats contour_stats_;
    std::optional<PixelPredictionRequest> prediction_request_;
    std::shared_ptr<const PixelContours> published_contours_;
    std::vector<std::uint64_t> published_ids_;bool published_=false;
    void clear_contours() {contours_.cancel();published_=false;published_contours_.reset();published_ids_.clear();}
    void cancel_prediction() {prediction_.cancel();prediction_request_.reset();historical_.reset();historical_record_=0;clear_contours();}
    PixelTrainingOptions options_;PixelPredictionOptions prediction_options_;ForestParameters parameters_;
    PixelTrainingJob training_;PixelPredictionJob prediction_;PixelTrainingStats training_stats_,prediction_stats_;
    std::shared_ptr<const PixelClassifier> model_;GuiPixelPreview preview_;
    bool open_=false,selected_only_=false,replace_=false,show_=true,trained_=false,preview_reset_=false;
    std::uint64_t target_=0,adopted_=0;std::size_t preview_limit_=0;double capture_ms_=0;float opacity_=.45f;
    std::vector<std::array<float,3>> colors_;std::vector<std::uint32_t> palette_;
    std::array<char,4096> path_{};std::array<char,256> scales_{};std::array<Rect,13> targets_{};
    std::string error_,io_state_="idle",io_error_;
    struct IoResult {std::shared_ptr<const PixelClassifier> model;std::string error;bool save=false;};
    std::future<IoResult> io_;std::atomic<bool> io_cancelled_{false};
    std::shared_ptr<const char> catalog_identity_;std::uint64_t catalog_revision_=0;
    std::map<std::uint64_t,Annotation>::const_iterator catalog_cursor_;
    std::vector<std::uint64_t> labelled_,areas_;bool catalog_complete_=false;
    std::size_t catalog_inspected_=0,catalog_max_step_=0;
    std::vector<std::uint64_t> captured_selection_;
    static std::size_t index(GuiInputTarget t) {return static_cast<std::size_t>(t)-static_cast<std::size_t>(GuiInputTarget::TrainPixelClassifier);}
    void scales_text() {std::ostringstream text;text.imbue(std::locale::classic());text<<std::setprecision(17);for(auto v:options_.features.sigmas) {if(text.tellp()>0) text<<", ";text<<v;}const auto s=text.str();scales_.fill(0);std::memcpy(scales_.data(),s.data(),std::min(s.size(),scales_.size()-1));}
    void parse_scales() {std::istringstream all(scales_.data());std::string field;std::vector<double> values;while(std::getline(all,field,',')) {std::istringstream part(field);part.imbue(std::locale::classic());double value;if(!(part>>value)) throw std::invalid_argument("Enter comma-separated feature scales");part>>std::ws;if(!part.eof()) throw std::invalid_argument("Invalid feature scale");values.push_back(value);}if(!all.eof() || (!std::string(scales_.data()).empty() && std::string(scales_.data()).back()==',')) throw std::invalid_argument("Invalid feature scale list");auto f=options_.features;f.sigmas=std::move(values);f.validate();options_.features=std::move(f);}
    void make_palette() {if(!std::isfinite(opacity_)) opacity_=.45f;opacity_=std::clamp(opacity_,0.f,1.f);palette_.clear();for(const auto& c:colors_) {std::uint32_t rgb=0;for(unsigned i=0;i<3;++i) rgb|=std::uint32_t(std::lround((std::isfinite(c[i])?std::clamp(c[i],0.f,1.f):0.f)*255))<<(8*i);palette_.push_back(rgb|(std::uint32_t(std::lround(opacity_*255))<<24));}}
    void adopt(std::shared_ptr<const PixelClassifier> model,const Document& doc) {
        cancel_prediction();model_=std::move(model);options_.features=model_->options();scales_text();colors_.clear();
        static constexpr std::uint32_t defaults[]={0x4c78a8,0xf58518,0xe45756,0x72b7b2,0x54a24b,0xeeca3b,0xb279a2,0xff9da6};
        for(std::size_t i=0;i<model_->classes().size();++i) {auto color=defaults[i%8];const auto found=doc.classes().find(model_->classes()[i]);if(found!=doc.classes().end() && found->second.color!=0x41e1be) color=found->second.color;colors_.push_back({float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255});}make_palette();
        try {options_.level=pixel_classifier_level(doc,*model_);}catch(const std::exception&) {}error_.clear();
    }
    void cancel_training() {training_.cancel();training_request_.reset();captured_selection_.clear();trained_=false;}
    void train(const Document& doc,const ObjectSelection& selection,const StainTransform& transform) {
        try {parse_scales();options_.validate();parameters_.validate();std::vector<std::uint64_t> ids;
            if(selected_only_) {if(selection.size()>4096) throw std::length_error("Select at most 4096 training areas");for(auto id:selection.ids()) {const auto a=doc.annotation(id);if(a.geometry.is_area() && !a.class_name.empty()) ids.push_back(id);}}
            else {if(!catalog_complete_) throw std::logic_error("Wait for the annotation list to finish");ids=labelled_;}
            if(ids.size()>4096) throw std::length_error("Too many labelled annotations; select up to 4096 training areas");
            PixelTrainingRequest request(doc,ids,transform,options_);capture_ms_=request.capture_ms();if(!training_.train(request,parameters_)) return;training_request_=request;cancel_prediction();trained_=false;captured_selection_=selected_only_?selection.ids():std::vector<std::uint64_t>{};error_.clear();
        }catch(const std::exception& e) {error_=e.what();}
    }
    void predict(const Document& doc,const ObjectSelection& selection) {
        try {if(!model_) throw std::logic_error("Train or load a pixel model first");const auto id=target_?target_:selection.primary();if(!id) throw std::invalid_argument("Select an area for prediction");const auto start=std::chrono::steady_clock::now();PixelPredictionRequest request(doc,id,*model_,prediction_options_);capture_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(prediction_.request(request)) {historical_.reset();historical_record_=0;clear_contours();prediction_request_=request;error_.clear();}}catch(const std::exception& e) {error_=e.what();}
    }
    void trace(const Document& doc) {
        try {
            if(!prediction_request_ || !prediction_.result() || !prediction_request_->matches(doc)) throw std::logic_error("Predict an area before tracing contours");
            if(contours_.request(*prediction_request_,prediction_.result(),contour_options_)) {published_=false;published_contours_.reset();published_ids_.clear();error_.clear();}
        }catch(const std::exception& e) {error_=e.what();}
    }
    void publish(Document& doc) {
        try {
            if(published_ || !prediction_request_ || contours_.poll(doc).state!="ready" || !contours_.result()) throw std::logic_error("Trace current pixel contours before publishing");
            auto traced=contours_.result();auto ids=publish_pixel_contours(doc,*prediction_request_,*traced);
            contours_.cancel();published_contours_=std::move(traced);published_ids_=std::move(ids);published_=true;error_.clear();
        }catch(const std::exception& e) {error_=e.what();}
    }
    void file(bool save) {
        if(io_.valid()) return;if(save && !model_) {io_error_="Train or load a pixel model first";return;}const std::string path(path_.data());auto model=model_;const bool replace=replace_;io_cancelled_.store(false);io_state_=save?"saving":"loading";io_error_.clear();
        try {io_=std::async(std::launch::async,[this,path,model=std::move(model),save,replace] {IoResult out;out.save=save;try {if(save) save_pixel_classifier(*model,path,replace,&io_cancelled_);else {auto loaded=load_pixel_classifier(path,&io_cancelled_);if(!loaded.resolution()) throw std::invalid_argument("Pixel preview requires a resolution-bound model; this file is an unbound buffer recipe");out.model=std::make_shared<const PixelClassifier>(std::move(loaded));}}catch(const std::exception& e) {out.error=e.what();}return out;});}catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}
    }
public:
    explicit GuiPixelClassifier(const GuiOptions& o):contour_options_(o.pixel_contour_options?*o.pixel_contour_options:TissueContourOptions{}),options_(o.pixel_training_options?*o.pixel_training_options:PixelTrainingOptions{}),prediction_options_(o.pixel_prediction_options?*o.pixel_prediction_options:PixelPredictionOptions{}),parameters_(o.pixel_parameters?*o.pixel_parameters:pixel_forest_parameters()),open_(o.pixel_open),selected_only_(o.pixel_selected_only),replace_(o.pixel_replace_model),show_(o.pixel_show_overlay),target_(o.pixel_object_id),preview_limit_(o.pixel_preview_budget) {
        contour_options_.validate();options_.validate();prediction_options_.validate();parameters_.validate();if(preview_limit_>128u*1024*1024) throw std::invalid_argument("pixel preview budget exceeds 128 MiB");bounded_utf8(o.pixel_model_path,path_.size()-1);std::memcpy(path_.data(),o.pixel_model_path.data(),o.pixel_model_path.size());scales_text();if(o.pixel_model) model_=std::make_shared<const PixelClassifier>(*o.pixel_model);
    }
    ~GuiPixelClassifier() {contours_.cancel();io_cancelled_.store(true);if(io_.valid()) io_.wait();}
    void open() {open_=true;}
    void invalidate_training() {if(training_.busy()) cancel_training();}
    void update(Document& doc,const ObjectSelection& selection) {
        if(preview_reset_) {preview_.clear();preview_reset_=false;}
        const bool changed_document=catalog_identity_ && catalog_identity_!=doc.analysis_identity_.token;
        if(changed_document) {cancel_training();cancel_prediction();target_=0;record_loaded_=false;}
        archives_.update(doc,doc.pixels_,doc.analysis_identity_.token,open_);
        if(historical_record_ && !doc.pixels_.count(historical_record_)) {historical_.reset();historical_record_=0;}
        if(catalog_identity_!=doc.analysis_identity_.token || catalog_revision_!=doc.revision()) {if(target_ && !doc.objects().count(target_)) target_=0;catalog_identity_=doc.analysis_identity_.token;catalog_revision_=doc.revision();catalog_cursor_=doc.objects().cbegin();catalog_complete_=false;catalog_inspected_=0;labelled_.clear();areas_.clear();}
        std::size_t step=0;while(open_ && !catalog_complete_ && step<1024) {if(catalog_cursor_==doc.objects().cend()) {catalog_complete_=true;break;}const auto& [id,a]=*catalog_cursor_++;if(a.kind==ObjectKind::Annotation && a.geometry.is_area()) {if(areas_.size()<10000) areas_.push_back(id);if(!a.class_name.empty() && labelled_.size()<4097) labelled_.push_back(id);}++step;++catalog_inspected_;}catalog_max_step_=std::max(catalog_max_step_,step);
        if(selected_only_ && !captured_selection_.empty() && (selection.size()!=captured_selection_.size() || selection.ids()!=captured_selection_)) cancel_training();
        if(io_.valid() && io_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto out=io_.get();if(io_cancelled_.load()) io_state_="cancelled";else if(!out.error.empty()) {io_state_="failed";io_error_=std::move(out.error);}else if(out.save) io_state_="saved";else {cancel_training();adopt(std::move(out.model),doc);io_state_="loaded";}}catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}}
        training_stats_=training_.poll(doc);prediction_stats_=prediction_.poll(doc);contour_stats_=contours_.poll(doc);
        if(prediction_request_ && !prediction_request_->matches(doc)) prediction_request_.reset();
        if(training_request_ && !training_request_->matches(doc)) training_request_.reset();
        if(training_stats_.state=="ready" && training_.model() && adopted_!=training_stats_.generation) {adopt(training_.model(),doc);adopted_=training_stats_.generation;trained_=true;}
        if(model_ && colors_.size()!=model_->classes().size()) {auto m=model_;adopt(std::move(m),doc);}
        preview_.update(preview_source(),palette_,false,preview_limit_);
    }
    void update_preview() {preview_.update(preview_source(),palette_,show_,preview_limit_);}
    void draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {if(show_) preview_.draw(draw,view,offset,size);}
    bool trained() const {return trained_ && !training_.busy();}
    bool predicted() const {return bool(prediction_.result()) && !prediction_.busy();}
    bool preview_ready() const {return preview_.ready();}
    bool show_overlay() const {return show_;}
    bool overlay_visible() const {return show_ && opacity_>0 && preview_.ready() && preview_source() && preview_source()->classified_pixels()>0;}
    bool settled() const {return !archives_.busy() && !contours_.busy() && !training_.busy() && !prediction_.busy() && !io_.valid() && (!show_ || !preview_source() || preview_.ready() || !preview_.error().empty());}
    const GuiPixelArchives& archives() const {return archives_;}
    bool record_loaded() const {return record_loaded_;}
    std::shared_ptr<const PixelPredictionResult> historical_result() const {return historical_;}
    const GuiPixelPreview& preview() const {return preview_;}
    const PixelTrainingStats& training_stats() const {return training_stats_;}
    const PixelTrainingStats& prediction_stats() const {return prediction_stats_;}
    const std::string& io_state() const {return io_state_;}
    const std::string& io_error() const {return io_error_;}
    std::string error() const {if(!error_.empty()) return error_;if(!archives_.error().empty()) return archives_.error();if(!training_stats_.error.empty()) return training_stats_.error;if(!prediction_stats_.error.empty()) return prediction_stats_.error;if(!contour_stats_.error.empty()) return contour_stats_.error;return preview_.error();}
    std::shared_ptr<const PixelClassifier> model() const {return model_;}
    std::shared_ptr<const PixelPredictionResult> result() const {return prediction_.result();}
    std::shared_ptr<const PixelTrainingTable> table() const {return training_.table();}
    bool contours_ready() const {return !contours_.busy() && bool(contours_.result());}
    bool contours_published() const {return published_;}
    std::string contour_state() const {return published_?"published":contour_stats_.state;}
    std::shared_ptr<const PixelContours> traced() const {return published_contours_?published_contours_:contours_.result();}
    const std::vector<std::uint64_t>& published_ids() const {return published_ids_;}
    double capture_ms() const {return capture_ms_;}
    std::size_t catalog_inspected() const {return catalog_inspected_;}
    std::size_t catalog_max_step() const {return catalog_max_step_;}
    bool catalog_complete() const {return catalog_complete_;}
    Rect bounds(GuiInputTarget t) const {return targets_.at(index(t));}
    void finish(Document& doc,const ObjectSelection& selection,bool finite) {if(!finite) {archives_.cancel();training_.cancel();cancel_prediction();io_cancelled_.store(true);}if(training_.busy()) training_.wait(doc);if(prediction_.busy()) prediction_.wait(doc);if(contours_.busy()) contours_.wait(doc);if(io_.valid()) io_.wait();archives_.wait();update(doc,selection);}
    void draw(Document& doc,const ObjectSelection& selection,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark,const std::function<StainTransform(bool)>& transform) {
        if(!ImGui::GetIO().WantTextInput && !blocked && ImGui::IsKeyPressed(ImGuiKey_F5,false)) open_=!open_;if(!open_) return;
        const auto* vp=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;ImGui::SetNextWindowSize({std::min(620.f*scale,vp->WorkSize.x*.88f),std::min(720.f*scale,vp->WorkSize.y*.88f)},ImGuiCond_Appearing);ImGui::SetNextWindowPos({vp->WorkPos.x+vp->WorkSize.x*.5f,vp->WorkPos.y+vp->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});ImGui::SetNextWindowSizeConstraints({std::min(260.f*scale,vp->WorkSize.x),std::min(160.f*scale,vp->WorkSize.y)},vp->WorkSize);
        if(!ImGui::Begin("Pixel classifiers",&open_)) {ImGui::End();return;}
        auto button=[&](const char* label,GuiInputTarget target,bool disabled){ImGui::BeginDisabled(disabled);const bool clicked=ui::Button(label);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};mark(target);ImGui::EndDisabled();return clicked;};
        const bool busy=archives_.busy() || training_.busy() || prediction_.busy() || contours_.busy() || io_.valid();const bool disabled=blocked || !has_image || busy;
        // Primary actions remain above advanced controls in narrow windows.
        if(button("Train labelled areas",GuiInputTarget::TrainPixelClassifier,disabled || !has_classifier_training() || (!selected_only_ && !catalog_complete_))) {try {train(doc,selection,transform(options_.features.stains));}catch(const std::exception& e) {error_=e.what();}}
        ImGui::SameLine();
        if(button("Predict area",GuiInputTarget::PredictPixelClasses,disabled || !model_)) predict(doc,selection);
        if(button("Trace pixel contours",GuiInputTarget::TracePixelContours,disabled || !has_geos() || !prediction_.result() || !prediction_request_ || published_)) trace(doc);
        ImGui::SameLine();
        if(button("Publish class annotations",GuiInputTarget::PublishPixelContours,disabled || !contours_ready() || published_)) publish(doc);
        if(button("Cancel pixel work",GuiInputTarget::CancelPixelClassifier,blocked || !busy)) {archives_.cancel();cancel_training();cancel_prediction();io_cancelled_.store(true);}
        ImGui::SameLine();
        if(button(show_?"Hide pixel overlay":"Show pixel overlay",GuiInputTarget::TogglePixelOverlay,blocked)) show_=!show_;
        if(button("Keep training cohort",GuiInputTarget::KeepPixelTraining,disabled || !training_request_ || !training_.table() || !training_.model())) archives_.keep(*training_request_,training_.table(),training_.model());
        ImGui::SameLine();
        if(button("Keep prediction mask",GuiInputTarget::KeepPixelPrediction,disabled || !prediction_request_ || !prediction_.result())) archives_.keep(*prediction_request_,prediction_.result());
        std::shared_ptr<const PixelClassifierRecord> record;
        if(archives_.selected()) {const auto found=doc.pixels_.find(archives_.selected());if(found!=doc.pixels_.end()) record=found->second;}
        ImGui::BeginDisabled(blocked || busy);
        if(ui::BeginCombo("Saved pixel records",record?record->name.c_str():"No saved records")) {
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(archives_.records().size()));
            while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {const auto& r=archives_.records()[i];const auto label="#"+std::to_string(r->id)+" / "+r->archive->purpose()+" / "+r->name;if(ImGui::Selectable(label.c_str(),archives_.selected()==r->id)) archives_.select(r->id);}
            ui::EndCombo();
        }
        ImGui::EndDisabled();
        if(button("Use saved model",GuiInputTarget::UsePixelRecordModel,disabled || !record)) {cancel_training();adopt(record->archive->model(),doc);record_loaded_=true;}
        ImGui::SameLine();
        if(button("Preview saved mask",GuiInputTarget::PreviewPixelRecord,disabled || !record || !record->archive->result())) {cancel_training();adopt(record->archive->model(),doc);historical_=record->archive->result();historical_record_=record->id;}
        ImGui::SameLine();
        if(button("Remove record",GuiInputTarget::RemovePixelRecord,disabled || !record)) {doc.remove_pixel_classifier_record(record->id);if(historical_record_==record->id) cancel_prediction();}
        if(historical_) ImGui::TextWrapped("Historical mask - recompute before publishing annotations.");
        if(archives_.state()!="idle") ImGui::Text("Record: %s",archives_.state().c_str());
        if(record && ImGui::CollapsingHeader("Saved record details")) {
            const auto& archive=*record->archive;ImGui::TextWrapped("%s / %zu original areas / %zu retained bytes",archive.purpose().c_str(),archive.annotations().size(),archive.payload_bytes());
            ImGui::TextWrapped("Artifact SHA-256: %s",archive.sha256().c_str());
            ImGui::TextWrapped("Captured document revision: %llu",static_cast<unsigned long long>(archive.provenance()->document_revision));
            if(archive.table()) ImGui::Text("%zu original training rows",archive.table()->row_count());
            if(archive.result()) ImGui::Text("%llu original classified pixels",static_cast<unsigned long long>(archive.result()->classified_pixels()));
        }
        if(training_stats_.state!="idle") ImGui::TextWrapped("Training: %s / %s (%llu/%llu tiles)",training_stats_.state.c_str(),training_stats_.phase.c_str(),static_cast<unsigned long long>(training_stats_.completed_tiles),static_cast<unsigned long long>(training_stats_.total_tiles));
        if(contour_state()!="idle") ImGui::TextWrapped("Contours: %s%s",contour_state().c_str(),published_?" (use Undo/Redo for this batch)":"");
        if(prediction_stats_.state!="idle") ImGui::TextWrapped("Prediction: %s (%llu/%llu tiles)",prediction_stats_.state.c_str(),static_cast<unsigned long long>(prediction_stats_.completed_tiles),static_cast<unsigned long long>(prediction_stats_.total_tiles));
        if(!error().empty()) ImGui::TextWrapped("%s",error().c_str());
        // Model I/O remains accessible without scrolling past feature settings.
        ImGui::BeginDisabled(blocked || busy);ui::InputText("Pixel model file",path_.data(),path_.size());ui::Checkbox("Replace an existing pixel model",&replace_);ImGui::EndDisabled();
        if(button("Save pixel model",GuiInputTarget::SavePixelModel,blocked || busy || !model_ || !path_[0])) file(true);ImGui::SameLine();if(button("Load pixel model",GuiInputTarget::LoadPixelModel,blocked || busy || !path_[0])) file(false);
        if(io_state_!="idle") ImGui::Text("Model file: %s",io_state_.c_str());if(!io_error_.empty()) ImGui::TextWrapped("%s",io_error_.c_str());
        ImGui::TextWrapped("Label area annotations with at least two classes, then train. Stain/white values come from the image analysis controls.");
        ImGui::BeginDisabled(disabled);bool changed=ui::Checkbox("Train selected labelled areas only",&selected_only_);if(!catalog_complete_) ImGui::TextWrapped("Indexing annotations: %zu objects inspected",catalog_inspected_);
        const auto preview=target_?"Area #"+std::to_string(target_):std::string("Current selection");
        if(ui::BeginCombo("Prediction area",preview.c_str())) {if(ImGui::Selectable("Current selection",target_==0)) {target_=0;cancel_prediction();}ImGuiListClipper clipper;clipper.Begin(static_cast<int>(areas_.size()));while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {const auto id=areas_[i];const auto label="Area #"+std::to_string(id);if(ImGui::Selectable(label.c_str(),target_==id)) {target_=id;cancel_prediction();}if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",doc.annotation(id).name.c_str());}ui::EndCombo();}
        if(ImGui::CollapsingHeader("Training features and sampling")) {
            changed=ui::Checkbox("RGB channels",&options_.features.rgb) || changed;changed=ui::Checkbox("Optical density channels",&options_.features.optical_density) || changed;changed=ui::Checkbox("Stain channels",&options_.features.stains) || changed;
            changed=ui::Checkbox("Gradient magnitude",&options_.features.gradient_magnitude) || changed;changed=ui::Checkbox("Laplacian",&options_.features.laplacian) || changed;changed=ui::InputText("Scales (grid pixels)",scales_.data(),scales_.size()) || changed;
            changed=ui::Checkbox("Use calibration when available",&options_.calibrated) || changed;changed=ui::Checkbox("Balance classes",&options_.balanced) || changed;changed=ui::InputScalar("Pyramid level",ImGuiDataType_S32,&options_.level) || changed;
            std::uint64_t samples=options_.max_samples,per_class=options_.max_samples_per_class;if(ui::InputScalar("Total sample cap",ImGuiDataType_U64,&samples)) {options_.max_samples=static_cast<std::size_t>(samples);changed=true;}if(ui::InputScalar("Per-class sample cap",ImGuiDataType_U64,&per_class)) {options_.max_samples_per_class=static_cast<std::size_t>(per_class);changed=true;}
            changed=ui::InputScalar("Sample seed",ImGuiDataType_U64,&options_.seed) || changed;changed=ui::InputScalar("Maximum scanned pixels",ImGuiDataType_U64,&options_.max_scanned_pixels) || changed;
        }
        if(ImGui::CollapsingHeader("Forest and memory settings")) {changed=ui::InputScalar("Trees",ImGuiDataType_U32,&parameters_.trees) || changed;changed=ui::InputScalar("Maximum depth",ImGuiDataType_U32,&parameters_.max_depth) || changed;changed=ui::InputScalar("Minimum samples",ImGuiDataType_U32,&parameters_.min_samples) || changed;changed=ui::InputScalar("Forest seed",ImGuiDataType_U64,&parameters_.seed) || changed;std::uint64_t mib=options_.work_bytes/(1024*1024);if(ui::InputScalar("Work limit (MiB)",ImGuiDataType_U64,&mib)) {options_.work_bytes=std::clamp<std::uint64_t>(mib,1,1024)*1024*1024;prediction_options_.work_bytes=options_.work_bytes;parameters_.work_bytes=options_.work_bytes;changed=true;}}
        std::uint64_t preview_mib=preview_limit_/(1024*1024);
        if(ui::InputScalar("Preview limit (MiB)",ImGuiDataType_U64,&preview_mib)) {preview_limit_=std::clamp<std::uint64_t>(preview_mib,1,128)*1024*1024;preview_reset_=true;}
        if(changed) cancel_training();if(ui::InputDouble("Minimum tree vote",&prediction_options_.min_vote_fraction,0,0,"%.3f")) cancel_prediction();ImGui::EndDisabled();
        if(model_) {const auto resolution=model_->resolution();ImGui::TextWrapped("Model: %zu classes, %zu features, %u trees",model_->classes().size(),model_->features().size(),model_->forest().tree_count());if(resolution) {if(resolution->physical()) ImGui::TextWrapped("Sampling: %.6g x %.6g um",*resolution->pixel_width_um,*resolution->pixel_height_um);else ImGui::TextWrapped("Sampling: %.6g source pixels",resolution->downsample);}ImGui::TextWrapped("Prediction uses the model's saved feature recipe.");}
        if(auto table=training_.table();table && ImGui::CollapsingHeader("Training cohort")) {ImGui::Text("%zu rows; %llu conflicting pixels excluded",table->row_count(),static_cast<unsigned long long>(table->ambiguous_pixels()));if(ImGui::BeginTable("Pixel cohort",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)) {ImGui::TableSetupColumn("Class");ImGui::TableSetupColumn("Eligible");ImGui::TableSetupColumn("Used");ImGui::TableHeadersRow();for(std::size_t i=0;i<table->classes().size();++i) {ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(table->classes()[i].c_str());ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(table->eligible_class_pixels()[i]));ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(table->sampled_class_counts()[i]));}ImGui::EndTable();}}
        if(ImGui::CollapsingHeader("Contour settings")) {
            ImGui::BeginDisabled(disabled);bool contour_changed=ui::Checkbox("Clip contours to prediction area",&contour_options_.clip_to_roi);
            auto limit=[&](const char* label,std::size_t& value,std::uint64_t maximum) {std::uint64_t v=value;if(ui::InputScalar(label,ImGuiDataType_U64,&v)) {value=static_cast<std::size_t>(std::clamp<std::uint64_t>(v,1,maximum));contour_changed=true;}};
            limit("Maximum contour classes",contour_options_.max_components,1000000);limit("Maximum contour edges",contour_options_.max_edges,1000000);limit("Maximum contour vertices",contour_options_.max_vertices,1000000);
            std::uint64_t mib=contour_options_.work_bytes/(1024*1024);if(ui::InputScalar("Contour work limit (MiB)",ImGuiDataType_U64,&mib)) {contour_options_.work_bytes=std::clamp<std::uint64_t>(mib,1,512)*1024*1024;contour_changed=true;}
            if(contour_changed) clear_contours();ImGui::EndDisabled();
            ImGui::TextWrapped("One multipolygon annotation per class. Clipped output is parented to the prediction area. Raw footprints are root annotations.");
        }
        if(auto geometry=this->traced()) {const auto& c=*geometry->contours();ImGui::TextWrapped("%zu classes / %zu edges / %zu vertices / %.1f ms",c.components().size(),c.edge_count(),c.vertex_count(),c.elapsed_ms());}
        if(published_) ImGui::TextWrapped("Published %zu class annotations",published_ids_.size());
        if(model_ && ImGui::CollapsingHeader("Overlay colors",colors_.size()<=8?ImGuiTreeNodeFlags_DefaultOpen:0)) {bool palette=ui::SliderFloat("Opacity",&opacity_,0,1);for(std::size_t i=0;i<colors_.size();++i) {ImGui::PushID(static_cast<int>(i));const auto label="Class "+std::to_string(i+1)+" / "+model_->classes()[i];palette=ui::ColorEdit3(label.c_str(),colors_[i].data()) || palette;ImGui::PopID();}if(palette) make_palette();}
        if(auto result=prediction_.result()) {ImGui::TextWrapped("%lld x %lld / level %d / %.1f ms",static_cast<long long>(result->width()),static_cast<long long>(result->height()),result->level(),result->elapsed_ms());ImGui::TextWrapped("%llu classified pixels; preview %s",static_cast<unsigned long long>(result->classified_pixels()),preview_.ready()?"ready":"preparing");}
        ImGui::End();
    }
};
}
