#pragma once
#include "qupathxx/project.hpp"

namespace qpx {
struct AutosaveStats {
    std::string state="idle",path,error;
    std::uint64_t completed=0;
    std::size_t snapshot_bytes=0;
    double capture_ms=0,write_ms=0;
    std::size_t work_directories_cleaned=0;
    std::string work_cleanup_error;
};
// Owner-thread controller. Only an immutable private snapshot enters its worker.
// One job, no backlog. Destruction cancels publication and joins its worker.
class ProjectAutosave {
public:
    explicit ProjectAutosave(std::size_t snapshot_budget=256*1024*1024);
    ~ProjectAutosave();
    ProjectAutosave(const ProjectAutosave&)=delete;
    bool request(const Project&,const std::string& path="");
    AutosaveStats poll();
    AutosaveStats wait();
    void cancel();
    void clear();
    bool busy() const;
    const AutosaveStats& stats() const;
    const std::string& session() const;
    std::string default_path(const Project&) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
