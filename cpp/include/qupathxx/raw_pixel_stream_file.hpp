#pragma once
#include "qupathxx/raw_pixel_stream.hpp"
#include "qupathxx/provenance.hpp"
namespace qpx {
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
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    std::shared_ptr<const RawPixelWindowClassification> read_block(std::uint64_t,const std::atomic<bool>* cancelled=nullptr) const;
    void verify(const std::atomic<bool>* cancelled=nullptr) const;
    void cancel();bool verified() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    friend struct RawPixelStreamFileCodec;
};
// Explicit durable prefix checkpoints: max_new_windows bounds this save call.
// Resume verifies every retained source input without inference, then computes
// remaining windows. Atomic replacement preserves a previous checkpoint on error
// or cancellation; source files/aliases cannot be used as output destinations.
RawPixelStreamFileSummary save_raw_pixel_stream(const RawPixelTiledPredictionRequest&,const std::string&,
    RawPixelStreamSaveOptions={},const std::atomic<bool>* cancelled=nullptr);
}
