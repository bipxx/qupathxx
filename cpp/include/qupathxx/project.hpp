#pragma once
#include "qupathxx/core.hpp"

namespace qpx {
class ProjectAutosave;
struct RecoveryInfo {
    std::string path,identity,session,base_path,base_status,error;
    std::uint64_t base_generation=0;
    std::int64_t created_unix_ms=0;
    std::size_t images=0,objects=0;
};
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
    static RecoveryInfo inspect_recovery(const std::string& path);
    static std::shared_ptr<Project> recover(const std::string& path);
    static void discard_recovery(const std::string& path,const std::string& expected_identity);
    static std::vector<RecoveryInfo> find_recoveries(const std::string& base_path="",std::size_t limit=64);
    void restore_recovery(const std::string& path);
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
    const std::string& recovery_source() const { return recovery_source_; }
    std::uint64_t generation() const { return generation_; }
private:
    friend class ProjectAutosave;
    friend GuiReport run_gui(Document&,int,const std::string&,const GuiOptions&);
    void adopt_recovery(Project& candidate) noexcept;
    static std::shared_ptr<Project> open_impl(const std::string& path,bool recovery);
    std::unique_ptr<Project> snapshot_for_save(std::size_t budget,std::size_t& bytes) const;
    void save_impl(const std::string& path,const std::string& link_directory,const std::atomic<bool>* cancelled=nullptr);
    void save_recovery(const std::string& path,const std::string& link_directory,const RecoveryInfo& info,const std::atomic<bool>* cancelled=nullptr);
    struct Entry { ProjectImage info; std::shared_ptr<Document> document; };
    const Entry& entry(std::uint64_t id) const;
    std::map<std::uint64_t,Entry> entries_;
    std::map<std::uint64_t,ProjectView> views_;
    std::map<std::uint64_t,std::uint64_t> saved_revisions_;
    std::string path_,identity_,instance_identity_,recovery_source_;
    std::uint64_t next_id_=1,active_image_=0,generation_=0,revision_=0,saved_revision_=0;
};
bool has_project();
std::string sqlite_version();
}
