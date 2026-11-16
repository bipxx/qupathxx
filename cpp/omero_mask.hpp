#pragma once
#include "qupathxx/omero_rois.hpp"
#include <nlohmann/json.hpp>
namespace qpx::detail {
std::uint32_t omero_rgba_int(const nlohmann::json&);
std::size_t omero_mask_pixels(const nlohmann::json&, const OmeroImageInfo&);
void decode_omero_mask(OmeroShape&, const OmeroImageInfo&, const std::vector<std::uint8_t>& png,
                       const std::atomic<bool>*);
nlohmann::json omero_mask_value(const OmeroShape&);
} // namespace qpx::detail
