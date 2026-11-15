#pragma once
#include "gui_molecular_job.hpp"
#include "qupathxx/expression_reduction.hpp"
#include <cmath>
#include <limits>

namespace qpx::detail {
class GuiExpressionReduction {
public:
    enum class Action {Run,Stop,Archive,Keep,Reopen,Remove,Verify,Embedding,Loadings,Plot,Count};
private:
    GuiMolecularJob<ExpressionReductionResult> job_;
    MolecularActions<Action> actions_;
    ExpressionReductionOptions options_;
    std::vector<std::string> records_,normalizations_;
    std::uint64_t revision_=UINT64_MAX,generation_=0;
    std::string source_,archive_,archive_id_,opening_archive_;
    bool normalized_=false,exporting_archive_=false,verifying_=false,verified_=false;
    int record_=0,normalization_=0,observation_=0;
    const ExpressionReductionResult* plotted_=nullptr;
    double min_x_=0,max_x_=1,min_y_=0,max_y_=1;
    static ImU32 cluster_color(std::uint32_t label){float r,g,b;ImGui::ColorConvertHSVtoRGB(std::fmod(label*.618034f,.9999f),.65f,.95f,r,g,b);return ImGui::ColorConvertFloat4ToU32({r,g,b,1});}
    void scatter(const ExpressionReductionResult& result){
        const auto components=result.options().components;const auto n=result.observation_ids().size();const auto& scores=result.embedding();
        if(plotted_!=&result){
            min_x_=min_y_=std::numeric_limits<double>::infinity();max_x_=max_y_=-min_x_;
            for(std::size_t i=0;i<n;++i){const auto x=scores[i*components],y=components>1?scores[i*components+1]:0.;min_x_=std::min(min_x_,x);max_x_=std::max(max_x_,x);min_y_=std::min(min_y_,y);max_y_=std::max(max_y_,y);}
            if(min_x_==max_x_){min_x_-=.5;max_x_+=.5;}if(min_y_==max_y_){min_y_-=.5;max_y_+=.5;}plotted_=&result;
        }
        ui::TextUnformatted(components>1?"Components 1 and 2":"Component 1; vertical axis is zero");
        const auto a=ImGui::GetCursorScreenPos();const ImVec2 size{ui::available(),220*ui::design::scale()};
        ImGui::InvisibleButton("##expression-embedding",size);const bool hovered=ImGui::IsItemHovered();const auto mouse=ImGui::GetIO().MousePos;
        auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(a,{a.x+size.x,a.y+size.y},IM_COL32(20,23,29,255));draw->PushClipRect(a,{a.x+size.x,a.y+size.y},true);
        std::optional<std::size_t> hit;double closest=36;
        for(std::size_t i=0;i<std::min<std::size_t>(n,4096);++i){
            const auto x=scores[i*components],y=components>1?scores[i*components+1]:0.;
            const ImVec2 p{a.x+8+float((x-min_x_)/(max_x_-min_x_))*(size.x-16),a.y+size.y-8-float((y-min_y_)/(max_y_-min_y_))*(size.y-16)};
            draw->AddCircleFilled(p,3*ui::design::scale(),cluster_color(result.labels()[i]));
            if(hovered){const double dx=mouse.x-p.x,dy=mouse.y-p.y,d=dx*dx+dy*dy;if(d<closest){closest=d;hit=i;}}
        }
        draw->PopClipRect();if(n>4096)ui::Text("Showing first 4096 of %zu observations; exports include all.",n);
        if(hit){ImGui::BeginTooltip();ui::DataTextWrapped("%s",result.observation_ids()[*hit].c_str());ui::Text("Cluster: %u",result.labels()[*hit]);ui::DataTextWrapped("%.17g / %.17g",scores[*hit*components],components>1?scores[*hit*components+1]:0.);ImGui::EndTooltip();}
    }
    void results(){
        const auto result=job_.result();if(!result)return;
        ui::PaneTitle("Results");ui::TextUnformatted(result->options().center?"Centered PCA":"Uncentered SVD");
        ui::DataTextWrapped("%s",result->value_domain().c_str());ui::Text("Features / observations: %zu / %zu",result->features().size(),result->observation_ids().size());
        ui::Text("Fitted components / clusters: %u / %u",result->options().components,result->options().clusters);
        ui::Text("Fitted seed: %llu",static_cast<unsigned long long>(result->options().seed));
        ui::TextUnformatted(result->converged()?"Clustering converged":"Clustering reached the iteration limit");
        if(verified_)ui::TextUnformatted("Verified for this session");
        ui::TextWrapped("Cluster numbers are exploratory labels, not cell types or biological replicates.");
        scatter(*result);
        ui::InputInt("Observation index (0 based)",&observation_);observation_=std::clamp(observation_,0,int(result->observation_ids().size()-1));
        ui::DataTextWrapped("%s",result->observation_ids()[std::size_t(observation_)].c_str());ui::Text("Cluster: %u",result->labels()[std::size_t(observation_)]);
        for(std::size_t c=0;c<result->options().components;++c)ui::Text("Component %zu: %.17g",c+1,result->embedding()[std::size_t(observation_)*result->options().components+c]);
    }
public:
    bool busy() const{return job_.busy();}
    void cancel(){job_.cancel();}
    void poll(){
        job_.poll();
        if(!busy() && verifying_){verified_=job_.state()=="ready";verifying_=false;}
        if(!job_.output().empty() && exporting_archive_ && job_.result()){archive_=job_.output();archive_id_=job_.result()->id();exporting_archive_=false;}
        if(job_.state()=="ready" && !opening_archive_.empty()){archive_=opening_archive_;opening_archive_.clear();archive_id_=job_.result()->id();}
        if(job_.state()=="failed" || job_.state()=="cancelled")opening_archive_.clear();
    }
    const auto& result() const{return job_.result();}
    const auto& error() const{return job_.error();}
    const auto& state() const{return job_.state();}
    ExpressionReductionOptions& settings(){return options_;}
    Rect bounds(Action a) const{return actions_.bounds[std::size_t(a)];}
    bool enabled(Action a) const{return actions_.enabled[std::size_t(a)];}
    void draw(Project* project,std::uint64_t image,std::shared_ptr<const ExpressionMatrixSource> matrix,bool blocked,OutputGate* gate,
        ui::PathField& archive,ui::PathField& embedding,ui::PathField& loadings,ui::PathField& plot,const ui::PathContext& context){
        actions_.clear();const auto doc=project && image?project->document(image):nullptr;const auto source=matrix?matrix->metadata().source_sha256:std::string{};
        if(doc && (revision_!=doc->revision() || generation_!=project->generation() || source_!=source)){
            records_=expression_reduction_ids(*project,image);normalizations_=matrix?expression_normalization_ids(*project,image,source):std::vector<std::string>{};
            revision_=doc->revision();generation_=project->generation();source_=source;
        }
        const bool edit=doc && matrix && !blocked && !busy();
        auto button=[&](Action a,const char* caption,bool available,auto call,const char* reason="Choose a full indexed matrix first."){
            actions_.button(a,caption,available,[&]{job_.attempt(call);},blocked || (busy() && a!=Action::Stop)?"Wait for the current work to finish.":reason,a==Action::Run);
        };
        ui::TextWrapped("All source features; displayed genes do not limit this analysis.");
        ui::PaneTitle("Next run settings");
        if(!has_expression_reduction())ui::TextWrapped("This build can open fitted results but cannot fit PCA or clustering.");
        ImGui::BeginDisabled(!edit);
        ui::Checkbox("Center features (PCA)",&options_.center);
        auto number=[&](const char* caption,std::uint32_t& value){int draft=int(value);if(ui::InputInt(caption,&draft))value=std::uint32_t(draft);};
        number("Components",options_.components);number("Clusters",options_.clusters);ui::InputScalar("Random seed",ImGuiDataType_U64,&options_.seed);
        if(ui::CollapsingHeader("Advanced fit options")){number("Oversampling",options_.oversampling);number("Power iterations",options_.power_iterations);number("Clustering iteration limit",options_.max_iterations);ui::InputDouble("Clustering tolerance",&options_.cluster_tolerance);}
        if(!normalizations_.empty()){ui::Checkbox("Use kept normalization",&normalized_);if(normalized_)molecular_records("Normalization",normalizations_,normalization_,"Normalization %d");}else normalized_=false;
        ImGui::EndDisabled();
        button(Action::Run,"Run PCA / clustering",edit && has_expression_reduction(),[&]{
            auto options=options_;options.work_bytes=molecular_work;options.normalization_recipe=normalized_?expression_normalization_recipe(*project,image,normalizations_.at(std::size_t(normalization_))):std::string{};
            verified_=false;job_.run(*doc,project,image,source,[matrix,options](const std::atomic<bool>* c){return reduce_expression(matrix,options,c);});opening_archive_.clear();
        });
        button(Action::Stop,"Stop",busy(),[&]{cancel();},"Nothing is running.");
        const auto result=job_.result();const bool same=doc && result && result->source_sha256()==source && job_.current(*doc,project,source);
        const bool kept=result && std::find(records_.begin(),records_.end(),result->id())!=records_.end();
        if(result && !same)ui::TextWrapped("Inputs changed; run again before keeping.");
        archive.draw(context,blocked || busy());
        button(Action::Archive,"Export fitted archive",result && gate && !blocked && !busy() && archive.state().valid,[&]{
            exporting_archive_=true;job_.write(*gate,archive,"Fitted archive",{matrix?matrix->path():std::string{},archive_},[result](const std::string& path,const std::atomic<bool>* c){save_expression_reduction_archive(*result,path,molecular_work,c);});
        },"Run an analysis and choose a new output path first.");
        button(Action::Keep,"Keep fitted result",edit && same && !kept && archive_id_==result->id(),[&]{matrix->require_current();keep_expression_reduction(*project,image,*result,archive_,true,molecular_work);job_.kept(*doc);},kept?"This recipe is already kept.":"Export the fitted archive before keeping this result.");
        if(!records_.empty()){
            molecular_records("Kept fitted result",records_,record_,"Fit %d");
            button(Action::Reopen,"Open fitted result",edit,[&]{const auto request=expression_reduction_open_request(*project,image,records_[std::size_t(record_)]);verified_=false;opening_archive_=request.archive;job_.run(*doc,project,image,source,[request](const std::atomic<bool>* c){return request.open(molecular_work,c);});});
            button(Action::Remove,"Remove fitted result",edit,[&]{const bool was_current=job_.current(*doc,project,source);remove_expression_reduction(*project,image,records_[std::size_t(record_)]);if(was_current)job_.kept(*doc);});
        }
        button(Action::Verify,"Verify fitted result",edit && result && result->source_sha256()==source && has_expression_reduction(),[&]{
            verified_=false;verifying_=true;
            job_.run(*doc,project,image,source,[matrix,result](const std::atomic<bool>* c){replay_expression_reduction(matrix,*result,molecular_work,c);return result;});
        },"Open or run a fitted result first.");
        results();job_.status();
        const std::vector<std::string> protection{matrix?matrix->path():std::string{},archive_};
        auto output=[&](Action a,ui::PathField& field,const char* caption,const char* kind,auto writer){
            field.draw(context,blocked || busy());button(a,caption,result && gate && !blocked && !busy() && field.state().valid,[&]{exporting_archive_=false;job_.write(*gate,field,kind,protection,[result,writer](const std::string& path,const std::atomic<bool>* c){writer(*result,path,molecular_work,c);});},"Run an analysis and choose a new output path first.");
        };
        output(Action::Embedding,embedding,"Export embedding CSV","Embedding CSV",save_expression_embedding_csv);
        output(Action::Loadings,loadings,"Export loadings CSV","Loadings CSV",save_expression_loadings_csv);
        output(Action::Plot,plot,"Export scatter SVG","Scatter SVG",save_expression_reduction_svg);
    }
};
} // namespace qpx::detail
