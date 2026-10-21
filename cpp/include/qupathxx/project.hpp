#pragma once
#include "qupathxx/core.hpp"
#include <utility>

namespace qpx {
class ProjectAutosave;
class ProjectSaveJob;
struct RecoveryRetention {
    bool enabled=false;
    std::size_t keep_latest=10,max_deletions=32;
    double max_age_days=30;
    void validate() const;
};
struct RecoveryRetentionStats {
    std::size_t inspected=0,removed=0,failed=0;
    double elapsed_ms=0;
    std::string error;
};
struct RecoveryInfo {
    std::string path,identity,session,base_path,base_status,error,fingerprint;
    std::string associated_base_path;
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
    static RecoveryInfo review_recovery(const std::string& path);
    static void discard_recovery_checked(const RecoveryInfo& reviewed);
    static std::size_t cleanup_recovery_work(const std::string& path);
    static RecoveryRetentionStats prune_recoveries(const std::string& current_path,const RecoveryRetention& policy);
    static std::shared_ptr<Project> recover(const std::string& path,const std::string& base_path="");
    static void discard_recovery(const std::string& path,const std::string& expected_identity);
    static std::vector<RecoveryInfo> find_recoveries(const std::string& base_path="",std::size_t limit=64);
    void restore_recovery(const std::string& path,const std::string& base_path="");
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
    friend class ProjectSaveJob;
    friend GuiReport run_gui(Document&,int,const std::string&,const GuiOptions&);
    void adopt_workspace(Project& candidate) noexcept;
    bool can_retire_on_worker() const;
    static std::shared_ptr<Project> open_impl(const std::string& path,bool recovery,const std::string& base_path="");
    static std::pair<std::string,std::uint64_t> read_save_identity(const std::string& path);
    static RecoveryInfo inspect_recovery_header(const std::string& path);
    std::unique_ptr<Project> snapshot_for_save(std::size_t budget,std::size_t& bytes) const;
    void save_impl(const std::string& path,const std::string& link_directory,const std::atomic<bool>* cancelled=nullptr);
    void save_recovery(const std::string& path,const std::string& link_directory,const RecoveryInfo& info,const std::atomic<bool>* cancelled=nullptr);
    // Only native factories can assert that the shared_ptr control block and
    // its deleter contain no Python references. External documents default off.
    struct Entry { ProjectImage info; std::shared_ptr<Document> document; bool native_lifetime=false; };
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
