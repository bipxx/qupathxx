// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstddef>
#include <string>

namespace qpx::detail {
struct OmeTiffXmlExport {
    std::string path;
    std::string metadata_source;
    std::size_t bytes;
    std::string sha256;
};

OmeTiffXmlExport export_ome_tiff_xml(const std::string& input, const std::string& output, bool resolve_companion,
                                   const std::atomic<bool>* cancelled=nullptr);
}
