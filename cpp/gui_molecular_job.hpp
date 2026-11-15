#pragma once
#include "gui_tool_scope.hpp"
#include "gui_output_gate.hpp"
#include "gui_path_field.hpp"
#include <chrono>
#include <cstdio>
#include <future>

namespace qpx::detail {
inline constexpr std::size_t molecular_work=256u*1024*1024;
inline std::string molecular_ordinal(const char* format,int index){
    char text[128];std::snprintf(text,sizeof(text),ui::i18n::text(format),index+1);return text;
}
inline void molecular_records(const char* caption,const std::vector<std::string>& ids,int& selected,const char* format){
    if(ids.empty()){selected=0;return;}
    selected=std::clamp(selected,0,int(ids.size()-1));
    if(ui::ComboScope combo{caption,molecular_ordinal(format,selected).c_str()};combo)
        for(std::size_t i=0;i<ids.size();++i)
            if(ui::Selectable(ui::data_label(molecular_ordinal(format,int(i)),std::to_string(i)).c_str(),i==std::size_t(selected)))selected=int(i);
}

// Frontend lifetime and publication context only. Scientific identity, data
// admission, persistence, numerical work and exports stay in native services.
template<class Result> class GuiMolecularJob {
    struct Context {ToolScope scope;const Project* project=nullptr;std::weak_ptr<ImageServer> server;std::string source;};
    struct Completion {std::shared_ptr<const Result> value;std::string output;};
    struct Progress {std::optional<bool> done;std::string error;};
    std::future<Completion> pending_;
    std::atomic<bool> cancel_{false};bool discard_=false,writing_=false;
    std::shared_ptr<const Result> result_;
    Context owner_,pending_owner_;
    std::shared_ptr<Progress> progress_=std::make_shared<Progress>();
    std::string error_,state_="idle",output_;
    template<class Work> void start(Work task,bool writing){
        if(busy())throw std::logic_error("Wait for the current work to finish.");
        cancel_=false;discard_=false;writing_=writing;error_.clear();output_.clear();state_="running";
        if(!writing)progress_=std::make_shared<Progress>();
        pending_=std::async(std::launch::async,[this,task=std::move(task)]() mutable{return task(&cancel_);});
    }
public:
    ~GuiMolecularJob(){cancel_=true;if(pending_.valid()){pending_.wait();poll();}}
    bool busy() const{return pending_.valid();}
    void cancel(){discard_=true;cancel_=true;}
    void poll(){
        if(!busy() || pending_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try{
            auto completed=pending_.get();
            if(!completed.output.empty()){output_=std::move(completed.output);progress_->done=true;state_="saved";}
            else if(discard_)state_="cancelled";
            else{result_=std::move(completed.value);owner_=pending_owner_;state_="ready";}
        }catch(const WorkflowCancelled&){if(writing_)progress_->done=false;state_="cancelled";}
        catch(const std::exception& e){fail(e.what());if(writing_){progress_->error=error_;progress_->done=false;}}
    }
    template<class Work> void run(const Document& doc,const Project* project,std::uint64_t image,std::string source,Work task){
        pending_owner_={};update(pending_owner_.scope,doc,image);pending_owner_.project=project;
        pending_owner_.server=doc.server();pending_owner_.source=std::move(source);
        start([task=std::move(task)](const std::atomic<bool>* cancel) mutable{return Completion{task(cancel),{}};},false);
    }
    bool current(const Document& doc,const Project* project,const std::string& source) const{
        return result_ && owner_.project==project && owner_.source==source &&
            tool_document_identity(doc).lock()==owner_.scope.document_identity.lock() &&
            doc.server()==owner_.server.lock() && doc.active_plane()==owner_.scope.plane && doc.revision()==owner_.scope.document_revision;
    }
    void kept(const Document& doc){owner_.scope.document_revision=doc.revision();}
    const std::shared_ptr<const Result>& result() const{return result_;}
    const std::string& output() const{return output_;}
    const std::string& state() const{return state_;}
    const std::string& error() const{return error_.empty()?progress_->error:error_;}
    void fail(std::string error){error_=std::move(error);state_="failed";}
    template<class Work> void attempt(Work call){try{call();error_.clear();}catch(const std::exception& e){fail(e.what());}}
    template<class Writer> void write(OutputGate& gate,const ui::PathField& field,const char* kind,std::vector<std::string> sources,Writer writer){
        output_.clear();
        progress_=std::make_shared<Progress>();const auto progress=progress_;
        gate.submit(field.state().resolved,output_scope(kind,false,"Genes"),
            [this,writer=std::move(writer),progress,&gate](const std::string& path,ExportWritePolicy,const std::vector<std::string>& protection){
                start([path,protection,writer](const std::atomic<bool>* c){guard_output_intent(path,protection);writer(path,c);return Completion{{},path};},true);
                gate.watch(path,[progress]{return progress->done;},[progress]{return progress->error;},"Genes");
            },[progress](const std::string& message){progress->error=message;progress->done=false;},false,
            ui::i18n::text("Choose a new file; this export cannot replace an existing file."),std::move(sources));
    }
    void status() const{
        if(!error().empty())ui::DataTextWrapped("%s",error().c_str());
        if(busy())ui::TextUnformatted("Running...");else if(state_=="cancelled")ui::TextUnformatted("Cancelled");
        else if(state_=="ready")ui::TextUnformatted("Ready");
    }
};

template<class Action> struct MolecularActions {
    std::array<Rect,std::size_t(Action::Count)> bounds{};
    std::array<bool,std::size_t(Action::Count)> enabled{};
    void clear(){bounds={};enabled={};}
    template<class Work> void button(Action action,const char* text,bool available,Work call,
        const char* reason="Choose a full indexed matrix first.",bool primary=false){
        const ui::Availability state{available,available?nullptr:reason};
        const bool clicked=ui::AvailableButton(text,state,primary?ui::design::Action::Primary:ui::design::Action::Secondary);
        const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();bounds[std::size_t(action)]={a.x,a.y,b.x-a.x,b.y-a.y};
        enabled[std::size_t(action)]=available && ImGui::IsItemVisible();if(clicked)call();
    }
};
} // namespace qpx::detail
