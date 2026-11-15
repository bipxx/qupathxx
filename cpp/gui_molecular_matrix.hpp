#pragma once
#include "gui_molecular_job.hpp"
#include "qupathxx/expression_matrix_project.hpp"

namespace qpx::detail {
// Reuses an already open indexed Genes source when selected. Nonspatial native
// associations open explicitly on the same owned-request worker boundary.
class GuiMolecularMatrix {
    GuiMolecularJob<ExpressionMatrixSource> job_;
    std::vector<std::string> ids_;int selected_=0;
    std::uint64_t revision_=UINT64_MAX,generation_=0;
    std::string wanted_,running_,ready_;
    std::shared_ptr<const ExpressionMatrixSource> matrix_;
    Rect open_{};bool enabled_=false;
    static std::string key(const ExpressionMatrixOpenRequest& request){return request.path+"\n"+request.association.source_sha256+"\n"+request.association.file_sha256;}
public:
    bool busy() const{return job_.busy();}
    void cancel(){job_.cancel();}
    void poll(){job_.poll();if(!busy() && job_.state()=="ready")ready_=running_;}
    const auto& matrix() const{return matrix_;}
    Rect open_bounds() const{return open_;}
    bool open_enabled() const{return enabled_;}
    void draw(Project* project,std::uint64_t image,std::shared_ptr<const ExpressionMatrixSource> shared,bool blocked){
        open_={};enabled_=false;matrix_.reset();const auto doc=project && image?project->document(image):nullptr;
        if(!doc)return;
        if(revision_!=doc->revision() || generation_!=project->generation()){
            ids_=expression_matrix_ids(*project,image);revision_=doc->revision();generation_=project->generation();
            if(wanted_.empty() && shared){const auto i=std::find(ids_.begin(),ids_.end(),shared->metadata().source_sha256);if(i!=ids_.end())selected_=int(i-ids_.begin());}
        }
        ImGui::BeginDisabled(blocked || busy());molecular_records("Full analysis matrix",ids_,selected_,"Matrix %d");ImGui::EndDisabled();
        std::optional<ExpressionMatrixOpenRequest> request;
        try{if(!ids_.empty()){request=expression_matrix_open_request(*project,image,ids_[std::size_t(selected_)]);wanted_=key(*request);}}
        catch(const std::exception& e){job_.fail(e.what());}
        if(request && shared && shared->metadata().source_sha256==request->association.source_sha256 && shared->path()==request->path)matrix_=std::move(shared);
        else if(request && !busy() && job_.state()=="ready" && ready_==wanted_)matrix_=job_.result();
        const bool available=request && !matrix_ && !busy() && !blocked;
        if(ui::AvailableButton("Open analysis matrix",{available,available?nullptr:"Choose a kept matrix first."}))job_.attempt([&]{
            const auto captured=*request;running_=wanted_;job_.run(*doc,project,image,captured.association.source_sha256,[captured](const std::atomic<bool>* c){return captured.open(molecular_work,c);});
        });
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();open_={a.x,a.y,b.x-a.x,b.y-a.y};enabled_=available && ImGui::IsItemVisible();
        if(busy() && ui::Button("Stop"))cancel();
        if(!matrix_)job_.status();
    }
};
}
