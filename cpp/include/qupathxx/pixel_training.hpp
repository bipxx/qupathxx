#pragma once
#include "qupathxx/pixel_classifier.hpp"
#include "qupathxx/object_classifier.hpp"
namespace qpx {
ForestParameters pixel_forest_parameters(); // High-level pixel preset: 32 trees, depth 8.
struct PixelTrainingOptions {
    PixelFeatureOptions features;
    std::int32_t level=0;
    bool calibrated=true,balanced=true;
    std::size_t tile_size=128,max_samples=20000,max_samples_per_class=2000,work_bytes=256u*1024*1024;
    std::uint64_t seed=1,max_scanned_pixels=10000000;
    void validate() const;
};
// Owner capture/matches; kernels never inspect a live Document. The controller
// retains source ownership until its worker has joined.
class PixelTrainingRequest {
public:
    struct Data;
    PixelTrainingRequest(const Document&,const std::vector<std::uint64_t>& annotation_ids,const StainTransform&,PixelTrainingOptions={});
    bool matches(const Document&) const;
    PixelTrainingOptions options() const;
    PixelResolution resolution() const;
    std::uint64_t document_revision() const;
    std::size_t snapshot_bytes() const;
    double capture_ms() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
private:
    std::shared_ptr<const Data> data_;
    std::optional<ObjectFeatureRequest> authority_;
    std::shared_ptr<ImageServer> source_;
    friend struct PixelTrainingKernel;
    friend struct PixelClassifierArchiveCodec;
    friend class Document;
};
class PixelTrainingTable : public std::enable_shared_from_this<PixelTrainingTable> {
public:
    const char* algorithm() const {return "qpx.annotation-pixel-sampling/1";}
    std::size_t row_count() const {return labels_.size();}
    const std::vector<float>& values() const {return values_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<std::int64_t>& coordinates() const {return coordinates_;} // row-major x,y grid indices.
    const std::vector<std::uint64_t>& annotation_ids() const {return ids_;}
    const std::vector<std::string>& classes() const;
    const std::vector<ClassifierFeature>& features() const {return features_;}
    const std::vector<std::uint64_t>& eligible_class_pixels() const {return eligible_;}
    const std::vector<std::uint64_t>& sampled_class_counts() const {return counts_;}
    std::uint64_t ambiguous_pixels() const {return ambiguous_;}
    std::uint64_t scanned_pixels() const {return scanned_;}
    std::uint64_t tiles() const {return tiles_;}
    PixelResolution resolution() const;
    PixelTrainingOptions options() const;
    const StainTransform& transform() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    const std::string& sha256() const {return sha_;}
    const std::string& input_sha256() const {return input_sha_;}
    std::size_t estimated_work_bytes() const {return work_;}
    std::size_t retained_bytes() const;
    double elapsed_ms() const {return ms_;}
private:
    PixelTrainingTable()=default;
    std::shared_ptr<const PixelTrainingRequest::Data> snapshot_;
    std::vector<float> values_;
    std::vector<std::uint32_t> labels_;
    std::vector<std::int64_t> coordinates_;
    std::vector<std::uint64_t> ids_,eligible_,counts_;
    std::vector<ClassifierFeature> features_;
    std::uint64_t ambiguous_=0,scanned_=0,tiles_=0;
    std::size_t work_=0;double ms_=0;
    std::string sha_,input_sha_;
    friend struct PixelTrainingKernel;
    friend struct PixelClassifierArchiveCodec;
    friend class Document;
};
std::shared_ptr<const PixelTrainingTable> sample_pixel_training(const PixelTrainingRequest&,const std::atomic<bool>* cancelled=nullptr);
PixelClassifier train_pixel_classifier(const PixelTrainingTable&,ForestParameters=pixel_forest_parameters(),const std::atomic<bool>* cancelled=nullptr);
// Natural levels only; physical models match both calibrated axes, otherwise
// downsample is matched. No silent resampling or automatic model reinterpretation.
std::int32_t pixel_classifier_level(const Document&,const PixelClassifier&);
struct PixelTrainingStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t generation=0,completed_tiles=0,total_tiles=0;
    double elapsed_ms=0;
};
class PixelTrainingJob {
public:
    PixelTrainingJob();~PixelTrainingJob();PixelTrainingJob(const PixelTrainingJob&)=delete;
    bool train(const PixelTrainingRequest&,ForestParameters=pixel_forest_parameters());
    bool busy() const;void cancel();
    PixelTrainingStats poll(const Document&);PixelTrainingStats wait(const Document&);
    // Native readiness only: no Document access, delivery or captured-owner release.
    void await_completion();
    std::shared_ptr<const PixelTrainingTable> table() const;
    std::shared_ptr<const PixelClassifier> model() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
