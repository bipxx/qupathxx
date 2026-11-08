#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace qpx::detail {
bool nucls_space(std::uint32_t);
std::uint32_t nucls_codepoint(const std::string&,std::size_t&);
bool nucls_padded(const std::string&);
double nucls_number(const std::string&);
class NuclsCsvReader {
    const std::string& data_;
    std::size_t at_=0,line_=0;
public:
    explicit NuclsCsvReader(const std::string&);
    bool next(std::vector<std::string>&);
    std::size_t line() const {return line_;}
};
}
