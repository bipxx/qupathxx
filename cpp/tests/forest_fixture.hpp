#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace qpx_test {
inline void u32(std::vector<std::uint8_t>& b,std::uint32_t n) {for(unsigned i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
inline void u64(std::vector<std::uint8_t>& b,std::uint64_t n) {u32(b,static_cast<std::uint32_t>(n));u32(b,static_cast<std::uint32_t>(n>>32));}
inline void str(std::vector<std::uint8_t>& b,const std::string& s) {u32(b,static_cast<std::uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end());}
// Independently authored two-tree model: first tree splits x<=1; second
// always votes B. An equal vote must choose the first declared class A.
inline std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b;u32(b,0x46585051);u32(b,1);u32(b,2);u32(b,2);u32(b,2);u32(b,0);u64(b,1);u64(b,4*1024*1024);u64(b,4);
    str(b,std::string(64,'a'));str(b,"independent-test/1");u32(b,1);str(b,"area");str(b,"um2");u32(b,2);str(b,"A");str(b,"B");u32(b,2);u32(b,0);u32(b,3);u32(b,4);
    for(auto n:std::vector<std::vector<std::uint32_t>>{{0,1,2,0,0x3f800000},{0,0,0,0,0},{0,0,0,1,0},{0,0,0,1,0}}) for(auto x:n) u32(b,x);
    return b;
}
}
