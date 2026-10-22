#pragma once
#include "qupathxx/classifier.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/provenance.hpp"

namespace qpx {
struct ObjectFeatureOptions {
    bool calibrated=true, nucleus_shape=false, cell_rgb=false, cytoplasm_rgb=false;
    std::size_t snapshot_bytes=64*1024*1024;
    void validate() const;
    static ObjectFeatureOptions from_model(const ForestModel&);
};
// Capture/matches are owner-thread operations. Feature kernels use only native
// immutable values, and never call an ImageServer or read a live Document.
class ObjectFeatureRequest {
public:
    ObjectFeatureRequest(const Document&,const std::vector<std::uint64_t>& ids,ObjectFeatureOptions={});
    bool matches(const Document&) const;
    std::size_t row_count() const;
    std::size_t snapshot_bytes() const;
    std::uint64_t document_revision() const;
    double capture_ms() const;
    ObjectFeatureOptions options() const;
    const std::shared_ptr<const AnalysisProvenance>& source_anchor() const;
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    std::shared_ptr<const char> identity_;
    std::shared_ptr<ImageServer> source_; // Retired on the controller thread.
    friend struct ObjectClassifierKernel;
    friend class ObjectFeatureTable;
};
class ObjectFeatureTable {
public:
    const std::vector<std::uint64_t>& object_ids() const {return ids_;}
    const std::vector<std::uint64_t>& object_revisions() const {return revisions_;}
    const std::vector<ObjectKind>& object_kinds() const {return kinds_;}
    ObjectFeatureOptions options() const;
    const std::vector<std::string>& class_names() const {return labels_;}
    const std::vector<ClassifierFeature>& features() const {return features_;}
    const std::vector<float>& values() const {return values_;}
    const std::string& sha256() const {return digest_;}
    const std::shared_ptr<const AnalysisProvenance>& source_anchor() const {return anchor_;}
private:
    ObjectFeatureTable()=default;
    std::shared_ptr<const ObjectFeatureRequest::Data> snapshot_;
    std::vector<std::uint64_t> ids_,revisions_;
    std::vector<ObjectKind> kinds_;
    std::vector<std::string> labels_;
    std::vector<ClassifierFeature> features_;
    std::vector<float> values_;
    std::string digest_;
    std::shared_ptr<const AnalysisProvenance> anchor_;
    friend struct ObjectClassifierKernel;
};
std::shared_ptr<const ObjectFeatureTable> extract_object_features(const ObjectFeatureRequest&,const std::atomic<bool>* cancelled=nullptr);
ForestModel train_object_forest(const ObjectFeatureTable&,ForestParameters={},const std::atomic<bool>* cancelled=nullptr);

class ObjectClassificationResult {
public:
    const std::shared_ptr<const ObjectFeatureTable>& table() const {return table_;}
    const std::shared_ptr<const ForestModel>& model() const {return model_;}
    const std::vector<ClassifierPrediction>& predictions() const {return predictions_;}
    const std::string& model_sha256() const {return model_sha256_;}
    double elapsed_ms() const {return elapsed_ms_;}
private:
    ObjectClassificationResult()=default;
    std::shared_ptr<const ObjectFeatureTable> table_;
    std::shared_ptr<const ForestModel> model_;
    std::vector<ClassifierPrediction> predictions_;
    std::string model_sha256_;
    double elapsed_ms_=0;
    friend struct ObjectClassifierKernel;
};
std::shared_ptr<const ObjectClassificationResult> classify_objects(const ObjectFeatureRequest&,const ForestModel&,const std::atomic<bool>* cancelled=nullptr);
// One compound class edit, preserving names, geometry, hierarchy and cell links.
// Low-vote rows remain unchanged. The complete request must still be current.
std::vector<std::uint64_t> apply_object_classification(Document&,const ObjectFeatureRequest&,const ObjectClassificationResult&,double min_vote_fraction=0);

struct ObjectClassifierStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t generation=0,completed_rows=0,total_rows=0;
    double elapsed_ms=0;
};
class ObjectClassifierJob {
public:
    ObjectClassifierJob();~ObjectClassifierJob();
    ObjectClassifierJob(const ObjectClassifierJob&)=delete;
    bool train(const ObjectFeatureRequest&,ForestParameters={});
    bool predict(const ObjectFeatureRequest&,const ForestModel&);
    bool busy() const;void cancel();
    ObjectClassifierStats poll(const Document&);
    ObjectClassifierStats wait(const Document&);
    std::shared_ptr<const ForestModel> model() const;
    std::shared_ptr<const ObjectFeatureTable> table() const;
    std::shared_ptr<const ObjectClassificationResult> result() const;
    std::vector<std::uint64_t> apply(Document&,double min_vote_fraction=0);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
