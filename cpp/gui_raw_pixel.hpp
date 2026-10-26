#pragma once
#include "gui_raw_axis.hpp"
#include "raw_pixel_frontend.hpp"
#include "gui_pixel_preview.hpp"
#include "gui_layout.hpp"
#include "utf8_text.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <locale>
namespace qpx::detail {
class GuiRawPixel {
    GuiRawPixelOptions options_;RawPixelFrontend jobs_;GuiPixelPreview live_,history_;
    static constexpr auto first=GuiInputTarget::RawPixelTrain,last=GuiInputTarget::RawPixelApplyFile;
    std::array<Rect,std::size_t(last)-std::size_t(first)+1> targets_{};
    std::array<bool,std::size_t(last)-std::size_t(first)+1> enabled_{};
    std::array<char,32769> model_path_{},stream_path_{},checkpoint_path_{},resume_training_{},resume_stream_{};
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
        if(!contour_ready_) {if(!contour_error_.empty()) ImGui::TextWrapped("%s",contour_error_.c_str());return;}
        const auto colors=palette(doc,std::vector<std::uint64_t>(model?model->forest().classes().size():0),model);
        if(ImGui::CollapsingHeader("Contour classes")) {for(std::size_t i=0;i<std::min<std::size_t>(contour_paths_.size(),16);++i) {const auto label=contour_paths_[i].first;if(!label || !model || label>colors.size()) continue;const auto color=colors[label-1];ImGui::PushStyleColor(ImGuiCol_Text,ImVec4((color&255)/255.f,((color>>8)&255)/255.f,((color>>16)&255)/255.f,1));ImGui::TextWrapped("%s",model->forest().classes()[label-1].c_str());ImGui::PopStyleColor();}if(contour_paths_.size()>16) ImGui::Text("%zu further classes",contour_paths_.size()-16);}
        const ImVec2 a=ImGui::GetCursorScreenPos(),size{std::max(1.f,ImGui::GetContentRegionAvail().x),180};auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(a,{a.x+size.x,a.y+size.y},IM_COL32(24,24,24,255));draw->PushClipRect(a,{a.x+size.x,a.y+size.y},true);
        const auto report=jobs_.report();const auto width=report.historical_contours?report.historical_contours->link()->lattice().image_width():doc.metadata().width,height=report.historical_contours?report.historical_contours->link()->lattice().image_height():doc.metadata().height;
        const auto scale=std::min(double(size.x)/width,double(size.y)/height);
        for(const auto& [label,paths]:contour_paths_) for(const auto& ring:paths) {for(const auto& p:ring) draw->PathLineTo({a.x+float(p.x*scale),a.y+float(p.y*scale)});draw->PathStroke(label && label<=colors.size()?colors[label-1]|0xff000000:IM_COL32(192,192,192,255),ImDrawFlags_Closed,1.5f);}
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
    template<class F> void run(F f) {try {f();error_.clear();}catch(const std::exception& e) {try {error_=e.what();bounded_utf8(error_,8192);}catch(...) {error_="Raw classifier operation failed";}}}
    std::vector<std::uint32_t> palette(const Document& doc,const std::vector<std::uint64_t>& counts,const std::shared_ptr<const RawPixelClassifier>& model) const {
        static constexpr std::uint32_t defaults[]={0x4c78a8,0xf58518,0xe45756,0x72b7b2,0x54a24b,0xeeca3b,0xb279a2,0xff9da6};std::vector<std::uint32_t> out;
        for(std::size_t i=0;i<counts.size();++i) {auto rgb=defaults[i%8];if(model && i<model->forest().classes().size()) {const auto found=doc.classes().find(model->forest().classes()[i]);if(found!=doc.classes().end()) rgb=found->second.color;}out.push_back(((rgb>>16)&255)|(rgb&0xff00)|((rgb&255)<<16)|(std::uint32_t(115)<<24));}return out;
    }
public:
    explicit GuiRawPixel(const GuiOptions& options):options_(options.raw_pixel?*options.raw_pixel:GuiRawPixelOptions{}) {
        options_.stream_contour_options.validate();options_.contours.validate();if(options_.contour_work_bytes<1024*1024 || options_.contour_work_bytes>1024ULL*1024*1024) throw std::invalid_argument("Raw contour inference work must be 1..1024 MiB");
        text(contour_prefix_.data(),contour_prefix_.size(),options_.contour_name_prefix);
        options_.training.validate();options_.stream.validate();options_.save.limits.validate();options_.read_limits.validate();if(!options_.preview_budget || options_.preview_budget>128u*1024*1024) throw std::invalid_argument("raw preview budget must be 1..128 MiB");
        text(model_path_.data(),model_path_.size(),options_.model_path);text(stream_path_.data(),stream_path_.size(),options_.stream_path);text(checkpoint_path_.data(),checkpoint_path_.size(),options_.training.checkpoint_path);text(resume_training_.data(),resume_training_.size(),options_.training.checkpoint.resume_path);text(resume_stream_.data(),resume_stream_.size(),options_.save.resume_path);
        text(offsets_.data(),offsets_.size(),values(options_.training.features.offsets));text(scales_.data(),scales_.size(),values(options_.training.features.scales));std::vector<double> sigmas;for(const auto& v:options_.training.features.sigmas) {sigmas.push_back(v.x);sigmas.push_back(v.y);}text(sigmas_.data(),sigmas_.size(),values(sigmas));
        if(options_.training.lattice) {text(x_.data(),x_.size(),axis_text(options_.training.lattice->x()));text(y_.data(),y_.size(),axis_text(options_.training.lattice->y()));}
        if(options_.preview) {const auto& w=*options_.preview;px_=w.x().first_core_index();py_=w.y().first_core_index();pw_=w.core_width();ph_=w.core_height();text(x_.data(),x_.size(),axis_text(w.x().lattice()));text(y_.data(),y_.size(),axis_text(w.y().lattice()));}
        record_=options_.record_id;index_=options_.read_index;if(options_.model) jobs_.adopt(*options_.model);
    }
    void open() {options_.open=true;}
    void update(Document& doc) {if(identity_.lock()!=doc.analysis_identity_.token) {identity_=doc.analysis_identity_.token;jobs_.reset_records();record_=0;}jobs_.poll(doc);const auto report=jobs_.report();update_contours(report.historical_contours?report.historical_contours->contours():report.stream_contours?report.stream_contours->contours():report.contours?report.contours->contours():nullptr);live_.update(report.prediction,report.prediction?palette(doc,report.prediction->class_counts(),report.model):std::vector<std::uint32_t>{},options_.show_overlay,options_.preview_budget);history_.update(report.historical,report.historical?palette(doc,report.historical->class_counts(),report.historical_model):std::vector<std::uint32_t>{},true,options_.preview_budget);}
    bool settled() const {const auto r=jobs_.report();return !jobs_.busy() && (!options_.show_overlay || !r.prediction || live_.ready() || !live_.error().empty()) && (!r.historical || history_.ready() || !history_.error().empty());}
    GuiRawPixelReport report() const {auto r=jobs_.report();if(!error_.empty()) r.error=error_;else if(!live_.error().empty()) r.error=live_.error();r.preview_bytes=live_.bytes()+history_.bytes();r.preview_ready=live_.ready();r.contour_preview_ready=contour_ready_;r.contour_preview_bytes=contour_bytes_;if(r.error.empty()) r.error=contour_error_;return r;}
    const GuiPixelPreview& preview() const {return live_;}bool shown() const {return options_.show_overlay;}
    void draw_preview(ImDrawList* draw,const Viewport& view,ImVec2 offset,ImVec2 size) const {if(options_.show_overlay) live_.draw(draw,view,offset,size);}
    bool enabled(GuiInputTarget target) const {return enabled_.at(std::size_t(target)-std::size_t(first));}
    Rect bounds(GuiInputTarget target) const {return targets_.at(std::size_t(target)-std::size_t(first));}
    void finish(Document& doc,bool finite) {if(!finite) jobs_.cancel();jobs_.wait(doc);update(doc);}
    void draw(Document& doc,const ObjectSelection& selection,const Project* project,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark) {
        targets_.fill({});enabled_.fill(false);if(!options_.open) return;ImGui::SetNextWindowSize({480,700},ImGuiCond_FirstUseEver);
        if(!ImGui::Begin("Raw pixel classifier",&options_.open)) {ImGui::End();return;}
        auto item=[&](GuiInputTarget target,bool on=true) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const auto i=std::size_t(target)-std::size_t(first);targets_[i]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[i]=on;if(on) mark(target);};
        auto button=[&](const char* label,GuiInputTarget target,bool disabled=false) {ImGui::BeginDisabled(disabled);const bool hit=ui::Button(label);item(target,!disabled);ImGui::EndDisabled();return hit;};
        const auto current=jobs_.report();const bool disabled=blocked || !has_image || jobs_.busy();
        ImGui::TextWrapped("%s",current.state.c_str());const auto shown=report();if(!shown.error.empty()) ImGui::TextWrapped("%s",shown.error.c_str());
        if(button("Cancel operation",GuiInputTarget::RawPixelCancel,!jobs_.busy())) jobs_.cancel();ImGui::SameLine();if(button("Hide panel",GuiInputTarget::RawPixelHide)) options_.open=false;
        if(ImGui::BeginTabBar("raw-pixel-tabs")) {
            bool tab=ImGui::BeginTabItem("Training");item(GuiInputTarget::RawPixelTrainingTab);if(tab) {
                ImGui::BeginDisabled(disabled);
                ImGui::Checkbox("Selected labelled areas only",&options_.selected_only);bool axes_changed=ui::InputText("X axis (O,E,N,C)",x_.data(),x_.size());axes_changed|=ui::InputText("Y axis (O,E,N,C)",y_.data(),y_.size());if(axes_changed) {jobs_.discard_contours();update_contours({});}ImGui::TextWrapped("Empty axes use unit level-zero samples. Exact spacing must match a loaded model.");
                int channel=int(options_.training.selection.first_channel),channels=int(options_.training.selection.channels),z=int(options_.training.selection.z),t=int(options_.training.selection.t);
                if(ui::InputInt("First channel",&channel)) options_.training.selection.first_channel=std::uint32_t(std::clamp(channel,0,65535));if(ui::InputInt("Channels",&channels)) options_.training.selection.channels=std::uint32_t(std::clamp(channels,1,128));
                if(ui::InputInt("Z",&z)) options_.training.selection.z=std::uint32_t(std::clamp(z,0,65535));if(ui::InputInt("Time",&t)) options_.training.selection.t=std::uint32_t(std::clamp(t,0,65535));
                if(ImGui::CollapsingHeader("Features and sampling")) {ui::InputText("Offsets",offsets_.data(),offsets_.size());ui::InputText("Scales",scales_.data(),scales_.size());ui::InputText("Sigma X,Y pairs",sigmas_.data(),sigmas_.size());ImGui::Checkbox("Gradient magnitude",&options_.training.features.gradient_magnitude);ImGui::Checkbox("Laplacian",&options_.training.features.laplacian);ImGui::Checkbox("Physical sigmas",&options_.training.physical_sigmas);int trees=int(options_.training.forest.trees);if(ui::InputInt("Trees",&trees)) options_.training.forest.trees=std::uint32_t(std::clamp(trees,1,4096));}
                ui::InputText("Training checkpoint",checkpoint_path_.data(),checkpoint_path_.size());ui::InputText("Resume training from",resume_training_.data(),resume_training_.size());ImGui::Checkbox("Replace checkpoint",&options_.training.checkpoint.replace_existing);ImGui::EndDisabled();
                if(button("Train raw classifier",GuiInputTarget::RawPixelTrain,disabled || !has_classifier_training())) run([&] {auto settings=options_.training;settings.lattice=lattice(doc);settings.features.offsets=numbers(offsets_.data());settings.features.scales=numbers(scales_.data());const auto sigmas=numbers(sigmas_.data());if(sigmas.empty() || sigmas.size()%2) throw std::invalid_argument("Enter paired raw feature sigmas");settings.features.sigmas.clear();for(std::size_t i=0;i<sigmas.size();i+=2) settings.features.sigmas.push_back({sigmas[i],sigmas[i+1]});settings.checkpoint_path=checkpoint_path_.data();settings.checkpoint.resume_path=resume_training_.data();if(!settings.checkpoint_path.empty()) guard_raw_pixel_frontend_output(project,settings.checkpoint_path);std::vector<std::uint64_t> ids;if(options_.selected_only) {if(selection.size()>4096) throw std::length_error("Select at most 4096 areas");for(auto id:selection.ids()) {auto a=doc.annotation(id);if(a.geometry.is_area() && !a.class_name.empty()) ids.push_back(id);}}else for(const auto& a:doc.annotations()) if(a.geometry.is_area() && !a.class_name.empty()) {if(ids.size()==4096) throw std::length_error("Select at most 4096 labelled areas");ids.push_back(a.id);}jobs_.train(doc,ids,std::move(settings));});
                if(button("Keep raw training",GuiInputTarget::RawPixelKeepTraining,disabled || !jobs_.can_keep_training(doc) || current.training_record)) run([&] {jobs_.keep_training(doc);});
                if(current.training) ImGui::Text("Training rows: %zu",current.training->tiled_table()->row_count());if(current.checkpoint) ImGui::Text("Checkpoint windows: %llu / %llu",(unsigned long long)current.checkpoint->completed_windows(),(unsigned long long)current.checkpoint->total_windows());ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem("Preview and files");item(GuiInputTarget::RawPixelFilesTab);if(tab) {
                ImGui::BeginDisabled(jobs_.busy());ui::InputText("Model path",model_path_.data(),model_path_.size());ImGui::Checkbox("Replace model",&options_.replace_model);ImGui::EndDisabled();
                if(button("Load raw model",GuiInputTarget::RawPixelLoadModel,jobs_.busy())) run([&] {jobs_.load_model(model_path_.data());});ImGui::SameLine();if(button("Save raw model",GuiInputTarget::RawPixelSaveModel,disabled || !current.model)) run([&] {guard_raw_pixel_frontend_output(project,model_path_.data());jobs_.save_model(model_path_.data(),options_.replace_model);});
                ImGui::InputScalar("First sample X",ImGuiDataType_S64,&px_);ImGui::InputScalar("First sample Y",ImGuiDataType_S64,&py_);ImGui::InputScalar("Preview width",ImGuiDataType_S64,&pw_);ImGui::InputScalar("Preview height",ImGuiDataType_S64,&ph_);
                if(button("Predict raw window",GuiInputTarget::RawPixelPredict,disabled || !current.model)) run([&] {const auto grid=lattice(doc);if(px_<0 || py_<0 || px_>=grid.x().samples() || py_>=grid.y().samples()) throw std::invalid_argument("Preview starts outside the global sample lattice");const auto w=std::min(pw_,grid.x().samples()-px_),h=std::min(ph_,grid.y().samples()-py_);if(w<1 || h<1 || std::uint64_t(w)*std::uint64_t(h)>options_.preview_budget/9) throw std::length_error("Choose a smaller raw preview window");jobs_.predict(doc,*current.model,grid.window(px_,py_,w,h),options_.object_id?options_.object_id:selection.primary(),options_.stream.min_vote_fraction,options_.stream.work_bytes);});
                bool changed=ImGui::Checkbox("Show raw overlay",&options_.show_overlay);item(GuiInputTarget::RawPixelToggleOverlay);(void)changed;
                ui::InputText("Prediction file",stream_path_.data(),stream_path_.size());ui::InputText("Resume prediction from",resume_stream_.data(),resume_stream_.size());ImGui::Checkbox("Replace prediction file",&options_.save.replace_existing);
                if(button("Export raw prediction",GuiInputTarget::RawPixelExport,disabled || !current.model)) run([&] {guard_raw_pixel_frontend_output(project,stream_path_.data());auto save=options_.save;save.resume_path=resume_stream_.data();jobs_.save(doc,*current.model,lattice(doc),stream_path_.data(),options_.stream,save,options_.object_id?options_.object_id:selection.primary());});
                if(button("Open and verify raw file",GuiInputTarget::RawPixelOpenFile,jobs_.busy())) run([&] {auto limits=options_.read_limits;limits.work_bytes=std::min(limits.work_bytes,options_.preview_budget);jobs_.open(stream_path_.data(),limits,index_);});
                auto file_limits=options_.read_limits;file_limits.work_bytes=std::min(file_limits.work_bytes,options_.preview_budget);
                if(button("Inspect saved contours",GuiInputTarget::RawPixelTraceFile,jobs_.busy() || !has_geos())) run([&] {jobs_.trace_file(doc,stream_path_.data(),file_limits,options_.stream_contour_options);});
                if(button("Reverify saved contours for apply",GuiInputTarget::RawPixelReplayFile,disabled || !has_geos())) run([&] {jobs_.trace_file(doc,stream_path_.data(),file_limits,options_.stream_contour_options,true);});
                const auto file_report=jobs_.report();if(button("Apply reverified contours",GuiInputTarget::RawPixelApplyFile,disabled || !file_report.file_replay || !file_report.stream_contours || file_report.contours_published || !contour_ready_)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(file_report.historical_contours || (file_report.file_replay && file_report.stream_contours)) {ImGui::TextWrapped(file_report.file_replay?"Reverified against current pixels; ready for atomic apply.":"Historical geometry only. Reverify current pixels before applying.");draw_contours(doc,file_report.historical_model);}
                ImGui::InputScalar("Historical block",ImGuiDataType_U64,&index_);ImGui::TextWrapped("File preview is historical; it does not grant live apply authority.");
                if(current.historical_model && current.link) {const auto& classes=current.historical_model->forest().classes();for(std::size_t i=0;i<std::min<std::size_t>(classes.size(),16);++i) ImGui::Text("Class %zu: %s (%llu samples)",i+1,classes[i].c_str(),(unsigned long long)current.link->summary().class_counts[i]);if(classes.size()>16) ImGui::Text("%zu further classes",classes.size()-16);}
                if(history_.ready()) {const auto a=ImGui::GetCursorScreenPos();const ImVec2 size{std::max(1.f,ImGui::GetContentRegionAvail().x),140};history_.draw_local(ImGui::GetWindowDrawList(),a,size);ImGui::Dummy(size);}ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem("Retained files");item(GuiInputTarget::RawPixelRecordsTab);if(tab) {
                ImGui::Text("Raw files: %zu",doc.raw_pixel_stream_record_count());ImGui::InputScalar("Record ID (0 = latest)",ImGuiDataType_U64,&record_);
                auto selected=[&] {if(record_) return doc.raw_pixel_stream_record(record_);auto records=doc.raw_pixel_stream_records();if(records.empty()) throw std::logic_error("No raw file records");return records.back();};
                if(button("Keep verified raw file",GuiInputTarget::RawPixelKeepLink,disabled || !current.link || current.stream_record)) run([&] {jobs_.keep_link(doc);});
                if(button("Open retained raw file",GuiInputTarget::RawPixelOpenRecord,jobs_.busy() || !has_image || !doc.raw_pixel_stream_record_count())) run([&] {auto record=selected();auto limits=options_.read_limits;limits.work_bytes=std::min(limits.work_bytes,options_.preview_budget);jobs_.open("",limits,index_,record->link);});
                auto retained_limits=options_.read_limits;retained_limits.work_bytes=std::min(retained_limits.work_bytes,options_.preview_budget);
                if(button("Inspect retained contours",GuiInputTarget::RawPixelTraceFile,jobs_.busy() || !doc.raw_pixel_stream_record_count() || !has_geos())) run([&] {jobs_.trace_file(doc,"",retained_limits,options_.stream_contour_options,false,selected()->link);});
                if(button("Reverify retained contours for apply",GuiInputTarget::RawPixelReplayFile,disabled || !doc.raw_pixel_stream_record_count() || !has_geos())) run([&] {jobs_.trace_file(doc,"",retained_limits,options_.stream_contour_options,true,selected()->link);});
                const auto retained_report=jobs_.report();if(button("Apply reverified retained contours",GuiInputTarget::RawPixelApplyFile,disabled || !retained_report.file_replay || !retained_report.stream_contours || retained_report.contours_published || !contour_ready_)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(retained_report.historical_contours || (retained_report.file_replay && retained_report.stream_contours)) draw_contours(doc,retained_report.historical_model);
                if(button("Remove raw file record",GuiInputTarget::RawPixelRemoveRecord,disabled || !doc.raw_pixel_stream_record_count())) run([&] {const auto id=selected()->id;doc.remove_raw_pixel_stream_record(id);if(current.stream_record==id) jobs_.reset_records();});ImGui::TextWrapped("Removing a record leaves the external prediction file intact.");ImGui::EndTabItem();
            }
            tab=ImGui::BeginTabItem("Contours");item(GuiInputTarget::RawPixelContoursTab);if(tab) {
                ImGui::TextWrapped("Trace from the current model and exact axes. Stream tiles for large targets. File previews cannot be applied. Apply adds one undoable edit.");
                ImGui::BeginDisabled(disabled);bool changed=ImGui::Checkbox("Stream tiled prediction",&options_.stream_contours);auto& active_contours=options_.stream_contours?options_.stream_contour_options.contours:options_.contours;changed|=ImGui::Checkbox("Clip contours to ROI",&active_contours.clip_to_roi);
                auto limit=[&](const char* label,std::size_t& value,std::uint64_t maximum) {std::uint64_t n=value;if(ui::InputScalar(label,ImGuiDataType_U64,&n)) {value=std::size_t(std::clamp<std::uint64_t>(n,1,maximum));changed=true;}};
                limit("Contour class limit",active_contours.max_components,1000000);limit("Contour edge limit",active_contours.max_edges,1000000);limit("Contour vertex limit",active_contours.max_vertices,1000000);
                limit("Inference work bytes",options_.contour_work_bytes,1024ULL*1024*1024);limit("Contour work bytes",active_contours.work_bytes,512ULL*1024*1024);
                if(options_.stream_contours && ImGui::CollapsingHeader("Stream limits")) {limit("Boundary run limit",options_.stream_contour_options.max_boundary_runs,1000000);limit("Boundary work bytes",options_.stream_contour_options.boundary_work_bytes,512ULL*1024*1024);limit("Total tracing work bytes",options_.stream_contour_options.work_bytes,1024ULL*1024*1024);}
                ui::InputText("Annotation prefix",contour_prefix_.data(),contour_prefix_.size());ImGui::EndDisabled();if(changed) {jobs_.discard_contours();update_contours({});}
                if(button("Trace raw contours",GuiInputTarget::RawPixelTrace,disabled || !current.model || !has_geos())) run([&] {const auto grid=lattice(doc);const auto object=options_.object_id?options_.object_id:selection.primary();if(options_.stream_contours) {auto stream=options_.stream;stream.work_bytes=options_.contour_work_bytes;auto options=options_.stream_contour_options;jobs_.trace_stream(doc,*current.model,grid,stream,options,{},object);}else jobs_.trace(doc,*current.model,ScientificGrid(grid.image_width(),grid.image_height(),grid.x(),grid.y(),options_.contour_work_bytes),options_.contours,object,options_.stream.min_vote_fraction,options_.contour_work_bytes);});
                const auto contours=jobs_.report();if(button("Apply raw contours",GuiInputTarget::RawPixelApply,disabled || (!contours.contours && !contours.stream_contours) || contours.contours_published || !contour_ready_)) run([&] {jobs_.publish(doc,contour_prefix_.data());});
                if(contours.contours || contours.stream_contours) {const auto& c=*(contours.stream_contours?contours.stream_contours->contours():contours.contours->contours());ImGui::Text("%zu classes, %zu vertices",c.components().size(),c.vertex_count());draw_contours(doc,current.model);}if(contours.contours_published) ImGui::Text("Applied %zu annotations",contours.published_ids.size());ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }ImGui::End();
    }
};
}
