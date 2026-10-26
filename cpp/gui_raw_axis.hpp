#pragma once
#include "qupathxx/scientific_grid.hpp"
#include <charconv>
#include <string>
#include <stdexcept>
namespace qpx::detail {
inline ScientificGridAxis gui_raw_axis(const char* value) {
        std::int64_t n[4]{};std::string s(value);std::size_t at=0;
        for(int i=0;i<4;++i) {auto end=s.find(',',at);if((i<3)==(end==std::string::npos)) throw std::invalid_argument("Axis requires origin,extent,samples,context");const auto stop=end==std::string::npos?s.size():end;const auto r=std::from_chars(s.data()+at,s.data()+stop,n[i]);if(r.ec!=std::errc{} || r.ptr!=s.data()+stop) throw std::invalid_argument("Axis fields must be exact signed integers");at=stop+1;}
        return {n[0],n[1],n[2],n[3]};
    }
}
