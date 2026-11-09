#pragma once
#include "qupathxx/fluorescence_tiled_workflow.hpp"
namespace qpx {
struct FluorescenceCheckpointOptions {
    std::size_t record_bytes=4u*1024*1024;
    std::uint64_t total_bytes=2ull*1024*1024*1024;
    void validate(std::size_t work_bytes) const;
};
// New-only atomic threshold/tile/complete records in a dedicated directory.
// Resume verifies source/options/global threshold and current raw tile hashes;
// completed segmentation is reused. A sink receives both reused and new tiles.
FluorescenceTiledReport checkpoint_tiled_fluorescence_cells(const FluorescenceTiledRequest&,
    const std::string& directory,FluorescenceCheckpointOptions={},
    const std::function<void(std::size_t,const FluorescenceTileResult&)>& on_tile={},
    const std::atomic<bool>* cancelled=nullptr);
}
