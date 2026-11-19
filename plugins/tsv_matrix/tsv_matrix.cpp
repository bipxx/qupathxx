// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/delimited_matrix.hpp"

namespace {
struct Definition final : qpx_delimited_matrix::Table<'\t'> {
    static constexpr const char *id = "org.qupathxx.tsv-matrix", *format = "tsv-matrix",
        *name = "TSV expression matrix", *version = QPX_TSV_MATRIX_VERSION,
        *contract = QPX_TSV_MATRIX_CONTRACT;
};
}

extern "C" QPX_MATRIX_TYPED_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v2(
    std::uint32_t requested, qpx_matrix_format_api_v2* out, qpx_reader_text_v1* error) {
    return qpx_matrix_table::Plugin<Definition>::entry(requested, out, error);
}
