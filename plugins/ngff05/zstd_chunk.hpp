// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#undef ZSTD_STATIC_LINKING_ONLY

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace qpx::detail {

static_assert(ZSTD_VERSION_NUMBER == 10507, "Use the pinned Zstandard 1.5.7 source");

inline constexpr std::size_t ngff05_zstd_workspace_bytes = 256u * 1024;

inline bool decode_ngff05_zstd_frame(const void* encoded,std::size_t encoded_bytes,
    void* decoded,std::size_t decoded_bytes,std::size_t max_window_bytes,std::optional<bool> checksum) {
    if (encoded_bytes == 0 || decoded_bytes == 0 || decoded_bytes > max_window_bytes)
        throw std::invalid_argument("OME-Zarr: invalid Zstandard chunk sizes");

    ZSTD_FrameHeader header{};
    if (ZSTD_getFrameHeader(&header, encoded, encoded_bytes) != 0 ||
        header.frameType != ZSTD_frame || header.dictID != 0 ||
        header.windowSize > max_window_bytes)
        throw std::invalid_argument("OME-Zarr: invalid or unsupported Zstandard frame header");
    if (checksum && header.checksumFlag != unsigned(*checksum))
        throw std::invalid_argument("OME-Zarr: Zstandard frame checksum flag differs from metadata");
    if (header.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN && header.frameContentSize != decoded_bytes)
        throw std::invalid_argument("OME-Zarr: Zstandard frame content size differs from chunk size");

    const auto frame_bytes = ZSTD_findFrameCompressedSize(encoded, encoded_bytes);
    if (ZSTD_isError(frame_bytes) || frame_bytes != encoded_bytes)
        throw std::invalid_argument("OME-Zarr: truncated or trailing data in Zstandard chunk");

    // A static, aligned context prevents the decoder from allocating extra
    // memory. Single-shot decoding uses the destination for frame history.
    constexpr auto workspace_words = ngff05_zstd_workspace_bytes / sizeof(std::uint64_t);
    std::unique_ptr<std::uint64_t[]> workspace(new std::uint64_t[workspace_words]);
    auto* decoder = ZSTD_initStaticDCtx(workspace.get(), ngff05_zstd_workspace_bytes);
    if (!decoder)
        throw std::runtime_error("OME-Zarr: Zstandard decoder workspace is insufficient");

    const auto written = ZSTD_decompressDCtx(
        decoder, decoded, decoded_bytes, encoded, encoded_bytes);
    return !ZSTD_isError(written) && written == decoded_bytes;
}

inline std::vector<std::uint8_t> decode_ngff05_zstd(
    const std::vector<std::uint8_t>& encoded,std::size_t decoded_bytes,bool checksum) {
    constexpr std::size_t max_window_bytes=64u*1024*1024;
    if(decoded_bytes>max_window_bytes)
        throw std::invalid_argument("OME-Zarr: invalid Zstandard chunk sizes");
    std::vector<std::uint8_t> decoded(decoded_bytes);
    if(!decode_ngff05_zstd_frame(encoded.data(),encoded.size(),
        decoded.data(),decoded.size(),max_window_bytes,checksum))
        throw std::invalid_argument("OME-Zarr: corrupt Zstandard chunk or decoded size differs");
    return decoded;
}

}
