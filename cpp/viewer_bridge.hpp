#pragma once
#include "qupathxx/viewer_session.hpp"
#include "qupathxx/project_save.hpp"
#include <functional>
namespace qpx::detail {
struct ViewerContext {
    std::shared_ptr<Project> project;
    const std::vector<ProjectImage>* catalog=nullptr;
    std::uint64_t epoch=0,frame=0,active_image=0;
    bool blocked=false,closing=false,opening=false;
    std::function<void(std::uint64_t)> activate;
    std::function<void(const std::vector<std::uint64_t>&)> select;
    std::function<bool(const std::string&)> save;
    std::function<ProjectSaveStats()> save_stats;
    std::function<void()> cancel_save;
    std::function<void(bool)> close;
};
// Entire controller is owner-thread-only. Worker closures borrow Work storage;
// endpoint threads touch only mutex-protected tickets and cancellation atomics.
class ViewerOwner {
public:
    explicit ViewerOwner(ViewerSession&);~ViewerOwner();
    void poll(const ViewerContext&);
    void pump(const ViewerContext&);
    void finish(const ViewerContext&);
    void fail(const std::string&);
    ViewerStats stats() const;void cancel_active();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
