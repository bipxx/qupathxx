#pragma once
#include "qupathxx/raw_pixel_stream.hpp"
#include "raw_pixel_window_prediction_internal.hpp"
namespace qpx {
struct RawPixelStreamGeometry {
    ScientificRawLattice lattice;RawPixelStreamRegion region;RawPixelStreamOptions options;
    std::uint64_t columns=0,windows=0,samples=0;
    RawPixelStreamGeometry(ScientificRawLattice,RawPixelStreamRegion,RawPixelStreamOptions);
    ScientificRawWindow window(std::uint64_t index) const;
    std::uint64_t samples_before(std::uint64_t index) const;
};
struct RawPixelTiledPredictionRequest::Data : RawPixelStreamGeometry {
    std::shared_ptr<const RawPixelWindowPredictionRequest::Data> prototype;
    ImageServer* source=nullptr;
    std::string sha;
    Data(ScientificRawLattice l,RawPixelStreamRegion r,RawPixelStreamOptions o):RawPixelStreamGeometry(std::move(l),r,o) {}
};
std::string raw_pixel_stream_request_sha256(const RawPixelStreamGeometry&,const RawPixelClassifier&,const std::string& reader,const std::string& epoch,RawClassifierCalibration,const std::optional<Roi>&);
std::string raw_pixel_stream_chain(const char* domain,const std::string& previous,std::uint64_t index,const std::string& block);
}
