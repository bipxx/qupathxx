// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "mask.hpp"
#include <cstddef>
#include <bzlib.h>
#include <zlib.h>

namespace qpx_ome {
inline constexpr std::size_t mask_codec_work=3u*1024*1024;
inline constexpr std::size_t mask_max_encoded_chars=4*((2u*1024*1024+2)/3);

inline void* mask_codec_allocate(void* context,std::size_t count,std::size_t size) noexcept {
    if(!size || count>mask_codec_work/size)return nullptr;
    try {
        return static_cast<std::pmr::memory_resource*>(context)->allocate(count*size,alignof(std::max_align_t));
    } catch(...) {
        return nullptr;
    }
}

template<class Stream,class Step>
void mask_decode_stream(Stream& stream,Step step,int end,std::pmr::vector<std::uint8_t>& input,
    std::pmr::vector<std::uint8_t>& output,qpx_reader_cancel_v1 cancel) {
    constexpr std::size_t block=65536;
    std::size_t in=0,out=0;
    for(;;) {
        qpx_binary::poll(cancel);
        if(!stream.avail_in && in<input.size()) {
            stream.next_in=reinterpret_cast<decltype(stream.next_in)>(input.data()+in);
            stream.avail_in=static_cast<unsigned>(std::min(block,input.size()-in));
            in+=stream.avail_in;
        }
        if(!stream.avail_out && out<output.size()) {
            stream.next_out=reinterpret_cast<decltype(stream.next_out)>(output.data()+out);
            stream.avail_out=static_cast<unsigned>(std::min(block,output.size()-out));
            out+=stream.avail_out;
        }
        const auto before_in=stream.avail_in,before_out=stream.avail_out;
        const auto status=step(stream);
        if(status==end) {
            qpx_binary::valid(in-stream.avail_in==input.size() && out-stream.avail_out==output.size()-1,
                "OME mask stream has trailing data or a different decoded extent");
            output.pop_back();
            return;
        }
        qpx_binary::valid(status==0 && (stream.avail_in!=before_in || stream.avail_out!=before_out) &&
            out-stream.avail_out<output.size(),"Corrupt, truncated or oversized OME mask stream");
    }
}

inline std::pmr::vector<std::uint8_t> decompress_mask(std::string_view compression,
    std::pmr::vector<std::uint8_t>& input,std::size_t expected,std::byte* workspace,
    std::pmr::memory_resource* memory,qpx_reader_cancel_v1 cancel) {
    std::pmr::monotonic_buffer_resource scratch(workspace,mask_codec_work,std::pmr::null_memory_resource());
    std::pmr::memory_resource* resource=&scratch;
    std::pmr::vector<std::uint8_t> output(expected+1,memory);
    if(compression=="zlib") {
        z_stream decoder{};
        decoder.opaque=resource;
        decoder.zalloc=[](void* p,unsigned n,unsigned size) noexcept {return mask_codec_allocate(p,n,size);};
        decoder.zfree=[](void*,void*) noexcept {};
        qpx_binary::valid(inflateInit(&decoder)==Z_OK,"Cannot initialize OME mask zlib stream");
        struct Close {z_stream& s;~Close(){inflateEnd(&s);}} close{decoder};
        mask_decode_stream(decoder,[](z_stream& s){return inflate(&s,Z_NO_FLUSH);},
            Z_STREAM_END,input,output,cancel);
    } else {
        bz_stream decoder{};
        decoder.opaque=resource;
        decoder.bzalloc=[](void* p,int n,int size) noexcept -> void* {
            return n>0 && size>0?mask_codec_allocate(p,std::size_t(n),std::size_t(size)):nullptr;
        };
        decoder.bzfree=[](void*,void*) noexcept {};
        qpx_binary::valid(BZ2_bzDecompressInit(&decoder,0,1)==BZ_OK,"Cannot initialize OME mask bzip2 stream");
        struct Close {bz_stream& s;~Close(){BZ2_bzDecompressEnd(&s);}} close{decoder};
        mask_decode_stream(decoder,[](bz_stream& s){return BZ2_bzDecompress(&s);},
            BZ_STREAM_END,input,output,cancel);
    }
    return output;
}
}
