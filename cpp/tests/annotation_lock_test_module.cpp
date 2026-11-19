// SPDX-License-Identifier: AGPL-3.0-only
// Controlled ABI negotiation values; never a shipped scientific reader.
#include "annotation_source.hpp"
#include "format_plugin_profile.hpp"
namespace {
struct Definition {
  static constexpr const char *id = "org.qupathxx.test-lock";
  static constexpr const char *format = "test-lock";
  static constexpr const char *name = "Controlled lock flags";
  static constexpr const char *contract = "test-lock/1";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1 v) {
    qpx_binary::require(!v.size, "No test options");
  }
  static void decode(qpx_annotation_source::Handle &h, qpx_binary::Bytes input,
                     const qpx_reader_open_v1 &) {
    h.rows.emplace_back(&h.arena.memory);
    auto &row = h.rows.back();
    row.id = "locked-value";
    row.flags = input.data[0] == '2' ? 2 : 8;
    qpx_annotation_source::GeometryEncoder e{row.geometry};
    e.header(5);
    e.u32(1);
    e.real(1);
    e.real(2);
  }
};
} // namespace
#if QPX_TEST_ABI == 3
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi,
                               qpx_annotation_format_api_v3 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<Definition, 3>::entry(abi, out, error);
}
#else
extern "C" QPX_ANNOTATION_LOCKED_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v4(std::uint32_t abi,
                               qpx_annotation_format_api_v4 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<Definition, 4>::entry(abi, out, error);
}
#endif
