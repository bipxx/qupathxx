#pragma once
#include "qupathxx/raw_pixel_stream.hpp"
#include "qupathxx/provenance.hpp"
namespace qpx {
class RawPixelStreamLink;
struct FileVersion;
struct RawPixelStreamFileLimits {
    std::uint64_t max_bytes=1ULL<<40;
    // Independent file metadata/one-block budget. A simultaneous live producer
    // retains its separately admitted request budget.
    std::size_t work_bytes=256u*1024*1024;
    void validate() const;
};
struct RawPixelStreamFileSummary {
    std::uint64_t bytes=0,total_windows=0,completed_windows=0,valid_samples=0,classified_samples=0;
    std::vector<std::uint64_t> class_counts;
    bool complete=false;
    std::string input_sha256,sha256,content_sha256;
    std::uint64_t reused_windows=0; // Save-call observation; not encoded history.
};
struct RawPixelStreamSaveOptions {
    RawPixelStreamFileLimits limits;
    std::string resume_path;
    std::uint64_t max_new_windows=UINT64_MAX;
    bool replace_existing=false;
};
// Immutable historical prefix or complete stream. Opening checks bounded header,
// footer and framing; read_block validates its block, verify validates all prefix
// blocks/chains/totals. No Document or live capture is restored by decoding.
class RawPixelStreamFile {
public:
    explicit RawPixelStreamFile(const std::string&,RawPixelStreamFileLimits={});
    ~RawPixelStreamFile();RawPixelStreamFile(const RawPixelStreamFile&)=delete;
    const RawPixelStreamFileSummary& declared_summary() const;
    const RawPixelClassifier& model() const;
    const ScientificRawLattice& lattice() const;
    const RawPixelStreamRegion& region() const;
    const RawPixelStreamOptions& options() const;
    const RawPixelStreamFileLimits& limits() const;
    std::size_t payload_bytes() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    std::shared_ptr<const RawPixelWindowClassification> read_block(std::uint64_t,const std::atomic<bool>* cancelled=nullptr) const;
    void verify(const std::atomic<bool>* cancelled=nullptr) const;
    // Lightweight identity/stamp check for delivery of previously read data.
    // Does not scan blocks or turn an unverified prefix into a verified one.
    void check_unchanged() const;
    std::shared_ptr<const RawPixelStreamLink> link() const;
    void cancel();bool verified() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    friend struct RawPixelStreamFileCodec;friend struct RawPixelFileContourKernel;
};
// Explicit durable prefix checkpoints: max_new_windows bounds this save call.
// Resume verifies every retained source input without inference, then computes
// remaining windows. Atomic replacement preserves a previous checkpoint on error
// or cancellation; source files/aliases cannot be used as output destinations.
RawPixelStreamFileSummary save_raw_pixel_stream(const RawPixelTiledPredictionRequest&,const std::string&,
    RawPixelStreamSaveOptions={},const std::atomic<bool>* cancelled=nullptr);
// Historical external-file metadata. Preparing requires full prefix verification;
// project decoding restores no verification observation or Document authority.
class RawPixelStreamLink {
public:
    const std::string& path() const {return path_;}
    const ScientificRawLattice& lattice() const {return lattice_;}
    const RawPixelStreamRegion& region() const {return region_;}
    const RawPixelStreamOptions& options() const {return options_;}
    const RawPixelStreamFileSummary& summary() const {return summary_;}
    const std::string& model_sha256() const {return model_;}
    const std::string& request_sha256() const {return request_;}
    const std::string& header_sha256() const {return header_;}
    std::uint64_t object_id() const {return object_;}
    std::uint64_t document_revision() const {return revision_;}
    bool same_content(const RawPixelStreamLink&) const;
    std::size_t payload_bytes() const;
private:
    RawPixelStreamLink(ScientificRawLattice l,RawPixelStreamRegion r,RawPixelStreamOptions o):lattice_(std::move(l)),region_(r),options_(o) {}
    std::string path_,model_,request_,header_;
    ScientificRawLattice lattice_;RawPixelStreamRegion region_;RawPixelStreamOptions options_;
    RawPixelStreamFileSummary summary_;
    std::uint64_t object_=0,revision_=0,header_bytes_=0;
    std::shared_ptr<const FileVersion> version_;
    void validate() const;
    void check_verified_file() const;
    friend class RawPixelStreamFile;friend struct RawPixelStreamRecordCodec;friend class Document;
};
std::shared_ptr<const RawPixelStreamLink> prepare_raw_pixel_stream_link(const std::string&,
    RawPixelStreamFileLimits={},const std::atomic<bool>* cancelled=nullptr);
struct RawPixelStreamRecord {
    std::uint64_t id=0;std::string name;std::shared_ptr<const RawPixelStreamLink> link;
    std::size_t payload_bytes() const {return sizeof(*this)+name.capacity()+(link?link->payload_bytes():0);}
};
}
