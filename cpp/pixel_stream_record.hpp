#pragma once
#include "qupathxx/pixel_stream_file.hpp"
namespace qpx {
struct PixelStreamRecordCodec {
    static std::vector<std::uint8_t> encode(const PixelStreamRecord&,const std::string& directory,const std::atomic<bool>*);
    static std::shared_ptr<const PixelStreamRecord> decode(const std::uint8_t*,std::size_t,const std::string& directory,const std::atomic<bool>*);
};
}
