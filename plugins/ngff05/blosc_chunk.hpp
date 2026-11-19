// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <blosc.h>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace qpx::detail {

static_assert(BLOSC_VERSION_MAJOR==1 && BLOSC_VERSION_MINOR==21 && BLOSC_VERSION_RELEASE==6,
    "Use the admitted C-Blosc 1.21.6 build");
inline constexpr std::size_t ngff05_blosc_max_block_bytes=4u*1024*1024;
inline constexpr std::size_t ngff05_blosc_workspace(std::size_t decoded){
    // Single-threaded pinned C-Blosc allocates three block buffers plus split
    // overhead. The extra allowance covers its decoder and shuffle context.
    return 3*std::min(decoded,ngff05_blosc_max_block_bytes)+4*BLOSC_MAX_TYPESIZE+256u*1024;
}

struct Ngff05Blosc {
    enum class Codec { BloscLz, Lz4, Zstandard, Zlib, Snappy };
    Codec codec=Codec::BloscLz;
    int shuffle=0;
    std::size_t type_size=0;
};

inline const char* ngff05_blosc_library(Ngff05Blosc::Codec codec) {
    switch(codec) {
        case Ngff05Blosc::Codec::BloscLz:return BLOSC_BLOSCLZ_LIBNAME;
        case Ngff05Blosc::Codec::Lz4:return BLOSC_LZ4_LIBNAME;
        case Ngff05Blosc::Codec::Zstandard:return BLOSC_ZSTD_LIBNAME;
        case Ngff05Blosc::Codec::Zlib:return BLOSC_ZLIB_LIBNAME;
        case Ngff05Blosc::Codec::Snappy:return BLOSC_SNAPPY_LIBNAME;
    }
    throw std::logic_error("Invalid Blosc codec");
}

inline std::vector<std::uint8_t> decode_ngff05_blosc(
    const std::vector<std::uint8_t>& encoded,std::size_t decoded_bytes,const Ngff05Blosc& codec) {
    std::size_t decoded_size=0;
    if(encoded.size()<BLOSC_MIN_HEADER_LENGTH || encoded[0]!=BLOSC_VERSION_FORMAT ||
       blosc_cbuffer_validate(encoded.data(),encoded.size(),&decoded_size)!=0 || decoded_size!=decoded_bytes)
        throw std::invalid_argument("OME-Zarr: invalid Blosc frame or decoded chunk size");

    std::size_t header_size=0,compressed_size=0,block_size=0,type_size=0;
    int flags=0;
    blosc_cbuffer_sizes(encoded.data(),&header_size,&compressed_size,&block_size);
    blosc_cbuffer_metainfo(encoded.data(),&type_size,&flags);
    const auto* library=blosc_cbuffer_complib(encoded.data());
    const auto shuffle=flags&(BLOSC_DOSHUFFLE|BLOSC_DOBITSHUFFLE);
    if(header_size!=decoded_bytes || compressed_size!=encoded.size() || !block_size ||
       block_size>std::min(decoded_bytes,ngff05_blosc_max_block_bytes) ||
       !type_size || (codec.type_size && type_size!=codec.type_size) || shuffle!=codec.shuffle ||
       !library || std::string_view(library)!=ngff05_blosc_library(codec.codec) ||
       ((flags&BLOSC_MEMCPYED) && compressed_size!=decoded_bytes+BLOSC_MIN_HEADER_LENGTH))
        throw std::invalid_argument("OME-Zarr: Blosc frame differs from codec metadata or chunk bounds");

    std::vector<std::uint8_t> decoded(decoded_bytes);
    if(blosc_decompress_ctx(encoded.data(),decoded.data(),decoded.size(),1)!=int(decoded_bytes))
        throw std::invalid_argument("OME-Zarr: corrupt Blosc chunk");
    return decoded;
}

}
