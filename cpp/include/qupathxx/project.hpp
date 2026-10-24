#pragma once
#include "qupathxx/core.hpp"
#include <utility>
#include <functional>

namespace qpx {
namespace detail {struct BatchCheckpoint;}
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
    bool cancelled=false;
    double metadata_ms=0,ownership_ms=0,removal_ms=0;
    std::size_t marker_prefetch_peak=0;
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
    // Read the current committed state without writing a hot-journal repair.
    static std::shared_ptr<Project> open_without_journal_recovery(const std::string& path);
    static RecoveryInfo inspect_recovery(const std::string& path);
    static RecoveryInfo review_recovery(const std::string& path);
    static void discard_recovery_checked(const RecoveryInfo& reviewed);
    static std::size_t cleanup_recovery_work(const std::string& path);
    static std::size_t cleanup_abandoned_recovery_work(const std::string& current_path,std::size_t limit=32);
    static RecoveryRetentionStats prune_recoveries(const std::string& current_path,const RecoveryRetention& policy);
    static std::shared_ptr<Project> recover(const std::string& path,const std::string& base_path="");
    static void discard_recovery(const std::string& path,const std::string& expected_identity);
    static std::vector<RecoveryInfo> find_recoveries(const std::string& base_path="",std::size_t limit=64);
    void restore_recovery(const std::string& path,const std::string& base_path="");
    void save(const std::string& path);
    // Snapshot and atomically publish a new project; never overwrite a target.
    // Native publication_guard runs after staging and before commit, on owner.
    void save_new(const std::string& path,std::size_t snapshot_bytes=512ull*1024*1024,
        std::uint64_t file_bytes=1024ull*1024*1024,const std::atomic<bool>* cancelled=nullptr,
        const std::function<void()>& publication_guard={});
    std::uint64_t add_image(const std::string& path,std::string reader="auto",std::string name="");
    std::uint64_t add_tiff_image(const std::string& path,const TiffOpenOptions&,std::string name="");
    std::uint64_t add_tiff_source(std::shared_ptr<ImageServer>,std::string name="");
    void set_tiff_view(std::uint64_t id,const TiffView&);
    // An omitted source embeds dense RGB data, or keeps an opened native
    // OpenSlide/TIFF document linked to its captured locator and reader.
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
    friend class detail::GuiRaw;
    friend class CellTableRequest;
    friend class MeasurementTableRequest;
    friend struct ProjectBatchKernel;
    friend class ProjectBatchRequest;
    friend class ProjectAutosave;
    friend class ProjectSaveJob;
    friend GuiReport run_gui(Document&,int,const std::string&,const GuiOptions&);
    void adopt_workspace(Project& candidate) noexcept;
    bool can_retire_on_worker() const;
    struct LoadProgress {
        std::atomic<bool> cancelled{false};
        const std::atomic<bool>* external_cancelled=nullptr;
        const std::atomic<bool>* cancellation() const {return external_cancelled?external_cancelled:&cancelled;}
        std::atomic<int> phase{1};
        std::atomic<std::size_t> images{0},objects{0},total_images{0},total_objects{0};
    };
    static std::shared_ptr<Project> open_impl(const std::string& path,bool recovery,const std::string& base_path="",LoadProgress* progress=nullptr,bool repair_journal=true);
    static std::pair<std::string,std::uint64_t> read_save_identity(const std::string& path);
    static RecoveryInfo inspect_recovery_header(const std::string& path,const std::atomic<bool>* cancelled=nullptr);
    static std::optional<RecoveryInfo> inspect_recovery_work(const std::string& path,const std::atomic<bool>* cancelled);
    static std::size_t cleanup_recovery_work_impl(const std::string& path,std::size_t limit,
        const std::atomic<bool>* cancelled,const RecoveryInfo* scope=nullptr);
    struct RetentionProgress {
        std::atomic<int> phase{0};
        std::atomic<bool> published{false};
        std::atomic<std::size_t> inspected{0},removed{0};
    };
    static RecoveryRetentionStats prune_recoveries_impl(const std::string& path,const RecoveryRetention&,
        const std::atomic<bool>* cancelled,RetentionProgress* progress);
    std::unique_ptr<Project> snapshot_for_save(std::size_t budget,std::size_t& bytes,const std::set<std::uint64_t>* selected=nullptr) const;
    void save_impl(const std::string& path,const std::string& link_directory,const std::atomic<bool>* cancelled=nullptr,const RecoveryInfo* recovery=nullptr,const detail::BatchCheckpoint* batch=nullptr,std::uint64_t file_bytes=0);
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
