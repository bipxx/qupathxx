// SPDX-License-Identifier: AGPL-3.0-only
#include "blosc_chunk.hpp"
#include "zstd_chunk.hpp"

// Called by the private, patched C-Blosc decoder once per compressed split.
extern "C" std::size_t qpx_ngff05_blosc_zstd(const void* input,std::size_t input_bytes,
    void* output,std::size_t output_bytes) noexcept {
    try {
        return qpx::detail::decode_ngff05_zstd_frame(input,input_bytes,output,output_bytes,
            qpx::detail::ngff05_blosc_max_block_bytes,std::nullopt)?output_bytes:0;
    } catch(...) {
        return 0;
    }
}
