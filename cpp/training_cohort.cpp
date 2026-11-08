#include "qupathxx/training_cohort.hpp"
#include "qupathxx/core.hpp"
namespace qpx {
RawPixelTiledTrainingRequest capture_labelled_raw_pixel_training(const Document& doc,
    ScientificRawLattice lattice,ScientificRawOptions selection,RawPixelFeatureOptions features,
    RawPixelTrainingOptions options,RawPixelTileOptions tiles,bool physical_sigmas) {
    std::vector<std::uint64_t> ids;
    for(const auto& [id,object]:doc.objects())if(!object.class_name.empty() && object.geometry.is_area()) {
        if(ids.size()==4096)throw std::invalid_argument("Raw training requires at most 4096 labelled areas");
        ids.push_back(id);
    }
    return RawPixelTiledTrainingRequest(doc,ids,std::move(lattice),selection,std::move(features),options,tiles,physical_sigmas);
}
}
