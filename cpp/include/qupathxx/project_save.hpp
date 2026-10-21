#pragma once
#include "qupathxx/project.hpp"

namespace qpx {
struct ProjectSaveStats {
    std::string state="idle",path,error;
    std::uint64_t completed=0,generation=0;
    bool applied=false;
    std::size_t snapshot_bytes=0;
    double capture_ms=0,write_ms=0;
};
// Owner-thread controller: one immutable snapshot, no backlog. Completion
// reconciles only its captured save baseline, never the live canonical graph.
// Destruction joins and reconciles an authorized save; cancel is explicit.
class ProjectSaveJob {
public:
    explicit ProjectSaveJob(std::size_t snapshot_budget=256*1024*1024);
    ~ProjectSaveJob();
    ProjectSaveJob(const ProjectSaveJob&)=delete;
    bool request(std::shared_ptr<Project>,const std::string& path);
    ProjectSaveStats poll();
    ProjectSaveStats wait();
    void cancel();
    bool busy() const;
    const ProjectSaveStats& stats() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
