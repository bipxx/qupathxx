#pragma once
#include "qupathxx/raw_pixel_window_workflows.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
namespace qpx {
struct RawPixelWindowPredictionRequest::Data {
    ScientificRawWindow window;RawPixelClassifier model;RawPixelSourceSnapshot source;
    std::optional<Roi> mask;std::shared_ptr<const char> capture=std::make_shared<const char>();
    double minimum=0;std::size_t budget=0,fixed=0,work=0;
    Data(ScientificRawWindow w,const RawPixelClassifier& m,RawPixelSourceSnapshot s):window(std::move(w)),model(m),source(std::move(s)) {}
};
struct RawPixelWindowPredictionKernel {
    static std::string result_sha256(const RawPixelWindowClassification&,double,const std::atomic<bool>*);
    static std::shared_ptr<const RawPixelWindowClassification> run(const RawPixelWindowPredictionRequest::Data&,ImageServer&,
        const std::atomic<bool>*,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr);
    // The source epoch belongs to the captured prototype. Traversal geometry
    // changes only core indices/shape on that same original global lattice.
    static std::shared_ptr<const RawPixelWindowClassification> run_window(const RawPixelWindowPredictionRequest::Data&,
        const ScientificRawWindow&,ImageServer&,const std::atomic<bool>*,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr);
    static std::shared_ptr<const RawPixelWindowClassification> run(const RawPixelWindowPredictionRequest&,const std::atomic<bool>*);
};
}
