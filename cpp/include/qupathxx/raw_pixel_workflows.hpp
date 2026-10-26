#pragma once
#include "qupathxx/raw_pixel_classifier.hpp"

namespace qpx {
class Document;
struct RawPixelTrainingOptions {
    std::size_t max_samples=20000,max_samples_per_class=2000,work_bytes=256u*1024*1024;
    std::uint64_t seed=1,max_scanned_samples=10000000;
    bool balanced=true;
    void validate() const;
};
// Capture and matches run on the document owner. Workers see native immutable
// ROI/recipe/source snapshots and never inspect a live Document.
class RawPixelTrainingRequest {
public:
    struct Data;
    RawPixelTrainingRequest(const Document&,const std::vector<std::uint64_t>& annotation_ids,
        ScientificGrid,ScientificRawOptions,RawPixelFeatureOptions,RawPixelTrainingOptions={},bool physical_sigmas=false);
    bool matches(const Document&) const;
    const ScientificGrid& grid() const;
    const RawPixelRecipe& recipe() const;
    const RawPixelTrainingOptions& options() const;
    std::size_t snapshot_bytes() const;
    std::size_t estimated_work_bytes() const;
private:
    std::shared_ptr<const Data> data_;
    struct Authority;
    std::shared_ptr<const Authority> authority_;
    std::shared_ptr<ImageServer> source_;
    bool publication_matches(const Document&) const;
    friend struct RawPixelTrainingKernel;
    friend class RawPixelTrainingJob;
};
class RawPixelTrainingTable {
public:
    const RawPixelRecipe& recipe() const;
    const ScientificGrid& grid() const;
    const std::vector<float>& values() const {return values_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<std::string>& classes() const;
    const RawPixelTrainingOptions& options() const;
    const std::vector<std::int64_t>& coordinates() const {return coordinates_;} // Sample indices, X/Y pairs.
    const std::vector<std::uint64_t>& annotation_ids() const {return ids_;}
    const std::vector<std::uint64_t>& eligible_class_samples() const {return eligible_;}
    const std::vector<std::uint64_t>& sampled_class_counts() const {return counts_;}
    std::uint64_t ambiguous_samples() const {return ambiguous_;}
    std::size_t row_count() const {return labels_.size();}
    std::size_t estimated_work_bytes() const;
    std::size_t retained_bytes() const;
    const std::string& input_sha256() const {return input_sha_;}
    const std::string& sha256() const {return sha_;}
private:
    RawPixelTrainingTable()=default;
    std::shared_ptr<const RawPixelTrainingRequest::Data> snapshot_;
    std::vector<float> values_;
    std::vector<std::uint32_t> labels_;
    std::vector<std::int64_t> coordinates_;
    std::vector<std::uint64_t> ids_,eligible_,counts_;
    std::uint64_t ambiguous_=0;
    std::string input_sha_,sha_;
    friend struct RawPixelTrainingKernel;
};
std::shared_ptr<const RawPixelTrainingTable> sample_raw_pixel_training(const RawPixelTrainingRequest&,const std::atomic<bool>* cancelled=nullptr);
RawPixelClassifier train_raw_pixel_classifier(const RawPixelTrainingTable&,ForestParameters={},const std::atomic<bool>* cancelled=nullptr);
class RawPixelPredictionRequest {
public:
    struct Data;
    RawPixelPredictionRequest(const Document&,const RawPixelClassifier&,ScientificGrid,std::uint64_t object_id=0,
        double min_vote_fraction=0,std::size_t work_bytes=0);
    bool matches(const Document&) const;
    const ScientificGrid& grid() const;
    const RawPixelClassifier& model() const;
    std::size_t estimated_work_bytes() const;
private:
    std::shared_ptr<const Data> data_;
    struct Authority;
    std::shared_ptr<const Authority> authority_;
    std::shared_ptr<ImageServer> source_;
    bool publication_matches(const Document&) const;
    friend struct RawPixelPredictionKernel;
    friend class RawPixelPredictionJob;
};
std::shared_ptr<const RawPixelClassification> predict_raw_pixel_region(const RawPixelPredictionRequest&,const std::atomic<bool>* cancelled=nullptr);
struct RawPixelJobStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t generation=0,completed_samples=0,total_samples=0;
    double elapsed_ms=0;
};
class RawPixelTrainingJob {
public:
    RawPixelTrainingJob();~RawPixelTrainingJob();RawPixelTrainingJob(const RawPixelTrainingJob&)=delete;
    bool sample(const RawPixelTrainingRequest&);
    bool train(const RawPixelTrainingRequest&,ForestParameters={});
    bool busy() const;void cancel();void await_completion();
    RawPixelJobStats poll(const Document&);RawPixelJobStats wait(const Document&);
    std::shared_ptr<const RawPixelTrainingTable> table() const;
    std::shared_ptr<const RawPixelClassifier> model() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    bool start(const RawPixelTrainingRequest&,std::optional<ForestParameters>);
};
class RawPixelPredictionJob {
public:
    RawPixelPredictionJob();~RawPixelPredictionJob();RawPixelPredictionJob(const RawPixelPredictionJob&)=delete;
    bool request(const RawPixelPredictionRequest&);
    bool busy() const;void cancel();void await_completion();
    RawPixelJobStats poll(const Document&);RawPixelJobStats wait(const Document&);
    std::shared_ptr<const RawPixelClassification> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
