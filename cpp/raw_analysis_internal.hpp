#pragma once
#include "qupathxx/raw_analysis.hpp"
namespace qpx {
struct RawAnalysisCodec {
    static std::vector<std::uint8_t> encode(const RawAnalysisRecord&,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
    static std::shared_ptr<const RawAnalysisRecord> decode(const std::uint8_t*,std::size_t,const ImageMetadata&,const std::atomic<bool>* cancelled=nullptr);
};
}
