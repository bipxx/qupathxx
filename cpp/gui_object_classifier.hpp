#pragma once
#include "qupathxx/object_classifier_archive.hpp"
#include "gui_layout.hpp"
#include "utf8_text.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <future>
#include <functional>

namespace qpx::detail {
class GuiObjectClassifier {
    ObjectFeatureOptions features_;
    ForestParameters parameters_;
    ObjectClassifierJob job_;
    ObjectClassifierStats stats_;
    std::shared_ptr<const ForestModel> model_;
    bool open_=false,selected_only_=false,unclassified_only_=true,replace_=false,trained_=false,applied_=false;
    double minimum_=.5,capture_ms_=0;
    std::size_t changed_=0;
    std::uint64_t adopted_=0,record_id_=0,selected_record_=0;
    bool record_loaded_=false,retained_=false;
    std::vector<std::uint64_t> captured_selection_;
    std::string error_,io_state_="idle",io_error_;
    std::array<char,4096> path_{};
    std::array<Rect,8> targets_{};
    struct IoResult {std::shared_ptr<const ForestModel> model;std::optional<ObjectFeatureOptions> features;std::string error;bool save=false;};
    std::future<IoResult> io_;
    std::atomic<bool> io_cancelled_{false};
    static std::size_t index(GuiInputTarget t) {return static_cast<std::size_t>(t)-static_cast<std::size_t>(GuiInputTarget::TrainObjectClassifier);}
    void changed_settings() {job_.cancel();captured_selection_.clear();error_.clear();applied_=false;}
    void poll_io() {
        if(io_.valid() && io_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {auto result=io_.get();
                if(io_cancelled_.load()) io_state_="cancelled";
                else if(!result.error.empty()) {io_state_="failed";io_error_=std::move(result.error);}
                else if(result.save) io_state_="saved";
                else {job_.cancel();model_=std::move(result.model);const auto budget=features_.snapshot_bytes;features_=*result.features;features_.snapshot_bytes=budget;io_state_="loaded";trained_=false;applied_=false;error_.clear();}
            } catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}
        }
    }
    std::vector<std::uint64_t> choose(const Document& doc,const ObjectSelection& selection,bool train) const {
        std::vector<std::uint64_t> ids;
        auto add=[&](const Annotation& a) {if(train?a.class_name.empty():(unclassified_only_ && !a.class_name.empty())) return;ids.push_back(a.id);};
        if(selected_only_) for(auto id:selection.ids()) add(doc.annotation(id));
        else for(const auto& [id,a]:doc.objects()) if(a.kind==ObjectKind::Cell) add(a);
        return ids;
    }
    void begin(const Document& doc,const ObjectSelection& selection,bool train) {
        job_.cancel();captured_selection_.clear();applied_=false;changed_=0;retained_=false;
        if(train) trained_=false;
        try {
            auto ids=choose(doc,selection,train);if(ids.empty()) throw std::invalid_argument(train?"No labelled objects in the chosen scope":"No prediction targets in the chosen scope");
            ObjectFeatureRequest request(doc,ids,features_);capture_ms_=request.capture_ms();
            captured_selection_=selected_only_?selection.ids():std::vector<std::uint64_t>{};
            if(train) {job_.train(request,parameters_);trained_=false;}
            else {if(!model_) throw std::logic_error("Train or load a model first");job_.predict(request,*model_);}
            applied_=false;changed_=0;error_.clear();
        } catch(const std::exception& e) {error_=e.what();}
    }
    void file(bool save) {
        if(io_.valid()) return;
        if(save && !model_) {io_error_="Train or load a model first";return;}
        auto path=std::string(path_.data());auto model=model_;const bool replace=replace_;
        io_cancelled_.store(false);io_state_=save?"saving":"loading";io_error_.clear();
        try {io_=std::async(std::launch::async,[this,path=std::move(path),model=std::move(model),save,replace] {
            IoResult result;result.save=save;
            try {if(save) save_forest_model(*model,path,replace,&io_cancelled_);else {result.model=std::make_shared<const ForestModel>(load_forest_model(path,&io_cancelled_));result.features=ObjectFeatureOptions::from_model(*result.model);}}
            catch(const std::exception& e) {result.error=e.what();}return result;
        });} catch(const std::exception& e) {io_state_="failed";io_error_=e.what();}
    }
public:
    explicit GuiObjectClassifier(const GuiOptions& options):features_(options.classifier_features?*options.classifier_features:ObjectFeatureOptions{}),parameters_(options.classifier_parameters?*options.classifier_parameters:ForestParameters{}),
        open_(options.classifier_open),selected_only_(options.classifier_selected_only),unclassified_only_(options.classifier_unclassified_only),replace_(options.classifier_replace_model),minimum_(options.classifier_min_vote_fraction) {
        selected_record_=options.classifier_record_id;
        if(!options.classifier_parameters) parameters_.max_depth=8;
        features_.validate();parameters_.validate();
        if(!std::isfinite(minimum_) || minimum_<0 || minimum_>1) throw std::invalid_argument("invalid GUI classifier minimum vote fraction");
        bounded_utf8(options.classifier_model_path,path_.size()-1);std::memcpy(path_.data(),options.classifier_model_path.data(),options.classifier_model_path.size());
        if(options.classifier_model) {model_=std::make_shared<const ForestModel>(*options.classifier_model);if(!options.classifier_features) features_=ObjectFeatureOptions::from_model(*model_);}
    }
    ~GuiObjectClassifier() {io_cancelled_.store(true);if(io_.valid()) io_.wait();}
    void open() {open_=true;}
    void update(const Document& doc,const ObjectSelection& selection) {
        poll_io();
        if(selected_only_ && !captured_selection_.empty() && captured_selection_!=selection.ids()) {job_.cancel();captured_selection_.clear();applied_=false;}
        stats_=job_.poll(doc);
        if(stats_.state=="ready" && !job_.result() && job_.model() && adopted_!=stats_.generation) {model_=job_.model();adopted_=stats_.generation;trained_=true;}
    }
    Rect bounds(GuiInputTarget t) const {return targets_.at(index(t));}
    bool trained() const {return trained_ && !job_.busy();}
    bool predicted() const {return stats_.state=="ready" && bool(job_.result());}
    bool applied() const {return applied_;}
    bool retained() const {return retained_;}
    bool record_loaded() const {return record_loaded_;}
    std::uint64_t record_id() const {return record_id_;}
    const std::string& io_state() const {return io_state_;}
    const std::string& io_error() const {return io_error_;}
    std::string error() const {return error_.empty()?stats_.error:error_;}
    const ObjectClassifierStats& stats() const {return stats_;}
    std::shared_ptr<const ForestModel> model() const {return model_;}
    std::shared_ptr<const ObjectClassificationResult> result() const {return job_.result();}
    std::size_t changed() const {return changed_;}
    double capture_ms() const {return capture_ms_;}
    void finish(const Document& doc,const ObjectSelection& selection,bool finite) {
        if(!finite) {job_.cancel();io_cancelled_.store(true);}
        if(job_.busy()) job_.wait(doc);
        if(io_.valid()) io_.wait();
        update(doc,selection);
    }
    void draw(Document& doc,const ObjectSelection& selection,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark) {
        const auto& io=ImGui::GetIO();
        if(!io.WantTextInput && !blocked && ImGui::IsKeyPressed(ImGuiKey_F4,false)) open_=!open_;
        if(!open_) return;
        const auto* viewport=ImGui::GetMainViewport();const auto size=viewport->WorkSize;const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(620.f*scale,size.x*.88f),std::min(720.f*scale,size.y*.88f)},ImGuiCond_Appearing);
        ImGui::SetNextWindowPos({viewport->WorkPos.x+size.x*.5f,viewport->WorkPos.y+size.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
        ImGui::SetNextWindowSizeConstraints({std::min(260.f*scale,size.x),std::min(160.f*scale,size.y)},size);
        if(!ImGui::Begin("Object classifiers",&open_)) {ImGui::End();return;}
        auto button=[&](const char* label,GuiInputTarget target,bool disabled) {
            ImGui::BeginDisabled(disabled);const bool clicked=ui::Button(label);
            const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();targets_[index(target)]={lo.x,lo.y,hi.x-lo.x,hi.y-lo.y};mark(target);ImGui::EndDisabled();return clicked;
        };
        ImGui::BeginDisabled(job_.busy() || io_.valid() || blocked);
        bool changed=ui::Checkbox("Use selected objects (otherwise all cells)",&selected_only_);
        changed=ui::Checkbox("Predict unclassified objects only",&unclassified_only_) || changed;
        changed=ui::Checkbox("Use calibration when available",&features_.calibrated) || changed;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Unknown calibration uses pixel units. Prediction requires matching model feature units.");
        changed=ui::Checkbox("Include nucleus shape",&features_.nucleus_shape) || changed;
        changed=ui::Checkbox("Include verified cell and nucleus RGB",&features_.cell_rgb) || changed;
        if(!features_.cell_rgb) features_.cytoplasm_rgb=false;
        ImGui::BeginDisabled(!features_.cell_rgb);changed=ui::Checkbox("Include cytoplasm RGB",&features_.cytoplasm_rgb) || changed;ImGui::EndDisabled();
        if(ImGui::CollapsingHeader("Training settings")) {
            ui::SetNextItemWidth(120);changed=ui::InputScalar("Trees",ImGuiDataType_U32,&parameters_.trees) || changed;
            ui::SetNextItemWidth(120);changed=ui::InputScalar("Maximum depth",ImGuiDataType_U32,&parameters_.max_depth) || changed;
            ui::SetNextItemWidth(120);changed=ui::InputScalar("Minimum samples",ImGuiDataType_U32,&parameters_.min_samples) || changed;
            ui::SetNextItemWidth(120);changed=ui::InputScalar("Active features (0 auto)",ImGuiDataType_U32,&parameters_.active_features) || changed;
            ui::SetNextItemWidth(160);changed=ui::InputScalar("Seed",ImGuiDataType_U64,&parameters_.seed) || changed;
            auto mib=parameters_.work_bytes/(1024*1024);ui::SetNextItemWidth(120);
            if(ui::InputScalar("Work limit (MiB)",ImGuiDataType_U64,&mib)) {parameters_.work_bytes=std::clamp<std::uint64_t>(mib,1,1024)*1024*1024;changed=true;}
        }
        ImGui::EndDisabled();if(changed) changed_settings();
        auto unavailable=[&]{return blocked || !has_image || job_.busy() || io_.valid();};
        if(button("Train from labelled objects",GuiInputTarget::TrainObjectClassifier,unavailable() || !has_classifier_training())) begin(doc,selection,true);
        if(stats_.state=="ready" && !job_.result() && button("Keep training record in project",GuiInputTarget::KeepClassifierTraining,unavailable())) {
            try {record_id_=job_.retain_training(doc);selected_record_=record_id_;retained_=true;stats_=job_.poll(doc);error_.clear();}catch(const std::exception& e) {error_=e.what();}
        }
        if(!has_classifier_training()) ImGui::TextWrapped("Training is unavailable in this build. Load a model to predict.");
        if(button("Predict object classes",GuiInputTarget::PredictObjectClasses,unavailable() || !model_)) begin(doc,selection,false);
        ui::SetNextItemWidth(120);ui::InputDouble("Minimum tree vote fraction",&minimum_,0,0,"%.3f");
        ImGui::TextWrapped("Tree vote fractions are not calibrated probabilities.");
        if(button("Apply predictions (one Undo)",GuiInputTarget::ApplyObjectClasses,unavailable() || !predicted())) {
            try {changed_=job_.apply(doc,minimum_).size();record_id_=doc.revision();stats_=job_.poll(doc);applied_=true;error_.clear();}catch(const std::exception& e) {error_=e.what();}
        }
        if(button("Cancel classifier work",GuiInputTarget::CancelObjectClassifier,!job_.busy() && !io_.valid())) {job_.cancel();io_cancelled_.store(true);applied_=false;}
        if(job_.busy()) ImGui::Text("%s: %llu / %llu feature rows",stats_.phase.c_str(),static_cast<unsigned long long>(stats_.completed_rows),static_cast<unsigned long long>(stats_.total_rows));
        if(applied_) ImGui::Text("Changed %zu objects (Undo available)",changed_);
        if(!error().empty()) ImGui::TextWrapped("%s",error().c_str());
        if(model_) {
            ImGui::SeparatorText("Active model");ImGui::Text("%llu training rows / %u trees / %zu features",static_cast<unsigned long long>(model_->training_rows()),model_->tree_count(),model_->features().size());
            if(ImGui::CollapsingHeader("Model details")) {
                ImGui::TextWrapped("%s",model_->backend().c_str());
                for(const auto& feature:model_->features()) ImGui::TextWrapped("%s [%s]",feature.name.c_str(),feature.unit.c_str());
                ImGui::TextWrapped("Training input: %s",model_->training_sha256().c_str());
            }
        }
        ui::InputText("Model file",path_.data(),path_.size());ui::Checkbox("Replace an existing model file",&replace_);
        const bool file_busy=blocked || job_.busy() || io_.valid();
        if(button("Save model",GuiInputTarget::SaveClassifierModel,file_busy || !model_ || !path_[0])) file(true);
        if(button("Load model",GuiInputTarget::LoadClassifierModel,file_busy || !path_[0])) file(false);
        if(io_state_!="idle") ImGui::Text("Model file: %s",io_state_.c_str());
        if(!io_error_.empty()) ImGui::TextWrapped("%s",io_error_.c_str());
        if(doc.classifier_record_count() && ImGui::CollapsingHeader("Project classifier records",!model_?ImGuiTreeNodeFlags_DefaultOpen:0)) {
        const auto records=doc.classifier_records();std::shared_ptr<const ObjectClassifierRecord> selected;
        for(const auto& r:records) if(r->id==selected_record_) selected=r;
        if(!selected && !records.empty()) {selected=records.back();selected_record_=selected->id;}
        if(!records.empty()) {
            if(ui::BeginCombo("Saved classifier record",selected?selected->name.c_str():"Select record")) {
                ImGuiListClipper clipper;clipper.Begin(static_cast<int>(records.size()));while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                    const auto& record=records[i];ImGui::PushID(static_cast<int>(i));const auto label=record->name+" (#"+std::to_string(record->id)+")";if(ImGui::Selectable(label.c_str(),record->id==selected_record_)) selected_record_=record->id;ImGui::PopID();
                }ui::EndCombo();
            }
            if(selected) ImGui::Text("Record %llu / %s / %zu feature rows",static_cast<unsigned long long>(selected->id),selected->archive->purpose().c_str(),selected->archive->table()->object_ids().size());
        }
        if(button("Use saved classifier model",GuiInputTarget::UseClassifierRecord,unavailable() || !selected)) {
            try {job_.cancel();model_=selected->archive->model();const auto budget=features_.snapshot_bytes;features_=ObjectFeatureOptions::from_model(*model_);features_.snapshot_bytes=budget;record_id_=selected->id;record_loaded_=true;trained_=false;applied_=false;error_.clear();}catch(const std::exception& e) {error_=e.what();}
        }
        ImGui::TextWrapped("Applied predictions keep an audit record. Undo restores labels and keeps the record.");
        }

        if(auto result=job_.result()) {
            const auto& ids=result->table()->object_ids();const auto& predictions=result->predictions();
            ImGui::Text("Preview: %zu objects; snapshot %.3f ms",ids.size(),capture_ms_);
            if(ImGui::BeginTable("Classifier preview",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,ImGui::GetTextLineHeightWithSpacing()*7})) {
                ImGui::TableSetupColumn("Object");ImGui::TableSetupColumn("Class");ImGui::TableSetupColumn("Tree vote");ImGui::TableHeadersRow();
                ImGuiListClipper clipper;clipper.Begin(static_cast<int>(ids.size()));while(clipper.Step()) for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
                    const auto& p=predictions[row];ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(ids[row]));
                    ImGui::TableNextColumn();const auto& name=result->model()->classes().at(p.class_index);ImGui::TextUnformatted(name.c_str());if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",name.c_str());ImGui::TableNextColumn();ImGui::Text("%.3f",p.vote_fraction);
                }ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};
}
