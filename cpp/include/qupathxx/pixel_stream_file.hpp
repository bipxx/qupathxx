#pragma once
#include "qupathxx/pixel_stream.hpp"
namespace qpx {
class PixelStreamLink;
struct PixelStreamFileSummary {
    std::uint64_t bytes=0,blocks=0,valid_pixels=0,classified_pixels=0;
    std::vector<std::uint64_t> class_counts;
    std::string sha256;
};
// Numeric historical output. Opening validates bounded metadata and completion;
// read_block validates that block; verify validates every block and final totals.
class PixelStreamFile {
public:
    explicit PixelStreamFile(const std::string& path,std::uint64_t max_bytes=1ULL<<40);
    ~PixelStreamFile();PixelStreamFile(const PixelStreamFile&)=delete;
    std::shared_ptr<const PixelPredictionResult> read_block(std::uint64_t index) const;
    void verify(const std::atomic<bool>* cancelled=nullptr) const;
    void cancel();bool cancelled() const;
    bool verified() const;
    Rect level_bounds() const;std::size_t block_size() const;bool windowed() const;
    const PixelStreamFileSummary& declared_summary() const;
    std::shared_ptr<const PixelStreamLink> link() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    friend struct PixelStreamFileCodec;
    friend PixelStreamFileSummary save_pixel_stream(PixelPredictionStream&,const std::string&,bool,std::uint64_t,const std::atomic<bool>*);
};
PixelStreamFileSummary save_pixel_stream(PixelPredictionStream&,const std::string& path,bool replace_existing=false,std::uint64_t max_bytes=1ULL<<40,const std::atomic<bool>* cancelled=nullptr);
struct PixelStreamExportStats {
    std::string state="idle",error;
    std::uint64_t generation=0,completed_blocks=0,total_blocks=0,valid_pixels=0,classified_pixels=0;
    std::size_t estimated_work_bytes=0;
    double elapsed_ms=0;
    bool current=false;
    std::shared_ptr<const PixelStreamFileSummary> summary;
};
// Owner-thread controller; cancel and producer progress use native atomics.
// The worker never accesses Document or publishes document mutations.
class PixelStreamExportJob {
public:
    PixelStreamExportJob();~PixelStreamExportJob();
    PixelStreamExportJob(const PixelStreamExportJob&)=delete;
    bool request(const Document&,std::uint64_t,const PixelClassifier&,const std::string&,
        PixelStreamOptions={},std::optional<Rect> window={},bool replace_existing=false,std::uint64_t max_bytes=1ULL<<40);
    bool busy() const;void cancel();
    PixelStreamExportStats poll(const Document&);PixelStreamExportStats wait(const Document&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
struct PixelStreamReadStats {
    std::string state="idle",error;
    std::uint64_t generation=0,block_index=0;
    double elapsed_ms=0;
    bool verified=false,windowed=false;
    std::size_t block_size=0;
    Rect level_bounds;
    std::shared_ptr<const PixelStreamFileSummary> declared_summary;
    std::shared_ptr<const PixelPredictionResult> block;
    std::shared_ptr<const PixelStreamLink> link;
};
// Owner-thread controller with one worker and no request queue. Values are
// historical; neither a live source nor a Document capability is restored.
class PixelStreamReadJob {
public:
    PixelStreamReadJob();~PixelStreamReadJob();PixelStreamReadJob(const PixelStreamReadJob&)=delete;
    bool open(const std::string& path,std::uint64_t index=0,std::uint64_t max_bytes=1ULL<<40);
    bool open_link(std::shared_ptr<const PixelStreamLink>,std::uint64_t index=0,std::uint64_t max_bytes=1ULL<<40);
    bool read(std::uint64_t index);bool verify();bool close();
    bool busy() const;void cancel();
    PixelStreamReadStats poll();PixelStreamReadStats wait();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    bool open_impl(const std::string&,std::uint64_t,std::uint64_t,std::shared_ptr<const PixelStreamLink>);
};
// Historical external-file descriptor. Preparing requires full verification;
// loading a project restores metadata only and never accesses its external file.
class PixelStreamLink : public std::enable_shared_from_this<PixelStreamLink> {
public:
    const std::string& path() const {return path_;}
    const PixelStreamFileSummary& summary() const {return summary_;}
    const std::string& model_sha256() const {return model_;}
    std::int64_t source_width() const {return width_;}std::int64_t source_height() const {return height_;}
    std::uint64_t object_id() const {return object_;}std::uint64_t document_revision() const {return revision_;}
    Rect level_bounds() const {return {double(grid_[0]),double(grid_[1]),double(grid_[2]),double(grid_[3])};}
    bool windowed() const {return windowed_;}std::size_t block_size() const {return block_;}
    bool same_content(const PixelStreamLink&) const;
    std::size_t payload_bytes() const;
private:
    std::string path_,model_;PixelStreamFileSummary summary_;
    std::int64_t width_=0,height_=0;std::uint64_t object_=0,revision_=0,grid_[4]{};
    bool windowed_=false;std::size_t block_=0;
    void validate() const;
    friend class PixelStreamFile;friend struct PixelStreamRecordCodec;
};
std::shared_ptr<const PixelStreamLink> prepare_pixel_stream_link(const std::string&,std::uint64_t max_bytes=1ULL<<40,const std::atomic<bool>* cancelled=nullptr);
struct PixelStreamRecord : public std::enable_shared_from_this<PixelStreamRecord> {
    std::uint64_t id=0;std::string name;std::shared_ptr<const PixelStreamLink> link;
    std::size_t payload_bytes() const;
};
}
