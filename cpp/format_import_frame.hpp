// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "qupathxx/registration.hpp"

namespace qpx::detail {
inline constexpr const char* matrix_import_frame_id = "qpx.format-import-cli-image/1";
inline constexpr const char* reader_bound_matrix_frame_id = "qpx.format-import-cli-image/2";

const char* indexed_matrix_frame_id(const Document&);
}
