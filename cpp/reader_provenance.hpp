#pragma once
#include "qupathxx/image_server.hpp"
namespace qpx {
// Preserve the file decoder descriptor while retaining normal dense ownership.
std::shared_ptr<ImageServer> ppm_image_server(const std::string& path);
}
