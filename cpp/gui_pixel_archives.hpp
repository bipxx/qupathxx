#pragma once
#include "qupathxx/pixel_classifier_archive.hpp"
#include <future>

namespace qpx::detail {
// Only immutable numeric values cross the worker boundary. Requests (and their
// source ownership) remain on the owner until preparation has been joined.
class GuiPixelArchives {
    struct Completion {std::shared_ptr<const PixelClassifierArchive> archive;std::string error;};
    std::future<Completion> pending_;
    std::atomic<bool> cancelled_{false};
    std::optional<PixelTrainingRequest> training_;
    std::optional<PixelPredictionRequest> prediction_;
    std::string state_="idle",error_;
    std::uint64_t kept_=0,selected_=0,revision_=0;
    std::shared_ptr<const char> identity_;
    using Records=std::map<std::uint64_t,std::shared_ptr<const PixelClassifierRecord>>;
    Records::const_iterator cursor_;
    std::vector<std::shared_ptr<const PixelClassifierRecord>> records_;
    bool complete_=false;
    std::size_t max_step_=0;
    template<class Work> void start(Work work) {
        cancelled_.store(false);state_="preparing";error_.clear();kept_=0;
        try {pending_=std::async(std::launch::async,[this,work=std::move(work)] {
            Completion out;try {out.archive=work(&cancelled_);}catch(const std::exception& e) {out.error=e.what();}return out;
        });}catch(const std::exception& e) {state_="failed";error_=e.what();training_.reset();prediction_.reset();}
    }
public:
    ~GuiPixelArchives() {cancel();if(pending_.valid()) pending_.wait();}
    bool busy() const {return pending_.valid();}
    void cancel() {cancelled_.store(true);}
    void wait() {if(pending_.valid()) pending_.wait();}
    void keep(const PixelTrainingRequest& request,std::shared_ptr<const PixelTrainingTable> table,std::shared_ptr<const PixelClassifier> model) {
        if(busy()) return;training_=request;prediction_.reset();
        start([table=std::move(table),model=std::move(model)](const std::atomic<bool>* cancel) {return archive_pixel_training(table,*model,cancel);});
    }
    void keep(const PixelPredictionRequest& request,std::shared_ptr<const PixelPredictionResult> result) {
        if(busy()) return;prediction_=request;training_.reset();
        start([result=std::move(result)](const std::atomic<bool>* cancel) {return archive_pixel_prediction(result,cancel);});
    }
    void update(Document& doc,const Records& source,const std::shared_ptr<const char>& identity,bool open) {
        if(identity_ && identity_!=identity) {cancel();kept_=selected_=0;error_.clear();if(!busy()) state_="idle";}
        if(pending_.valid() && pending_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            auto out=pending_.get();
            try {
                if(cancelled_.load()) state_="cancelled";
                else if((training_ && !training_->matches(doc)) || (prediction_ && !prediction_->matches(doc))) state_="stale";
                else if(!out.error.empty()) {state_="failed";error_=std::move(out.error);}
                else {kept_=training_?doc.keep_pixel_training(*training_,std::move(out.archive)):doc.keep_pixel_prediction(*prediction_,std::move(out.archive));selected_=kept_;state_="kept";}
            }catch(const std::exception& e) {state_="failed";error_=e.what();}
            training_.reset();prediction_.reset();
        }
        if(identity_!=identity || revision_!=doc.revision()) {
            identity_=identity;revision_=doc.revision();cursor_=source.cbegin();records_.clear();complete_=false;
            if(selected_ && !source.count(selected_)) selected_=0;
            if(!selected_ && !source.empty()) selected_=source.begin()->first;
        }
        std::size_t step=0;
        while(open && !complete_ && step<1024) {if(cursor_==source.cend()) {complete_=true;break;}records_.push_back((cursor_++)->second);++step;}
        max_step_=std::max(max_step_,step);
    }
    const auto& records() const {return records_;}
    std::uint64_t selected() const {return selected_;}
    void select(std::uint64_t id) {selected_=id;}
    std::uint64_t kept() const {return kept_;}
    const std::string& state() const {return state_;}
    const std::string& error() const {return error_;}
    bool complete() const {return complete_;}
    std::size_t max_step() const {return max_step_;}
};
}
