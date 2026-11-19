// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "roi.hpp"
namespace {
constexpr char provider_version[] = QPX_FORMAT_PLUGIN_PROVIDER_VERSION;
struct ImageJ {
  static constexpr const char *id = "org.qupathxx.imagej-roi";
  static constexpr const char *format = "imagej-roi";
  static constexpr const char *name = "ImageJ ROI and ZIP";
  static constexpr const char *contract = "imagej-pixel-roi/1";
  static constexpr const char *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
  static void validate_options(qpx_reader_string_v1 options) {
    qpx_binary::require(!options.size, "ImageJ ROI accepts empty options only");
  }
  static void decode(qpx_annotation_source::Handle &h, qpx_binary::Bytes input,
                     const qpx_reader_open_v1 &request) {
    if (input.size >= 4 && input.sub(0, 4).view() == "Iout") {
      const auto filename =
          std::filesystem::u8path(
              std::string_view(request.path.data,
                               std::size_t(request.path.size)))
              .stem()
              .u8string();
      qpx_annotation_source::Facts none(&h.arena.memory);
      qpx_imagej::roi(h, input, "roi", filename, none, request.cancel);
    } else
      qpx_imagej::zip(h, input, request.cancel);
  }
};
} // namespace
extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi,
                               qpx_annotation_format_api_v3 *out,
                               qpx_reader_text_v1 *error) {
  return qpx_annotation_source::Plugin<ImageJ, 3, provider_version>::entry(
      abi, out, error);
}
