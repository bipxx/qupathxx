#pragma once
#include "qupathxx/pixel_prediction.hpp"
#include "qupathxx/tissue.hpp"
namespace qpx {
// Exact raster-to-geometry result bound to one immutable prediction snapshot.
// Disconnected regions of a class share one multipolygon annotation.
class PixelContours {
public:
    const std::shared_ptr<const TissueContours>& contours() const {return contours_;}
    const std::string& prediction_sha256() const {return prediction_->sha256();}
private:
    PixelContours(std::shared_ptr<const PixelPredictionResult> prediction,std::shared_ptr<const TissueContours> contours):prediction_(std::move(prediction)),contours_(std::move(contours)) {}
    std::shared_ptr<const PixelPredictionResult> prediction_;
    std::shared_ptr<const TissueContours> contours_;
    friend std::shared_ptr<const PixelContours> trace_pixel_prediction(std::shared_ptr<const PixelPredictionResult>,TissueContourOptions,const std::atomic<bool>*);
    friend std::vector<std::uint64_t> publish_pixel_contours(Document&,const PixelPredictionRequest&,const PixelContours&,std::string);
};
std::shared_ptr<const PixelContours> trace_pixel_prediction(std::shared_ptr<const PixelPredictionResult>,TissueContourOptions={},const std::atomic<bool>* cancelled=nullptr);
std::vector<std::uint64_t> publish_pixel_contours(Document&,const PixelPredictionRequest&,const PixelContours&,std::string name_prefix="Pixel");
}
