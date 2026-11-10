#pragma once
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <string>
#include <vector>
#include "utf8_text.hpp"

namespace qpx::ui {
// UI preferences only: no project storage or directory scanning.
struct ProjectPreferences {
    static constexpr std::size_t recent_limit=8;
    std::vector<std::string> recent;
    std::string group;
    float inspector_fraction=.45f;
    static std::string encode(const std::string& value) {
        constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(value.size()*2);
        for(unsigned char c:value) {result+=hex[c>>4];result+=hex[c&15];}return result;
    }
    static std::string decode(const std::string& value) {
        if(value.size()%2 || value.size()>65536)return {};
        auto digit=[](char c)->int {return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;};
        std::string result;for(std::size_t i=0;i<value.size();i+=2) {const int a=digit(value[i]),b=digit(value[i+1]);if(a<0 || b<0)return {};result+=char(a*16+b);}
        try {detail::bounded_utf8(result,32768);}catch(...) {return {};}
        if(result.find('\0')!=std::string::npos)return {};return result;
    }
    void remember(const std::string& path) {
        if(path.empty() || path.size()>32768)return;
        try {
            const auto absolute=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();
            if(!recent.empty() && recent.front()==absolute)return;
            recent.erase(std::remove(recent.begin(),recent.end(),absolute),recent.end());recent.insert(recent.begin(),absolute);
            if(recent.size()>recent_limit)recent.resize(recent_limit);
        } catch(...) { /* Invalid preferences never interrupt an open project. */ }
    }
    void load(const std::string& header) {
        std::size_t at=0;while(at<header.size()) {
            const auto end=header.find_first_of("\r\n",at);const auto line=header.substr(at,end==std::string::npos?end:end-at);
            const std::string recent_prefix="; qupathxx recent-project=",group_prefix="; qupathxx project-group=",split_prefix="; qupathxx project-inspector=";
            if(line.rfind(recent_prefix,0)==0 && recent.size()<recent_limit) {
                const auto path=decode(line.substr(recent_prefix.size()));
                if(!path.empty() && std::filesystem::u8path(path).is_absolute() && std::find(recent.begin(),recent.end(),path)==recent.end())recent.push_back(path);
            } else if(line.rfind(group_prefix,0)==0)group=decode(line.substr(group_prefix.size()));
            else if(line.rfind(split_prefix,0)==0) {
                const auto number=line.substr(split_prefix.size());int value=0;
                const auto parsed=std::from_chars(number.data(),number.data()+number.size(),value);
                if(parsed.ec==std::errc{} && parsed.ptr==number.data()+number.size() && value>=2000 && value<=7500)inspector_fraction=value/10000.f;
            }
            if(end==std::string::npos)break;at=end+1;
        }
    }
    std::string header() const {
        std::string value;for(const auto& path:recent)value+="; qupathxx recent-project="+encode(path)+"\n";
        if(!group.empty())value+="; qupathxx project-group="+encode(group)+"\n";
        value+="; qupathxx project-inspector="+std::to_string(static_cast<int>(std::clamp(inspector_fraction,.2f,.75f)*10000+.5f))+"\n";return value;
    }
};
}
