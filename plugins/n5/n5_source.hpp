// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "qupathxx/image_server.hpp"

#include <cstddef>

namespace qpx::detail {
std::shared_ptr<ImageServer> open_n5_image(const std::string& directory,
    std::size_t work_bytes, const std::atomic<bool>* cancelled);
#ifdef QUPATHXX_HAS_BDV_N5
std::shared_ptr<ImageServer> open_bdv_n5_image(const std::string& project_directory,
    std::size_t work_bytes, const std::atomic<bool>* cancelled);
#endif
}
