#pragma once
#include "qupathxx/raw_pixel_features.hpp"

namespace qpx {
// Immutable reusable channel/normalization/sampling contract. No training image
// path, content hash, image dimensions, ROI or grid origin is a model property.
class RawPixelRecipe {
public:
    struct Data;
    RawPixelRecipe(const RawImageMetadata&,const ScientificGrid&,ScientificRawOptions,
        RawPixelFeatureOptions,bool physical_sigmas=false);
    RawPixelRecipe(const RawImageMetadata&,const ScientificRawWindow&,ScientificRawOptions,
        RawPixelFeatureOptions,bool physical_sigmas=false);
    RawPixelRecipe(const RawImageMetadata&,const ScientificRawLattice&,ScientificRawOptions,
        RawPixelFeatureOptions,bool physical_sigmas=false);
    const RawPixelFeatureOptions& requested_features() const;
    const RawPixelFeatureOptions& features() const; // Resolved sampled-grid units.
    const ScientificRawOptions& selection() const;
    SampleType sample_type() const;
    std::uint32_t series() const;
    const std::vector<std::string>& channel_names() const;
    GridFraction step_x() const;
    GridFraction step_y() const;
    bool physical_sigmas() const;
    std::optional<double> sampled_pixel_width_um() const;
    std::optional<double> sampled_pixel_height_um() const;
    std::vector<ClassifierFeature> feature_schema() const;
    bool matches(const RawImageMetadata&,const ScientificGrid&,const ScientificRawOptions&) const;
    bool matches(const RawImageMetadata&,const ScientificRawWindow&,const ScientificRawOptions&) const;
    bool matches(const RawImageMetadata&,const ScientificRawLattice&,const ScientificRawOptions&) const;
    std::vector<std::uint8_t> bytes() const;
    static RawPixelRecipe from_bytes(const std::vector<std::uint8_t>&);
private:
    std::shared_ptr<const Data> data_;
    explicit RawPixelRecipe(std::shared_ptr<const Data>);
    template<class Geometry> static std::shared_ptr<const Data> capture(const RawImageMetadata&,const Geometry&,ScientificRawOptions,RawPixelFeatureOptions,bool);
    template<class Geometry> bool matches_geometry(const RawImageMetadata&,const Geometry&,const ScientificRawOptions&) const;
    friend struct RawPixelModelCodec;
};
class RawPixelClassifier {
public:
    RawPixelClassifier(const ForestModel&,const RawPixelRecipe&);
    const ForestModel& forest() const;
    const RawPixelRecipe& recipe() const;
    const std::vector<std::uint8_t>& bytes() const;
    const std::string& sha256() const;
    static RawPixelClassifier from_bytes(const std::vector<std::uint8_t>&);
private:
    struct Data;
    std::shared_ptr<const Data> data_;
};
// Captured-grid classifier result. Grid retains both axes; labels are 0 for
// unclassified or class_index+1. Fractions are tree votes, not probabilities.
class RawPixelClassification {
public:
    const ScientificGrid& grid() const {return grid_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<float>& vote_fractions() const {return fractions_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<std::uint64_t>& class_counts() const {return counts_;}
    const std::string& model_sha256() const {return model_sha_;}
    const std::string& feature_sha256() const {return feature_sha_;}
    const std::string& input_sha256() const {return input_sha_;}
    const std::string& sha256() const {return sha_;}
    std::size_t estimated_work_bytes() const {return work_;}
private:
    explicit RawPixelClassification(const ScientificGrid& grid):grid_(grid) {}
    ScientificGrid grid_;
    std::vector<std::uint32_t> labels_;
    std::vector<float> fractions_;
    std::vector<std::uint8_t> valid_;
    std::vector<std::uint64_t> counts_;
    std::string model_sha_,feature_sha_,input_sha_,sha_;
    std::size_t work_=0;
    friend struct RawPixelClassifierKernel;
    friend struct RawPixelPredictionKernel;
    friend struct RawPixelArchiveCodec;
    friend class Document;
    friend struct RawPixelContourKernel;
    std::shared_ptr<const char> capture_;
    friend std::shared_ptr<const RawPixelClassification> classify_raw_pixels(
        const ScientificRawGrid&,const RawPixelClassifier&,double,std::size_t,const std::atomic<bool>*);
};
// Window output is a distinct owning type; no legacy grid/archive consumer can
// mistake local core counts for the original sampling denominators.
class RawPixelWindowClassification {
public:
    const ScientificRawWindow& window() const {return grid_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<float>& vote_fractions() const {return fractions_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<std::uint64_t>& class_counts() const {return counts_;}
    const std::string& model_sha256() const {return model_sha_;}
    const std::string& feature_sha256() const {return feature_sha_;}
    const std::string& input_sha256() const {return input_sha_;}
    const std::string& sha256() const {return sha_;}
    std::size_t estimated_work_bytes() const {return work_;}
private:
    explicit RawPixelWindowClassification(const ScientificRawWindow& window):grid_(window) {}
    ScientificRawWindow grid_;
    std::vector<std::uint32_t> labels_;
    std::vector<float> fractions_;
    std::vector<std::uint8_t> valid_;
    std::vector<std::uint64_t> counts_;
    std::string model_sha_,feature_sha_,input_sha_,sha_;
    std::size_t work_=0;
    std::shared_ptr<const char> capture_;
    friend struct RawPixelClassifierKernel;
    friend struct RawPixelWindowPredictionKernel;
    friend struct RawPixelStreamFileCodec;
};
std::shared_ptr<const RawPixelClassification> classify_raw_pixels(const ScientificRawGrid&,
    const RawPixelClassifier&,double min_vote_fraction=0,std::size_t work_bytes=0,
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<const RawPixelWindowClassification> classify_raw_pixel_window(const ScientificRawWindowData&,
    const RawPixelClassifier&,double min_vote_fraction=0,std::size_t work_bytes=0,const std::atomic<bool>* cancelled=nullptr);
RawPixelClassifier load_raw_pixel_classifier(const std::string&,const std::atomic<bool>* cancelled=nullptr);
void save_raw_pixel_classifier(const RawPixelClassifier&,const std::string&,bool replace_existing=false,
    const std::atomic<bool>* cancelled=nullptr);
}
