#pragma once
#include "qupathxx/interchange.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <limits>

namespace qpx::detail {
// Class colors retain their complete wire bit pattern; object colors above
// remain RGB-only. Gson's channel array is RGB[A], with opaque RGB by default.
inline std::optional<std::uint32_t> class_argb(const nlohmann::json& value) {
    if(value.is_null())return {};
    if(value.is_array() && (value.size()==3 || value.size()==4)) {
        std::uint32_t rgb=0,alpha=255;
        for(std::size_t i=0;i<value.size();++i) {
            if(!value[i].is_number_integer() || value[i]<0 || value[i]>255)throw std::invalid_argument("invalid class color channel");
            const auto channel=value[i].get<std::uint32_t>();
            if(i<3)rgb=(rgb<<8)|channel;else alpha=channel;
        }
        return (alpha<<24)|rgb;
    }
    if(!value.is_number_integer() || value<std::numeric_limits<std::int32_t>::min() || value>std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("invalid packed class ARGB");
    return static_cast<std::uint32_t>(value.get<std::int64_t>());
}

// Only the pinned .qpproj class-list field is a colon-delimited path.
// Match Java String.strip whitespace, without normalizing native/display keys.
inline std::vector<std::string> qupath_project_class_path(const std::string& encoded) {
    bounded_utf8(encoded,4096);
    auto strip=[](const std::string& value) {
        std::size_t first=value.size(),last=0;
        for(std::size_t i=0;i<value.size();) {
            const auto start=i;const auto c=static_cast<unsigned char>(value[i++]);
            std::uint32_t scalar=c;unsigned tail=0;
            if(c>=0xf0){scalar=c&7;tail=3;}else if(c>=0xe0){scalar=c&15;tail=2;}else if(c>=0xc0){scalar=c&31;tail=1;}
            for(unsigned j=0;j<tail;++j)scalar=(scalar<<6)|(static_cast<unsigned char>(value[i++])&63);
            const bool space=(scalar>=9 && scalar<=13) || (scalar>=28 && scalar<=32) || scalar==0x1680 ||
                (scalar>=0x2000 && scalar<=0x2006) || (scalar>=0x2008 && scalar<=0x200a) ||
                scalar==0x2028 || scalar==0x2029 || scalar==0x205f || scalar==0x3000;
            if(!space) {if(first==value.size())first=start;last=i;}
        }
        return first==value.size()?std::string{}:value.substr(first,last-first);
    };
    std::vector<std::string> raw;
    std::size_t start=0;
    for(;;) {
        const auto pos=encoded.find(':',start);raw.push_back(encoded.substr(start,pos==std::string::npos?pos:pos-start));
        if(pos==std::string::npos)break;start=pos+1;
    }
    // Java split discards literal empty trailing elements before strip.
    while(raw.size()>1 && raw.back().empty())raw.pop_back();
    if(raw.empty() || raw.size()>32)throw std::invalid_argument("invalid project class path");
    for(auto& value:raw) {value=strip(value);if(value.empty() || value.find('\n')!=std::string::npos)throw std::invalid_argument("invalid project class path component");}
    return raw;
}
InterchangeReport import_geojson_class_context(Document&,const std::string&,const std::map<std::string,ObjectClass>&);
}
