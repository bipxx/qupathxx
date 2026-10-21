#pragma once
#include "qupathxx/cells.hpp"
namespace qpx {
struct CellArchiveCodec {
    static std::vector<std::uint8_t> encode(const CellResult&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const CellResult> decode(const std::uint8_t*,std::size_t,Roi,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
};
}
