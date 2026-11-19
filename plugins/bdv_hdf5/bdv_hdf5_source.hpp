// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "qupathxx/image_server.hpp"

#include <cstddef>

namespace qpx::detail {
std::shared_ptr<ImageServer> open_bdv_hdf5_image(const std::string& project_directory,
    std::size_t work, const std::atomic<bool>* cancelled);
}
