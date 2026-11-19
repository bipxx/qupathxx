// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/source_identity.hpp"
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <map>
#include <string>

namespace qpx::detail {

using DirectoryInventory = std::map<std::string, FileVersion>;
inline constexpr auto directory_inventory_legacy_unbounded = std::numeric_limits<std::size_t>::max();

struct DirectoryInventoryLimits {
    std::size_t inventory_bytes, max_files, max_depth, absolute_path_bytes;
};

// One traversal authority. New store consumers supply bounded depth/path limits.
// The retained ZarrStore explicitly supplies legacy_unbounded for those two
// limits, preserving its existing observation contract and embedded layout.
DirectoryInventory observe_directory_inventory(const std::filesystem::path& root,
    const DirectoryInventoryLimits&, const std::atomic<bool>* cancelled);
std::size_t directory_inventory_payload(const DirectoryInventory&);
// Preserve the established ordered observed-store hash domain and file fields;
// this is an OS file-version/membership digest, never a content fingerprint.
std::string directory_inventory_sha256(const std::filesystem::path& root, const DirectoryInventory&);
void require_directory_inventory_current(const DirectoryInventory& expected, const DirectoryInventory& current);
void require_directory_file_version(const FileVersion& expected, const FileVersion& current);
void require_directory_path_bytes(const std::filesystem::path&, std::size_t maximum);
void require_directory_real_ancestors(const std::filesystem::path&, const std::atomic<bool>* cancelled);

}
