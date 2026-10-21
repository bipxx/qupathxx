#include "qupathxx/project_save.hpp"
#include <chrono>
#include <filesystem>
#include <future>
#include <stdexcept>

namespace qpx {
struct ProjectSaveJob::Impl {
    struct Result {
        bool committed=false;
        std::string path,error;
        std::uint64_t generation=0,revision=0;
        std::map<std::uint64_t,std::uint64_t> documents;
        double write_ms=0;
    };
    std::size_t budget;
    ProjectSaveStats stats;
    std::future<Result> future;
    std::shared_ptr<Project> owner; // Never copied to or released by the worker.
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::string instance,base_path;
    std::uint64_t base_generation=0;
    explicit Impl(std::size_t value):budget(value) {}
};
ProjectSaveJob::ProjectSaveJob(std::size_t budget):impl_(std::make_unique<Impl>(budget)) {
    if(!budget || budget>4ull*1024*1024*1024) throw std::invalid_argument("save snapshot budget must be between 1 byte and 4 GiB");
}
ProjectSaveJob::~ProjectSaveJob() { try {wait();} catch(...) {} }
bool ProjectSaveJob::busy() const {return impl_->future.valid();}
const ProjectSaveStats& ProjectSaveJob::stats() const {return impl_->stats;}
void ProjectSaveJob::cancel() {if(impl_->cancelled) impl_->cancelled->store(true);}
bool ProjectSaveJob::request(std::shared_ptr<Project> project,const std::string& path) {
    if(!has_project()) throw std::runtime_error("project saving requires QUPATHXX_BUILD_PROJECT=ON");
    poll(); if(busy()) return false;
    if(!project) throw std::invalid_argument("save requires a project");
    if(path.empty() || path.size()>32768 || path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid project save path");
    auto destination=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();
    if(destination.size()>32768) throw std::invalid_argument("project save path is too long");
    const auto start=std::chrono::steady_clock::now(); std::size_t bytes=0;
    auto snapshot=project->snapshot_for_save(impl_->budget,bytes);
    auto instance=project->instance_identity_,base_path=project->path_;
    const auto base_generation=project->generation_;
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    const auto capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    auto future=std::async(std::launch::async,[snapshot=std::move(snapshot),destination,cancelled]() mutable {
        Impl::Result result; const auto start=std::chrono::steady_clock::now();
        try {
            if(cancelled->load()) throw std::runtime_error("project save cancelled");
            snapshot->save_impl(destination,"",cancelled.get());
            result.committed=true;
            result.path=std::move(snapshot->path_); result.generation=snapshot->generation_; result.revision=snapshot->saved_revision_;
            result.documents=std::move(snapshot->saved_revisions_);
        } catch(const std::exception& e) {result.error=e.what();}
        snapshot.reset(); // Large native maps retire before the future is ready.
        result.write_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        return result;
    });
    impl_->owner=std::move(project); impl_->instance=std::move(instance); impl_->base_path=std::move(base_path); impl_->base_generation=base_generation;
    impl_->cancelled=std::move(cancelled); impl_->future=std::move(future);
    impl_->stats.state="saving"; impl_->stats.path=std::move(destination); impl_->stats.error.clear(); impl_->stats.applied=false;
    impl_->stats.snapshot_bytes=bytes; impl_->stats.capture_ms=capture_ms;
    return true;
}
ProjectSaveStats ProjectSaveJob::poll() {
    if(!busy() || impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return impl_->stats;
    try {
        auto result=impl_->future.get(); impl_->stats.write_ms=result.write_ms;
        if(result.committed) {
            ++impl_->stats.completed; impl_->stats.generation=result.generation;
            auto& project=*impl_->owner;
            if(project.instance_identity_==impl_->instance && project.path_==impl_->base_path && project.generation_==impl_->base_generation) {
                project.path_.swap(result.path); project.generation_=result.generation;
                project.saved_revision_=result.revision; project.saved_revisions_.swap(result.documents);
                impl_->stats.state="saved"; impl_->stats.applied=true;
            } else {
                impl_->stats.state="detached";
                impl_->stats.error="save committed, but the live workspace was replaced or saved separately; reopen the destination to use its generation";
            }
        } else if(impl_->cancelled->load()) {
            impl_->stats.state="cancelled"; impl_->stats.error="project save cancelled";
        } else {impl_->stats.state="error"; impl_->stats.error=std::move(result.error);}
    } catch(const std::exception& e) {impl_->stats.state="error"; impl_->stats.error=e.what();}
    impl_->owner.reset();
    return impl_->stats;
}
ProjectSaveStats ProjectSaveJob::wait() {
    if(busy()) impl_->future.wait();
    return poll();
}
}
