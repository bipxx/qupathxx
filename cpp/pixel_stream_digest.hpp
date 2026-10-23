#pragma once
#include "sha256.hpp"
namespace qpx::detail {
inline void pixel_stream_input_header(Sha256& h,const std::string& model,std::uint64_t blocks) {
    const char tag[]="qpx.pixel-stream-input/1";h.update(tag,sizeof(tag)-1);h.update(model.data(),model.size());h.integer(blocks);
}
inline void pixel_stream_input_block(Sha256& h,std::uint64_t index,const std::string& input) {h.integer(index);h.update(input.data(),input.size());}
}
