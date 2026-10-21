#pragma once
#include "qupathxx/tissue.hpp"
namespace qpx {
// Versioned little-endian transport; geometry uses the project's ROI codec.
struct TissueArchiveCodec {
    static std::vector<std::uint8_t> encode(const TissueResult&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const TissueResult> decode(const std::uint8_t*,std::size_t,Roi,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
};
}
