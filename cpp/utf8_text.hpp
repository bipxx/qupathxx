#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
namespace qpx::detail {
inline void bounded_utf8(const std::string& s,std::size_t maximum) {
    if(s.size()>maximum || s.find('\0')!=std::string::npos) throw std::invalid_argument("invalid bounded text");
    for(std::size_t i=0;i<s.size();) {
        const auto c=static_cast<unsigned char>(s[i++]);if(c<128) continue;
        unsigned count=0;std::uint32_t value=0,minimum=0;
        if(c>=0xc2 && c<=0xdf) {count=1;value=c&31;minimum=128;}
        else if(c>=0xe0 && c<=0xef) {count=2;value=c&15;minimum=2048;}
        else if(c>=0xf0 && c<=0xf4) {count=3;value=c&7;minimum=65536;}
        else throw std::invalid_argument("invalid UTF-8");
        if(count>s.size()-i) throw std::invalid_argument("truncated UTF-8");
        for(unsigned n=0;n<count;++n) {const auto next=static_cast<unsigned char>(s[i++]);if((next&0xc0)!=0x80) throw std::invalid_argument("invalid UTF-8 continuation");value=(value<<6)|(next&63);}
        if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) throw std::invalid_argument("invalid UTF-8 code point");
    }
}
}
