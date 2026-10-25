#pragma once
#include "qupathxx/tma.hpp"
#include "qupathxx/qptma.hpp"
#include "qupathxx/spatial_index.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include "gui_layout.hpp"
#include <imgui.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <future>
#include <limits>
#include <functional>
#include <chrono>

namespace qpx::detail {
class GuiTma {
    static constexpr auto first=GuiInputTarget::CreateTmaGrid,last=GuiInputTarget::ApplyTmaLabels;
    static constexpr std::size_t old_count=static_cast<std::size_t>(last)-static_cast<std::size_t>(first)+1;
    static constexpr std::size_t count=old_count+static_cast<std::size_t>(GuiInputTarget::AcknowledgeQptmaLosses)-static_cast<std::size_t>(GuiInputTarget::LoadQptmaPreview)+1;
    std::array<Rect,count> targets_{};std::array<bool,count> enabled_{};
    bool open_=false,explicit_options_=false,rename_names_=false,activate_tab_=true;int initial_tab_=0;
    TmaGridOptions options_;
    std::array<char,257> name_{},case_{};
    std::array<char,129> row_label_{},column_label_{};
    std::array<char,32769> path_{};
    std::array<char,32769> measurements_path_{};
    std::array<char,129> value_column_{},weight_column_{};
    bool case_summary_=false,allow_unversioned_=false,case_cohort_=false;
    std::string error_,initial_case_,export_state_="idle";
    std::optional<Rect> initial_bounds_;
    std::weak_ptr<const char> identity_;
    std::uint64_t revision_=std::numeric_limits<std::uint64_t>::max(),field_revision_=0;
    std::size_t selected_slot_=0,field_slot_=std::numeric_limits<std::size_t>::max(),catalog_max_=0,metadata_bytes_=0;
    std::shared_ptr<const TmaGrid> grid_;
    std::map<std::uint64_t,std::size_t> positions_;
    std::vector<std::uint64_t> created_,present_,selected_cores_;
    std::uint64_t selection_revision_=std::numeric_limits<std::uint64_t>::max();
    Point delta_;
    Rect edited_;
    struct Export {std::string state,error;std::size_t bytes=0;};
    std::future<Export> export_;std::atomic<bool> cancel_{false};
    static std::size_t index(GuiInputTarget t) {
        return t>=GuiInputTarget::LoadQptmaPreview?old_count+static_cast<std::size_t>(t)-static_cast<std::size_t>(GuiInputTarget::LoadQptmaPreview):static_cast<std::size_t>(t)-static_cast<std::size_t>(first);
    }
    std::array<char,32769> qptma_input_{},qptma_output_{};
    struct Exchange {std::optional<QptmaData> preview;std::unique_ptr<SpatialIndex> index;std::string state,error;std::size_t bytes=0,missing=0;};
    std::future<Exchange> exchange_;std::atomic<bool> exchange_cancel_{false};
    std::optional<QptmaData> preview_;std::unique_ptr<SpatialIndex> preview_index_;
    bool preview_truncated_=false;
    std::weak_ptr<const char> preview_identity_;std::uint64_t preview_revision_=0;
    std::string exchange_state_="idle";std::size_t preview_missing_=0,preview_max_step_=0,exchange_bytes_=0;
    bool exchange_open_=false,losses_acknowledged_=false,exchange_exporting_=false;
    void clear_preview() {preview_.reset();preview_index_.reset();preview_truncated_=false;}
    void cancel_exchange() {
        exchange_cancel_.store(true);clear_preview();preview_missing_=0;exchange_state_="cancelled";
    }
    void poll_exchange(Document& doc) {
        if(exchange_.valid() && exchange_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                auto result=exchange_.get();exchange_bytes_=result.bytes;
                if(exchange_cancel_.load()) {clear_preview();exchange_state_="cancelled";return;}
                if(result.preview && (preview_identity_.lock()!=doc.analysis_identity_.token || preview_revision_!=doc.revision() || doc.tma_core_count())) {
                    clear_preview();exchange_state_="stale";error_="The target image changed. Load a new grid preview.";return;
                }
                preview_=std::move(result.preview);preview_index_=std::move(result.index);preview_missing_=result.missing;exchange_state_=std::move(result.state);
                if(!result.error.empty())error_=std::move(result.error);
            }catch(const std::exception& e){failure(e);exchange_state_="failed";}
        }
        if(preview_ && (preview_identity_.lock()!=doc.analysis_identity_.token || preview_revision_!=doc.revision() || doc.tma_core_count())) {
            clear_preview();preview_missing_=0;exchange_state_="stale";error_="The target image changed. Load a new grid preview.";
        }
    }
    void load_preview(Document& doc) {
        if(exchange_.valid())return;
        if(doc.tma_core_count())throw std::invalid_argument("Clear the existing TMA grid explicitly before importing another grid.");
        const std::string path=qptma_input_.data();bounded_utf8(path,32768);
        if(path.empty())throw std::invalid_argument("Enter a QuPath .qptma input path.");
        const auto width=doc.metadata().width,height=doc.metadata().height;
        preview_identity_=doc.analysis_identity_.token;preview_revision_=doc.revision();
        clear_preview();preview_missing_=0;exchange_bytes_=0;exchange_cancel_.store(false);exchange_state_="loading";exchange_exporting_=false;
        exchange_=std::async(std::launch::async,[this,path,width,height] {
            Exchange result;
            try {
                auto data=load_qptma(path,8*1024*1024,&exchange_cancel_);
                std::vector<SpatialEntry> entries;entries.reserve(data.cores.size());std::uint64_t id=0;
                for(const auto& core:data.cores) {
                    export_checkpoint(&exchange_cancel_);const auto b=core.bounds;
                    if(b.x<0 || b.y<0 || b.x+b.width>static_cast<double>(width) || b.y+b.height>static_cast<double>(height))
                        throw std::invalid_argument("An imported core lies outside the selected image.");
                    result.missing+=core.missing;entries.push_back({id++,b});
                }
                result.index=std::make_unique<SpatialIndex>(std::move(entries));export_checkpoint(&exchange_cancel_);
                result.preview=std::move(data);result.state="preview ready";
            }catch(const std::exception& e){result.state=exchange_cancel_.load()?"cancelled":"failed";try{result.error=e.what();bounded_utf8(result.error,8192);}catch(...){result.error="QuPath grid load failed";}}
            return result;
        });
    }
    void write_qptma(const Document& doc,const Project* project) {
        if(exchange_.valid())return;
        if(!grid_)throw std::invalid_argument("There is no TMA grid to export.");
        if(!losses_acknowledged_)throw std::invalid_argument("Review and acknowledge the export's omitted metadata.");
        const auto path=export_path(qptma_output_.data());
        auto report=qptma_from_grid(*grid_,doc.server()->source_locator(),grid_->info()->name());
        std::vector<std::string> sources;
        if(!doc.server()->source_locator().empty())sources.push_back(doc.server()->source_locator());
        if(qptma_input_[0])sources.emplace_back(qptma_input_.data());
        if(project) {
            if(!project->path().empty())sources.push_back(project->path());
            if(!project->recovery_source().empty())sources.push_back(project->recovery_source());
            for(const auto& image:project->images())if(!image.embedded && !image.source_path.empty())sources.push_back(image.source_path);
        }
        for(const auto& source:sources)guard_file_output(export_path(source),"",path);
        exchange_cancel_.store(false);exchange_bytes_=0;exchange_state_="exporting";exchange_exporting_=true;
        exchange_=std::async(std::launch::async,[this,path,sources,data=std::move(report.data)] {
            Exchange result;try {
                const auto text=qptma_text(data,8*1024*1024,&exchange_cancel_);
                AtomicExportFile file(path,8*1024*1024,&exchange_cancel_);file.write(text.data(),text.size());
                for(const auto& source:sources)guard_file_output(export_path(source),"",path);
                file.commit(false);result.state="saved";result.bytes=text.size();
            }catch(const std::exception& e){result.state=exchange_cancel_.load()?"cancelled":"failed";try{result.error=e.what();bounded_utf8(result.error,8192);}catch(...){result.error="QuPath grid export failed";}}
            return result;
        });
    }
    template<std::size_t N> static void copy(std::array<char,N>& out,const std::string& value) {bounded_utf8(value,N-1);out.fill(0);std::copy(value.begin(),value.end(),out.begin());}
    void failure(const std::exception& e) {try {error_=e.what();bounded_utf8(error_,8192);}catch(...) {error_="TMA operation failed";}}
    void refresh(Document& doc) {
        const bool changed=identity_.lock()!=doc.analysis_identity_.token;
        if(changed) {if(export_.valid()) cancel_.store(true);if(exchange_.valid() || preview_)cancel_exchange();losses_acknowledged_=false;identity_=doc.analysis_identity_.token;revision_=std::numeric_limits<std::uint64_t>::max();field_slot_=std::numeric_limits<std::size_t>::max();selected_slot_=0;created_.clear();grid_.reset();positions_.clear();present_.clear();error_.clear();
            if(!explicit_options_) {const auto& m=doc.metadata();const auto sx=double(m.width)/(options_.columns+1),sy=double(m.height)/(options_.rows+1);options_.origin={sx,sy};options_.column_step={sx,0};options_.row_step={0,sy};options_.width=std::min(sx,sy)*.7;options_.height=options_.width;}}
        if(!open_ || revision_==doc.tma_revision()) return;
        grid_=doc.tma_grid();revision_=doc.tma_revision();positions_.clear();present_.clear();field_slot_=std::numeric_limits<std::size_t>::max();selection_revision_=std::numeric_limits<std::uint64_t>::max();
        if(grid_) {const auto columns=grid_->info()->columns();for(const auto& core:grid_->cores()) {positions_.emplace(core.id,core.tma_core->row()*columns+core.tma_core->column());if(!core.tma_core->missing()) present_.push_back(core.id);}copy(name_,grid_->info()->name());if(selected_slot_>=grid_->info()->rows()*columns) selected_slot_=0;}
        else {copy(name_,options_.name);field_slot_=std::numeric_limits<std::size_t>::max();}
    }
    void poll() {
        if(export_.valid() && export_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto value=export_.get();export_state_=value.state;metadata_bytes_=value.bytes;if(!value.error.empty()) error_=std::move(value.error);}catch(const std::exception& e) {failure(e);export_state_="failed";}}
    }
    void write_metadata(const Document& doc,const Project* project) {
      try {
        if(export_.valid()) return;if(!grid_) throw std::invalid_argument("there is no TMA grid");if(!path_[0]) throw std::invalid_argument("enter a new CSV file path");
        auto path=export_path(path_.data());
#ifdef _WIN32
        auto wide=std::filesystem::u8path(path).native();if(wide.rfind(L"\\\\?\\",0)!=0) {if(wide.rfind(L"\\\\",0)==0) wide=L"\\\\?\\UNC\\"+wide.substr(2);else wide=L"\\\\?\\"+wide;path=std::filesystem::path(wide).u8string();}
#endif
        std::vector<std::string> sources;const auto locator=doc.server()->source_locator();if(!locator.empty()) sources.push_back(locator);if(project) {if(!project->path().empty()) sources.push_back(project->path());if(!project->recovery_source().empty()) sources.push_back(project->recovery_source());}
        const bool summary=case_summary_,unversioned=allow_unversioned_;
        const std::string input=summary && measurements_path_[0]?export_path(measurements_path_.data()):"",value=value_column_.data(),weight=weight_column_.data();
        if(!input.empty()) sources.push_back(input);
        const bool pooling=summary && case_cohort_;TmaCohort cohort;
        if(pooling) {
            if(!project) throw std::invalid_argument("project case pooling requires an open project");
            std::size_t payload=0;constexpr std::size_t maximum=64*1024*1024;
            for(const auto& image:project->images()) {
                if(!image.embedded && !image.source_path.empty()) sources.push_back(image.source_path);
                const auto document=project->document(image.id);if(!document->tma_core_count()) continue;
                if(cohort.size()>=4096) throw std::length_error("TMA cohort exceeds 4096 grids");
                if(payload>=maximum) throw std::length_error("TMA cohort captures exceed byte limit");
                const auto captured=document->tma_grid(maximum-payload);payload+=captured->payload_bytes();cohort.emplace(image.id,captured);
            }
            if(cohort.empty()) throw std::invalid_argument("project contains no TMA grids");
        }
        const auto snapshot=grid_;cancel_.store(false);metadata_bytes_=0;export_state_="writing";
        export_=std::async(std::launch::async,[this,snapshot,path,sources,summary,unversioned,input,value,weight,pooling,cohort=std::move(cohort)] {
            Export result;try {
                export_checkpoint(&cancel_);
                for(const auto& source:sources) guard_file_output(export_path(source),"",path);
                if(summary) {
                    result.bytes=pooling?save_tma_cohort_case_summary_csv(cohort,path,input,value,weight,64*1024*1024,unversioned,&cancel_):
                        save_tma_case_summary_csv(*snapshot,path,input,value,weight,64*1024*1024,unversioned,&cancel_);
                    result.state="saved";return result;
                }
                const auto csv=tma_metadata_csv(*snapshot);
                export_checkpoint(&cancel_);for(const auto& source:sources) guard_file_output(export_path(source),"",path);
                AtomicExportFile output(path,64*1024*1024,&cancel_);for(std::size_t at=0;at<csv.size();) {const auto n=std::min<std::size_t>(65536,csv.size()-at);output.write(csv.data()+at,n);at+=n;}
                for(const auto& source:sources) guard_file_output(export_path(source),"",path);result.state="saved";result.bytes=csv.size();output.commit(false);
            }catch(const std::exception& e) {result.bytes=0;result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TMA CSV export failed";}}
            return result;
        });
      }catch(...) {metadata_bytes_=0;export_state_="failed";throw;}
    }
public:
    explicit GuiTma(const GuiOptions& initial):open_(initial.tma_open),explicit_options_(bool(initial.tma_options)),initial_tab_(initial.tma_tab),options_(initial.tma_options?*initial.tma_options:TmaGridOptions{}),initial_case_(initial.tma_case_id),initial_bounds_(initial.tma_core_bounds),delta_(initial.tma_translation) {if(initial_tab_<0 || initial_tab_>2) throw std::invalid_argument("invalid TMA panel tab");options_.validate();copy(qptma_input_,initial.qptma_input_path);copy(qptma_output_,initial.qptma_output_path);exchange_open_=qptma_input_[0] || qptma_output_[0];copy(name_,options_.name);copy(path_,initial.tma_metadata_path);case_summary_=initial.tma_case_summary;case_cohort_=initial.tma_case_cohort;allow_unversioned_=initial.tma_allow_unversioned;copy(measurements_path_,initial.tma_measurements_path);copy(value_column_,initial.tma_value_column);copy(weight_column_,initial.tma_weight_column);bounded_utf8(initial_case_,256);if(initial_bounds_) initial_bounds_->validate();}
    ~GuiTma() {finish(false);}
    void open() {open_=true;}
    static bool handles(GuiInputTarget target) {return (target>=first && target<=last) || (target>=GuiInputTarget::LoadQptmaPreview && target<=GuiInputTarget::AcknowledgeQptmaLosses);}
    const std::string& qptma_state() const {return exchange_state_;}
    std::size_t qptma_cores() const {return preview_?preview_->cores.size():0;}
    std::size_t qptma_missing() const {return preview_?preview_missing_:0;}
    std::size_t qptma_max_step() const {return preview_max_step_;}
    std::size_t qptma_bytes() const {return exchange_bytes_;}
    void draw_qptma_preview(const Document& doc,ImDrawList* draw,const Viewport& viewport,ImVec2 offset,ImVec2 size) {
        preview_truncated_=false;
        if(!open_ || !preview_ || !preview_index_ || preview_identity_.lock()!=doc.analysis_identity_.token || preview_revision_!=doc.revision())return;
        const auto a=viewport.screen_to_image({0,0}),b=viewport.screen_to_image({size.x,size.y});
        const auto ids=preview_index_->query({std::min(a.x,b.x),std::min(a.y,b.y),std::abs(b.x-a.x),std::abs(b.y-a.y)});
        preview_truncated_=ids.size()>512;std::size_t drawn=0;
        for(const auto id:ids) {
            if(drawn++==512)break;const auto& core=preview_->cores.at(static_cast<std::size_t>(id));const auto r=core.bounds;
            const auto lo=viewport.image_to_screen({r.x,r.y}),hi=viewport.image_to_screen({r.x+r.width,r.y+r.height});
            const ImVec2 center{offset.x+static_cast<float>((lo.x+hi.x)/2),offset.y+static_cast<float>((lo.y+hi.y)/2)};
            const ImVec2 radii{static_cast<float>((hi.x-lo.x)/2),static_cast<float>((hi.y-lo.y)/2)};
            draw->AddEllipse(center,radii,core.missing?IM_COL32(160,160,160,230):IM_COL32(100,180,255,255),0,0,2);
        }
    }
    Rect bounds(GuiInputTarget target) const {return targets_.at(index(target));}
    bool enabled(GuiInputTarget target) const {return enabled_.at(index(target));}
    const std::string& error() const {return error_;}
    const std::string& export_state() const {return export_state_;}
    const std::vector<std::uint64_t>& created() const {return created_;}
    std::size_t catalog_max_step() const {return catalog_max_;}
    std::size_t metadata_bytes() const {return metadata_bytes_;}
    void finish(bool finite) {if(exchange_.valid()) {if(!finite || !exchange_exporting_)exchange_cancel_.store(true);exchange_.wait();try{auto result=exchange_.get();if(result.state=="saved"){exchange_state_="saved";exchange_bytes_=result.bytes;}else {exchange_state_=exchange_cancel_.load()?"cancelled":result.state;if(!result.error.empty())error_=std::move(result.error);}}catch(...) {exchange_state_="failed";}clear_preview();}if(export_.valid()) {if(!finite) cancel_.store(true);export_.wait();poll();}}
    bool draw(Document& doc,ObjectSelection& selection,const Project* project,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark) {
        enabled_.fill(false);
        poll();if(!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_T,false)) open_=!open_;
        try {refresh(doc);poll_exchange(doc);}catch(const std::exception& e) {failure(e);}
        if(!open_) return false;
        const auto* viewport=ImGui::GetMainViewport();const auto scale=ImGui::GetFontSize()/13.f;
        ImGui::SetNextWindowSize({std::min(530.f*scale,viewport->WorkSize.x*.9f),std::min(780.f*scale,viewport->WorkSize.y*.9f)},ImGuiCond_Appearing);
        ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*.5f,viewport->WorkPos.y+viewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
        ImGui::SetNextWindowSizeConstraints({std::min(280.f*scale,viewport->WorkSize.x),std::min(180.f*scale,viewport->WorkSize.y)},viewport->WorkSize);
        if(!ImGui::Begin("Tissue microarray",&open_)) {ImGui::End();return false;}
        auto remember=[&](GuiInputTarget target,bool disabled) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(target)]=!disabled && ImGui::IsItemVisible();mark(target);};
        auto button=[&](const char* text,GuiInputTarget target,bool disabled) {ImGui::BeginDisabled(disabled);const bool clicked=ui::Button(text);remember(target,disabled);ImGui::EndDisabled();return clicked;};
        auto perform=[&](auto&& action) {try {action();error_.clear();refresh(doc);}catch(const std::exception& e) {failure(e);}};
        const bool disabled=blocked || !has_image;bool batch=false;
        if(ImGui::CollapsingHeader("QuPath grid exchange",exchange_open_?ImGuiTreeNodeFlags_DefaultOpen:0)) {
            ImGui::TextWrapped("Import into the selected image as one undoable edit. Image paths in the file are informational only.");
            ImGui::BeginDisabled(disabled || exchange_.valid());
            if(ui::InputTextWithHint("##QPTMA input","QuPath .qptma input",qptma_input_.data(),qptma_input_.size())) {clear_preview();preview_missing_=0;exchange_state_="idle";}ImGui::EndDisabled();
            if(button("Load grid preview",GuiInputTarget::LoadQptmaPreview,disabled || exchange_.valid() || bool(grid_) || !qptma_input_[0]))perform([&]{load_preview(doc);});
            if(preview_) {
                ImGui::TextWrapped("Target: %lld x %lld pixels. Grid: %zu rows x %zu columns, %zu cores (%zu present / %zu missing).",
                    static_cast<long long>(doc.metadata().width),static_cast<long long>(doc.metadata().height),preview_->rows,preview_->columns,preview_->cores.size(),preview_->cores.size()-preview_missing_,preview_missing_);
                ImGui::TextWrapped("%s",preview_->image_name.c_str());
                ImGui::TextWrapped("Blue outlines preview present cores; gray outlines preview missing cores. No objects are created until Apply.");
                if(preview_truncated_)ImGui::TextWrapped("More than 512 cores are visible. Zoom in to review their outlines; the full grid remains available in the list.");
                if(ImGui::BeginChild("qptma core preview",{0,100.f*scale},ImGuiChildFlags_Borders)) {
                    ImGuiListClipper clip;clip.Begin(static_cast<int>(preview_->cores.size()));std::size_t inspected=0;
                    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd && inspected<512;++i) {
                        ++inspected;const auto& core=preview_->cores[static_cast<std::size_t>(i)];const auto b=core.bounds;
                        const auto label=std::to_string(static_cast<std::size_t>(i)/preview_->columns+1)+"-"+std::to_string(static_cast<std::size_t>(i)%preview_->columns+1)+"  "+core.name+(core.missing?" (missing)":"");
                        ImGui::PushID(i);ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_Disabled,{std::max(1.f,ImGui::GetContentRegionAvail().x),0});ImGui::PopID();
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("Bounds: %.3f, %.3f, %.3f, %.3f\nCase: %s",b.x,b.y,b.width,b.height,core.case_id.c_str());
                    }
                    preview_max_step_=std::max(preview_max_step_,inspected);
                }ImGui::EndChild();
            }
            if(button("Apply imported grid",GuiInputTarget::ApplyQptmaPreview,disabled || exchange_.valid() || !preview_ || bool(grid_)))perform([&]{
                if(preview_identity_.lock()!=doc.analysis_identity_.token || preview_revision_!=doc.revision())throw std::invalid_argument("Grid preview is stale. Load it again.");
                created_=import_qptma(doc,*preview_);clear_preview();preview_missing_=0;exchange_state_="imported";losses_acknowledged_=false;
                if(!created_.empty())selection.replace({created_.front()},created_.front());
            });
            if(button("Discard grid preview",GuiInputTarget::DiscardQptmaPreview,!preview_)) {clear_preview();preview_missing_=0;exchange_state_="idle";}
            if(grid_) {
                ImGui::TextWrapped("Export retains core names, bounds, missing flags and case IDs. Native IDs, grid name and axis labels are omitted; class assignments and parent links, when present, are also omitted. Vacant slots cannot be exported.");
                ImGui::BeginDisabled(disabled || exchange_.valid());
                ui::Checkbox("I acknowledge the omitted metadata",&losses_acknowledged_);remember(GuiInputTarget::AcknowledgeQptmaLosses,disabled || exchange_.valid());
                ui::InputTextWithHint("##QPTMA output","New .qptma output file",qptma_output_.data(),qptma_output_.size());ImGui::EndDisabled();
            }
            if(button("Export QuPath grid",GuiInputTarget::ExportQptma,disabled || exchange_.valid() || !grid_ || !losses_acknowledged_ || !qptma_output_[0]))perform([&]{write_qptma(doc,project);});
            if(button("Cancel grid file operation",GuiInputTarget::CancelQptma,!exchange_.valid()))cancel_exchange();
            ImGui::TextWrapped("QuPath grid: %s",exchange_state_.c_str());
            ImGui::Separator();
        }
        if(!grid_) {
            int rows=static_cast<int>(options_.rows),columns=static_cast<int>(options_.columns);
            ImGui::BeginDisabled(disabled);ui::InputText("Grid name",name_.data(),name_.size());
            if(ui::InputInt("Rows",&rows)) options_.rows=static_cast<std::size_t>(std::clamp(rows,1,512));if(ui::InputInt("Columns",&columns)) options_.columns=static_cast<std::size_t>(std::clamp(columns,1,512));
            ui::InputDouble("First center X (px)",&options_.origin.x);ui::InputDouble("First center Y (px)",&options_.origin.y);
            ui::InputDouble("Column step X",&options_.column_step.x);ui::InputDouble("Column step Y",&options_.column_step.y);
            ui::InputDouble("Row step X",&options_.row_step.x);ui::InputDouble("Row step Y",&options_.row_step.y);
            ui::InputDouble("Core width (px)",&options_.width);ui::InputDouble("Core height (px)",&options_.height);ImGui::EndDisabled();
        }
        if(button("Create core grid",GuiInputTarget::CreateTmaGrid,disabled || bool(grid_))) perform([&]{options_.name=name_.data();created_=doc.create_tma_grid(options_);if(!created_.empty()) selection.replace({created_.front()},created_.front());});
        if(grid_) {
            const auto snapshot=grid_;const auto& info=*snapshot->info();
            if(const auto found=positions_.find(selection.primary());found!=positions_.end()) selected_slot_=found->second;
            const auto row=selected_slot_/info.columns(),column=selected_slot_%info.columns();auto selected=snapshot->core(row,column);
            if(field_slot_!=selected_slot_ || field_revision_!=(selected?selected->revision:0)) {
                field_slot_=selected_slot_;field_revision_=selected?selected->revision:0;copy(row_label_,info.row_labels()[row]);copy(column_label_,info.column_labels()[column]);
                copy(case_,initial_case_.empty()?(selected?selected->tma_core->case_id():""):initial_case_);initial_case_.clear();
                if(initial_bounds_) {edited_=*initial_bounds_;initial_bounds_.reset();}else if(selected) edited_=selected->geometry.bounds();else {const auto x=options_.origin.x+row*options_.row_step.x+column*options_.column_step.x,y=options_.origin.y+row*options_.row_step.y+column*options_.column_step.y;edited_={x-options_.width/2,y-options_.height/2,options_.width,options_.height};}
            }
            ImGui::TextWrapped("%s / %zu rows x %zu columns",info.name().c_str(),info.rows(),info.columns());
            if(button("Select present cores",GuiInputTarget::SelectTmaPresent,disabled)) perform([&]{selection.replace(present_);});
            if(button("Per-core batch...",GuiInputTarget::OpenTmaBatch,disabled || !project || present_.empty())) {selection.replace(present_);batch=true;}
            if(selection_revision_!=selection.revision()) {selected_cores_.clear();for(auto id:selection.ids()) if(positions_.count(id)) selected_cores_.push_back(id);if(selected_cores_.empty() && selected) selected_cores_.push_back(selected->id);selection_revision_=selection.revision();}
            auto tab=[&](const char* title,int value,GuiInputTarget target) {const bool shown=ImGui::BeginTabItem(title,nullptr,activate_tab_ && initial_tab_==value?ImGuiTabItemFlags_SetSelected:0);remember(target,disabled);return shown;};
            if(ImGui::BeginTabBar("TMA views")) {
                if(tab("Cores",0,GuiInputTarget::TmaCoreTab)) {
                    std::size_t inspected=0;
                    if(ImGui::BeginChild("core catalog",{0,140.f*scale},ImGuiChildFlags_Borders)) {
                        ImGuiListClipper clip;clip.Begin(static_cast<int>(info.rows()*info.columns()));while(clip.Step()) for(int slot=clip.DisplayStart;slot<clip.DisplayEnd && inspected<512;++slot) {
                            ++inspected;const auto r=std::size_t(slot)/info.columns(),c=std::size_t(slot)%info.columns();const auto core=snapshot->core(r,c);
                            const auto text=(core?core->name:info.row_labels()[r]+"-"+info.column_labels()[c])+(!core?" (vacant)":core->tma_core->missing()?" (missing)":"");ImGui::PushID(slot);
                            if(ImGui::Selectable(text.c_str(),std::size_t(slot)==selected_slot_,0,{std::max(1.f,ImGui::GetContentRegionAvail().x),0})) {selected_slot_=static_cast<std::size_t>(slot);if(core) selection.replace({core->id},core->id);else selection.clear();field_slot_=std::numeric_limits<std::size_t>::max();}ImGui::PopID();
                        }
                    }ImGui::EndChild();catalog_max_=std::max(catalog_max_,inspected);
                    ImGui::TextWrapped("Core %s-%s / %zu selected",info.row_labels()[row].c_str(),info.column_labels()[column].c_str(),selected_cores_.size());
                    bool missing=selected && selected->tma_core->missing();ImGui::BeginDisabled(disabled || !selected);if(ui::Checkbox("Missing / insufficient tissue",&missing)) perform([&]{doc.set_tma_missing(selected_cores_,missing);});remember(GuiInputTarget::ToggleTmaMissing,disabled || !selected);ImGui::EndDisabled();
                    ImGui::BeginDisabled(disabled);ui::InputText("Case ID",case_.data(),case_.size());ImGui::EndDisabled();
                    if(button("Apply case ID to selected cores",GuiInputTarget::ApplyTmaCase,disabled || selected_cores_.empty())) perform([&]{std::map<std::uint64_t,std::string> values;for(auto id:selected_cores_) values.emplace(id,case_.data());doc.set_tma_case_ids(values);});
                    ImGui::BeginDisabled(disabled);ui::InputDouble("Core minimum X (px)",&edited_.x);ui::InputDouble("Core minimum Y (px)",&edited_.y);ui::InputDouble("Core width (px)",&edited_.width);ui::InputDouble("Core height (px)",&edited_.height);ImGui::EndDisabled();
                    if(button(selected?"Apply core geometry":"Add core in vacant slot",GuiInputTarget::ApplyTmaGeometry,disabled)) perform([&]{if(selected) doc.update_roi(selected->id,Roi::ellipse(edited_));else {const auto id=doc.add_tma_core(row,column,edited_,case_.data());selection.replace({id},id);}});
                    ImGui::EndTabItem();
                }
                if(tab("Grid edits",1,GuiInputTarget::TmaGridTab)) {
                    ImGui::BeginDisabled(disabled);ui::InputText("Grid name",name_.data(),name_.size());ui::InputText("Row label",row_label_.data(),row_label_.size());ui::InputText("Column label",column_label_.data(),column_label_.size());ui::Checkbox("Update core names from labels",&rename_names_);ImGui::EndDisabled();
                    if(button("Apply labels",GuiInputTarget::ApplyTmaLabels,disabled)) perform([&]{auto rows=info.row_labels(),columns=info.column_labels();rows[row]=row_label_.data();columns[column]=column_label_.data();doc.set_tma_labels(name_.data(),rows,columns,rename_names_);});
                    ImGui::BeginDisabled(disabled);ui::InputDouble("Move X (px)",&delta_.x);ui::InputDouble("Move Y (px)",&delta_.y);ImGui::EndDisabled();
                    if(button("Move grid and child objects",GuiInputTarget::TranslateTmaGrid,disabled)) perform([&]{doc.translate_tma_grid(delta_.x,delta_.y);});
                    if(button("Dissolve grid (keep objects)",GuiInputTarget::ClearTmaGrid,disabled)) perform([&]{doc.clear_tma_grid();});
                    ImGui::EndTabItem();
                }
                if(tab("Export",2,GuiInputTarget::TmaExportTab)) {
                    ImGui::BeginDisabled(export_.valid());
                    ui::Checkbox("Summarize replicate cores by case",&case_summary_);
                    if(case_summary_) {
                        ImGui::BeginDisabled(!project);ui::Checkbox("Pool TMA grids across project",&case_cohort_);ImGui::EndDisabled();
                        if(case_cohort_) ImGui::TextWrapped("Capture every project image with a TMA grid. Measurement rows must include image_id; core IDs are local to each image.");
                        ImGui::TextWrapped("Missing cores contribute no measurements. Cores without a case ID stay separate. Leave the input empty for case inventory only.");
                        ImGui::TextUnformatted("Measurement CSV (optional)");
                        ui::InputTextWithHint("##TMA measurements path","Optional measurement CSV input",measurements_path_.data(),measurements_path_.size());
                        ui::InputText("Value column",value_column_.data(),value_column_.size());
                        ui::InputText("Weight column (optional)",weight_column_.data(),weight_column_.size());
                        ui::Checkbox("Allow input without core revisions",&allow_unversioned_);
                    } else ImGui::TextWrapped("Metadata CSV includes present, missing and vacant slots. It captures the grid when requested and creates a new file.");
                    ImGui::TextUnformatted("New CSV output");
                    ui::InputTextWithHint("##TMA metadata path","New CSV output file",path_.data(),path_.size());ImGui::EndDisabled();
                    if(button(case_summary_?"Export case summary CSV":"Export core metadata CSV",GuiInputTarget::ExportTmaMetadata,disabled || export_.valid() || !path_[0])) perform([&]{write_metadata(doc,project);});
                    if(button("Cancel CSV export",GuiInputTarget::CancelTmaMetadata,!export_.valid())) cancel_.store(true);
                    ImGui::TextWrapped("CSV export: %s",export_state_.c_str());ImGui::EndTabItem();
                }
                ImGui::EndTabBar();activate_tab_=false;
            }

        }
        if(!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());ImGui::End();return batch;
    }
};
}
