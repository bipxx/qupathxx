#pragma once
#include "qupathxx/core.hpp"
#include <string>
#include <vector>

namespace qpx::detail {
// Opaque ImGui RGBA entries. The document's default mint is unspecified;
// explicitly assigned colours override the categorical fallback.
inline std::vector<std::uint32_t> class_palette(const Document& doc,const std::vector<std::string>& classes) {
    constexpr std::uint32_t defaults[]={0x4c78a8,0xf58518,0xe45756,0x72b7b2,0x54a24b,0xeeca3b,0xb279a2,0xff9da6};
    std::vector<std::uint32_t> out;out.reserve(classes.size());
    for(std::size_t i=0;i<classes.size();++i) {
        auto rgb=defaults[i%8];const auto found=doc.classes().find(classes[i]);
        if(found!=doc.classes().end() && found->second.color!=0x41e1be) rgb=found->second.color;
        out.push_back(((rgb>>16)&255)|(rgb&0xff00)|((rgb&255)<<16)|0xff000000u);
    }
    return out;
}
}
