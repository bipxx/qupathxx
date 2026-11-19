// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/binary_input.hpp"
#include "decoder_memory.h"
#include <nanoarrow/nanoarrow_ipc.h>

namespace qpx_arrow {
using namespace qpx_binary;
inline constexpr std::size_t row_limit = QPX_ARROW_MAX_ROWS;

struct Consumer {
    virtual ~Consumer() = default;
    virtual void schema(const ArrowSchema&) = 0;
    virtual void batch(const ArrowArrayView&) = 0;
};

void read_file(Bytes file, std::pmr::memory_resource* memory,
               qpx_reader_cancel_v1 cancel, Consumer& consumer);
int find_column(const ArrowSchema& schema, std::string_view name);
ArrowType column_type(const ArrowSchema& column);
bool is_text(ArrowType type);
bool is_integer(ArrowType type);
bool is_signed_integer(ArrowType type);
// Resolve either a plain value or a checked dictionary index. A null index or
// dictionary value yields nullptr. On success row addresses the returned view.
const ArrowArrayView* value_at(const ArrowArrayView& column, int64_t& row);
std::string_view string_at(const ArrowArrayView&, int64_t row, std::size_t limit,
                           qpx_reader_cancel_v1);
} // namespace qpx_arrow
