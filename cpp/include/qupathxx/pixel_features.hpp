#pragma once
#include "qupathxx/classifier.hpp"
#include "qupathxx/stains.hpp"
#include <optional>
namespace qpx {
struct PixelFeatureOptions {
    bool rgb=true,optical_density=false,stains=false,gradient_magnitude=false,laplacian=false;
    std::vector<double> sigmas{0,1,2,4}; // Standard deviations in sampled-grid pixels.
    std::size_t work_bytes=256u*1024*1024;
    void validate() const;
    std::size_t feature_count() const;
    std::int64_t halo() const;
};
struct PixelFeatureWindow {
    std::int64_t image_width=0,image_height=0,x=0,y=0,width=0,height=0,input_x=0,input_y=0;
    PixelFeatureWindow()=default;
    PixelFeatureWindow(std::int64_t iw,std::int64_t ih,std::int64_t x_,std::int64_t y_,std::int64_t w,std::int64_t h,std::int64_t ix=0,std::int64_t iy=0):image_width(iw),image_height(ih),x(x_),y(y_),width(w),height(h),input_x(ix),input_y(iy) {}
};
std::vector<ClassifierFeature> pixel_feature_schema(const PixelFeatureOptions&,const StainTransform&);
std::size_t pixel_feature_work_bytes(std::int64_t input_width,std::int64_t input_height,const PixelFeatureOptions&,std::optional<PixelFeatureWindow> window={});
// Owned numeric output only. Alpha !=255 produces NaN features and valid=0.
// ROI bounds select output, while image context supplies smoothing/derivatives.
class PixelFeatureImage : public std::enable_shared_from_this<PixelFeatureImage> {
public:
    const char* algorithm() const {return "qpx.pixel-features/1";}
    std::int64_t width() const {return window_.width;}
    std::int64_t height() const {return window_.height;}
    PixelFeatureWindow window() const {return window_;}
    PixelFeatureOptions options() const {return options_;}
    const std::vector<ClassifierFeature>& features() const {return features_;}
    const std::vector<float>& values() const {return values_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const ColorVector& white() const {return white_;}
    const std::array<ColorVector,3>& stains() const {return stains_;}
    std::uint64_t valid_pixels() const {return valid_pixels_;}
    std::size_t estimated_work_bytes() const {return work_bytes_;}
    std::size_t retained_bytes() const;
    double elapsed_ms() const {return elapsed_ms_;}
    const std::string& sha256() const {return sha256_;}
    const std::string& input_sha256() const {return input_sha256_;}
    std::int64_t context_x() const {return cx_;}
    std::int64_t context_y() const {return cy_;}
    std::int64_t context_width() const {return cw_;}
    std::int64_t context_height() const {return ch_;}
private:
    PixelFeatureImage()=default;
    PixelFeatureWindow window_;
    PixelFeatureOptions options_;
    std::vector<ClassifierFeature> features_;
    std::vector<float> values_;
    std::vector<std::uint8_t> valid_;
    ColorVector white_;
    std::array<ColorVector,3> stains_;
    std::uint64_t valid_pixels_=0;
    std::size_t work_bytes_=0;
    double elapsed_ms_=0;
    std::string sha256_,input_sha256_;
    std::int64_t cx_=0,cy_=0,cw_=0,ch_=0;
    friend std::shared_ptr<const PixelFeatureImage> extract_pixel_features(const PixelBuffer&,const StainTransform&,PixelFeatureOptions,std::optional<PixelFeatureWindow>,const std::atomic<bool>*);
};
std::shared_ptr<const PixelFeatureImage> extract_pixel_features(const PixelBuffer&,const StainTransform&,PixelFeatureOptions={},std::optional<PixelFeatureWindow> window={},const std::atomic<bool>* cancelled=nullptr);
}
