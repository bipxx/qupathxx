#pragma once
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace qpx::detail {
// Compatibility facts are independently captured in qupath-class-hierarchy-0.7.0.
// Random color bytes use Java's published 48-bit Random protocol and UTF16 hash.
inline std::uint32_t qupath_class_default_argb(const std::vector<std::string>& path,
        std::optional<std::uint32_t> parent_argb={}) {
    if(path.empty() || path.size()>32)throw std::invalid_argument("class path must have one to32 components");
    std::size_t bytes=2*(path.size()-1);
    std::uint32_t hash=0;
    auto unit=[&](std::uint32_t value){hash=31u*hash+value;};
    for(std::size_t component=0;component<path.size();++component) {
        const auto& name=path[component];bounded_utf8(name,4096);
        if(name.empty() || name.size()>4096-bytes)throw std::invalid_argument("class path exceeds text limit");bytes+=name.size();
        if(component)unit(10);
        for(std::size_t i=0;i<name.size();) {
            auto c=static_cast<unsigned char>(name[i++]);std::uint32_t scalar=c;unsigned tail=0;
            if(c>=0xf0){scalar=c&7;tail=3;}
            else if(c>=0xe0){scalar=c&15;tail=2;}
            else if(c>=0xc0){scalar=c&31;tail=1;}
            for(unsigned j=0;j<tail;++j)scalar=(scalar<<6)|(static_cast<unsigned char>(name[i++])&63);
            if(scalar<=0xffff)unit(scalar);
            else {scalar-=0x10000;unit(0xd800+(scalar>>10));unit(0xdc00+(scalar&1023));}
        }
    }
    if(path.size()>1 && !parent_argb)throw std::invalid_argument("derived default color requires a resolved parent");
    const auto& leaf=path.back();
    const std::array<const char*,5> names={"1+","2+","3+","Positive","Negative"};
    const std::array<std::uint32_t,5> colors={0xffd700,0xe19632,0xc83232,0xc83232,0x5a5ab4};
    auto scaled=[](std::uint32_t rgb,double factor) {
        std::uint32_t out=0xff000000;
        for(unsigned shift:{16u,8u,0u})out|=static_cast<std::uint32_t>(std::min(255.,double((rgb>>shift)&255)*factor))<<shift;
        return out;
    };
    const auto special=std::find(names.begin(),names.end(),leaf);
    if(special!=names.end()) {
        const auto at=static_cast<std::size_t>(special-names.begin());
        if(path.size()==1)return scaled(colors[at],1.25);
        if(path.size()==2 && path.front()=="Tumor")return 0xff000000|colors[at];
        const std::array<double,5> factors={.9,.6,.4,.75,1.25};return scaled(*parent_argb,factors[at]);
    }
    constexpr std::uint64_t multiplier=0x5deece66dULL,mask=(1ULL<<48)-1;
    const auto signed_hash=hash<0x80000000u?std::int64_t(hash):std::int64_t(hash)-0x100000000LL;
    auto seed=(static_cast<std::uint64_t>(signed_hash)^multiplier)&mask;
    auto next_byte=[&]{seed=(seed*multiplier+11)&mask;return static_cast<std::uint32_t>(seed>>40);};
    for(;;) {
        const auto red=next_byte(),green=next_byte(),blue=next_byte();
        if(red>=40 || green>=40 || blue>=40)return 0xff000000|(red<<16)|(green<<8)|blue;
    }
}
}
