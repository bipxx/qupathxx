#pragma once
#include "qupathxx/raw_pixel_window_workflows.hpp"
namespace qpx {
struct RawPixelStreamRegion {
    std::int64_t first_x=0,first_y=0,width=1,height=1;
};
struct RawPixelStreamOptions {
    std::int64_t tile_width=256,tile_height=256;
    std::uint64_t max_samples=1000000000000ULL,max_windows=10000000;
    std::size_t work_bytes=256u*1024*1024;
    double min_vote_fraction=0;
    void validate() const;
};
class RawPixelTiledPredictionRequest {
public:
    struct Data;
    RawPixelTiledPredictionRequest(const Document&,const RawPixelClassifier&,ScientificRawLattice,
        RawPixelStreamOptions={},std::optional<RawPixelStreamRegion> region={},std::uint64_t object_id=0);
    bool matches(const Document&) const;
    const ScientificRawLattice& lattice() const;
    const RawPixelStreamRegion& region() const;
    const RawPixelStreamOptions& options() const;
    const RawPixelClassifier& model() const;
    ScientificRawWindow window(std::uint64_t index) const;
    std::uint64_t total_windows() const,total_samples() const;
    std::size_t estimated_work_bytes() const;
    const std::string& sha256() const;
    // Re-read decoded source commitments without feature planes or inference.
    std::string input_sha256(std::uint64_t index,const std::atomic<bool>* cancelled=nullptr) const;
private:
    std::shared_ptr<const Data> data_;
    std::shared_ptr<const RawPixelWindowPredictionRequest> authority_;
    bool publication_matches(const Document&) const;
    friend class RawPixelPredictionStream;
    friend class RawPixelPredictionStreamJob;
    friend struct RawPixelStreamFileCodec;
};
// Single-consumer synchronous producer. next() performs bounded native IO and
// inference; returned blocks own their arrays. Caller-retained blocks are outside
// the producer's work budget. Only cancel and progress getters may run concurrently
// with next(); the caller must join its worker before destroying the stream.
class RawPixelPredictionStream {
public:
    explicit RawPixelPredictionStream(const RawPixelTiledPredictionRequest&);
    ~RawPixelPredictionStream();RawPixelPredictionStream(const RawPixelPredictionStream&)=delete;
    std::shared_ptr<const RawPixelWindowClassification> next(); // Null only at successful exhaustion.
    void cancel();bool busy() const;
    bool matches(const Document&) const;
    std::string state() const;
    std::uint64_t total_windows() const,completed_windows() const,completed_samples() const;
    std::uint64_t valid_samples() const,classified_samples() const;
    std::vector<std::uint64_t> class_counts() const;
    std::size_t estimated_work_bytes() const;
    // Complete-stream commitments; partial/cancelled/failed streams reject.
    std::string input_sha256() const,sha256() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    RawPixelPredictionStream(const RawPixelTiledPredictionRequest&,std::shared_ptr<std::atomic<bool>>);
    friend class RawPixelPredictionStreamJob;
    friend struct RawPixelStreamFileCodec;
    std::shared_ptr<const RawPixelWindowClassification> next_impl(const std::atomic<bool>*);
    void restore_block(const RawPixelWindowClassification&,const std::atomic<bool>*);
    void accept_block(const RawPixelWindowClassification&,const std::atomic<bool>*);
};
struct RawPixelStreamJobStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t generation=0,total_windows=0,produced_windows=0,delivered_windows=0;
    std::uint64_t total_samples=0,delivered_samples=0,valid_samples=0,classified_samples=0;
    std::vector<std::uint64_t> class_counts;
    double elapsed_ms=0;
};
// One worker/result slot. poll/wait deliver readiness, take validates the owner
// and fresh files, returns that block and starts the next. Until take, no next
// block is decoded. Only cancel may run off the controller's owner thread.
class RawPixelPredictionStreamJob {
public:
    RawPixelPredictionStreamJob();~RawPixelPredictionStreamJob();
    RawPixelPredictionStreamJob(const RawPixelPredictionStreamJob&)=delete;
    bool request(const RawPixelTiledPredictionRequest&);
    bool busy() const;void cancel();void await_completion();
    RawPixelStreamJobStats poll(const Document&),wait(const Document&);
    std::shared_ptr<const RawPixelWindowClassification> take(const Document&);
    std::string input_sha256() const,sha256() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
