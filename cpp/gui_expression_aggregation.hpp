#pragma once
#include "gui_tool_scope.hpp"
#include "gui_output_gate.hpp"
#include "gui_path_field.hpp"
#include "qupathxx/expression_aggregation.hpp"
#include "qupathxx/landmark_registration.hpp"
#include <chrono>
#include <cstdio>
#include <future>

namespace qpx::detail {
// Frontend state only. All membership, reduction, replay and publication use
// the existing native services. Worker closures own snapshots, never a Document.
class GuiExpressionAggregation {
public:
    enum class Action {Capture,Run,Stop,Keep,Reopen,Remove,Export,Membership,Count};
    struct CohortDraft {
        std::uint64_t roi=0;
        std::array<char,257> id{},sample{},replicate{},group{};
    };
private:
    struct Completion {
        std::shared_ptr<const ExpressionAggregationResult> result;
        std::string exported;
    };
    std::future<Completion> pending_;
    std::atomic<bool> cancelled_{false};
    std::shared_ptr<const ExpressionAggregationResult> result_;
    struct Context {
        ToolScope scope;const Project* project=nullptr;
        std::weak_ptr<ImageServer> server;std::string map;
    } owner_,pending_owner_;
    struct ExportState {std::optional<bool> done;std::string error;};
    std::shared_ptr<ExportState> export_=std::make_shared<ExportState>();
    std::string error_,state_="idle";
    bool cancel_requested_=false,exporting_=false;
    std::vector<CohortDraft> drafts_;
    std::vector<std::string> records_,normalizations_;
    std::uint64_t records_revision_=UINT64_MAX,records_generation_=0;
    std::string records_source_;
    int record_=0,normalization_=0,feature_=0;
    bool pseudobulk_=false,independent_=false,registered_=false,normalized_=false;
    std::array<Rect,std::size_t(Action::Count)> bounds_{};
    std::array<bool,std::size_t(Action::Count)> enabled_{};
    static constexpr std::size_t work=256u*1024*1024;
    static std::string ordinal(const char* format,int index) {
        char text[128];std::snprintf(text,sizeof(text),ui::i18n::text(format),index+1);return text;
    }

    bool current(const Document& doc,const Project* project,const ExpressionMatrixSpatialScope* map) const {
        return result_ && project==owner_.project && map && owner_.map==map->sha256() &&
            result_->source_sha256()==map->matrix_source_sha256 &&
            tool_document_identity(doc).lock()==owner_.scope.document_identity.lock() &&
            doc.server()==owner_.server.lock() && doc.active_plane()==owner_.scope.plane &&
            doc.revision()==owner_.scope.document_revision;
    }
    template<class Work> void start(Work task,bool exporting=false) {
        if(busy())throw std::logic_error("Wait for the current work to finish.");
        cancelled_=false;cancel_requested_=false;exporting_=exporting;
        if(exporting){export_->done.reset();export_->error.clear();}
        else export_=std::make_shared<ExportState>();
        pending_=std::async(std::launch::async,[this,task=std::move(task)]() mutable{return task(&cancelled_);});
        state_="running";error_.clear();
    }
    void stamp(const Document& doc,const Project* project,std::uint64_t image,const ExpressionMatrixSpatialScope& map) {
        pending_owner_={};detail::update(pending_owner_.scope,doc,image);
        pending_owner_.project=project;pending_owner_.server=doc.server();pending_owner_.map=map.sha256();
    }
    void run(Project& project,std::uint64_t image,const ExpressionMatrixSpatialScope& map,
             std::shared_ptr<const ExpressionMatrixSource> matrix) {
        const auto doc=project.document(image);
        if(!matrix || drafts_.empty())throw std::invalid_argument("Choose a full indexed matrix and select regions first.");
        const auto request=ExpressionMatrixSpatialRequest::capture(*doc,matrix,map.frame_id,map.plane,work);
        const auto target=image_coordinate_frame(*doc,map.frame_id,map.plane);
        std::vector<ExpressionRegion> regions;regions.reserve(drafts_.size());
        for(const auto& draft:drafts_) {
            const auto object=doc->annotation(draft.roi);
            if(object.kind!=ObjectKind::Annotation || !object.geometry.is_area() || object.plane!=map.plane)
                throw std::invalid_argument("Expression region requires an area annotation on the selected plane");
            regions.push_back({draft.id.data(),object.geometry,draft.sample.data(),draft.replicate.data(),draft.group.data()});
        }
        std::optional<RegistrationDefinition> registration;
        if(registered_) {
            const auto id=active_registration_id(doc->image_data_properties());
            if(!id || !(registration=stored_registration(doc->image_data_properties(),*id)))
                throw std::invalid_argument("No accepted registration is available.");
        }
        ExpressionAggregationOptions options;
        options.kind=pseudobulk_?ExpressionAggregationKind::Pseudobulk:ExpressionAggregationKind::RegionalSummary;
        options.overlap=independent_?ExpressionOverlapPolicy::Independent:ExpressionOverlapPolicy::Reject;
        if(normalized_ && !normalizations_.empty())options.normalization_recipe=
            expression_normalization_recipe(project,image,normalizations_.at(std::size_t(normalization_)));
        stamp(*doc,&project,image,map);
        start([request,regions=std::move(regions),target,registration,options](const std::atomic<bool>* c) mutable {
            // Charge owner-captured state while native spatial/reduction buffers
            // are live. The native services independently admit their own work.
            std::size_t fixed=request.payload_bytes()+65536;
            workflow_capture_charge(fixed,regions.capacity()*sizeof(ExpressionRegion),work);
            for(const auto& row:regions)workflow_capture_charge(fixed,row.geometry.payload_bytes()+row.id.capacity()+row.sample.capacity()+row.replicate.capacity()+row.group.capacity(),work);
            const auto spatial=request.resolve(work-fixed,c);
            const auto cohorts=expression_region_cohorts(spatial,regions,target,options.overlap,registration,work-fixed,100000000,c);
            workflow_capture_charge(fixed,spatial.payload_bytes(),work);
            options.work_bytes=work-fixed;
            return Completion{aggregate_expression(request.source(),cohorts,options,c),{}};
        });
    }
    void reopen(Project& project,std::uint64_t image,const ExpressionMatrixSpatialScope& map) {
        const auto request=expression_aggregation_open_request(project,image,records_.at(std::size_t(record_)));
        stamp(*project.document(image),&project,image,map);
        start([request](const std::atomic<bool>* c){return Completion{request.open(work,c),{}};});
    }
    void export_csv(bool membership,OutputGate& gate,const ui::PathField& field) {
        const auto result=result_;
        export_=std::make_shared<ExportState>();const auto progress=export_;
        gate.submit(field.state().resolved,output_scope(membership?"Membership CSV":"Summary CSV",false,"Genes"),
            [this,result,membership,progress,&gate](const std::string& path,ExportWritePolicy,const std::vector<std::string>& paths) {
                start([result,membership,path,paths](const std::atomic<bool>* c) {
                    guard_output_intent(path,paths);
                    if(membership)save_expression_membership_csv(*result,path,work,work,c);
                    else save_expression_aggregation_csv(*result,path,work,c);
                    return Completion{{},path};
                },true);
                gate.watch(path,[progress]{return progress->done;},[progress]{return progress->error;},"Genes");
            },[progress](const std::string& text){progress->error=text;progress->done=false;},false,
            ui::i18n::text("Choose a new file; this export cannot replace an existing file."),
            {result->parent()->path()});
    }
    void results() {
        if(!result_)return;
        ui::PaneTitle("Results");
        ui::DataTextWrapped("%s",result_->kind()==ExpressionAggregationKind::Pseudobulk?ui::i18n::text("Pseudobulk"):ui::i18n::text("Regional summaries"));
        ui::DataTextWrapped("%s",result_->value_domain().c_str());
        ui::InputInt("Feature index (0 based)",&feature_);
        feature_=std::clamp(feature_,0,int(result_->features().size()-1));
        ui::DataTextWrapped("%s",result_->features()[std::size_t(feature_)].id.c_str());
        const auto& cohorts=result_->cohorts();
        for(std::size_t i=0;i<cohorts.size();++i) {
            ImGui::PushID(int(i));const auto& cohort=cohorts[i];
            ui::DataTextWrapped("%s",cohort.id.c_str());
            ui::Text("Observations: %zu",cohort.observations.size());
            if(result_->domain()==ExpressionValueDomain::Counts)
                ui::Text("Sum: %llu",static_cast<unsigned long long>(result_->counts()[std::size_t(feature_)*cohorts.size()+i]));
            else ui::Text("Sum: %.12g",result_->continuous()[std::size_t(feature_)*cohorts.size()+i]);
            if(const auto mean=result_->mean(std::size_t(feature_),i))ui::Text("Mean: %.12g",*mean);
            else ui::TextUnformatted("Mean: undefined (empty region)");
            ImGui::PopID();
        }
    }
public:
    ~GuiExpressionAggregation(){cancelled_=true;if(pending_.valid()){pending_.wait();poll();}}
    bool busy() const {return pending_.valid();}
    void cancel(){cancel_requested_=true;cancelled_=true;}
    void poll() {
        if(!pending_.valid() || pending_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try {
            auto completed=pending_.get();
            // A completed atomic export stays successful even if Stop arrives
            // afterwards. Unpublished analysis previews may still be discarded.
            if(!completed.exported.empty()){export_->done=true;state_="saved";}
            else if(cancel_requested_)state_="cancelled";
            else {result_=std::move(completed.result);owner_=pending_owner_;feature_=0;state_="ready";}
        }catch(const WorkflowCancelled&){if(exporting_)export_->done=false;state_="cancelled";}
        catch(const std::exception& e){error_=e.what();if(exporting_){export_->done=false;export_->error=error_;}state_="failed";}
    }
    void capture(const Document& doc,const std::vector<std::uint64_t>& selected) {
        if(selected.empty() || selected.size()>64)throw std::invalid_argument("Select 1 to 64 area annotations on this plane.");
        std::vector<CohortDraft> next;next.reserve(selected.size());
        for(auto id:selected) {
            const auto object=doc.annotation(id);
            if(object.kind!=ObjectKind::Annotation || !object.geometry.is_area() || object.plane!=doc.active_plane())
                throw std::invalid_argument("Expression region requires an area annotation on the selected plane");
            const auto old=std::find_if(drafts_.begin(),drafts_.end(),[&](const auto& row){return row.roi==id;});
            if(old!=drafts_.end())next.push_back(*old);
            else {CohortDraft row;row.roi=id;const auto name="roi-"+std::to_string(id);std::copy(name.begin(),name.end(),row.id.begin());next.push_back(row);}
        }
        drafts_=std::move(next);
    }
    const std::shared_ptr<const ExpressionAggregationResult>& result() const{return result_;}
    const std::string& error() const{return error_.empty()?export_->error:error_;}
    const std::string& state() const{return state_;}
    Rect bounds(Action action) const{return bounds_.at(std::size_t(action));}
    bool enabled(Action action) const{return enabled_.at(std::size_t(action));}
    std::vector<CohortDraft>& drafts(){return drafts_;}
    void pseudobulk(bool value){pseudobulk_=value;if(value){independent_=false;normalized_=false;}}
    void draw(Project* project,std::uint64_t image,const ExpressionMatrixSpatialScope* map,
              std::shared_ptr<const ExpressionMatrixSource> matrix,const ObjectSelection* selection,
              bool blocked,OutputGate* gate,ui::PathField& csv,ui::PathField& memberships,const ui::PathContext& context) {
        bounds_={};enabled_={};
        const auto doc=project && image?project->document(image):nullptr;
        const bool bound=doc && map && matrix && doc->active_plane()==map->plane;
        const auto source_id=matrix?matrix->metadata().source_sha256:std::string{};
        if(doc && (records_revision_!=doc->revision() || records_generation_!=project->generation() || records_source_!=source_id)) {
            records_=expression_aggregation_ids(*project,image);
            normalizations_=matrix?expression_normalization_ids(*project,image,matrix->metadata().source_sha256):std::vector<std::string>{};
            records_revision_=doc->revision();records_generation_=project->generation();
            records_source_=source_id;
            record_=records_.empty()?0:std::clamp(record_,0,int(records_.size()-1));
            normalization_=normalizations_.empty()?0:std::clamp(normalization_,0,int(normalizations_.size()-1));
        }
        auto button=[&](Action action,const char* label,bool available,auto call,const char* reason="Choose a full indexed matrix and select regions first.") {
            if(action!=Action::Stop && blocked)reason="Wait for the current work to finish.";
            else if(action!=Action::Stop && busy())reason="Wait for the current work to finish.";
            const ui::Availability status{available,available?nullptr:reason};
            const bool clicked=ui::AvailableButton(label,status,action==Action::Run?ui::design::Action::Primary:ui::design::Action::Secondary);
            const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds_[std::size_t(action)]={a.x,a.y,b.x-a.x,b.y-a.y};
            enabled_[std::size_t(action)]=available && ImGui::IsItemVisible();
            if(clicked)try{call();error_.clear();}catch(const std::exception& e){error_=e.what();}
        };
        ui::TextWrapped("All source features; displayed genes do not limit this analysis.");
        const bool edit=bound && !blocked && !busy();
        button(Action::Capture,"Use selected regions",edit && selection,[&]{capture(*doc,selection->ids());});
        ImGui::BeginDisabled(!edit);
        if(ui::Checkbox("Pseudobulk",&pseudobulk_) && pseudobulk_){independent_=false;normalized_=false;}
        if(pseudobulk_)ui::TextWrapped("Raw counts, disjoint regions, explicit sample / replicate / group labels.");
        else {
            ui::Checkbox("Allow overlapping regions",&independent_);
            if(!normalizations_.empty()) {
                ui::Checkbox("Use kept normalization",&normalized_);
                if(normalized_)if(ui::ComboScope combo{"Normalization",ordinal("Normalization %d",normalization_).c_str()};combo)
                    for(std::size_t i=0;i<normalizations_.size();++i)if(ui::Selectable(ui::data_label(ordinal("Normalization %d",int(i)),std::to_string(i)).c_str(),i==std::size_t(normalization_)))normalization_=int(i);
            }else normalized_=false;
        }
        ui::Checkbox("Use accepted registration",&registered_);
        for(auto& row:drafts_) {
            ImGui::PushID(std::to_string(row.roi).c_str());
            ui::InputText("Cohort",row.id.data(),row.id.size());
            if(pseudobulk_) {ui::InputText("Sample",row.sample.data(),row.sample.size());ui::InputText("Replicate",row.replicate.data(),row.replicate.size());ui::InputText("Group",row.group.data(),row.group.size());}
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        button(Action::Run,"Run aggregation",edit && !drafts_.empty(),[&]{run(*project,image,*map,matrix);});
        button(Action::Stop,"Stop",busy(),[&]{cancel();},"Nothing is running.");
        if(result_ && doc && !current(*doc,project,map))ui::TextWrapped("Inputs changed; run again before keeping.");
        const bool already_kept=result_ && std::find(records_.begin(),records_.end(),result_->id())!=records_.end();
        button(Action::Keep,"Keep aggregation",edit && !already_kept && current(*doc,project,map),[&]{keep_expression_aggregation(*project,image,*result_);owner_.scope.document_revision=doc->revision();},already_kept?"This aggregation is already kept.":"Inputs changed; run again before keeping.");
        if(!records_.empty()) {
            if(ui::ComboScope combo{"Kept aggregation",ordinal("Aggregation %d",record_).c_str()};combo)
                for(std::size_t i=0;i<records_.size();++i)if(ui::Selectable(ui::data_label(ordinal("Aggregation %d",int(i)),std::to_string(i)).c_str(),i==std::size_t(record_)))record_=int(i);
            button(Action::Reopen,"Reopen aggregation",edit,[&]{reopen(*project,image,*map);});
            button(Action::Remove,"Remove aggregation",edit,[&]{const bool was_current=current(*doc,project,map);remove_expression_aggregation(*project,image,records_[std::size_t(record_)]);if(was_current)owner_.scope.document_revision=doc->revision();});
        }
        results();
        if(!error().empty())ui::DataTextWrapped("%s",error().c_str());
        if(busy())ui::TextUnformatted("Running...");else if(state_=="cancelled")ui::TextUnformatted("Cancelled");
        csv.draw(context,blocked || busy());
        button(Action::Export,"Export summaries CSV",result_ && gate && !blocked && !busy() && csv.state().valid,[&]{export_csv(false,*gate,csv);},"Run an aggregation and choose a new CSV path first.");
        memberships.draw(context,blocked || busy());
        button(Action::Membership,"Export memberships CSV",result_ && gate && !blocked && !busy() && memberships.state().valid,[&]{export_csv(true,*gate,memberships);},"Run an aggregation and choose a new CSV path first.");
    }
};
} // namespace qpx::detail
