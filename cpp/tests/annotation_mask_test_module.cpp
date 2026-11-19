// SPDX-License-Identifier: AGPL-3.0-only
#include "annotation_source.hpp"
#include "format_plugin_profile.hpp"
#ifndef QPX_TEST_ABI
#error Define QPX_TEST_ABI
#endif
#define QPX_STRING_INNER(x) #x
#define QPX_STRING(x) QPX_STRING_INNER(x)
namespace {
using namespace qpx_annotation_source;
struct Definition {
  static constexpr const char *id =
      "org.qupathxx.mask-test-" QPX_STRING(QPX_TEST_ABI);
  static constexpr const char *format = "mask-test-" QPX_STRING(QPX_TEST_ABI);
  static constexpr const char *name = "Mask ABI trust-boundary fixture";
  static constexpr const char *contract = "mask-faults/1";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1) {}
  static void decode(Handle &h, Bytes input, const qpx_reader_open_v1 &) {
    h.rows.emplace_back(&h.arena.memory);
    auto &row = h.rows.back();
    row.id = "mask";
    row.encoding = 3;
    row.geometry = {'Q', 'B', 'M', 1, 16, 0, 0, 0, 16, 0, 0, 0};
    row.geometry.resize(44);
    row.geometry[12] = 128;
    const auto mode = input.data[0];
    if (mode == 'l')
      row.flags = QPX_ANNOTATION_OBJECT_LOCKED;
    if (mode == 'f')
      row.flags = 4;
    if (mode == 'c') {
      row.geometry = {'Q', 'B', 'M', 1, 8, 0, 0, 0, 8, 0,
                      0,   0,   128, 0, 0, 0, 0, 0, 0, 0};
    }
    if (mode == 't') {
      row.geometry = {'Q', 'B', 'M', 1, 3, 0, 0, 0, 1, 0, 0, 0, 129};
    }
    if (mode == 's')
      row.geometry.pop_back();
    if (mode == 'e')
      row.geometry[12] = 0;
  }
};
} // namespace
#if QPX_TEST_ABI == 3
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t a, qpx_annotation_format_api_v3 *t,
                               qpx_reader_text_v1 *e) {
  return Plugin<Definition, 3>::entry(a, t, e);
}
#elif QPX_TEST_ABI == 4
extern "C" QPX_ANNOTATION_LOCKED_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v4(std::uint32_t a, qpx_annotation_format_api_v4 *t,
                               qpx_reader_text_v1 *e) {
  return Plugin<Definition, 4>::entry(a, t, e);
}
#else
extern "C" QPX_ANNOTATION_MASK_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v5(std::uint32_t a, qpx_annotation_format_api_v5 *t,
                               qpx_reader_text_v1 *e) {
  return Plugin<Definition, 5>::entry(a, t, e);
}
#endif
