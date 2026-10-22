#pragma once
#include "qupathxx/pixel_training.hpp"
namespace qpx {
struct PixelPredictionOptions {
    std::size_t tile_size=128,work_bytes=256u*1024*1024;
    double min_vote_fraction=0;
    void validate() const;
};
class PixelPredictionRequest {
public:
    struct Data;
    PixelPredictionRequest(const Document&,std::uint64_t object_id,const PixelClassifier&,PixelPredictionOptions={});
    bool matches(const Document&) const;
    std::uint64_t object_id() const;
    std::uint64_t document_revision() const;
    std::int32_t level() const;
    double downsample() const;
    Rect level_bounds() const;
    std::size_t estimated_work_bytes() const;
    std::uint64_t total_tiles() const;
private:
    std::shared_ptr<const Data> data_;
    std::optional<ObjectFeatureRequest> authority_;
    std::shared_ptr<ImageServer> source_;
    friend struct PixelPredictionKernel;
    friend class PixelPredictionResult;
};
class PixelPredictionResult : public std::enable_shared_from_this<PixelPredictionResult> {
public:
    const char* algorithm() const {return "qpx.source-pixel-classification/1";}
    bool matches(const PixelPredictionRequest&) const;
    std::int64_t width() const;
    std::int64_t height() const;
    std::uint64_t object_id() const;
    std::int32_t level() const;
    double downsample() const;
    Rect bounds() const;
    Rect level_bounds() const;
    const Roi& roi() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    const PixelClassifier& model() const;
    PixelPredictionOptions options() const;
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<float>& vote_fractions() const {return fractions_;}
    const std::vector<std::uint64_t>& class_counts() const {return counts_;}
    std::uint64_t valid_pixels() const {return valid_pixels_;}
    std::uint64_t classified_pixels() const {return classified_pixels_;}
    std::size_t estimated_work_bytes() const;
    double elapsed_ms() const {return ms_;}
    const std::string& sha256() const {return sha_;}
    const std::string& input_sha256() const {return input_sha_;}
private:
    PixelPredictionResult()=default;
    std::shared_ptr<const PixelPredictionRequest::Data> snapshot_;
    std::vector<std::uint32_t> labels_;
    std::vector<std::uint8_t> valid_;
    std::vector<float> fractions_;
    std::vector<std::uint64_t> counts_;
    std::uint64_t valid_pixels_=0,classified_pixels_=0;double ms_=0;
    std::string sha_,input_sha_;
    friend struct PixelPredictionKernel;
};
std::shared_ptr<const PixelPredictionResult> predict_pixel_region(const PixelPredictionRequest&,const std::atomic<bool>* cancelled=nullptr);
class PixelPredictionJob {
public:
    PixelPredictionJob();~PixelPredictionJob();PixelPredictionJob(const PixelPredictionJob&)=delete;
    bool request(const PixelPredictionRequest&);
    bool busy() const;void cancel();
    PixelTrainingStats poll(const Document&);PixelTrainingStats wait(const Document&);
    std::shared_ptr<const PixelPredictionResult> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
