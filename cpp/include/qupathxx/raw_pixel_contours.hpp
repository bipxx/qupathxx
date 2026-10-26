#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
#include "qupathxx/tissue.hpp"
namespace qpx {
// One bounded grid only. Cross-tile stitching is a separate streaming contract.
class RawPixelContours {
public:
    const std::shared_ptr<const TissueContours>& contours() const {return contours_;}
    const std::string& prediction_sha256() const {return prediction_->sha256();}
private:
    RawPixelContours()=default;
    std::shared_ptr<const RawPixelPredictionRequest::Data> snapshot_;
    std::shared_ptr<const RawPixelClassification> prediction_;
    std::shared_ptr<const TissueContours> contours_;
    friend struct RawPixelContourKernel;
    friend std::vector<std::uint64_t> publish_raw_pixel_contours(Document&,const RawPixelPredictionRequest&,const RawPixelContours&,std::string);
};
std::shared_ptr<const RawPixelContours> trace_raw_pixel_prediction(const RawPixelPredictionRequest&,
    std::shared_ptr<const RawPixelClassification>,TissueContourOptions={},const std::atomic<bool>* cancelled=nullptr);
std::vector<std::uint64_t> publish_raw_pixel_contours(Document&,const RawPixelPredictionRequest&,const RawPixelContours&,std::string name_prefix="Raw pixel");
class RawPixelContourJob {
public:
    RawPixelContourJob();~RawPixelContourJob();RawPixelContourJob(const RawPixelContourJob&)=delete;
    bool request(const RawPixelPredictionRequest&,std::shared_ptr<const RawPixelClassification>,TissueContourOptions={});
    bool busy() const;void cancel();void await_completion();
    RawPixelJobStats poll(const Document&);RawPixelJobStats wait(const Document&);
    std::shared_ptr<const RawPixelContours> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
