// SPDX-License-Identifier: AGPL-3.0-only
// Test-only controlled ABI faults. The independently built production module
// contains none of these seams or filesystem events.
#define qpx_annotation_format_entry_v2 qpx_annotation_geometry_good_entry_v2
#include "../../plugins/geometry_tsv/geometry_tsv.cpp"
#undef qpx_annotation_format_entry_v2
#include <chrono>
#include <fstream>
#include <thread>

namespace {
qpx_annotation_format_api_v2 good{};
std::string fault;
std::filesystem::path events;
void event(const char *value) {
  std::ofstream out(events, std::ios::app | std::ios::binary);
  out << value << '\n';
}
qpx_reader_status_v1 QPX_FORMAT_CALL
bad_open(const qpx_reader_open_v1 *request, void **output,
         qpx_reader_text_v1 *error) noexcept {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(request && request->path.data, "Test open path is missing");
    const auto path = std::filesystem::u8path(
        std::string(request->path.data, std::size_t(request->path.size)));
    fault = path.filename().u8string();
    events = std::filesystem::u8path(path.u8string() + ".events");
    const auto status = good.open(request, output, error);
    if (status == QPX_READER_OK)
      event("open");
    return status;
  });
}
void QPX_FORMAT_CALL bad_destroy(void *value) noexcept {
  try {
    event("destroy");
  } catch (...) {
  }
  good.destroy(value);
}
qpx_reader_status_v1 QPX_FORMAT_CALL
bad_metadata(void *value, qpx_annotation_metadata_v2 *output,
             qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  const auto status = good.metadata(value, output, cancel, error);
  if (status != QPX_READER_OK)
    return status;
  if (fault == "metadata-encoding.tsv")
    output->geometry_encoding = 99;
  if (fault == "metadata-reserved.tsv")
    output->reserved64[1] = 1;
  if (fault == "metadata-zero.tsv")
    output->objects = 0;
  if (fault == "metadata-total.tsv")
    ++output->geometry_bytes;
  if (fault == "metadata-huge.tsv")
    output->geometry_bytes = UINT64_MAX;
  return status;
}
qpx_reader_status_v1 QPX_FORMAT_CALL
bad_objects(void *value, const qpx_annotation_objects_request_v2 *request,
            qpx_annotation_objects_buffer_v2 *output,
            qpx_reader_cancel_v1 cancel, qpx_reader_text_v1 *error) noexcept {
  if (fault == "wait-objects.tsv")
    return boundary(error, [&]() -> qpx_reader_status_v1 {
      event("entered");
      for (;;) {
        checkpoint(cancel);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  const auto status = good.objects(value, request, output, cancel, error);
  if (status == QPX_READER_BUFFER_TOO_SMALL) {
    if (fault == "probe-geometry-huge.tsv")
      output->geometry.size = 1024 * 1024 + 1;
    if (fault == "probe-text-huge.tsv")
      output->text.size = 8193;
    if (fault == "probe-count.tsv")
      ++output->written;
    if (fault == "probe-pointer.tsv")
      output->objects =
          reinterpret_cast<qpx_annotation_object_v2 *>(std::uintptr_t(1));
    if (fault == "probe-request.tsv")
      ++const_cast<qpx_annotation_objects_request_v2 *>(request)->first;
  }
  if (status == QPX_READER_OK && output->written) {
    auto &row = output->objects[0];
    if (fault == "fill-pointer.tsv")
      ++output->geometry.data;
    if (fault == "fill-count.tsv")
      output->written = 0;
    if (fault == "fill-size.tsv")
      --output->geometry.size;
    if (fault == "fill-offset.tsv")
      row.geometry_offset = 1;
    if (fault == "fill-overrun.tsv")
      ++row.geometry_size;
    if (fault == "fill-text-offset.tsv")
      row.source_id_offset = UINT64_MAX;
    if (fault == "fill-utf8.tsv")
      output->text.data[0] = char(0xff);
    if (fault == "fill-plane.tsv")
      row.z = -1;
    if (fault == "fill-reserved.tsv")
      row.reserved = 1;
    if (fault == "fill-qro.tsv")
      output->geometry.data[0] = 0;
    if (fault == "fill-kind.tsv")
      output->geometry.data[4] = 99;
    if (fault == "fill-qro-count.tsv") {
      output->geometry.data[4] = 5;
      std::memset(output->geometry.data + 5, 0xff, 4);
    }
    if (fault == "fill-nan.tsv") {
      const std::uint64_t nan = UINT64_C(0x7ff8000000000000);
      std::memcpy(output->geometry.data + 5, &nan, 8);
    }
    if (fault == "fill-negative.tsv") {
      const double negative = -1;
      std::memcpy(output->geometry.data + 5, &negative, 8);
    }
    if (fault == "fill-truncated.tsv")
      --row.geometry_size;
  }
  return status;
}
} // namespace
extern "C" QPX_ANNOTATION_GEOMETRY_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v2(std::uint32_t requested,
                               qpx_annotation_format_api_v2 *output,
                               qpx_reader_text_v1 *error) {
  const auto status =
      qpx_annotation_geometry_good_entry_v2(requested, output, error);
  if (status == QPX_READER_OK) {
    good = *output;
    output->open = bad_open;
    output->destroy = bad_destroy;
    output->metadata = bad_metadata;
    output->objects = bad_objects;
  }
  return status;
}
