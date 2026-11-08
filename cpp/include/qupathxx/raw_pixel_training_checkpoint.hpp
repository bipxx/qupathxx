#pragma once
#include "qupathxx/raw_pixel_tiled_training.hpp"

namespace qpx {
struct RawPixelTrainingCheckpointLimits {
    std::size_t max_bytes=8u*1024*1024,work_bytes=128u*1024*1024;
    void validate() const;
};
// Historical prefix commitments; no live capture or serialized reservoir.
class RawPixelTrainingCheckpoint {
public:
    struct Data;
    static std::shared_ptr<const RawPixelTrainingCheckpoint> from_bytes(
        const std::vector<std::uint8_t>&,RawPixelTrainingCheckpointLimits={},const std::atomic<bool>* cancelled=nullptr);
    const std::vector<std::uint8_t>& bytes() const;
    std::uint64_t completed_windows() const;
    std::uint64_t total_windows() const;
    bool complete() const;
    const std::string& request_sha256() const;
    const std::string& prefix_sha256() const;
private:
    explicit RawPixelTrainingCheckpoint(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    friend struct RawPixelTrainingCheckpointCodec;
};
struct RawPixelTrainingCheckpointOptions {
    RawPixelTrainingCheckpointLimits limits;
    std::string resume_path;
    std::uint64_t max_new_windows=UINT64_MAX;
    bool replace_existing=false;
};
struct RawPixelTrainingCheckpointResult {
    std::shared_ptr<const RawPixelTrainingCheckpoint> checkpoint;
    // Present only after complete replay/sampling. Retains this request's capture.
    std::shared_ptr<const RawPixelTiledTrainingTable> table;
    std::uint64_t replayed_windows=0;
};
std::shared_ptr<const RawPixelTrainingCheckpoint> load_raw_pixel_training_checkpoint(
    const std::string& path,RawPixelTrainingCheckpointLimits={},const std::atomic<bool>* cancelled=nullptr);
// Synchronous borrowed-source execution. Resume repeats prefix IO/features under
// the request budget. Join worker execution before releasing its reader; publish
// through existing owner verification. Successful explicit commits are resume points.
RawPixelTrainingCheckpointResult save_raw_pixel_training_checkpoint(
    const RawPixelTiledTrainingRequest&,const std::string& path,
    RawPixelTrainingCheckpointOptions={},const std::atomic<bool>* cancelled=nullptr);
}
