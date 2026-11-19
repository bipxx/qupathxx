// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace qpx {
class ImageServer;
namespace detail {
struct OmeTiffXmlExport {
    std::string path;
    std::string metadata_source;
    std::size_t bytes;
    std::string sha256;
};

OmeTiffXmlExport export_ome_tiff_xml(const std::string& input, const std::string& output, bool resolve_companion,
                                   const std::atomic<bool>* cancelled=nullptr);

struct OmeTiffRoiFrame {
    std::string image_id;
    std::uint32_t width=0,height=0;
    std::uint16_t orientation=1;
    bool transpose=false,reverse_x=false,reverse_y=false;
};
std::optional<OmeTiffRoiFrame> ome_tiff_roi_frame(const ImageServer&,const std::string& xml_sha256);
}
}
