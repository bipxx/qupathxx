#pragma once
#include <cstdint>
#include <stdexcept>

namespace qpx {
// Within one source series/document. Channel -1 applies to every channel;
// Z and time are concrete zero-based indices, never wildcards.
struct ObjectPlane {
    std::int32_t c=-1;
    std::uint32_t z=0,t=0;
    ObjectPlane(std::int32_t channel=-1,std::uint32_t z_index=0,std::uint32_t time_index=0):c(channel),z(z_index),t(time_index) {validate();}
    void validate() const {
        if(c < -1 || c >= 65536 || z >= 65536 || t >= 65536)
            throw std::out_of_range("object channel/Z/time index exceeds limits");
    }
    bool operator==(const ObjectPlane& other) const {return c==other.c && z==other.z && t==other.t;}
    bool operator!=(const ObjectPlane& other) const {return !(*this==other);}
    bool covers(const ObjectPlane& child) const {
        return z==child.z && t==child.t && (c==-1 || c==child.c);
    }
};
}
