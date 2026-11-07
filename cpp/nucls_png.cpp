#include "nucls_png.hpp"
#include "vendor/lodepng/lodepng.h"
#include <cstdlib>
#include <cstring>
namespace qpx::detail {
namespace {
struct State {
    LodePNGState value;
    State(){lodepng_state_init(&value);value.decoder.color_convert=0;}
    ~State(){lodepng_state_cleanup(&value);}
};
unsigned u32(const unsigned char* bytes) {
    return (unsigned(bytes[0])<<24)|(unsigned(bytes[1])<<16)|(unsigned(bytes[2])<<8)|unsigned(bytes[3]);
}
void valid_chunks(const unsigned char* data,std::size_t size,const std::atomic<bool>* cancelled) {
    bool end=false;std::size_t at=8;
    while(at<size) {
        export_checkpoint(cancelled);
        if(size-at<12)throw std::invalid_argument("Truncated PNG chunk");
        const auto length=std::size_t(u32(data+at));
        if(length>size-at-12)throw std::invalid_argument("PNG chunk exceeds file bounds");
        if(lodepng_crc32(data+at+4,length+4)!=u32(data+at+8+length))throw std::invalid_argument("PNG chunk checksum differs");
        if(std::memcmp(data+at+4,"IEND",4)==0) {
            if(length)throw std::invalid_argument("Invalid PNG end chunk");
            end=true;break;
        }
        at+=length+12;
    }
    if(!end)throw std::invalid_argument("PNG end chunk is missing");
}
}
NuclsRgb decode_nucls_rgb_png(const WorkflowInput& source,std::uint64_t available_pixels,const std::atomic<bool>* cancelled) {
    if(source.bytes()>32u*1024*1024)throw std::length_error("NuCLS PNG exceeds32 MiB");
    const auto data=read_workflow_input(source,cancelled);
    const auto* input=reinterpret_cast<const unsigned char*>(data.data());
    State state;NuclsRgb result;
    auto code=lodepng_inspect(&result.width,&result.height,&state.value,input,data.size());
    if(code)throw std::invalid_argument(std::string("Invalid PNG header: ")+lodepng_error_text(code));
    if(state.value.info_png.color.colortype!=LCT_RGB || state.value.info_png.color.bitdepth!=8)
        throw std::invalid_argument("Requires an RGB8 PNG; no automatic color conversion");
    const auto pixels=std::uint64_t(result.width)*result.height;
    if(!pixels || pixels>16000000 || pixels>available_pixels)throw NuclsPixelLimit("Exceeds aggregate16000000 PNG pixels");
    valid_chunks(input,data.size(),cancelled);export_checkpoint(cancelled);
    state.value.decoder.zlibsettings.max_output_size=static_cast<std::size_t>(4*pixels);
    unsigned char* decoded=nullptr;unsigned width=0,height=0;
    code=lodepng_decode(&decoded,&width,&height,&state.value,input,data.size());
    std::unique_ptr<unsigned char,decltype(&std::free)> owned(decoded,&std::free);
    source.verify(false,cancelled);
    if(code)throw std::invalid_argument(std::string("Invalid PNG pixels: ")+lodepng_error_text(code));
    if(width!=result.width || height!=result.height || state.value.info_raw.colortype!=LCT_RGB || state.value.info_raw.bitdepth!=8)
        throw std::runtime_error("PNG decoder changed the admitted RGB8 layout");
    result.pixels.assign(decoded,decoded+static_cast<std::size_t>(3*pixels));return result;
}
}
