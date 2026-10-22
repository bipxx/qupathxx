#pragma once
#include "forest_fixture.hpp"
#include <utility>
namespace qpx_test {
// Independently authored, backend-free forest using the object pixel schema.
inline std::vector<std::uint8_t> object_forest_fixture(const std::vector<std::pair<std::string,std::string>>& features={{"object.area","px2"},{"object.perimeter","px"},{"object.width","px"},{"object.height","px"},{"object.circularity","1"}}) {
    std::vector<std::uint8_t> b;u32(b,0x46585051);u32(b,1);u32(b,2);u32(b,2);u32(b,2);u32(b,0);u64(b,1);u64(b,4*1024*1024);u64(b,4);
    str(b,std::string(64,'a'));str(b,"independent-object-test/1");u32(b,static_cast<std::uint32_t>(features.size()));
    for(const auto& f:features) {str(b,f.first);str(b,f.second);}
    u32(b,2);str(b,"A");str(b,"B");u32(b,2);u32(b,0);u32(b,3);u32(b,4);
    for(auto n:std::vector<std::vector<std::uint32_t>>{{0,1,2,0,0x3f800000},{0,0,0,0,0},{0,0,0,1,0},{0,0,0,1,0}}) for(auto x:n) u32(b,x);
    return b;
}
}
