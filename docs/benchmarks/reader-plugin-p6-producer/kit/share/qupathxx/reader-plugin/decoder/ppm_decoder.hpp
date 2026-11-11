#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Shared by the core and the independently built P6 display plugin.
// This component deliberately has no qupathxx library or public type dependency.
namespace qpx_ppm {
struct DecodedImage {
    int width=0,height=0;
    std::vector<std::uint8_t> rgb;
};
DecodedImage read_ppm(const std::string& path);
}
