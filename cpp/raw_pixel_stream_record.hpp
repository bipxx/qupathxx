#pragma once
#include "qupathxx/raw_pixel_stream_file.hpp"
namespace qpx {
struct RawPixelStreamGeometry;
// Shared QWS1 framing arithmetic; links never invent a second file-size grammar.
std::uint64_t raw_pixel_stream_file_bytes(const RawPixelStreamGeometry&,std::uint64_t header,std::uint64_t completed,std::size_t classes);
struct RawPixelStreamRecordCodec {
    static std::vector<std::uint8_t> encode(const RawPixelStreamRecord&,const std::string& directory,const std::atomic<bool>*);
    static std::shared_ptr<const RawPixelStreamRecord> decode(const std::uint8_t*,std::size_t,const std::string& directory,const std::atomic<bool>*);
};
}
