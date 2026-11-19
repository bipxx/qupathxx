#pragma once

#include <zlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace qpx::image_plugin {

inline constexpr std::size_t gzip_workspace_bytes=65536;
enum class GzipResult { ok, invalid_stream, workspace_failure };

enum class DeflateWrapper { gzip, zlib, raw };

// The caller owns both spans and reserves gzip_workspace_bytes. Validate the
// selected wrapper and require exact span consumption with checkpoints.
template<class Checkpoint>
GzipResult decode_deflate_member(std::uint8_t* input,std::size_t input_size,
    std::uint8_t* output,std::size_t output_size,DeflateWrapper wrapper,Checkpoint checkpoint) {
    constexpr std::size_t step=65536;
    std::size_t space=gzip_workspace_bytes;
    z_stream decoder{};
    decoder.opaque=&space;
    decoder.zalloc=[](voidpf opaque,uInt count,uInt width) -> voidpf {
        auto& available=*static_cast<std::size_t*>(opaque);
        if(width && count>available/width)return nullptr;
        const auto memory=std::calloc(count,width);
        if(memory)available-=static_cast<std::size_t>(count)*width;
        return memory;
    };
    decoder.zfree=[](voidpf,voidpf memory){std::free(memory);};
    const auto bits=wrapper==DeflateWrapper::gzip?31:wrapper==DeflateWrapper::raw?-15:15;
    if(inflateInit2(&decoder,bits)!=Z_OK)return GzipResult::workspace_failure;
    struct End {
        z_stream& decoder;
        ~End(){inflateEnd(&decoder);}
    } end{decoder};
    std::size_t supplied=0;
    std::uint8_t overflow=0;
    for(;;) {
        checkpoint();
        if(!decoder.avail_in && supplied<input_size) {
            const auto count=std::min(step,input_size-supplied);
            decoder.next_in=input+supplied;
            decoder.avail_in=static_cast<uInt>(count);
            supplied+=count;
        }
        if(!decoder.avail_out) {
            const auto written=static_cast<std::size_t>(decoder.total_out);
            // Allow trailer consumption after the exact raster has been filled.
            decoder.next_out=written==output_size?&overflow:output+written;
            decoder.avail_out=static_cast<uInt>(written==output_size?1:std::min(step,output_size-written));
        }
        const auto status=inflate(&decoder,Z_NO_FLUSH);
        if(decoder.total_out>output_size)return GzipResult::invalid_stream;
        if(status==Z_STREAM_END)break;
        if(status==Z_MEM_ERROR)return GzipResult::workspace_failure;
        if(status!=Z_OK)return GzipResult::invalid_stream;
    }
    return decoder.total_in==input_size && decoder.total_out==output_size ? GzipResult::ok : GzipResult::invalid_stream;
}

template<class Checkpoint>
GzipResult decode_gzip_member(std::uint8_t* input,std::size_t input_size,
    std::uint8_t* output,std::size_t output_size,Checkpoint checkpoint) {
    return decode_deflate_member(input,input_size,output,output_size,DeflateWrapper::gzip,checkpoint);
}

} // namespace qpx::image_plugin
