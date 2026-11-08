#pragma once
#include "qupathxx/raw_pixel_tiled_training.hpp"
#include "object_feature_snapshot.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
namespace qpx {
struct RawPixelTrainingTile {std::int64_t x,y;};
struct RawPixelTiledTrainingRequest::Data {
    using Annotations=ObjectFeatureRequest::Data;
    std::shared_ptr<const char> capture=std::make_shared<const char>();
    ScientificRawLattice lattice;RawPixelRecipe recipe;RawPixelSourceSnapshot source;
    RawPixelTrainingOptions options;RawPixelTileOptions tiles;
    std::shared_ptr<const Annotations> annotations;
    std::vector<std::string> classes;std::vector<std::uint32_t> labels;
    std::shared_ptr<const SpatialIndex> index;
    std::vector<RawPixelTrainingTile> plan;std::uint64_t scanned=0;
    std::size_t snapshot=0,fixed=0,capacity=0;
    RawPixelFeatureOptions features;
    Data(ScientificRawLattice l,RawPixelRecipe r,RawPixelSourceSnapshot s,RawPixelTrainingOptions o,RawPixelTileOptions t):
        lattice(std::move(l)),recipe(std::move(r)),source(std::move(s)),options(o),tiles(t) {}
    ScientificRawWindow window(const RawPixelTrainingTile& t) const {return lattice.window(t.x,t.y,std::min(tiles.width,lattice.x().samples()-t.x),std::min(tiles.height,lattice.y().samples()-t.y));}
};
void prepare_raw_pixel_tiled_snapshot(RawPixelTiledTrainingRequest::Data&,std::size_t annotation_bytes,bool admit_features);
// Execution-only state. Historical checkpoints never contain owner authority or
// reservoir contents: replay reconstructs both the reservoir and original hash.
struct RawPixelTiledTrainingPrefix {
    std::uint64_t target=UINT64_MAX,expected_windows=0,completed=0;
    std::string expected_sha,sha;
};
std::string raw_pixel_tiled_request_sha256(const RawPixelTiledTrainingRequest::Data&);
struct RawPixelTiledTrainingKernel {
    static std::shared_ptr<const RawPixelTiledTrainingTable> run(const std::shared_ptr<const RawPixelTiledTrainingRequest::Data>&,ImageServer&,const std::atomic<bool>*,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr,RawPixelTiledTrainingPrefix* prefix=nullptr);
    static std::shared_ptr<const RawPixelTiledTrainingTable> run(const RawPixelTiledTrainingRequest&,const std::atomic<bool>*);
};
std::string raw_pixel_tiled_training_sha256(const RawPixelTiledTrainingTable&,const std::atomic<bool>*);
}
