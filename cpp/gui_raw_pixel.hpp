#pragma once
#include "gui_tool_catalog.hpp"
#include "gui_record_list.hpp"
#include "gui_result_state.hpp"
#include "gui_path_field.hpp"
#include "gui_output_gate.hpp"
#include "gui_class_palette.hpp"
#include "gui_record_removal.hpp"
#include "gui_raw_axis.hpp"
#include "raw_pixel_frontend.hpp"
#include "gui_pixel_preview.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include "utf8_text.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <locale>
namespace qpx::detail {
class GuiRawPixel {
    OutputGate* notice_gate_=nullptr;
    ui::PathField model_path_{{"Model path",ui::PathRole::OpenFile,{".qrm"},"",""}};
    ui::PathField model_output_{{"Export model path",ui::PathRole::NewFile,{".qrm"},"",""}};
    ui::PathField stream_path_{{"Prediction file",ui::PathRole::NewFile,{".qws"},"",""}};
    ui::PathField stream_input_{{"Open prediction file",ui::PathRole::OpenFile,{".qws"},"",""}};
    ui::PathField checkpoint_path_{{"Training checkpoint",ui::PathRole::NewFile,{".qtc"},"",""}};
    ui::PathField resume_training_{{"Resume training from",ui::PathRole::OpenFile,{".qtc"},"",""}};
    ui::PathField resume_stream_{{"Resume prediction from",ui::PathRole::OpenFile,{".qws"},"",""}};
    friend struct OutputWriterTestAccess;
    friend struct GuiRawPixelTestAccess;
    friend struct ModelSafetyTestAccess;
    bool model_unsaved_=false;
    std::shared_ptr<const RawPixelClassifier> saving_model_;
    friend struct RecordListTestAccess;
    std::uint64_t matched_record_=0,lookup_record_=0,lookup_revision_=0;
    bool lookup_valid_=false;
    ui::RecordRows record_rows_;
    GuiRawPixelOptions options_;RawPixelFrontend jobs_;GuiPixelPreview live_,history_;
    static constexpr auto first=GuiInputTarget::RawPixelTrain,last=GuiInputTarget::RawPixelApplyFile;
    std::array<Rect,std::size_t(last)-std::size_t(first)+1> targets_{};
    std::array<bool,std::size_t(last)-std::size_t(first)+1> enabled_{};
    std::array<char,256> x_{},y_{},offsets_{},scales_{},sigmas_{};
    std::weak_ptr<const char> identity_;
    std::array<char,257> contour_prefix_{};
    std::shared_ptr<const TissueContours> contour_preview_;
    std::vector<std::pair<std::uint32_t,std::vector<Ring>>> contour_paths_;
    std::size_t contour_bytes_=0;bool contour_ready_=false;std::string contour_error_;
    void update_contours(std::shared_ptr<const TissueContours> result) {
        if(result==contour_preview_) return;contour_preview_=std::move(result);decltype(contour_paths_){}.swap(contour_paths_);contour_bytes_=0;contour_ready_=false;contour_error_.clear();
        if(!contour_preview_) return;
        try {
            const auto count=contour_preview_->components().size();if(count>options_.preview_budget/sizeof(decltype(contour_paths_)::value_type)) throw std::length_error("Raw contour preview exceeds its byte budget");
            contour_paths_.reserve(count);contour_bytes_=contour_paths_.capacity()*sizeof(decltype(contour_paths_)::value_type);
            for(const auto& c:contour_preview_->components()) {
                if(c.geometry.payload_bytes()>options_.preview_budget-contour_bytes_) throw std::length_error("Raw contour preview exceeds its byte budget");
                auto paths=c.geometry.paths();std::size_t bytes=paths.capacity()*sizeof(Ring);for(const auto& path:paths) bytes+=path.capacity()*sizeof(Point);
                if(bytes>options_.preview_budget-contour_bytes_) throw std::length_error("Raw contour preview exceeds its byte budget");
                contour_paths_.emplace_back(c.label,std::move(paths));contour_bytes_+=bytes;
            }contour_ready_=true;
        }catch(const std::exception& e) {decltype(contour_paths_){}.swap(contour_paths_);contour_bytes_=0;contour_error_=e.what();}
    }
    void draw_contours(const Document& doc,const std::shared_ptr<const RawPixelClassifier>& model) const {
        if(!contour_ready_) {if(!contour_error_.empty()) ui::DataTextWrapped("%s",contour_error_.c_str());return;}
        const auto colors=palette(doc,std::vector<std::uint64_t>(model?model->forest().classes().size():0),model);
        if(ui::CollapsingHeader("Contour classes")) {for(std::size_t i=0;i<std::min<std::size_t>(contour_paths_.size(),16);++i) {const auto label=contour_paths_[i].first;if(!label || !model || label>colors.size()) continue;const auto color=colors[label-1];ImGui::PushStyleColor(ImGuiCol_Text,ImVec4((color&255)/255.f,((color>>8)&255)/255.f,((color>>16)&255)/255.f,1));ui::DataTextWrapped("%s",model->forest().classes()[label-1].c_str());ImGui::PopStyleColor();}if(contour_paths_.size()>16) ui::DataTextf("%zu further classes",contour_paths_.size()-16);}
        const ImVec2 a=ImGui::GetCursorScreenPos(),size{std::max(1.f,ImGui::GetContentRegionAvail().x),180};auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(a,{a.x+size.x,a.y+size.y},IM_COL32(24,24,24,255));draw->PushClipRect(a,{a.x+size.x,a.y+size.y},true);
        const auto report=jobs_.report();const auto width=report.historical_contours?report.historical_contours->link()->lattice().image_width():doc.metadata().width,height=report.historical_contours?report.historical_contours->link()->lattice().image_height():doc.metadata().height;
        const auto scale=std::min(double(size.x)/width,double(size.y)/height);
        for(const auto& [label,paths]:contour_paths_) for(const auto& ring:paths) {for(const auto& p:ring) draw->PathLineTo({a.x+float(p.x*scale),a.y+float(p.y*scale)});draw->PathStroke(label && label<=colors.size()?colors[label-1]|0xff000000:IM_COL32(192,192,192,255),1.5f,ImDrawFlags_Closed);}
        draw->PopClipRect();ImGui::Dummy(size);
    }
    std::string error_;std::int64_t px_=0,py_=0,pw_=256,ph_=256;std::uint64_t record_=0,index_=0;
    static void text(char* target,std::size_t size,const std::string& value) {bounded_utf8(value,size-1);std::memcpy(target,value.c_str(),value.size()+1);}
    static ScientificGridAxis axis(const char* value) {return gui_raw_axis(value);}
    static std::string axis_text(const ScientificGridAxis& a) {return std::to_string(a.origin())+','+std::to_string(a.extent())+','+std::to_string(a.samples())+','+std::to_string(a.context());}
    static std::vector<double> numbers(const char* value) {
        std::vector<double> out;std::istringstream in(value);in.imbue(std::locale::classic());std::string item;
        while(std::getline(in,item,',')) {double v;std::istringstream part(item);part.imbue(std::locale::classic());if(!(part>>v) || !std::isfinite(v)) throw std::invalid_argument("Enter finite comma-separated feature values");part>>std::ws;if(!part.eof() || out.size()>=256) throw std::invalid_argument("Invalid feature values");out.push_back(v);}if(*value && std::string(value).back()==',') throw std::invalid_argument("Trailing feature separator");return out;
    }
    static std::string values(const std::vector<double>& v) {std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);for(std::size_t i=0;i<v.size();++i) {if(i) s<<',';s<<v[i];}return s.str();}
    ScientificRawLattice lattice(const Document& doc) const {if(!*x_.data() && !*y_.data()) return raw_pixel_frontend_lattice(doc,{});if(!*x_.data() || !*y_.data()) throw std::invalid_argument("Set both raw axes or leave both empty for unit spacing");return {doc.metadata().width,doc.metadata().height,axis(x_.data()),axis(y_.data())};}
    template<class F> void run(F f) {try {error_.clear();f();}catch(const std::exception& e) {try {error_=e.what();bounded_utf8(error_,8192);}catch(...) {error_="Raw classifier operation failed";}}}
    std::vector<std::uint32_t> palette(const Document& doc,const std::vector<std::uint64_t>& counts,const std::shared_ptr<const RawPixelClassifier>& model) const {
        auto names=model?model->forest().classes():std::vector<std::string>{};names.resize(counts.size());
        auto out=class_palette(doc,names);for(auto& color:out) color=(color&0xffffffu)|(115u<<24);return out;
    }
public:
    std::vector<ui::PathField*> paths(){return {&model_path_,&model_output_,&stream_path_,&stream_input_,&checkpoint_path_,&resume_training_,&resume_stream_};}
    explicit GuiRawPixel(const GuiOptions& options):options_(options.raw_pixel?*options.raw_pixel:GuiRawPixelOptions{}) {
        options_.stream_contour_options.validate();options_.contours.validate();if(options_.contour_work_bytes<1024*1024 || options_.contour_work_bytes>1024ULL*1024*1024) throw std::invalid_argument("Raw contour inference work must be 1..1024 MiB");
        text(contour_prefix_.data(),contour_prefix_.size(),options_.contour_name_prefix);
        options_.training.validate();options_.stream.validate();options_.save.limits.validate();options_.read_limits.validate();if(!options_.preview_budget || options_.preview_budget>128u*1024*1024) throw std::invalid_argument("raw preview budget must be 1..128 MiB");
        model_path_.set_option(options_.model_path);model_output_.set_option(options_.model_path);stream_path_.set_option(options_.stream_path);stream_input_.set_option(options_.stream_path);checkpoint_path_.set_option(options_.training.checkpoint_path);resume_training_.set_option(options_.training.checkpoint.resume_path);resume_stream_.set_option(options_.save.resume_path);
        text(offsets_.data(),offsets_.size(),values(options_.training.features.offsets));text(scales_.data(),scales_.size(),values(options_.training.features.scales));std::vector<double> sigmas;for(const auto& v:options_.training.features.sigmas) {sigmas.push_back(v.x);sigmas.push_back(v.y);}text(sigmas_.data(),sigmas_.size(),values(sigmas));
        if(options_.training.lattice) {text(x_.data(),x_.size(),axis_text(options_.training.lattice->x()));text(y_.data(),y_.size(),axis_text(options_.training.lattice->y()));}
        if(options_.preview) {const auto& w=*options_.preview;px_=w.x().first_core_index();py_=w.y().first_core_index();pw_=w.core_width();ph_=w.core_height();text(x_.data(),x_.size(),axis_text(w.x().lattice()));text(y_.data(),y_.size(),axis_text(w.y().lattice()));}
        record_=options_.record_id;index_=options_.read_index;if(options_.model) jobs_.adopt(*options_.model);
    }
    void open() {options_.open=true;}
    void notifications(OutputGate* gate){notice_gate_=gate;}
    void update(Document& doc) {
        if(identity_.lock()!=doc.analysis_identity_.token) {identity_=doc.analysis_identity_.token;jobs_.reset_records();if(matched_record_ && record_==matched_record_)record_=0;matched_record_=0;lookup_valid_=false;}
        const bool was_busy=jobs_.busy();const auto before=jobs_.report();jobs_.poll(doc);const auto after=jobs_.report();
        if(was_busy && !jobs_.busy() && notice_gate_ && notice_gate_->notifications()) {
            const auto source=ui::command(ui::CommandId::Tool_RawPixelClassifier).caption;
            if(!notice_gate_->watching(source)) {
                const auto state=ui::parse_run_state(after.state);
                if(state==ui::RunState::Ready || state==ui::RunState::Verified)notice_gate_->notifications()->post({ui::Severity::Success,source,"The operation completed.",{},true});
                else if(state==ui::RunState::Failed || state==ui::RunState::Partial)notice_gate_->notifications()->post({ui::Severity::Error,source,after.error.empty()?"The operation failed.":after.error,{},after.error.empty()});
            }
        }
        if(after.model!=before.model)model_unsaved_=after.state=="trained";
        if(saving_model_ && !jobs_.busy()) {if(after.state=="saved" && after.model==saving_model_)model_unsaved_=false;saving_model_.reset();}
        // Only a selection previously resolved in this document may fall back.
        // An unknown scripted ID must keep its native error boundary.
        if(!lookup_valid_ || lookup_record_!=record_ || lookup_revision_!=doc.revision()) {
            if(record_)try {doc.raw_pixel_stream_record(record_);matched_record_=record_;}catch(const std::out_of_range&) {if(record_==matched_record_)record_=0;matched_record_=0;}
            lookup_record_=record_;lookup_revision_=doc.revision();lookup_valid_=true;
        }
        if(!record_ && doc.raw_pixel_stream_record_count())matched_record_=record_=doc.raw_pixel_stream_records().back()->id;
        if(after.stream_record)try {doc.raw_pixel_stream_record(after.stream_record);}catch(const std::out_of_range&) {jobs_.reset_records();}
        update_contours(after.historical_contours?after.historical_contours->contours():after.stream_contours?after.stream_contours->contours():after.contours?after.contours->contours():nullptr);
        live_.update(after.prediction,after.prediction?palette(doc,after.prediction->class_counts(),after.model):std::vector<std::uint32_t>{},options_.show_overlay,options_.preview_budget);
        history_.update(after.historical,after.historical?palette(doc,after.historical->class_counts(),after.historical_model):std::vector<std::uint32_t>{},true,options_.preview_budget);
    }
    bool settled() const {const auto r=jobs_.report();return !jobs_.busy() && (!options_.show_overlay || !r.prediction || live_.ready() || !live_.error().empty()) && (!r.historical || history_.ready() || !history_.error().empty());}
    GuiRawPixelReport report() const {auto r=jobs_.report();if(!error_.empty()) r.error=error_;else if(!live_.error().empty()) r.error=live_.error();r.preview_bytes=live_.bytes()+history_.bytes();r.preview_ready=live_.ready();r.contour_preview_ready=contour_ready_;r.contour_preview_bytes=contour_bytes_;if(r.error.empty()) r.error=contour_error_;return r;}
    const GuiPixelPreview& preview() const {return live_;}bool shown() const {return options_.show_overlay;}
    void draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {if(options_.show_overlay) live_.draw(draw,view,offset,size);}
    bool enabled(GuiInputTarget target) const {return enabled_.at(std::size_t(target)-std::size_t(first));}
    Rect bounds(GuiInputTarget target) const {return targets_.at(std::size_t(target)-std::size_t(first));}
    void finish(Document& doc,bool finite) {if(!finite) jobs_.cancel();jobs_.wait(doc);update(doc);}
    void draw(Document& doc,const ObjectSelection& selection,const Project* project,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark,RecordRemovalDialog* removal=nullptr,std::uint64_t epoch=0,OutputGate* gate=nullptr,const ui::PathContext& context={}) {
    ui::i18n::SurfaceTrace translation_surface("tool/Raw pixel classifier");
        notice_gate_=gate;
        targets_.fill({});enabled_.fill(false);if(!options_.open) return;
        ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::RawPixelClassifier),{480,900},ui::ToolHeight::Content);
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::RawPixelClassifier),&options_.open)) {ImGui::End();return;}
        auto item=[&](GuiInputTarget target,bool on=true) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const auto i=std::size_t(target)-std::size_t(first);targets_[i]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[i]=on;if(on) mark(target);};
        const auto current=jobs_.report();const bool disabled=blocked || !has_image || jobs_.busy();
        std::optional<GuiInputTarget> primary_action;
        if(!disabled) {
            if(current.file_replay && current.stream_contours && !current.contours_published && contour_ready_)primary_action=GuiInputTarget::RawPixelApplyFile;
            else if((current.contours || current.stream_contours) && !current.contours_published && contour_ready_)primary_action=GuiInputTarget::RawPixelApply;
            else if(current.model && current.prediction && has_geos() && !current.contours && !current.stream_contours)primary_action=GuiInputTarget::RawPixelTrace;
            else if(current.model)primary_action=GuiInputTarget::RawPixelPredict;
            else if(has_classifier_training())primary_action=GuiInputTarget::RawPixelTrain;
        }
        const auto availability=[&](const char* original)->ui::Availability {
            if(std::strcmp(original,"Cancel operation")==0) {

                return {};
            }
            if(std::strcmp(original,"Train raw classifier")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!has_classifier_training())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Keep raw training")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!jobs_.can_keep_training(doc))return {false,"Train a model first."};
                if(current.training_record)return {false,"Run again for a new result."};
                return {};
            }
            if(std::strcmp(original,"Inspect saved contours")==0) {
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Reverify saved contours for apply")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Apply reverified contours")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.file_replay)return {false,"Run a preview first."};
                if(!current.stream_contours)return {false,"Run a preview first."};
                if(current.contours_published)return {false,"Run again for a new result."};
                if(!contour_ready_)return {false,"Run a preview first."};
                return {};
            }
            if(std::strcmp(original,"Load raw model")==0) {
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                return {};
            }
            if(std::strcmp(original,"Save raw model")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.model)return {false,"Train or load a model first."};
                return {};
            }
            if(std::strcmp(original,"Predict raw window")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.model)return {false,"Train or load a model first."};
                return {};
            }
            if(std::strcmp(original,"Export raw prediction")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.model)return {false,"Train or load a model first."};
                return {};
            }
            if(std::strcmp(original,"Open and verify raw file")==0) {
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                return {};
            }
            if(std::strcmp(original,"Keep verified raw file")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.link)return {false,"Run a preview first."};
                if(current.stream_record)return {false,"Run again for a new result."};
                return {};
            }
            if(std::strcmp(original,"Open retained raw file")==0) {
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!has_image)return {false,"Open an image first."};
                if(!doc.raw_pixel_stream_record_count())return {false,"Select a kept result first."};
                return {};
            }
            if(std::strcmp(original,"Inspect retained contours")==0) {
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!doc.raw_pixel_stream_record_count())return {false,"Select a kept result first."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Reverify retained contours for apply")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!doc.raw_pixel_stream_record_count())return {false,"Select a kept result first."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Apply reverified retained contours")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.file_replay)return {false,"Run a preview first."};
                if(!current.stream_contours)return {false,"Run a preview first."};
                if(current.contours_published)return {false,"Run again for a new result."};
                if(!contour_ready_)return {false,"Run a preview first."};
                return {};
            }
            if(std::strcmp(original,"Remove raw file record")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!doc.raw_pixel_stream_record_count())return {false,"Select a kept result first."};
                return {};
            }
            if(std::strcmp(original,"Trace raw contours")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if(!current.model)return {false,"Train or load a model first."};
                if(!has_geos())return {false,"Use a build with this feature enabled."};
                return {};
            }
            if(std::strcmp(original,"Apply raw contours")==0) {
                if(blocked)return ui::WorkflowBlocked();
                if(!has_image)return {false,"Open an image first."};
                if(jobs_.busy())return {false,"Wait for the current work to finish."};
                if((!current.contours && !current.stream_contours))return {false,"Trace contours"};
                if(current.contours_published)return {false,"Run again for a new result."};
                if(!contour_ready_)return {false,"Trace contours"};
                return {};
            }
            return {};
        };
        auto button=[&](const char* label,GuiInputTarget target,std::optional<ui::Icon> icon={}) {
            const auto action=primary_action==target?ui::design::Action::Primary:ui::design::Action::Secondary;
            const auto available=availability(label);const bool hit=icon?ui::WorkflowIcon(label,*icon,available):ui::WorkflowAction(label,action,available);item(target,available.enabled);if(target==(current.model?GuiInputTarget::RawPixelPredict:GuiInputTarget::RawPixelTrain))ui::WorkflowReason(label,available);return hit;
        };
        ui::DataTextWrapped("%s",ui::caption(ui::parse_run_state(current.state),current.state).c_str());const auto shown=report();if(!shown.error.empty()) ui::DataTextWrapped("%s",shown.error.c_str());
        if(current.model) {
            const auto colors=class_palette(doc,current.model->forest().classes());
            for(std::size_t i=0;i<colors.size();++i) {const auto color=colors[i];ImGui::PushID(int(i));
                ImGui::ColorButton("##raw-class-color",{float(color&255)/255,float((color>>8)&255)/255,float((color>>16)&255)/255,1},ImGuiColorEditFlags_NoTooltip,{ImGui::GetFontSize(),ImGui::GetFontSize()});ImGui::SameLine();
                ui::DataTextWrapped("%s",current.model->forest().classes()[i].c_str());ImGui::PopID();}
        }
        ui::NativeToolCloseTarget("Hide panel",[&](Rect bounds) {
            const auto i=std::size_t(GuiInputTarget::RawPixelHide)-std::size_t(first);
            targets_[i]=bounds;enabled_[i]=true;mark(GuiInputTarget::RawPixelHide);
        });
        if(jobs_.busy() && button("Cancel operation",GuiInputTarget::RawPixelCancel,ui::Icon::Stop)) jobs_.cancel();
        if(ImGui::BeginTabBar("raw-pixel-tabs")) {
            bool tab=ImGui::BeginTabItem("Training");item(GuiInputTarget::RawPixelTrainingTab);if(tab) {
                ImGui::BeginDisabled(disabled);
                ui::Checkbox("Selected labelled areas only",&options_.selected_only);bool axes_changed=ui::InputText("X axis (O,E,N,C)",x_.data(),x_.size());axes_changed|=ui::InputText("Y axis (O,E,N,C)",y_.data(),y_.size());if(axes_changed) {jobs_.discard_contours();update_contours({});}ui::DataTextWrapped("Empty axes use unit level-zero samples. Exact spacing must match a loaded model.");
                int channel=int(options_.training.selection.first_channel),channels=int(options_.training.selection.channels),z=int(options_.training.selection.z),t=int(options_.training.selection.t);
                if(ui::InputInt("First channel",&channel)) options_.training.selection.first_channel=std::uint32_t(std::clamp(channel,0,65535));if(ui::InputInt("Channels",&channels)) options_.training.selection.channels=std::uint32_t(std::clamp(channels,1,128));
                if(ui::InputInt("Z",&z)) options_.training.selection.z=std::uint32_t(std::clamp(z,0,65535));if(ui::InputInt("Time",&t)) options_.training.selection.t=std::uint32_t(std::clamp(t,0,65535));
                if(ui::CollapsingHeader("Features and sampling")) {ui::InputText("Offsets",offsets_.data(),offsets_.size());ui::InputText("Scales",scales_.data(),scales_.size());ui::InputText("Sigma X,Y pairs",sigmas_.data(),sigmas_.size());ui::Checkbox("Gradient magnitude",&options_.training.features.gradient_magnitude);ui::Checkbox("Laplacian",&options_.training.features.laplacian);ui::Checkbox("Physical sigmas",&options_.training.physical_sigmas);int trees=int(options_.training.forest.trees);if(ui::InputInt("Trees",&trees)) options_.training.forest.trees=std::uint32_t(std::clamp(trees,1,4096));}
                checkpoint_path_.draw(context);resume_training_.draw(context);ImGui::EndDisabled();
                if(button("Train raw classifier",GuiInputTarget::RawPixelTrain)) run([&] {auto settings=options_.training;settings.lattice=lattice(doc);settings.features.offsets=numbers(offsets_.data());settings.features.scales=numbers(scales_.data());const auto sigmas=numbers(sigmas_.data());if(sigmas.empty() || sigmas.size()%2) throw std::invalid_argument("Enter paired raw feature sigmas");settings.features.sigmas.clear();for(std::size_t i=0;i<sigmas.size();i+=2) settings.features.sigmas.push_back({sigmas[i],sigmas[i+1]});settings.checkpoint_path=checkpoint_path_.state().resolved;settings.checkpoint.resume_path=resume_training_.state().resolved;std::vector<std::uint64_t> ids;if(options_.selected_only) {if(selection.size()>4096) throw std::length_error("Select at most 4096 areas");for(auto id:selection.ids()) {auto a=doc.annotation(id);if(a.geometry.is_area() && !a.class_name.empty()) ids.push_back(id);}}else for(const auto& a:doc.annotations()) if(a.geometry.is_area() && !a.class_name.empty()) {if(ids.size()==4096) throw std::length_error("Select at most 4096 labelled areas");ids.push_back(a.id);}if(settings.checkpoint_path.empty())jobs_.train(doc,ids,std::move(settings));
                    else if(output_gate_available(gate,error_)) {
                        const auto path=export_path(settings.checkpoint_path);guard_raw_pixel_frontend_output(project,path);
                        // A matching resume path extends the native checkpoint; it is not replacement.
                        const bool resume=!resume_training_.state().resolved.empty() && resume_training_.state().resolved==checkpoint_path_.state().resolved;
                        if(resume) {guard_output_intent(path,gate->protected_paths());jobs_.train(doc,ids,std::move(settings));gate->watch(path,[this]()->std::optional<bool>{if(jobs_.busy())return {};const auto status=jobs_.report();return status.state!="cancelled" && bool(status.checkpoint);},[this]{const auto status=jobs_.report();return status.state=="failed"?status.error:std::string{};},ui::command(ui::CommandId::Tool_RawPixelClassifier).caption);}
                        else gate->submit(path,output_scope("Training checkpoint",false,ui::command(ui::CommandId::Tool_RawPixelClassifier).caption),
                            [this,gate,project,document=&doc,ids,settings](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>&) mutable {
                                guard_raw_pixel_frontend_output(project,path);settings.checkpoint_path=path;settings.checkpoint.replace_existing=policy==ExportWritePolicy::ReplaceExisting;
                                jobs_.train(*document,ids,std::move(settings));
                                gate->watch(path,[this]()->std::optional<bool>{if(jobs_.busy())return {};const auto status=jobs_.report();return status.state!="cancelled" && bool(status.checkpoint);},[this]{const auto status=jobs_.report();return status.state=="failed"?status.error:std::string{};});
                            },[this](const std::string& message){error_=message;},settings.checkpoint.replace_existing);
                    }});
                if(button("Keep raw training",GuiInputTarget::RawPixelKeepTraining)) run([&] {jobs_.keep_training(doc);model_unsaved_=false;});
                if(current.training) ui::DataTextf("Training rows: %zu",current.training->tiled_table()->row_count());if(current.checkpoint) ui::DataTextf("Checkpoint windows: %llu / %llu",(unsigned long long)current.checkpoint->completed_windows(),(unsigned long long)current.checkpoint->total_windows());ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem("Preview and files");item(GuiInputTarget::RawPixelFilesTab);if(tab) {
                auto file_limits=options_.read_limits;file_limits.work_bytes=std::min(file_limits.work_bytes,options_.preview_budget);
                if(button("Inspect saved contours",GuiInputTarget::RawPixelTraceFile)) run([&] {jobs_.trace_file(doc,stream_input_.state().resolved,file_limits,options_.stream_contour_options);});
                if(button("Reverify saved contours for apply",GuiInputTarget::RawPixelReplayFile)) run([&] {jobs_.trace_file(doc,stream_input_.state().resolved,file_limits,options_.stream_contour_options,true);});
                const auto file_report=jobs_.report();if(button("Apply reverified contours",GuiInputTarget::RawPixelApplyFile)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(file_report.historical_contours || (file_report.file_replay && file_report.stream_contours)) {if(file_report.contours_published) ui::Text("Added %zu rechecked annotations",file_report.published_ids.size());else ui::TextWrapped(file_report.file_replay?"Rechecked against current pixels; ready to add together.":"Historical geometry only. Recheck current pixels before adding.");draw_contours(doc,file_report.historical_model);}
                if(model_unsaved_)ui::TextWrapped(ui::session_model_warning());
                model_path_.draw(context,jobs_.busy(),{},[&](bool){if(button("Load raw model",GuiInputTarget::RawPixelLoadModel,ui::Icon::Open)) run([&] {jobs_.load_model(model_path_.state().resolved);});});
                model_output_.draw(context,jobs_.busy(),{},[&](bool){if(button("Save raw model",GuiInputTarget::RawPixelSaveModel,ui::Icon::Save)) run([&] {if(!output_gate_available(gate,error_))return;guard_raw_pixel_frontend_output(project,model_output_.state().resolved);gate->submit(model_output_.state().resolved,output_scope("Raw pixel file",false,ui::command(ui::CommandId::Tool_RawPixelClassifier).caption),
                    [this,gate,project](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>&) {
                        guard_raw_pixel_frontend_output(project,path);jobs_.save_model(path,policy==ExportWritePolicy::ReplaceExisting);saving_model_=jobs_.report().model;
                        gate->watch(path,[this]()->std::optional<bool>{if(jobs_.busy())return {};return jobs_.report().state=="saved";},[this]{const auto status=jobs_.report();return status.state=="failed"?status.error:std::string{};});
                    },[this](const std::string& message){error_=message;},options_.replace_model);});});
                ui::number_value("First sample X",ImGuiDataType_S64,&px_);ui::number_value("First sample Y",ImGuiDataType_S64,&py_);ui::number_value("Preview width",ImGuiDataType_S64,&pw_);ui::number_value("Preview height",ImGuiDataType_S64,&ph_);
                if(button("Predict raw window",GuiInputTarget::RawPixelPredict)) run([&] {const auto grid=lattice(doc);if(px_<0 || py_<0 || px_>=grid.x().samples() || py_>=grid.y().samples()) throw std::invalid_argument("Preview starts outside the global sample lattice");const auto w=std::min(pw_,grid.x().samples()-px_),h=std::min(ph_,grid.y().samples()-py_);if(w<1 || h<1 || std::uint64_t(w)*std::uint64_t(h)>options_.preview_budget/9) throw std::length_error("Choose a smaller raw preview window");jobs_.predict(doc,*current.model,grid.window(px_,py_,w,h),options_.object_id?options_.object_id:selection.primary(),options_.stream.min_vote_fraction,options_.stream.work_bytes);});
                bool changed=ui::Checkbox("Show raw overlay",&options_.show_overlay);item(GuiInputTarget::RawPixelToggleOverlay);(void)changed;
                stream_path_.draw(context);stream_input_.draw(context);resume_stream_.draw(context);
                if(button("Export raw prediction",GuiInputTarget::RawPixelExport)) run([&] {if(!output_gate_available(gate,error_))return;auto save=options_.save;save.resume_path=resume_stream_.state().resolved;const auto path=export_path(stream_path_.state().resolved);guard_raw_pixel_frontend_output(project,path);
                    const auto grid=lattice(doc);const auto object=options_.object_id?options_.object_id:selection.primary();
                    const bool resume=!resume_stream_.state().resolved.empty() && resume_stream_.state().resolved==stream_path_.state().resolved;
                    if(resume) {guard_output_intent(path,gate->protected_paths());jobs_.save(doc,*current.model,grid,path,options_.stream,save,object);gate->watch(path,[this]()->std::optional<bool>{if(jobs_.busy())return {};return jobs_.report().state=="saved";},[this]{const auto status=jobs_.report();return status.state=="failed"?status.error:std::string{};},ui::command(ui::CommandId::Tool_RawPixelClassifier).caption);}
                    else if(output_gate_available(gate,error_))gate->submit(path,output_scope("Prediction file",false,ui::command(ui::CommandId::Tool_RawPixelClassifier).caption),
                        [this,gate,project,document=&doc,model=current.model,grid,stream=options_.stream,save,object](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>&) mutable {
                            guard_raw_pixel_frontend_output(project,path);save.replace_existing=policy==ExportWritePolicy::ReplaceExisting;jobs_.save(*document,*model,grid,path,stream,save,object);
                            gate->watch(path,[this]()->std::optional<bool>{if(jobs_.busy())return {};return jobs_.report().state=="saved";},[this]{const auto status=jobs_.report();return status.state=="failed"?status.error:std::string{};});
                        },[this](const std::string& message){error_=message;},save.replace_existing);});
                if(button("Open and verify raw file",GuiInputTarget::RawPixelOpenFile)) run([&] {auto limits=options_.read_limits;limits.work_bytes=std::min(limits.work_bytes,options_.preview_budget);jobs_.open(stream_input_.state().resolved,limits,index_);});
                ui::number_value("Historical block",ImGuiDataType_U64,&index_);ui::DataTextWrapped("File preview is historical; it does not grant live apply authority.");
                if(current.historical_model && current.link) {const auto& classes=current.historical_model->forest().classes();const auto colors=class_palette(doc,classes);
                    for(std::size_t i=0;i<std::min<std::size_t>(classes.size(),16);++i) {const auto color=colors[i];ImGui::PushID(int(i));ImGui::ColorButton("##historical-class-color",{float(color&255)/255,float((color>>8)&255)/255,float((color>>16)&255)/255,1},ImGuiColorEditFlags_NoTooltip,{ImGui::GetFontSize(),ImGui::GetFontSize()});ImGui::SameLine();ui::DataTextWrapped("Class %zu: %s (%llu samples)",i+1,classes[i].c_str(),(unsigned long long)current.link->summary().class_counts[i]);ImGui::PopID();}if(classes.size()>16) ui::DataTextf("%zu further classes",classes.size()-16);}
                if(history_.ready()) {const auto a=ImGui::GetCursorScreenPos();const ImVec2 size{std::max(1.f,ImGui::GetContentRegionAvail().x),140};history_.draw_local(ImGui::GetWindowDrawList(),a,size);ImGui::Dummy(size);}ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem(ui::i18n::label("Kept files###Retained files").c_str());item(GuiInputTarget::RawPixelRecordsTab);if(tab) {
                ui::DataTextf("Raw files: %zu",doc.raw_pixel_stream_record_count());record_rows_.refresh(&doc,doc.revision(),doc.raw_pixel_stream_record_count(),[&](auto& rows){for(const auto& r:doc.raw_pixel_stream_records())rows.push_back(ui::record_row(r->id,r->name,"Raw pixel file"));},doc.server().get());
                if(!record_ && !record_rows_.rows.empty())record_=record_rows_.rows.back().id;
                record_=ui::draw_record_list("Kept raw files###Record ID (0 = latest)",record_rows_,record_,{});
                auto selected=[&] {if(record_) return doc.raw_pixel_stream_record(record_);auto records=doc.raw_pixel_stream_records();if(records.empty()) throw std::logic_error("No raw file records");return records.back();};
                if(button("Keep verified raw file",GuiInputTarget::RawPixelKeepLink)) run([&] {jobs_.keep_link(doc);});
                if(button("Open retained raw file",GuiInputTarget::RawPixelOpenRecord)) run([&] {auto record=selected();auto limits=options_.read_limits;limits.work_bytes=std::min(limits.work_bytes,options_.preview_budget);jobs_.open("",limits,index_,record->link);});
                auto retained_limits=options_.read_limits;retained_limits.work_bytes=std::min(retained_limits.work_bytes,options_.preview_budget);
                if(button("Inspect retained contours",GuiInputTarget::RawPixelTraceFile)) run([&] {jobs_.trace_file(doc,"",retained_limits,options_.stream_contour_options,false,selected()->link);});
                if(button("Reverify retained contours for apply",GuiInputTarget::RawPixelReplayFile)) run([&] {jobs_.trace_file(doc,"",retained_limits,options_.stream_contour_options,true,selected()->link);});
                const auto retained_report=jobs_.report();if(button("Apply reverified retained contours",GuiInputTarget::RawPixelApplyFile)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(retained_report.historical_contours || (retained_report.file_replay && retained_report.stream_contours)) {if(retained_report.contours_published) ui::Text("Added %zu rechecked annotations",retained_report.published_ids.size());else if(retained_report.file_replay) ui::TextWrapped("Rechecked against current pixels; ready to add together.");else ui::TextWrapped("Historical geometry only; recheck before adding.");draw_contours(doc,retained_report.historical_model);}
                if(button("Remove raw file record",GuiInputTarget::RawPixelRemoveRecord)) run([&]{if(removal)removal->request_record(doc,epoch,"Raw pixel file",selected()->id,&Document::raw_pixel_stream_record,&Document::remove_raw_pixel_stream_record);});ui::DataTextWrapped("Removing a record leaves the external prediction file intact.");ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem("Contours");item(GuiInputTarget::RawPixelContoursTab);if(tab) {
                ui::DataTextWrapped("Trace from the current model and exact axes. Stream tiles for large targets. File previews cannot be applied. Apply adds one undoable edit.");
                ImGui::BeginDisabled(disabled);bool changed=ui::Checkbox("Stream tiled prediction",&options_.stream_contours);auto& active_contours=options_.stream_contours?options_.stream_contour_options.contours:options_.contours;changed|=ui::Checkbox("Clip contours to ROI",&active_contours.clip_to_roi);
                auto limit=[&](const char* label,std::size_t& value,std::uint64_t maximum) {std::uint64_t n=value;if(ui::InputScalar(label,ImGuiDataType_U64,&n)) {value=std::size_t(std::clamp<std::uint64_t>(n,1,maximum));changed=true;}};
                limit("Contour class limit",active_contours.max_components,1000000);limit("Contour edge limit",active_contours.max_edges,1000000);limit("Contour vertex limit",active_contours.max_vertices,1000000);
                limit("Inference work bytes",options_.contour_work_bytes,1024ULL*1024*1024);limit("Contour work bytes",active_contours.work_bytes,512ULL*1024*1024);
                if(options_.stream_contours && ui::CollapsingHeader("Stream limits")) {limit("Boundary run limit",options_.stream_contour_options.max_boundary_runs,1000000);limit("Boundary work bytes",options_.stream_contour_options.boundary_work_bytes,512ULL*1024*1024);limit("Total tracing work bytes",options_.stream_contour_options.work_bytes,1024ULL*1024*1024);}
                ui::InputText("Annotation prefix",contour_prefix_.data(),contour_prefix_.size());ImGui::EndDisabled();if(changed) {jobs_.discard_contours();update_contours({});}
                if(button("Trace raw contours",GuiInputTarget::RawPixelTrace)) run([&] {const auto grid=lattice(doc);const auto object=options_.object_id?options_.object_id:selection.primary();if(options_.stream_contours) {auto stream=options_.stream;stream.work_bytes=options_.contour_work_bytes;auto options=options_.stream_contour_options;jobs_.trace_stream(doc,*current.model,grid,stream,options,{},object);}else jobs_.trace(doc,*current.model,ScientificGrid(grid.image_width(),grid.image_height(),grid.x(),grid.y(),options_.contour_work_bytes),options_.contours,object,options_.stream.min_vote_fraction,options_.contour_work_bytes);});
                const auto contours=jobs_.report();if(button("Apply raw contours",GuiInputTarget::RawPixelApply)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(contours.contours || contours.stream_contours) {const auto& c=*(contours.stream_contours?contours.stream_contours->contours():contours.contours->contours());ui::DataTextf("%zu classes, %zu vertices",c.components().size(),c.vertex_count());draw_contours(doc,current.model);}if(contours.contours_published) ui::DataTextf("Applied %zu annotations",contours.published_ids.size());ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }ImGui::End();
    }
};
}
