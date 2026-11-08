#pragma once
#include "utf8_text.hpp"
#include <string>
namespace qpx::detail {
inline std::string json_string(const std::string& value) {
    bounded_utf8(value,64*1024*1024);
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {
        if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
        else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
}
