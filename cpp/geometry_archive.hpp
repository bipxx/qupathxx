#pragma once
#include "qupathxx/geometry.hpp"
namespace qpx {
struct GeometryArchiveCodec {
    static std::vector<std::uint8_t> encode(const Roi&);
    static Roi decode(const std::uint8_t*,std::size_t);
};
}
