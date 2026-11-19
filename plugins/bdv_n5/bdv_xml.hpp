// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qpx::detail {

struct BdvXmlSetup {
    std::uint32_t id = 0;
    std::string name;
};

struct BdvXmlPartitionView {
    std::size_t file = 0;
    std::uint32_t setup = 0;
    std::uint32_t time = 0;
};

struct BdvXmlProject {
    std::string data_path;
    std::vector<BdvXmlSetup> setups;
    std::vector<std::string> partition_paths;
    // Channel-major, then ordinal time. File indices refer to partition_paths.
    std::vector<BdvXmlPartitionView> partition_views;
    // Channel-major and then ordinal time, like partition_views.
    std::vector<bool> missing_views;
    std::array<std::int64_t, 3> dimensions{};
    std::array<double, 3> voxel_size{};
    std::array<double, 3> voxel_um{};
    std::uint32_t first_time = 0;
    std::uint32_t time_count = 0;

    bool setup_has_views(std::size_t channel) const {
        const auto first = missing_views.begin() + channel * time_count;
        return std::find(first, first + time_count, false) != first + time_count;
    }
};

enum class BdvStorage { N5, Hdf5 };

// Read aligned SpimData 0.2. Explicit channel setups are returned in
// ascending setup-ID order. All setups use the same geometry.
// The caller retains ownership and guards of the XML and all dataset members.
BdvXmlProject parse_bdv_xml(const std::vector<std::uint8_t>& bytes, BdvStorage storage);

}
