#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"
namespace qpx {
// One owner-captured indexed window. This does not allocate a full global grid.
class RawPixelWindowPredictionRequest {
public:
    struct Data;
    RawPixelWindowPredictionRequest(const Document&,const RawPixelClassifier&,ScientificRawWindow,
        std::uint64_t object_id=0,double min_vote_fraction=0,std::size_t work_bytes=0);
    bool matches(const Document&) const;
    const ScientificRawWindow& window() const;
    const RawPixelClassifier& model() const;
    std::size_t estimated_work_bytes() const;
private:
    std::shared_ptr<const Data> data_;
    std::shared_ptr<ImageServer> source_;
    std::shared_ptr<const char> identity_;
    std::uint64_t revision_=0;
    bool publication_matches(const Document&) const;
    friend struct RawPixelWindowPredictionKernel;
    friend class RawPixelWindowPredictionJob;
    friend class RawPixelTiledPredictionRequest;
};
std::shared_ptr<const RawPixelWindowClassification> predict_raw_pixel_window(const RawPixelWindowPredictionRequest&,const std::atomic<bool>* cancelled=nullptr);
class RawPixelWindowPredictionJob {
public:
    RawPixelWindowPredictionJob();~RawPixelWindowPredictionJob();RawPixelWindowPredictionJob(const RawPixelWindowPredictionJob&)=delete;
    bool request(const RawPixelWindowPredictionRequest&);
    bool busy() const;void cancel();void await_completion();
    RawPixelJobStats poll(const Document&);RawPixelJobStats wait(const Document&);
    std::shared_ptr<const RawPixelWindowClassification> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
