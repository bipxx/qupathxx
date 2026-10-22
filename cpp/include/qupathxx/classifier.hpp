#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace qpx {
struct ClassifierFeature {
    std::string name, unit;
    ClassifierFeature()=default;
    ClassifierFeature(std::string n,std::string u):name(std::move(n)),unit(std::move(u)) {}
};
struct ForestParameters {
    std::uint32_t trees=64, max_depth=12, min_samples=2, active_features=0;
    std::uint64_t seed=1, work_bytes=256*1024*1024;
    void validate() const;
};
struct ClassifierPrediction {
    std::uint32_t class_index=0;
    double vote_fraction=0; // Tree vote fraction; not a calibrated probability.
    std::vector<std::uint32_t> votes;
};
// Immutable ordered-float forest. Prediction and persistence need no OpenCV.
// Feature order and units are an exact contract, including at prediction time.
class ForestModel {
public:
    struct Data;
    explicit ForestModel(std::shared_ptr<const Data>);
    const std::vector<ClassifierFeature>& features() const;
    const std::vector<std::string>& classes() const;
    const ForestParameters& parameters() const;
    const std::string& training_sha256() const;
    const std::string& backend() const;
    std::uint32_t tree_count() const;
    std::size_t node_count() const;
    std::uint64_t training_rows() const;
    std::vector<ClassifierPrediction> predict(const std::vector<float>& row_major,
        const std::vector<ClassifierFeature>& schema) const;
    std::vector<std::uint8_t> bytes() const;
    std::string sha256() const;
    static ForestModel from_bytes(const std::vector<std::uint8_t>&);
private:
    std::shared_ptr<const Data> data_;
};
bool has_classifier_training();
// Labels index the supplied class list. Every class must have >=2 rows.
// Inputs: <=100000 rows, <=128 features, 2..256 classes, finite values with
// magnitude <=FLT_MAX/4. Work budget is a conservative allocation estimate,
// not a process RSS limit. No automatic scaling, imputation or balancing.
ForestModel train_forest(const std::vector<float>& row_major,
    const std::vector<std::uint32_t>& labels,
    const std::vector<ClassifierFeature>& schema,
    const std::vector<std::string>& classes, ForestParameters={});
}
