// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/annotation_source.hpp"
namespace qpx_imagej {
using namespace qpx_annotation_source;
void roi(Handle &, Bytes, std::string_view id, std::string_view fallback,
         const Facts &, qpx_reader_cancel_v1);
void zip(Handle &, Bytes, qpx_reader_cancel_v1);
} // namespace qpx_imagej
