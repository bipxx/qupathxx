#pragma once
#include "qupathxx/image_data_properties.hpp"
namespace qpx::detail {
std::vector<std::uint8_t> encode_image_data_properties(const ImageDataProperties&);
std::shared_ptr<const ImageDataProperties> decode_image_data_properties(const void*,std::size_t);
}
