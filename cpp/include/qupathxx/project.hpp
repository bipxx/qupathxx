#pragma once
#include "qupathxx/core.hpp"

namespace qpx {
struct ProjectImage {
    std::uint64_t id=0;
    std::string name,source_path,reader;
    std::int64_t width=0,height=0;
    bool embedded=false,available=false;
};
struct ProjectView {
    Point center;
    double zoom=1;
    DisplaySettings display;
    void validate() const;
    bool operator==(const ProjectView&) const;
};
// Single-threaded native authority for a multi-image workspace. Documents are
// shared with callers. Save/load retain canonical state, not edit history.
class Project {
public:
    Project();
    static std::shared_ptr<Project> open(const std::string& path);
    void save(const std::string& path);
    std::uint64_t add_image(const std::string& path,std::string reader="auto",std::string name="");
    std::uint64_t add_document(std::shared_ptr<Document> document,std::string name="",
        std::string source_path="",std::string reader="auto");
    std::shared_ptr<Document> document(std::uint64_t id) const;
    std::vector<ProjectImage> images() const;
    void remove_image(std::uint64_t id);
    void rename_image(std::uint64_t id,std::string name);
    void relink(std::uint64_t id,const std::string& path);
    std::optional<ProjectView> view(std::uint64_t id) const;
    void set_view(std::uint64_t id,ProjectView view);
    void clear_view(std::uint64_t id);
    std::uint64_t active_image() const { return active_image_; }
    void set_active_image(std::uint64_t id);
    bool dirty() const;
    const std::string& path() const { return path_; }
    const std::string& identity() const { return identity_; }
    std::uint64_t generation() const { return generation_; }
private:
    struct Entry { ProjectImage info; std::shared_ptr<Document> document; };
    const Entry& entry(std::uint64_t id) const;
    std::map<std::uint64_t,Entry> entries_;
    std::map<std::uint64_t,ProjectView> views_;
    std::map<std::uint64_t,std::uint64_t> saved_revisions_;
    std::string path_,identity_;
    std::uint64_t next_id_=1,active_image_=0,generation_=0,revision_=0,saved_revision_=0;
};
bool has_project();
std::string sqlite_version();
}
