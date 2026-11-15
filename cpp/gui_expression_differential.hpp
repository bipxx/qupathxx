#pragma once
#include "gui_molecular_job.hpp"
#include "qupathxx/expression_differential.hpp"

namespace qpx::detail {
class GuiExpressionDifferential {
public:
    enum class Action {OpenBulk,Run,Stop,Keep,Reopen,Remove,Genes,Replicates,Count};
private:
    GuiMolecularJob<ExpressionAggregationResult> bulk_;
    GuiMolecularJob<ExpressionDifferentialResult> job_;
    MolecularActions<Action> actions_;
    std::vector<std::string> aggregations_,records_;
    std::uint64_t revision_=UINT64_MAX,generation_=0;
    int aggregation_=0,record_=0,feature_=0;
    std::array<char,257> group_a_{},group_b_{};
    double target_=10000;
    static const char* caption(ExpressionDifferentialStatus value){
        switch(value){
        case ExpressionDifferentialStatus::Tested:return "Tested";
        case ExpressionDifferentialStatus::AllZero:return "All zero";
        case ExpressionDifferentialStatus::InsufficientReplicates:return "Insufficient biological replicates";
        case ExpressionDifferentialStatus::ZeroVariance:return "Zero variance";
        case ExpressionDifferentialStatus::TailUnderflow:return "Tail underflow (p and q rounded to zero)";
        }return "Unknown status";
    }
    void results(){
        const auto result=job_.result();if(!result)return;
        ui::PaneTitle("Results");ui::TextWrapped("Unpaired Welch; log1p pseudobulk; BH over all testable genes.");
        ui::DataTextWrapped("%s / %s",result->options().group_a.c_str(),result->options().group_b.c_str());
        ui::Text("Result library target: %.17g",result->options().library_target);
        ui::Text("Replicates A / B: %zu / %zu",result->n_a(),result->n_b());ui::Text("BH correction features: %zu",result->correction_features());
        ui::InputInt("Feature index (0 based)",&feature_);feature_=std::clamp(feature_,0,int(result->genes().size()-1));
        ui::DataTextWrapped("%s",result->parent()->features()[std::size_t(feature_)].id.c_str());const auto& gene=result->genes()[std::size_t(feature_)];
        ui::TextUnformatted(caption(gene.status));ui::Text("Mean A: %.17g",gene.mean_a);ui::Text("Mean B: %.17g",gene.mean_b);ui::Text("Effect (mean log1p B minus A): %.17g",gene.effect);
        if(gene.p)ui::Text("p: %.17g",*gene.p);else ui::TextUnformatted("p: absent");
        if(gene.q)ui::Text("q: %.17g",*gene.q);else ui::TextUnformatted("q: absent");
        if(ui::CollapsingHeader("Replicate associations"))for(const auto& r:result->replicates()){
            ui::DataTextWrapped("%s / %s / %s",r.id.c_str(),r.sample.c_str(),r.group.c_str());
            ui::Text("Raw library / observations: %llu / %llu",static_cast<unsigned long long>(r.library),static_cast<unsigned long long>(r.observations));
        }
    }
public:
    bool busy() const{return bulk_.busy() || job_.busy();}
    void cancel(){bulk_.cancel();job_.cancel();}
    void poll(){bulk_.poll();job_.poll();}
    const auto& result() const{return job_.result();}
    const auto& parent() const{return bulk_.result();}
    const std::string& error() const{return job_.error().empty()?bulk_.error():job_.error();}
    Rect bounds(Action a) const{return actions_.bounds[std::size_t(a)];}
    bool enabled(Action a) const{return actions_.enabled[std::size_t(a)];}
    void groups(const std::string& a,const std::string& b){
        if(a.size()>=group_a_.size() || b.size()>=group_b_.size())throw std::invalid_argument("Group label exceeds256 UTF-8 bytes");
        group_a_.fill(0);group_b_.fill(0);std::copy(a.begin(),a.end(),group_a_.begin());std::copy(b.begin(),b.end(),group_b_.begin());
    }
    double& library_target(){return target_;}
    void draw(Project* project,std::uint64_t image,std::shared_ptr<const ExpressionMatrixSource> matrix,bool blocked,OutputGate* gate,
        ui::PathField& genes,ui::PathField& replicates,const ui::PathContext& context){
        actions_.clear();const auto doc=project && image?project->document(image):nullptr;const auto source=matrix?matrix->metadata().source_sha256:std::string{};
        if(doc && (revision_!=doc->revision() || generation_!=project->generation())){
            aggregations_=expression_aggregation_ids(*project,image);records_=expression_differential_ids(*project,image);revision_=doc->revision();generation_=project->generation();
        }
        const bool edit=doc && matrix && !blocked && !busy();
        auto button=[&](Action a,const char* caption,bool available,auto call,const char* reason="Open a kept pseudobulk first."){
            actions_.button(a,caption,available,[&]{job_.attempt(call);},blocked || (busy() && a!=Action::Stop)?"Wait for the current work to finish.":reason,a==Action::Run);
        };
        ui::TextWrapped("Use independent biological replicates; repeated regions are merged by replicate ID.");
        if(!has_expression_statistics())ui::TextWrapped("This build does not include differential-expression statistics.");
        ImGui::BeginDisabled(!edit);molecular_records("Kept aggregation",aggregations_,aggregation_,"Aggregation %d");ImGui::EndDisabled();
        button(Action::OpenBulk,"Open pseudobulk",edit && !aggregations_.empty(),[&]{
            const auto request=expression_aggregation_open_request(*project,image,aggregations_.at(std::size_t(aggregation_)));
            bulk_.run(*doc,project,image,source,[request,source](const std::atomic<bool>* c){
                auto result=request.open(molecular_work,c);if(result->kind()!=ExpressionAggregationKind::Pseudobulk)throw std::invalid_argument("Selected aggregation is a regional summary; choose a kept pseudobulk.");
                if(result->source_sha256()!=source)throw WorkflowInputChanged("Pseudobulk belongs to another analysis matrix.");return result;
            });
        });
        const auto parent=bulk_.result();const bool retained=parent && parent->source_sha256()==source && std::find(aggregations_.begin(),aggregations_.end(),parent->id())!=aggregations_.end();
        if(parent){ui::PaneTitle("Opened pseudobulk");const auto at=std::find(aggregations_.begin(),aggregations_.end(),parent->id());if(at!=aggregations_.end())ui::DataTextWrapped("%s",molecular_ordinal("Aggregation %d",int(at-aggregations_.begin())).c_str());ui::Text("Pseudobulk cohorts: %zu",parent->cohorts().size());if(ui::CollapsingHeader("Pseudobulk labels"))for(const auto& row:parent->cohorts())ui::DataTextWrapped("%s / %s / %s / %s",row.id.c_str(),row.sample.c_str(),row.replicate.c_str(),row.group.c_str());}
        ui::PaneTitle("Next run settings");
        ImGui::BeginDisabled(!edit);ui::InputText("Group A",group_a_.data(),group_a_.size());ui::InputText("Group B",group_b_.data(),group_b_.size());ui::InputDouble("Library target",&target_);ImGui::EndDisabled();
        button(Action::Run,"Run differential expression",edit && retained && has_expression_statistics(),[&]{
            const auto request=expression_aggregation_open_request(*project,image,parent->id());if(request.recipe!=parent->recipe())throw WorkflowInputChanged("Retained pseudobulk changed; open it again.");
            ExpressionDifferentialOptions options;options.group_a=group_a_.data();options.group_b=group_b_.data();options.library_target=target_;options.work_bytes=molecular_work;
            job_.run(*doc,project,image,source,[parent,options](const std::atomic<bool>* c){return differential_expression(parent,options,c);});
        });
        button(Action::Stop,"Stop",busy(),[&]{cancel();},"Nothing is running.");
        const auto result=job_.result();const bool same=doc && result && result->parent()->source_sha256()==source && job_.current(*doc,project,source);
        const bool kept=result && std::find(records_.begin(),records_.end(),result->id())!=records_.end();
        if(result && !same)ui::TextWrapped("Inputs changed; run again before keeping.");
        button(Action::Keep,"Keep differential result",edit && same && !kept,[&]{keep_expression_differential(*project,image,*result);job_.kept(*doc);},kept?"This recipe is already kept.":"Inputs changed; run again before keeping.");
        if(!records_.empty()){
            molecular_records("Kept differential result",records_,record_,"DE %d");
            button(Action::Reopen,"Reopen differential result",edit,[&]{const auto request=expression_differential_open_request(*project,image,records_[std::size_t(record_)]);job_.run(*doc,project,image,source,[request](const std::atomic<bool>* c){return request.open(molecular_work,c);});});
            button(Action::Remove,"Remove differential result",edit,[&]{const bool was_current=job_.current(*doc,project,source);remove_expression_differential(*project,image,records_[std::size_t(record_)]);if(was_current)job_.kept(*doc);});
        }
        results();bulk_.status();job_.status();
        const std::vector<std::string> protection{matrix?matrix->path():std::string{},result?result->parent()->parent()->path():std::string{}};
        genes.draw(context,blocked || busy());
        button(Action::Genes,"Export differential genes CSV",result && gate && !blocked && !busy() && genes.state().valid,[&]{job_.write(*gate,genes,"Differential genes CSV",protection,[result](const std::string& path,const std::atomic<bool>* c){save_expression_differential_csv(*result,path,molecular_work,c);});},"Run an analysis and choose a new output path first.");
        replicates.draw(context,blocked || busy());
        button(Action::Replicates,"Export differential replicates CSV",result && gate && !blocked && !busy() && replicates.state().valid,[&]{job_.write(*gate,replicates,"Differential replicates CSV",protection,[result](const std::string& path,const std::atomic<bool>* c){save_expression_differential_replicates_csv(*result,path,molecular_work,c);});},"Run an analysis and choose a new output path first.");
    }
};
} // namespace qpx::detail
