#pragma once
#include "qupathxx/fluorescence_tiles.hpp"
#include <functional>
namespace qpx {
class Document;
struct FluorescenceTileOptions {
    std::uint32_t width=256,height=256,halo=64;
    std::size_t max_tiles=100000;
    void validate() const;
};
class FluorescenceTiledRequest {
public:
    // Captures source/ROI/calibration on owner; no raw pixels are read.
    FluorescenceTiledRequest(const Document&,RawRegionRequest,std::uint64_t object_id=0,
        FluorescenceCellOptions={},FluorescenceTileOptions={});
    bool matches(const Document&) const;
    bool publication_matches(const Document&) const;
    std::size_t tile_count() const;
    const std::string& sha256() const;
    const std::string& source_sha256() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    void require_owner() const;
    friend struct FluorescenceTiledKernel;
    friend class FluorescenceCellJob;
};
struct FluorescenceTiledReport {
    std::string state,request_sha256,source_sha256,input_sha256;
    std::string checkpoint_path,output_sha256;
    double resolved_threshold=0,elapsed_ms=0;
    std::uint64_t finite_pixels=0,cells=0;
    std::size_t tiles=0,unresolved_tiles=0,accounted_work_bytes=0;
    std::size_t reused_tiles=0,segmented_tiles=0;
};
// Streams one immutable tile result at a time. The native sink may write bounded
// records; it must not reference a live Document/UI or retain unbounded results.
// Global Otsu uses two bounded score passes and the untiled256-bin/tie contract;
// source/raw hashes must agree through threshold and segmentation passes.
FluorescenceTiledReport detect_tiled_fluorescence_cells(const FluorescenceTiledRequest&,
    const std::function<void(std::size_t,const FluorescenceTileResult&)>& on_tile={},
    const std::atomic<bool>* cancelled=nullptr);
}
