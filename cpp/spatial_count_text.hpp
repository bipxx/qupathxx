#pragma once
#include "utf8_text.hpp"
#include <array>
#include <cstdint>
#include <charconv>
#include <string_view>
#include <stdexcept>
namespace qpx::detail {
inline std::uint64_t spatial_unsigned(std::string_view text){std::uint64_t value=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),value);if(text.empty() || p.ec!=std::errc{} || p.ptr!=text.data()+text.size())throw std::invalid_argument("Expected unsigned spatial-count integer");return value;}
inline std::string spatial_identifier(std::string_view text){std::string value(text);bounded_utf8(value,256);if(value.empty() || value.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Empty or invalid spatial-count identifier");return value;}
inline std::array<std::string_view,3> spatial_triple(std::string_view text,bool tabs=false){std::array<std::string_view,3> out;std::size_t n=0;while(true){if(!tabs){const auto begin=text.find_first_not_of(" \t");if(begin==std::string_view::npos)break;text.remove_prefix(begin);}if(n==3)throw std::invalid_argument("Extra spatial-count fields");const auto end=tabs?text.find('\t'):text.find_first_of(" \t");out[n++]=text.substr(0,end);if(end==std::string_view::npos)break;text.remove_prefix(end+1);}if(n!=3)throw std::invalid_argument("Expected three spatial-count fields");return out;}
}
