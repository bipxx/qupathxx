#pragma once
#include "qupathxx/pixel_prediction.hpp"
namespace qpx {
struct PixelStreamOptions {
    PixelPredictionOptions prediction;
    std::size_t block_size=256;
    std::uint64_t max_pixels=1000000000000ULL,max_blocks=10000000;
    void validate() const;
};
// Single-consumer, synchronous native producer. Only cancel and atomic progress
// queries may run concurrently with next(). Returned blocks own their arrays;
// retaining blocks is caller-owned storage, outside the producer's work budget.
class PixelPredictionStream {
public:
    PixelPredictionStream(const Document&,std::uint64_t object_id,const PixelClassifier&,PixelStreamOptions={},std::optional<Rect> window={});
    ~PixelPredictionStream();PixelPredictionStream(const PixelPredictionStream&)=delete;
    std::shared_ptr<const PixelPredictionResult> next(); // Empty only at successful exhaustion.
    void cancel();bool busy() const;
    bool matches(const Document&) const;
    std::string state() const;
    Rect level_bounds() const;std::int32_t level() const;double downsample() const;
    bool windowed() const;
    std::uint64_t object_id() const,document_revision() const,total_blocks() const,completed_blocks() const;
    std::uint64_t valid_pixels() const,classified_pixels() const;
    std::vector<std::uint64_t> class_counts() const;
    std::size_t estimated_work_bytes() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
