#pragma once
#include "gui_draft.hpp"
#include "gui_tool_scope.hpp"
#include "gui_tool_catalog.hpp"
#include "gui_path_field.hpp"
#include "gui_output_gate.hpp"
#include "qupathxx/tma.hpp"
#include "qupathxx/qptma.hpp"
#include "qupathxx/tma_dearray.hpp"
#include "qupathxx/spatial_index.hpp"
#include "qupathxx/project.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include "gui_layout.hpp"
#include "gui_workflow_ui.hpp"
#include <imgui.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <future>
#include <limits>
#include <functional>
#include <chrono>

namespace qpx::detail {
inline bool tma_label_name_matches(std::string_view name,std::string_view row,std::string_view column) noexcept {
    return name.size()==row.size()+1+column.size() && name.substr(0,row.size())==row && name[row.size()]=='-' && name.substr(row.size()+1)==column;
}
class GuiTma {
    ui::PathField path_{{"##TMA metadata path",ui::PathRole::NewFile,{".csv"},"","New .csv file"}};
    ui::PathField measurements_path_{{"##TMA measurements path",ui::PathRole::OpenFile,{".csv"},"","Optional measurement CSV input"}};
    ui::PathField qptma_input_{{"##QPTMA input",ui::PathRole::OpenFile,{".qptma"},"","QuPath .qptma input"}};
    ui::PathField qptma_output_{{"##QPTMA output",ui::PathRole::NewFile,{".qptma"},"","New .qptma output file"}};
    friend struct OutputWriterTestAccess;
    friend struct GuiTmaTestAccess;
    inline static constexpr auto input_targets=std::array{
        GuiInputTarget::CreateTmaGrid,
        GuiInputTarget::ToggleTmaMissing,
        GuiInputTarget::SelectTmaPresent,
        GuiInputTarget::ApplyTmaCase,
        GuiInputTarget::ApplyTmaGeometry,
        GuiInputTarget::TranslateTmaGrid,
        GuiInputTarget::ClearTmaGrid,
        GuiInputTarget::OpenTmaBatch,
        GuiInputTarget::ExportTmaMetadata,
        GuiInputTarget::CancelTmaMetadata,
        GuiInputTarget::TmaCoreTab,
        GuiInputTarget::TmaGridTab,
        GuiInputTarget::TmaExportTab,
        GuiInputTarget::ApplyTmaLabels,
        GuiInputTarget::LoadQptmaPreview,
        GuiInputTarget::ApplyQptmaPreview,
        GuiInputTarget::ExportQptma,
        GuiInputTarget::CancelQptma,
        GuiInputTarget::DiscardQptmaPreview,
        GuiInputTarget::AcknowledgeQptmaLosses,
        GuiInputTarget::TmaInsertRowBefore,
        GuiInputTarget::TmaInsertRowAfter,
        GuiInputTarget::TmaInsertColumnBefore,
        GuiInputTarget::TmaInsertColumnAfter,
        GuiInputTarget::TmaRemoveRow,
        GuiInputTarget::TmaRemoveColumn,
        GuiInputTarget::TmaDetectGrid};
    std::array<Rect,input_targets.size()> targets_{};std::array<bool,input_targets.size()> enabled_{};
    struct LabelsDraft {
        std::array<char,257> name{};std::array<char,129> row{},column{};
        bool operator==(const LabelsDraft& b) const {return std::string_view(name.data())==b.name.data() && std::string_view(row.data())==b.row.data() && std::string_view(column.data())==b.column.data();}
    };
    detail::Draft<detail::DraftText<257>> case_draft_;
    detail::Draft<detail::GeometryDraft> core_draft_;
    detail::Draft<LabelsDraft> labels_draft_;
    detail::DraftTextOwner draft_text_;
    std::array<std::optional<double>,4> core_inputs_;
    bool discarded_=false,fields_loaded_=false;
    std::uint64_t field_id_=0,fields_document_revision_=~std::uint64_t(0);
    bool open_=false,explicit_options_=false,rename_names_=false,activate_tab_=true;int initial_tab_=0;
    std::optional<GuiInputTarget> reveal_target_;
    TmaGridOptions options_;
    std::array<char,257>& name_=labels_draft_.value.name;detail::DraftText<257>& case_=case_draft_.value;
    std::array<char,129>& row_label_=labels_draft_.value.row;std::array<char,129>& column_label_=labels_draft_.value.column;
    std::array<char,129> insert_label_{};
    bool insert_cores_=true,insert_missing_=false;
    std::array<char,129> value_column_{},weight_column_{};
    bool case_summary_=false,allow_unversioned_=false,case_cohort_=false;
    std::string error_,initial_case_,export_state_="idle",csv_destination_,qptma_destination_;
    std::optional<Rect> initial_bounds_;
    ToolScope scope_;int scope_frame_=-1;
    std::uint64_t revision_=std::numeric_limits<std::uint64_t>::max();
    std::size_t selected_slot_=0,field_slot_=std::numeric_limits<std::size_t>::max(),catalog_max_=0,metadata_bytes_=0;
    std::shared_ptr<const TmaGrid> grid_;
    std::map<std::uint64_t,std::size_t> positions_;
    std::vector<std::uint64_t> created_,present_,selected_cores_;
    std::uint64_t selection_revision_=std::numeric_limits<std::uint64_t>::max();
    Point delta_;
    Rect& edited_=core_draft_.value.bounds;
    struct Export {std::string state,error;std::size_t bytes=0;};
    std::future<Export> export_;std::atomic<bool> cancel_{false};
    static constexpr std::size_t index(GuiInputTarget t) {
        for(std::size_t i=0;i<input_targets.size();++i) if(input_targets[i]==t) return i;
        return input_targets.size();
    }
    struct Exchange {std::shared_ptr<const QptmaData> preview;std::shared_ptr<const TmaDearrayResult> dearray;std::unique_ptr<SpatialIndex> index;std::string state,error;std::size_t bytes=0,missing=0;};
    std::future<Exchange> exchange_;std::atomic<bool> exchange_cancel_{false};
    std::shared_ptr<const QptmaData> preview_;std::unique_ptr<SpatialIndex> preview_index_;
    std::shared_ptr<const TmaDearrayResult> dearray_result_;
    TmaDearrayOptions dearray_options_;bool dearray_open_=false;int setup_mode_=0;
    std::array<char,257> dearray_name_{};
    bool preview_truncated_=false;
    std::weak_ptr<const char> preview_identity_;ObjectPlane preview_plane_;
    std::string exchange_state_="idle";std::size_t preview_missing_=0,preview_max_step_=0,exchange_bytes_=0;
    ui::NoticeJob import_notice_;std::uint64_t import_notice_generation_=0;
    bool exchange_open_=false,losses_acknowledged_=false,exchange_exporting_=false;
    std::weak_ptr<const char> export_review_identity_;
    std::uint64_t export_review_revision_=std::numeric_limits<std::uint64_t>::max();
    std::vector<std::string> export_omissions_;
    std::string export_review_error_;
    void review_qptma(const Document& doc) {
        const auto identity=doc.analysis_identity_.token;
        if(export_review_identity_.lock()==identity && export_review_revision_==doc.revision())return;
        export_review_identity_=identity;export_review_revision_=doc.revision();
        export_omissions_.clear();export_review_error_.clear();losses_acknowledged_=false;
        try {
            if(!grid_)throw std::invalid_argument("There is no TMA grid to export.");
            // Convert only when this document changes, not in the hot draw path.
            // Membership and refusals remain owned by the native conversion.
            export_omissions_=qptma_from_grid(*grid_,doc.server()->source_locator(),grid_->info()->name()).omitted_metadata;
        }catch(const std::exception& e){try {export_review_error_=e.what();bounded_utf8(export_review_error_,8192);}catch(...) {export_review_error_="QuPath grid export failed";}}
    }
    static const char* omission_caption(const std::string& key) {
        if(key=="native_object_ids")return "Object IDs";
        if(key=="native_grid_name")return "Grid name";
        if(key=="axis_labels")return "Row and column labels";
        if(key=="class_assignments")return "Class assignments";
        if(key=="parent_links")return "Parent links";
        return key.c_str();
    }
    void clear_preview() {preview_.reset();preview_index_.reset();dearray_result_.reset();preview_truncated_=false;}
    void cancel_exchange() {
        exchange_cancel_.store(true);clear_preview();preview_missing_=0;exchange_state_="cancelled";
    }
    void poll_exchange(Document& doc) {
        if(exchange_.valid() && exchange_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                auto result=exchange_.get();exchange_bytes_=result.bytes;
                if(exchange_cancel_.load() && result.state!="saved") {clear_preview();exchange_state_="cancelled";return;}
                if(result.preview && preview_identity_.lock()!=doc.analysis_identity_.token) {
                    clear_preview();exchange_state_="stale";error_="The target image changed. Load a new grid preview.";return;
                }
                preview_=std::move(result.preview);dearray_result_=preview_?std::move(result.dearray):nullptr;preview_index_=std::move(result.index);preview_missing_=result.missing;exchange_state_=std::move(result.state);
                if(!result.error.empty())error_=std::move(result.error);
            }catch(const std::exception& e){failure(e);exchange_state_="failed";}
        }
        if(preview_ && preview_identity_.lock()!=doc.analysis_identity_.token) {
            clear_preview();preview_missing_=0;exchange_state_="stale";error_="The target image changed. Load a new grid preview.";
        }
    }
    void load_preview(Document& doc,bool detecting=false) {
        if(exchange_.valid())return;
        if(doc.tma_core_count())throw std::invalid_argument("Clear the existing TMA grid explicitly before importing another grid.");
        const std::string path=qptma_input_.state().resolved;bounded_utf8(path,32768);
        if(!detecting && path.empty())throw std::invalid_argument("Enter a QuPath .qptma input path.");
        auto captured_options=dearray_options_;
        if(detecting) {try {captured_options=tma_dearray_options_for_document(doc,dearray_options_);}catch(...) {clear_preview();preview_missing_=0;exchange_state_="failed";throw;}}
        const auto width=doc.metadata().width,height=doc.metadata().height;
        preview_identity_=doc.analysis_identity_.token;preview_plane_=doc.active_plane();
        clear_preview();preview_missing_=0;exchange_bytes_=0;exchange_cancel_.store(false);exchange_state_="loading";exchange_exporting_=false;++import_notice_generation_;
        exchange_=std::async(std::launch::async,[this,path,width,height,detecting,server=doc.server(),options=std::move(captured_options)] {
            Exchange result;
            try {
                std::shared_ptr<const QptmaData> data;
                if(detecting) {
                    result.dearray=detect_tma_grid(*server,options,&exchange_cancel_);
                    data=std::shared_ptr<const QptmaData>(result.dearray,&result.dearray->grid);
                }else data=std::make_shared<const QptmaData>(load_qptma(path,8*1024*1024,&exchange_cancel_));
                std::vector<SpatialEntry> entries;entries.reserve(data->cores.size());std::uint64_t id=0;
                for(const auto& core:data->cores) {
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
    void write_qptma(const Document& doc,const Project*,const std::string& path,ExportWritePolicy policy,std::vector<std::string> sources) {
        if(exchange_.valid())return;
        if(!grid_)throw std::invalid_argument("There is no TMA grid to export.");
        if(!losses_acknowledged_)throw std::invalid_argument("Review and acknowledge the export's omitted metadata.");
        auto report=qptma_from_grid(*grid_,doc.server()->source_locator(),grid_->info()->name());
        if(qptma_input_.text()[0])sources.emplace_back(qptma_input_.state().resolved);
        for(const auto& source:sources)guard_file_output(export_path(source),"",path);
        exchange_cancel_.store(false);exchange_bytes_=0;exchange_state_="exporting";exchange_exporting_=true;qptma_destination_=path;
        exchange_=std::async(std::launch::async,[this,path,sources,policy,data=std::move(report.data)] {
            Exchange result;try {
                const auto text=qptma_text(data,8*1024*1024,&exchange_cancel_);
                AtomicExportFile file(path,8*1024*1024,&exchange_cancel_);file.write(text.data(),text.size());
                for(const auto& source:sources)guard_file_output(export_path(source),"",path);
                file.commit(policy==ExportWritePolicy::ReplaceExisting);result.state="saved";result.bytes=text.size();
            }catch(const std::exception& e){result.state=exchange_cancel_.load()?"cancelled":"failed";try{result.error=e.what();bounded_utf8(result.error,8192);}catch(...){result.error="QuPath grid export failed";}}
            return result;
        });
    }
    template<std::size_t N> static void copy(std::array<char,N>& out,const std::string& value) {bounded_utf8(value,N-1);out.fill(0);std::copy(value.begin(),value.end(),out.begin());}
    template<std::size_t N> static void copy(detail::DraftText<N>& out,const std::string& value) {copy(out.bytes,value);}
    void failure(const std::exception& e) {try {error_=e.what();bounded_utf8(error_,8192);}catch(...) {error_="TMA operation failed";}}
    void refresh(Document& doc) {
        ScopeChange change;
        if(!ImGui::GetCurrentContext() || scope_frame_!=ImGui::GetFrameCount()) {scope_frame_=ImGui::GetCurrentContext()?ImGui::GetFrameCount():-1;change=detail::update(scope_,doc,0);}
        const bool changed=change.image || change.source;
        if(changed) {const bool lost_preview=bool(preview_) || (exchange_.valid() && !exchange_exporting_);discarded_=fields_loaded_ && (case_draft_.dirty() || core_draft_.dirty() || labels_draft_.dirty());detail::retire_draft_text(draft_text_);fields_loaded_=false;if(export_.valid()) cancel_.store(true);if(exchange_.valid() || preview_)cancel_exchange();losses_acknowledged_=false;revision_=std::numeric_limits<std::uint64_t>::max();field_slot_=std::numeric_limits<std::size_t>::max();selected_slot_=0;created_.clear();grid_.reset();positions_.clear();present_.clear();error_=lost_preview?"The target image changed. Load a new grid preview.":"";
            if(!explicit_options_) {const auto& m=doc.metadata();const auto sx=double(m.width)/(options_.columns+1),sy=double(m.height)/(options_.rows+1);options_.origin={sx,sy};options_.column_step={sx,0};options_.row_step={0,sy};options_.width=std::min(sx,sy)*.7;options_.height=options_.width;}}
        if(!open_ || revision_==doc.tma_revision()) return;
        grid_=doc.tma_grid();revision_=doc.tma_revision();positions_.clear();present_.clear();selection_revision_=std::numeric_limits<std::uint64_t>::max();
        if(grid_) {const auto columns=grid_->info()->columns();for(const auto& core:grid_->cores()) {positions_.emplace(core.id,core.tma_core->row()*columns+core.tma_core->column());if(!core.tma_core->missing()) present_.push_back(core.id);}if(selected_slot_>=grid_->info()->rows()*columns) selected_slot_=0;}
        else {if(fields_loaded_) {discarded_|=case_draft_.dirty() || core_draft_.dirty() || labels_draft_.dirty();detail::retire_draft_text(draft_text_);fields_loaded_=false;}copy(name_,options_.name);field_slot_=std::numeric_limits<std::size_t>::max();}
    }
    void poll() {
        if(export_.valid() && export_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto value=export_.get();export_state_=value.state;metadata_bytes_=value.bytes;if(!value.error.empty()) error_=std::move(value.error);}catch(const std::exception& e) {failure(e);export_state_="failed";}}
    }
    ui::Availability availability(GuiInputTarget target,bool unavailable,const char* reason,bool blocked,bool has_image,const Project* project) const {
        if(target==GuiInputTarget::CancelQptma || target==GuiInputTarget::CancelTmaMetadata)return {};
        if(!unavailable)return {};
        if(target!=GuiInputTarget::DiscardQptmaPreview) {
            if(!has_image)return {false,"Open an image first."};
            if(blocked)return ui::WorkflowBlocked();
        }
        if(reason)return {false,reason};
        if(exchange_.valid() || (target==GuiInputTarget::ExportTmaMetadata && export_.valid()))return {false,"Wait for the current work to finish."};
        switch(target) {
        case GuiInputTarget::TmaDetectGrid:case GuiInputTarget::LoadQptmaPreview:case GuiInputTarget::CreateTmaGrid:
            if(grid_)return {false,"Clear the existing TMA grid before creating another."};
            return {false,"Choose a file path first."};
        case GuiInputTarget::ApplyQptmaPreview:
            if(grid_)return {false,"Clear the existing TMA grid before creating another."};
            return {false,"Run a preview first."};
        case GuiInputTarget::DiscardQptmaPreview:return {false,"Run a preview first."};
        case GuiInputTarget::OpenTmaBatch:
            if(!project)return {false,"Open a project first."};
            return {false,"The grid has no present cores."};
        case GuiInputTarget::ExportQptma:
            if(!grid_)return {false,"Create or import a TMA grid first."};
            if(!export_review_error_.empty())return {false,"Resolve the grid export error shown above."};
            if(!losses_acknowledged_)return {false,"Acknowledge the omitted metadata first."};
            return {false,"Choose a file path first."};
        case GuiInputTarget::ExportTmaMetadata:return {false,"Choose a file path first."};
        default:return ui::WorkflowBlocked();
        }
    }
    void write_metadata(const Document& doc,const Project* project,std::string path,ExportWritePolicy policy,std::vector<std::string> sources) {
        const auto destination=path;
#ifdef _WIN32
        auto wide=std::filesystem::u8path(path).native();if(wide.rfind(L"\\\\?\\",0)!=0) {if(wide.rfind(L"\\\\",0)==0) wide=L"\\\\?\\UNC\\"+wide.substr(2);else wide=L"\\\\?\\"+wide;path=std::filesystem::path(wide).u8string();}
#endif
      try {
        if(export_.valid()) return;if(!grid_) throw std::invalid_argument("there is no TMA grid");if(!path_.text()[0]) throw std::invalid_argument("enter a new CSV file path");
        const bool summary=case_summary_,unversioned=allow_unversioned_;
        const std::string input=summary && measurements_path_.text()[0]?export_path(measurements_path_.state().resolved):"",value=value_column_.data(),weight=weight_column_.data();
        if(!input.empty()) sources.push_back(input);
        const bool pooling=summary && case_cohort_;TmaCohort cohort;
        if(pooling) {
            if(!project) throw std::invalid_argument("project case pooling requires an open project");
            std::size_t payload=0;constexpr std::size_t maximum=64*1024*1024;
            for(const auto& image:project->images()) {
                const auto document=project->document(image.id);if(!document->tma_core_count()) continue;
                if(cohort.size()>=4096) throw std::length_error("TMA cohort exceeds 4096 grids");
                if(payload>=maximum) throw std::length_error("TMA cohort captures exceed byte limit");
                const auto captured=document->tma_grid(maximum-payload);payload+=captured->payload_bytes();cohort.emplace(image.id,captured);
            }
            if(cohort.empty()) throw std::invalid_argument("project contains no TMA grids");
        }
        const auto snapshot=grid_;cancel_.store(false);metadata_bytes_=0;export_state_="writing";csv_destination_=destination;
        export_=std::async(std::launch::async,[this,snapshot,path,sources,policy,summary,unversioned,input,value,weight,pooling,cohort=std::move(cohort)] {
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
                for(const auto& source:sources) guard_file_output(export_path(source),"",path);result.state="saved";result.bytes=csv.size();output.commit(policy==ExportWritePolicy::ReplaceExisting);
            }catch(const std::exception& e) {result.bytes=0;result.state=cancel_.load()?"cancelled":"failed";try {result.error=e.what();bounded_utf8(result.error,8192);}catch(...) {result.error="TMA CSV export failed";}}
            return result;
        });
      }catch(...) {metadata_bytes_=0;export_state_="failed";throw;}
    }
public:
    std::vector<ui::PathField*> paths(){return {&path_,&measurements_path_,&qptma_input_,&qptma_output_};}
    explicit GuiTma(const GuiOptions& initial):open_(initial.tma_open),explicit_options_(bool(initial.tma_options)),initial_tab_(initial.tma_tab),options_(initial.tma_options?*initial.tma_options:TmaGridOptions{}),initial_case_(initial.tma_case_id),initial_bounds_(initial.tma_core_bounds),delta_(initial.tma_translation) {if(initial_tab_<0 || initial_tab_>2) throw std::invalid_argument("invalid TMA panel tab");options_.validate();copy(insert_label_,initial.tma_insert_label);insert_cores_=initial.tma_insert_cores;insert_missing_=initial.tma_insert_missing;qptma_input_.set_option(initial.qptma_input_path);qptma_output_.set_option(initial.qptma_output_path);exchange_open_=qptma_input_.text()[0] || qptma_output_.text()[0] || bool(initial.tma_dearray_options);dearray_open_=bool(initial.tma_dearray_options);if(initial.tma_dearray_options)dearray_options_=*initial.tma_dearray_options;else if(initial.tma_options){dearray_options_.rows=options_.rows;dearray_options_.columns=options_.columns;dearray_options_.core_width=options_.width;dearray_options_.core_height=options_.height;dearray_options_.name=options_.name;}if(initial.tma_dearray_options)dearray_options_.validate();setup_mode_=initial.tma_dearray_options?0:qptma_input_.text()[0]?1:initial.tma_options?2:0;copy(dearray_name_,dearray_options_.name);copy(name_,options_.name);path_.set_option(initial.tma_metadata_path);case_summary_=initial.tma_case_summary;case_cohort_=initial.tma_case_cohort;allow_unversioned_=initial.tma_allow_unversioned;measurements_path_.set_option(initial.tma_measurements_path);copy(value_column_,initial.tma_value_column);copy(weight_column_,initial.tma_weight_column);bounded_utf8(initial_case_,256);if(initial_bounds_) initial_bounds_->validate();}
    ~GuiTma() {finish(false);}
    void open() {open_=true;}
    void reveal_target(GuiInputTarget target) {
        if(!open_)return;
        if(target==GuiInputTarget::AcknowledgeQptmaLosses || target==GuiInputTarget::ExportQptma || target==GuiInputTarget::ExportTmaMetadata) {initial_tab_=2;activate_tab_=true;reveal_target_=target;}
    }
    static std::weak_ptr<const char> document_identity(const Document& doc){return doc.analysis_identity_.token;}
    static constexpr bool handles(GuiInputTarget target) {return index(target)<input_targets.size();}
    const std::string& qptma_state() const {return exchange_state_;}
    std::shared_ptr<const TmaDearrayResult> dearray_result() const {return dearray_result_;}
    std::size_t qptma_cores() const {return preview_?preview_->cores.size():0;}
    std::size_t qptma_missing() const {return preview_?preview_missing_:0;}
    std::size_t qptma_max_step() const {return preview_max_step_;}
    std::size_t qptma_bytes() const {return exchange_bytes_;}
    void draw_qptma_preview(const Document& doc,ImDrawList* draw,const Viewport& viewport,ImVec2 offset,ImVec2 size) {
        preview_truncated_=false;
        if(!open_ || !preview_ || !preview_index_ || preview_identity_.lock()!=doc.analysis_identity_.token || preview_plane_!=doc.active_plane())return;
        const auto source=doc.server()->reader_source();if(scope_.source_generation!=(source?std::string_view(source->generation):std::string_view{}) && (!source || !scope_.observed_source || !same_reader_files(*scope_.observed_source,*source)))return;
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
    bool draw(Document& doc,ObjectSelection& selection,const Project* project,bool blocked,bool has_image,const std::function<void(GuiInputTarget)>& mark,OutputGate* gate=nullptr,const ui::PathContext& context={}) {
    ui::i18n::SurfaceTrace translation_surface("tool/TMA");
        enabled_.fill(false);
        const bool completing_import=exchange_.valid() && !exchange_exporting_;
        poll();if(!blocked && ui::chord_pressed(ui::CommandId::Tool_Tma,ui::frame_commands())) open_=!open_;
        try {refresh(doc);poll_exchange(doc);if(open_ && grid_)review_qptma(doc);}catch(const std::exception& e) {failure(e);}
        if(gate && gate->notifications())import_notice_.observe(*gate->notifications(),ui::command(ui::CommandId::Tool_Tma).caption,exchange_state_,exchange_state_=="preview ready",exchange_state_=="failed" && completing_import,error_,qptma_input_.state().resolved,import_notice_generation_);
        if(!open_)return false;
        ui::SetNextToolWindow(ui::tool_window_label(ui::ToolId::Tma),{530,780},ui::ToolHeight::Content);
        if(!ImGui::Begin(ui::tool_window_label(ui::ToolId::Tma),&open_)) {ImGui::End();return false;}
        if(!grid_ && discarded_)ui::TextWrapped("Unapplied changes were discarded.");
        auto remember=[&](GuiInputTarget target,bool disabled) {const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();targets_[index(target)]={a.x,a.y,b.x-a.x,b.y-a.y};enabled_[index(target)]=!disabled && ImGui::IsItemVisible();mark(target);
            if(reveal_target_==target) {if(!ImGui::IsItemVisible())ImGui::SetScrollHereY(.5f);else reveal_target_.reset();}
        };
        auto button=[&](const char* text,GuiInputTarget target,bool unavailable,const char* reason=nullptr) {
            const auto available=availability(target,unavailable,reason,blocked,has_image,project);
            auto action=ui::design::Action::Secondary;
            switch(target) {
            case GuiInputTarget::TmaDetectGrid:case GuiInputTarget::LoadQptmaPreview:
            case GuiInputTarget::CreateTmaGrid:case GuiInputTarget::ApplyQptmaPreview:
            case GuiInputTarget::ExportQptma:action=ui::design::Action::Primary;break;
            case GuiInputTarget::ExportTmaMetadata:action=qptma_output_.text()[0]?ui::design::Action::Secondary:ui::design::Action::Primary;break;
            default:break;
            }
            std::string axis_caption;
            if(grid_ && (target==GuiInputTarget::TmaRemoveRow || target==GuiInputTarget::TmaRemoveColumn)) {
                const bool row_axis=target==GuiInputTarget::TmaRemoveRow;const auto& info=*grid_->info();
                const auto axis=row_axis?selected_slot_/info.columns():selected_slot_%info.columns();std::size_t count=0;
                for(std::size_t i=0;i<(row_axis?info.columns():info.rows());++i) if(row_axis?grid_->core(axis,i):grid_->core(i,axis)) ++count;
                std::vector<char> formatted((row_axis?info.row_labels()[axis]:info.column_labels()[axis]).size()+512);std::snprintf(formatted.data(),formatted.size(),ui::i18n::text(ui::workflow_command(text).caption),(row_axis?info.row_labels()[axis]:info.column_labels()[axis]).c_str(),count);
                axis_caption=formatted.data();
            }
            const bool clicked=target==GuiInputTarget::CancelQptma || target==GuiInputTarget::CancelTmaMetadata?
                ui::WorkflowIcon(text,ui::Icon::Stop):ui::WorkflowAction(text,action,available,axis_caption.empty()?nullptr:axis_caption.c_str());
            remember(target,!available.enabled);if(action==ui::design::Action::Primary)ui::WorkflowReason(text,available);return clicked;
        };
        auto perform=[&](auto&& action) {try {error_.clear();action();refresh(doc);}catch(const std::exception& e) {failure(e);}};
        const bool disabled=blocked || !has_image;bool batch=false;
        if(!grid_) {ui::SetNextItemWidth(-1);ui::Combo("Grid setup",&setup_mode_,"Detect cores\0Import QuPath grid\0Manual grid\0");}
            if(!grid_ && setup_mode_==0) {
                ui::TextWrapped("Enter the grid dimensions and approximate core size, then preview the outlines. Entirely absent rows or columns cannot be inferred.");
                ImGui::BeginDisabled(disabled || exchange_.valid() || bool(grid_));
                int rows=static_cast<int>(dearray_options_.rows),columns=static_cast<int>(dearray_options_.columns);
                ui::InputText("Grid name###Detected grid name",dearray_name_.data(),dearray_name_.size());
                if(ui::InputInt("Rows###Detected rows",&rows)) {dearray_options_.rows=static_cast<std::size_t>(std::clamp(rows,1,512));}
                if(ui::InputInt("Columns###Detected columns",&columns)) {dearray_options_.columns=static_cast<std::size_t>(std::clamp(columns,1,512));}
                bool physical=dearray_options_.core_units==TmaCoreUnits::Micrometers;
                if(ui::Checkbox("Core size in micrometers",&physical)) {dearray_options_.core_units=physical?TmaCoreUnits::Micrometers:TmaCoreUnits::Pixels;dearray_options_.pixel_width_um.reset();dearray_options_.pixel_height_um.reset();}
                ui::CompactInputDouble(physical?ui::unit_caption("Width (%s)",ui::unit::um,"Core width (um)").c_str():"Width (px)###Core width (image px)",&dearray_options_.core_width);
                ui::CompactInputDouble(physical?ui::unit_caption("Height (%s)",ui::unit::um,"Core height (um)").c_str():"Height (px)###Core height (image px)",&dearray_options_.core_height);
                if(physical) {
                    if(doc.pixel_width_um() && doc.pixel_height_um())ui::TextWrapped("Image calibration: %.6g x %.6g %s.",*doc.pixel_width_um(),*doc.pixel_height_um(),ui::unit::um_per_px);
                    else ui::DataTextWrapped("Set both image calibration axes before detecting in micrometers.");
                }
                ui::CompactInputDouble("Minimum tissue coverage###Missing tissue fraction",&dearray_options_.missing_density);
                ui::TextDisabled("Cores below this value are marked missing (0–1).");
                bool dark=dearray_options_.polarity==TmaPolarity::DarkOnLight;
                if(ui::Checkbox("Dark cores on light background",&dark)) {dearray_options_.polarity=dark?TmaPolarity::DarkOnLight:TmaPolarity::LightOnDark;}
                if(ImGui::TreeNode("Preview limits")) {
                    ui::InputInt("Preview width (px)",&dearray_options_.thumbnail.max_width);
                    ui::InputInt("Preview height (px)",&dearray_options_.thumbnail.max_height);
                    auto source=static_cast<std::int64_t>(dearray_options_.thumbnail.source_pixel_limit),output=static_cast<std::int64_t>(dearray_options_.thumbnail.output_pixel_limit);
                    if(ui::InputInt64("Source pixel budget",&source)) {dearray_options_.thumbnail.source_pixel_limit=static_cast<std::uint64_t>(std::clamp<std::int64_t>(source,1,67108864));}
                    if(ui::InputInt64("Preview pixel budget",&output)) {dearray_options_.thumbnail.output_pixel_limit=static_cast<std::uint64_t>(std::clamp<std::int64_t>(output,1,16777216));}
                    int mib=static_cast<int>(dearray_options_.work_bytes/(1024*1024));
                    if(ui::InputInt("Working memory (MiB)",&mib)) {dearray_options_.work_bytes=static_cast<std::size_t>(std::clamp(mib,1,512))*1024*1024;}
                    ImGui::TreePop();
                }
                if(ImGui::TreeNode("Component and fit limits")) {
                    ui::CompactInputDouble("Bounds scale",&dearray_options_.bounds_scale);
                    ui::CompactInputDouble("Minimum valid fraction",&dearray_options_.min_valid_fraction);
                    ui::CompactInputDouble("Minimum core area ratio",&dearray_options_.min_component_area);
                    ui::CompactInputDouble("Maximum core area ratio",&dearray_options_.max_component_area);
                    ui::CompactInputDouble("Minimum component extent",&dearray_options_.min_component_extent);
                    ui::CompactInputDouble("Maximum fit residual",&dearray_options_.max_fit_error);
                    ui::CompactInputDouble("Maximum rotation (deg)",&dearray_options_.max_rotation_degrees);
                    int candidates=static_cast<int>(dearray_options_.max_candidates);
                    if(ui::InputInt("Candidate limit",&candidates)) {dearray_options_.max_candidates=static_cast<std::size_t>(std::clamp(candidates,1,10000));}
                    auto pairs=static_cast<std::int64_t>(dearray_options_.pair_checks);
                    if(ui::InputInt64("Pair work limit",&pairs)) {dearray_options_.pair_checks=static_cast<std::uint64_t>(std::clamp<std::int64_t>(pairs,1,50000000));}
                    ImGui::TreePop();
                }
                ImGui::EndDisabled();
                if(button("Detect grid preview",GuiInputTarget::TmaDetectGrid,disabled || exchange_.valid() || bool(grid_)))perform([&]{
                    dearray_options_.name=dearray_name_.data();load_preview(doc,true);
                });
            }
            if(!grid_ && setup_mode_==1) {
            ui::TextWrapped("Adds the grid to this image as one undoable edit. File paths in the grid are not used to open images.");
            ImGui::BeginDisabled(disabled || exchange_.valid());
            qptma_input_.draw(context);ImGui::EndDisabled();
            if(button("Load grid preview",GuiInputTarget::LoadQptmaPreview,disabled || exchange_.valid() || bool(grid_) || !qptma_input_.text()[0]))perform([&]{load_preview(doc);});
            }
        if(!grid_ && setup_mode_==2) {
            int rows=static_cast<int>(options_.rows),columns=static_cast<int>(options_.columns);
            ImGui::BeginDisabled(disabled);ui::InputText("Grid name",name_.data(),name_.size());
            if(ui::InputInt("Rows",&rows)) options_.rows=static_cast<std::size_t>(std::clamp(rows,1,512));if(ui::InputInt("Columns",&columns)) options_.columns=static_cast<std::size_t>(std::clamp(columns,1,512));
            ui::CompactInputDouble("First center X (px)",&options_.origin.x);ui::CompactInputDouble("First center Y (px)",&options_.origin.y);
            ui::CompactInputDouble("Column step X",&options_.column_step.x);ui::CompactInputDouble("Column step Y",&options_.column_step.y);
            ui::CompactInputDouble("Row step X",&options_.row_step.x);ui::CompactInputDouble("Row step Y",&options_.row_step.y);
            ui::CompactInputDouble("Width (px)###Core width (px)",&options_.width);ui::CompactInputDouble("Height (px)###Core height (px)",&options_.height);ImGui::EndDisabled();
        }
        if(!grid_ && setup_mode_==2 && button("Create core grid",GuiInputTarget::CreateTmaGrid,disabled || bool(grid_))) perform([&]{options_.name=name_.data();created_=doc.create_tma_grid(options_);if(!created_.empty()) selection.replace({created_.front()},created_.front());});
            if(exchange_.valid() || preview_ || exchange_state_!="idle") {
            if(exchange_.valid() && button("Cancel grid operation",GuiInputTarget::CancelQptma,false))cancel_exchange();
            if(exchange_state_!="idle")ui::DataTextWrapped("Grid operation: %s",(exchange_exporting_?ui::caption(ui::parse_export_state(exchange_state_),exchange_state_):ui::caption(ui::parse_run_state(exchange_state_),exchange_state_)).c_str());if(exchange_state_=="saved")ui::TextWrapped("%s",qptma_destination_.c_str());
            if(preview_) {
                ui::SeparatorText(dearray_result_?"Detected grid preview":"Imported grid preview");
                if(dearray_result_) ui::DataTextWrapped("Detected candidates: %zu. Fit RMSE: %.4f core diameters. Inferred slots: %zu.",dearray_result_->candidates,dearray_result_->lattice_rmse,dearray_result_->cores.size()-dearray_result_->candidates);
                ui::DataTextWrapped("Target: %lld x %lld pixels. Grid: %zu rows x %zu columns, %zu cores (%zu present / %zu missing).",
                    static_cast<long long>(doc.metadata().width),static_cast<long long>(doc.metadata().height),preview_->rows,preview_->columns,preview_->cores.size(),preview_->cores.size()-preview_missing_,preview_missing_);
                ui::DataTextWrapped("%s",preview_->image_name.c_str());
                ui::DataTextWrapped("Blue outlines preview present cores; gray outlines preview missing cores. No objects are created until Apply.");
                if(preview_truncated_)ui::DataTextWrapped("More than 512 cores are visible. Zoom in to review their outlines; the full grid remains available in the list.");
                ui::LayoutRegion preview_region("Core preview");ui::PaneTitle("Core preview");
                if(ui::BeginCollection("qptma core preview",preview_->cores.size())) {
                    ImGuiListClipper clip;clip.Begin(static_cast<int>(preview_->cores.size()));std::size_t inspected=0;
                    while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd && inspected<512;++i) {
                        ++inspected;const auto& core=preview_->cores[static_cast<std::size_t>(i)];const auto b=core.bounds;
                        const auto label=std::to_string(static_cast<std::size_t>(i)/preview_->columns+1)+"-"+std::to_string(static_cast<std::size_t>(i)%preview_->columns+1)+"  "+core.name+(core.missing?" (missing)":"");
                        ImGui::PushID(i);ui::Selectable(ui::data_label(label,"preview").c_str(),false,ImGuiSelectableFlags_Disabled,{std::max(1.f,ImGui::GetContentRegionAvail().x),0});ImGui::PopID();
                        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("Bounds: %.3f, %.3f, %.3f, %.3f\nCase: %s",b.x,b.y,b.width,b.height,core.case_id.c_str());
                    }
                    preview_max_step_=std::max(preview_max_step_,inspected);
                }ImGui::EndChild();
            if(button("Apply grid preview",GuiInputTarget::ApplyQptmaPreview,disabled || exchange_.valid() || !preview_ || bool(grid_)))perform([&]{
                if(preview_identity_.lock()!=doc.analysis_identity_.token)throw std::invalid_argument("Grid preview is stale. Load it again.");
                created_=import_qptma(doc,*preview_);clear_preview();preview_missing_=0;exchange_state_="imported";losses_acknowledged_=false;
                if(!created_.empty() && doc.object_visible(created_.front()))selection.replace({created_.front()},created_.front());else selection.clear();
            });
            if(button("Discard grid preview",GuiInputTarget::DiscardQptmaPreview,!preview_)) {clear_preview();preview_missing_=0;exchange_state_="idle";}
            }
            }
            ui::Separator();
        if(grid_) {
            const auto snapshot=grid_;const auto& info=*snapshot->info();
            if(const auto found=positions_.find(selection.primary());found!=positions_.end()) selected_slot_=found->second;
            const auto row=selected_slot_/info.columns(),column=selected_slot_%info.columns();auto selected=snapshot->core(row,column);
            if(!fields_loaded_ || field_slot_!=selected_slot_ || field_id_!=(selected?selected->id:0) || fields_document_revision_!=doc.revision()) {
                const bool same_core=field_id_ && selected && field_id_==selected->id;
                const bool same_vacant_owner=!field_id_ && field_slot_==selected_slot_;
                const bool dropped=fields_loaded_ && !same_core && !same_vacant_owner;
                if(dropped)discarded_|=case_draft_.dirty() || core_draft_.dirty() || labels_draft_.dirty();
                if(!fields_loaded_ || dropped)detail::retire_draft_text(draft_text_);
                detail::DraftText<257> case_value;copy(case_value,selected?selected->tma_core->case_id():"");
                LabelsDraft labels;copy(labels.name,info.name());copy(labels.row,info.row_labels()[row]);copy(labels.column,info.column_labels()[column]);
                const auto x=options_.origin.x+row*options_.row_step.x+column*options_.column_step.x,y=options_.origin.y+row*options_.row_step.y+column*options_.column_step.y;
                const detail::GeometryDraft geometry{selected?selected->geometry.bounds():Rect{x-options_.width/2,y-options_.height/2,options_.width,options_.height},0};
                if(!fields_loaded_ || dropped) {case_draft_.reset(case_value);core_draft_.reset(geometry);labels_draft_.reset(labels);}
                else {
                    case_draft_.rebase(case_value);detail::rebase_geometry(core_draft_,geometry);
                    auto& value=labels_draft_.value;const auto& baseline=labels_draft_.baseline;
                    if(std::string_view(value.name.data())==baseline.name.data())value.name=labels.name;
                    if(std::string_view(value.row.data())==baseline.row.data())value.row=labels.row;
                    if(std::string_view(value.column.data())==baseline.column.data())value.column=labels.column;
                    labels_draft_.rebase(labels);
                }
                if(!initial_case_.empty()) {copy(case_,initial_case_);initial_case_.clear();}
                if(initial_bounds_) {edited_=*initial_bounds_;initial_bounds_.reset();}
                fields_loaded_=true;field_id_=selected?selected->id:0;field_slot_=selected_slot_;fields_document_revision_=doc.revision();
            }
            ui::DataTextWrapped("%s / %zu rows x %zu columns",info.name().c_str(),info.rows(),info.columns());
            if(button("Select present cores",GuiInputTarget::SelectTmaPresent,disabled)) perform([&]{selection.replace(present_);});
            if(button("Per-core batch...",GuiInputTarget::OpenTmaBatch,disabled || !project || present_.empty())) perform([&]{selection.replace(present_);batch=true;});
            if(selection_revision_!=selection.revision()) {selected_cores_.clear();for(auto id:selection.ids()) if(positions_.count(id)) selected_cores_.push_back(id);if(selected_cores_.empty() && selected) selected_cores_.push_back(selected->id);selection_revision_=selection.revision();}
            ui::TextWrapped("Core %s-%s / %zu selected",info.row_labels()[row].c_str(),info.column_labels()[column].c_str(),selected_cores_.size());
            auto tab=[&](const char* title,int value,GuiInputTarget target) {const bool shown=ImGui::BeginTabItem(title,nullptr,activate_tab_ && initial_tab_==value?ImGuiTabItemFlags_SetSelected:0);remember(target,disabled);return shown;};
            if(ImGui::BeginTabBar("TMA views")) {
                if(tab("Cores",0,GuiInputTarget::TmaCoreTab)) {
                    std::size_t inspected=0;
                    ui::LayoutRegion cores_region("Core catalog");ui::PaneTitle("Cores");
                    if(ui::BeginCollection("core catalog",info.rows()*info.columns())) {
                        ImGuiListClipper clip;clip.Begin(static_cast<int>(info.rows()*info.columns()));while(clip.Step()) for(int slot=clip.DisplayStart;slot<clip.DisplayEnd && inspected<512;++slot) {
                            ++inspected;const auto r=std::size_t(slot)/info.columns(),c=std::size_t(slot)%info.columns();const auto core=snapshot->core(r,c);
                            const auto text=(core?core->name:info.row_labels()[r]+"-"+info.column_labels()[c])+(!core?" (vacant)":core->tma_core->missing()?" (missing)":"");ImGui::PushID(slot);
                            if(ui::Selectable(ui::data_label(text,"core").c_str(),std::size_t(slot)==selected_slot_,0,{std::max(1.f,ImGui::GetContentRegionAvail().x),0})) perform([&]{if(core) selection.replace({core->id},core->id);else selection.clear();selected_slot_=static_cast<std::size_t>(slot);});ImGui::PopID();
                        }
                    }ImGui::EndChild();catalog_max_=std::max(catalog_max_,inspected);
                    bool missing=selected && selected->tma_core->missing();ImGui::BeginDisabled(disabled || !selected);if(ui::Checkbox("Missing / insufficient tissue",&missing)) perform([&]{doc.set_tma_missing(selected_cores_,missing);discarded_=false;});remember(GuiInputTarget::ToggleTmaMissing,disabled || !selected);ImGui::EndDisabled();if(ImGui::GetItemRectMax().x+ImGui::CalcTextSize(ui::i18n::text("Applies to %zu cores.")).x+40<ImGui::GetCurrentWindow()->WorkRect.Max.x)ImGui::SameLine();ui::TextWrapped("Applies to %zu cores.",selected_cores_.size());
                    ImGui::BeginDisabled(disabled);if(ui::InputText("Case ID",case_.data(),case_.size()))discarded_=false;draft_text_.observe();ui::trace_workflow_action("Case ID","Case ID");ImGui::EndDisabled();
                    const bool case_changes=std::any_of(selected_cores_.begin(),selected_cores_.end(),[&](auto id){return doc.objects().at(id).tma_core->case_id()!=case_.data();});
                    const auto apply_reason=[&](bool changes) {return disabled?ui::WorkflowBlocked():ui::DraftAvailability(changes);};
                    const auto case_available=apply_reason(case_changes);
                    if(button("Apply case ID to selected cores",GuiInputTarget::ApplyTmaCase,!case_available.enabled,case_available.reason)) perform([&]{std::map<std::uint64_t,std::string> values;for(auto id:selected_cores_) values.emplace(id,case_.data());doc.set_tma_case_ids(values);case_draft_.reset(case_);discarded_=false;});if(ImGui::GetItemRectMax().x+ImGui::CalcTextSize(ui::i18n::text("Applies to %zu cores.")).x+40<ImGui::GetCurrentWindow()->WorkRect.Max.x)ImGui::SameLine();ui::TextWrapped("Applies to %zu cores.",selected_cores_.size());
                    const auto before_geometry=core_draft_.value;
                    ImGui::BeginDisabled(disabled);ui::DraftInputDouble("X (px)###Core minimum X (px)",&edited_.x,core_inputs_[0]);draft_text_.observe();ui::DraftInputDouble("Y (px)###Core minimum Y (px)",&edited_.y,core_inputs_[1]);draft_text_.observe();ui::DraftInputDouble("Width (px)###Core width (px)",&edited_.width,core_inputs_[2]);draft_text_.observe();ui::DraftInputDouble("Height (px)###Core height (px)",&edited_.height,core_inputs_[3]);draft_text_.observe();ImGui::EndDisabled();
                    if(!(before_geometry==core_draft_.value))discarded_=false;
                    const auto core_available=apply_reason(!selected || !detail::same_bounds(edited_,selected->geometry.bounds()));
                    if(button(selected?"Apply core geometry":"Add core in vacant slot",GuiInputTarget::ApplyTmaGeometry,!core_available.enabled,core_available.reason)) perform([&]{if(selected) doc.update_roi(selected->id,Roi::ellipse(edited_));else {const auto id=doc.add_tma_core(row,column,edited_,case_.data());selection.replace({id},id);}core_draft_.reset(core_draft_.value);discarded_=false;});
                    if(discarded_)ui::TextWrapped("Unapplied changes were discarded.");
                    ImGui::EndTabItem();
                }
                if(tab("Grid edits",1,GuiInputTarget::TmaGridTab)) {
                    const auto before_labels=labels_draft_.value;
                    ImGui::BeginDisabled(disabled);
                    ui::InputText("Grid name",name_.data(),name_.size());draft_text_.observe();ui::trace_workflow_action("Grid name","Grid name");
                    ui::InputText("Row label",row_label_.data(),row_label_.size());draft_text_.observe();ui::trace_workflow_action("Row label","Row label");
                    ui::InputText("Column label",column_label_.data(),column_label_.size());draft_text_.observe();ui::trace_workflow_action("Column label","Column label");
                    if(ui::Checkbox("Update core names from labels",&rename_names_))discarded_=false;ImGui::EndDisabled();
                    if(!(before_labels==labels_draft_.value))discarded_=false;
                    const bool label_changes=(name_[0] && info.name()!=name_.data()) || info.row_labels()[row]!=row_label_.data() || info.column_labels()[column]!=column_label_.data() ||
                        (rename_names_ && std::any_of(snapshot->cores().begin(),snapshot->cores().end(),[&](const auto& core){const std::string_view r=core.tma_core->row()==row?std::string_view(row_label_.data()):std::string_view(info.row_labels()[core.tma_core->row()]);const std::string_view c=core.tma_core->column()==column?std::string_view(column_label_.data()):std::string_view(info.column_labels()[core.tma_core->column()]);return !tma_label_name_matches(core.name,r,c);}));
                    const auto labels_available=disabled?ui::WorkflowBlocked():ui::DraftAvailability(label_changes);
                    if(button("Apply labels",GuiInputTarget::ApplyTmaLabels,!labels_available.enabled,labels_available.reason)) perform([&]{auto rows=info.row_labels(),columns=info.column_labels();rows[row]=row_label_.data();columns[column]=column_label_.data();doc.set_tma_labels(name_.data(),rows,columns,rename_names_);labels_draft_.reset(labels_draft_.value);discarded_=false;});
                    if(discarded_)ui::TextWrapped("Unapplied changes were discarded.");
                    if(ui::CollapsingHeader("Rows and columns",ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::BeginDisabled(disabled);
                        ui::InputText("New axis label (optional)",insert_label_.data(),insert_label_.size());
                        ui::Checkbox("Fill inserted slots with cores",&insert_cores_);
                        ImGui::BeginDisabled(!insert_cores_);ui::Checkbox("Mark new cores missing",&insert_missing_);ImGui::EndDisabled();ImGui::EndDisabled();
                        ui::DataTextWrapped("Existing cores stay in place. New positions use neighboring cores; leave slots vacant when spacing cannot be inferred.");
                        auto insert=[&](bool column_axis,std::size_t at) {
                            const auto bounds=insert_cores_?std::optional<std::vector<Rect>>{}:std::optional<std::vector<Rect>>(std::vector<Rect>{});
                            created_=column_axis?doc.insert_tma_column(at,insert_label_.data(),bounds,insert_missing_):doc.insert_tma_row(at,insert_label_.data(),bounds,insert_missing_);
                            losses_acknowledged_=false;field_slot_=std::numeric_limits<std::size_t>::max();
                            if(!created_.empty()) selection.replace({created_.front()},created_.front());
                        };
                        if(button("Add row above",GuiInputTarget::TmaInsertRowBefore,disabled)) perform([&]{insert(false,row);});ImGui::SameLine();
                        if(button("Add row below",GuiInputTarget::TmaInsertRowAfter,disabled)) perform([&]{insert(false,row+1);});
                        if(button("Add column left",GuiInputTarget::TmaInsertColumnBefore,disabled)) perform([&]{insert(true,column);});ImGui::SameLine();
                        if(button("Add column right",GuiInputTarget::TmaInsertColumnAfter,disabled)) perform([&]{insert(true,column+1);});
                        ui::TextWrapped("Removing an axis deletes its cores and their child objects. Cores belonging to other axes are kept.");
                        if(button("Remove row",GuiInputTarget::TmaRemoveRow,disabled)) perform([&]{doc.remove_tma_row(row);losses_acknowledged_=false;});ImGui::SameLine();
                        if(button("Remove column",GuiInputTarget::TmaRemoveColumn,disabled)) perform([&]{doc.remove_tma_column(column);losses_acknowledged_=false;});
                    }
                    ImGui::BeginDisabled(disabled);ui::CompactInputDouble("Move X (px)",&delta_.x);ui::CompactInputDouble("Move Y (px)",&delta_.y);ImGui::EndDisabled();
                    if(button("Move grid and child objects",GuiInputTarget::TranslateTmaGrid,disabled)) perform([&]{doc.translate_tma_grid(delta_.x,delta_.y);});
                    if(button("Dissolve grid (keep objects)",GuiInputTarget::ClearTmaGrid,disabled)) perform([&]{doc.clear_tma_grid();});
                    ImGui::EndTabItem();
                }
                if(tab("Export",2,GuiInputTarget::TmaExportTab)) {
                    ui::SeparatorText("QuPath grid");
                    ui::TextWrapped("Core names, bounds, missing flags and case IDs are kept.");
                    if(!export_omissions_.empty()) {
                        ui::TextUnformatted("Not included:");
                        for(const auto& omission:export_omissions_) {ui::TextWrapped("• %s",ui::i18n::text(omission_caption(omission)));ui::trace_workflow_action(("qptma-omission-"+omission).c_str(),omission_caption(omission));}
                    }
                    if(!export_review_error_.empty()) {ui::DataTextWrapped("%s",export_review_error_.c_str());ui::trace_workflow_action("qptma-review-error",export_review_error_.c_str());}
                    ImGui::BeginDisabled(disabled || exchange_.valid() || !export_review_error_.empty());
                    ui::Checkbox("I acknowledge the omitted metadata",&losses_acknowledged_);remember(GuiInputTarget::AcknowledgeQptmaLosses,disabled || exchange_.valid() || !export_review_error_.empty());ImGui::EndDisabled();
                    ImGui::BeginDisabled(disabled || exchange_.valid());
                    qptma_output_.draw(context);ImGui::EndDisabled();
                    if(button("Export QuPath grid",GuiInputTarget::ExportQptma,disabled || exchange_.valid() || !grid_ || !export_review_error_.empty() || !losses_acknowledged_ || !qptma_output_.text()[0]))perform([&]{if(output_gate_available(gate,error_))gate->submit(qptma_output_.state().resolved,output_scope("Export QuPath grid",false,ui::command(ui::CommandId::Tool_Tma).caption),
                        [this,gate,document=&doc,project](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& paths) {
                            write_qptma(*document,project,path,policy,paths);
                            gate->watch(path,[this]()->std::optional<bool>{if(exchange_.valid())return {};return exchange_state_=="saved";},[this]{return exchange_state_=="failed"?error_:std::string{};});
                        },[this](const std::string& message){error_=message;},false,{},qptma_input_.text()[0]?std::vector<std::string>{qptma_input_.state().resolved}:std::vector<std::string>{});});
                    ui::Separator();
                    ImGui::BeginDisabled(export_.valid());
                    ui::Checkbox("Summarize replicate cores by case",&case_summary_);
                    if(case_summary_) {
                        ImGui::BeginDisabled(!project);ui::Checkbox("Pool TMA grids across project",&case_cohort_);ImGui::EndDisabled();
                        if(case_cohort_) ui::DataTextWrapped("Capture every project image with a TMA grid. Measurement rows must include image_id; core IDs are local to each image.");
                        ui::DataTextWrapped("Missing cores contribute no measurements. Cores without a case ID stay separate. Leave the input empty for case inventory only.");
                        ui::DataText("Measurement CSV (optional)");
                        measurements_path_.draw(context);
                        ui::InputText("Value column",value_column_.data(),value_column_.size());
                        ui::InputText("Weight column (optional)",weight_column_.data(),weight_column_.size());
                        ui::Checkbox("Allow input without core revisions",&allow_unversioned_);
                    } else ui::DataTextWrapped("Metadata CSV includes present, missing and vacant slots. It captures the grid when requested. Existing files ask before replacement.");
                    ui::DataText("CSV file");
                    path_.draw(context);ImGui::EndDisabled();
                    if(button(case_summary_?"Export case summary CSV":"Export core metadata CSV",GuiInputTarget::ExportTmaMetadata,disabled || export_.valid() || !path_.text()[0])) perform([&]{if(output_gate_available(gate,error_))gate->submit(path_.state().resolved,output_scope(case_summary_?"Export case summary CSV":"Export core metadata CSV",case_summary_ && case_cohort_,ui::command(ui::CommandId::Tool_Tma).caption),
                        [this,gate,document=&doc,project](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& paths) {
                            write_metadata(*document,project,path,policy,paths);
                            gate->watch(path,[this]()->std::optional<bool>{if(export_.valid())return {};return export_state_=="saved";},[this]{return export_state_=="failed"?error_:std::string{};});
                        },[this](const std::string& message){error_=message;},false,case_summary_?ui::i18n::text("This file already exists and this export never replaces a file. Choose a new file name."):"",case_summary_ && measurements_path_.text()[0]?std::vector<std::string>{measurements_path_.state().resolved}:std::vector<std::string>{},case_summary_ && case_cohort_);});
                    if(export_.valid() && button("Cancel CSV export",GuiInputTarget::CancelTmaMetadata,false)) cancel_.store(true);
                    ui::DataTextWrapped("CSV export: %s",ui::caption(ui::parse_export_state(export_state_),export_state_).c_str());if(export_state_=="saved")ui::TextWrapped("%s",csv_destination_.c_str());ImGui::EndTabItem();
                }
                ImGui::EndTabBar();activate_tab_=false;
            }

        }
        if(!error_.empty()) ui::DataTextWrapped("%s",error_.c_str());ImGui::End();return batch;
    }
};
}
