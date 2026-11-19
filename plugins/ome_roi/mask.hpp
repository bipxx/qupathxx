// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/annotation_source.hpp"
#include <array>

namespace qpx_ome {
inline std::pmr::vector<std::uint8_t> mask_bytes(std::string_view text,
    std::size_t encoded_chars,std::pmr::memory_resource* memory,qpx_reader_cancel_v1 cancel) {
    constexpr std::string_view digits="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::pmr::vector<std::uint8_t> bytes(memory);
    bytes.reserve(encoded_chars/4*3);
    std::array<unsigned,4> values{};
    unsigned count=0;
    std::size_t seen=0;
    bool padded=false;
    for(std::size_t i=0;i<text.size();++i) {
        if(!(i%4096))qpx_binary::poll(cancel);
        const auto ch=text[i];
        if(ch==' ' || ch=='\t' || ch=='\r' || ch=='\n')continue;
        qpx_binary::valid(!padded && seen++<encoded_chars,"Trailing OME mask base64 data");
        const auto digit=digits.find(ch);
        qpx_binary::valid(ch=='=' || digit!=digits.npos,"Invalid OME mask base64 encoding");
        values[count++]=ch=='='?64:static_cast<unsigned>(digit);
        if(count!=4)continue;
        const auto a=values[0],b=values[1],c=values[2],d=values[3];
        qpx_binary::valid(a<64 && b<64 && (c<64 || (d==64 && !(b&15))) &&
            (d<64 || c==64 || !(c&3)),"Invalid OME mask base64 padding");
        bytes.push_back(static_cast<std::uint8_t>((a<<2)|(b>>4)));
        if(c<64)bytes.push_back(static_cast<std::uint8_t>((b<<4)|(c>>2)));
        if(d<64)bytes.push_back(static_cast<std::uint8_t>((c<<6)|d));
        padded=d==64;
        count=0;
    }
    qpx_binary::valid(!count && seen==encoded_chars,"OME mask BinData Length differs from base64 character count");
    return bytes;
}
}
