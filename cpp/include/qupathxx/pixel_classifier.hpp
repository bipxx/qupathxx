#pragma once
#include "qupathxx/pixel_features.hpp"
namespace qpx {
// Immutable grid-pixel recipe. The caller supplies the sampled resolution;
// source/Document authority and resampling are not inferred from an RGBA buffer.
class PixelClassifier {
public:
    PixelClassifier(const ForestModel&,PixelFeatureOptions,const StainTransform&);
    const ForestModel& forest() const;
    PixelFeatureOptions options() const;
    const StainTransform& transform() const;
    const std::vector<ClassifierFeature>& features() const;
    const std::vector<std::string>& classes() const;
    const std::vector<std::uint8_t>& bytes() const;
    const std::string& sha256() const;
    static PixelClassifier from_bytes(const std::vector<std::uint8_t>&);
private:
    struct Data;
    std::shared_ptr<const Data> data_;
};
class PixelClassification : public std::enable_shared_from_this<PixelClassification> {
public:
    const char* algorithm() const {return "qpx.pixel-classification/1";}
    PixelFeatureWindow window() const {return window_;}
    std::int64_t width() const {return window_.width;}
    std::int64_t height() const {return window_.height;}
    const std::vector<std::uint32_t>& labels() const {return labels_;} // 0=unclassified, otherwise class index+1.
    const std::vector<float>& vote_fractions() const {return fractions_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::vector<std::string>& classes() const {return classes_;}
    const std::vector<std::uint64_t>& class_counts() const {return counts_;}
    std::uint64_t valid_pixels() const {return valid_pixels_;}
    std::uint64_t classified_pixels() const {return classified_pixels_;}
    double min_vote_fraction() const {return minimum_;}
    double elapsed_ms() const {return elapsed_ms_;}
    std::size_t estimated_work_bytes() const {return work_;}
    const std::string& model_sha256() const {return model_sha256_;}
    const std::string& feature_sha256() const {return feature_sha256_;}
    const std::string& input_sha256() const {return input_sha256_;}
    const std::string& sha256() const {return sha256_;}
private:
    PixelClassification()=default;
    PixelFeatureWindow window_;
    std::vector<std::uint32_t> labels_;
    std::vector<float> fractions_;
    std::vector<std::uint8_t> valid_;
    std::vector<std::string> classes_;
    std::vector<std::uint64_t> counts_;
    std::uint64_t valid_pixels_=0,classified_pixels_=0;
    double minimum_=0,elapsed_ms_=0;
    std::size_t work_=0;
    std::string model_sha256_,feature_sha256_,input_sha256_,sha256_;
    friend std::shared_ptr<const PixelClassification> classify_pixels(const PixelBuffer&,const PixelClassifier&,double,std::optional<PixelFeatureWindow>,std::size_t,const std::atomic<bool>*);
};
std::shared_ptr<const PixelClassification> classify_pixels(const PixelBuffer&,const PixelClassifier&,double min_vote_fraction=0,std::optional<PixelFeatureWindow> window={},std::size_t work_bytes=0,const std::atomic<bool>* cancelled=nullptr);
PixelClassifier load_pixel_classifier(const std::string&,const std::atomic<bool>* cancelled=nullptr);
void save_pixel_classifier(const PixelClassifier&,const std::string&,bool replace_existing=false,const std::atomic<bool>* cancelled=nullptr);
}
