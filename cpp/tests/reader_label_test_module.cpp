// SPDX-License-Identifier: AGPL-3.0-only
// Authored transport fixture only; no NGFF schema or decoder implementation.
#define QPX_TEST_LABEL_DESCRIPTION 1
#include "reader_store_test_module.cpp"
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {
std::atomic<unsigned> label_calls{0}, label_polls{0};

qpx_reader_status_v1 QPX_READER_CALL label_description(void *p,
    qpx_reader_text_v1 *out, qpx_reader_cancel_v1 cancel,
    qpx_reader_text_v1 *error) {
  ++label_calls;
  const auto *handle = static_cast<Handle *>(p);
  // Deliberate C-ABI violations exercise the host's defensive exception path.
  if (handle && fault(handle->options, "label-throw-mutates")) {
    mutate(handle->root);
    throw std::runtime_error("Authored callback throw after store mutation");
  }
  if (handle && fault(handle->options, "label-throw"))
    throw std::runtime_error("Authored callback exception");
  return boundary(error, [&] {
    if (!handle || !out)
      return QPX_READER_INVALID_REQUEST;
    out->size = 0;
    ++label_polls;
    if (cancelled(cancel))
      return QPX_READER_CANCELLED;
    if (fault(handle->options, "label-wait-cancel")) {
      const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (std::chrono::steady_clock::now() < until) {
        ++label_polls;
        if (cancelled(cancel))
          return QPX_READER_CANCELLED;
        std::this_thread::yield();
      }
      text(*error, "Authored cancellation test timed out");
      return QPX_READER_INTERNAL_ERROR;
    }
    if (fault(handle->options, "label-cancel"))
      return QPX_READER_CANCELLED;
    if (fault(handle->options, "label-mutates") ||
        fault(handle->options, "label-failed-mutates"))
      mutate(handle->root);
    if (fault(handle->options, "label-primary-mutates"))
      std::ofstream(handle->root / "primary/metadata.json", std::ios::app) << ' ';
    if (fault(handle->options, "label-failure") ||
        fault(handle->options, "label-failed-mutates")) {
      text(*error, "Authored label description failure");
      return QPX_READER_IO_ERROR;
    }
    if (fault(handle->options, "label-empty"))
      return QPX_READER_OK;
    if (fault(handle->options, "label-oversize")) {
      out->size = 65537;
      return QPX_READER_OK;
    }
    if (fault(handle->options, "label-buffer-too-small")) {
      out->size = 65537;
      return QPX_READER_BUFFER_TOO_SMALL;
    }
    if (fault(handle->options, "label-wrong-required-size")) {
      out->size = 1;
      return QPX_READER_BUFFER_TOO_SMALL;
    }
    if (fault(handle->options, "label-error-invalid-utf8")) {
      text(*error, std::string("\xc0\xaf", 2));
      return QPX_READER_IO_ERROR;
    }
    if (fault(handle->options, "label-unknown-status")) {
      text(*error, "Authored unknown status");
      return qpx_reader_status_v1(99);
    }
    std::string description =
        R"({"schema":"qpx.authored-label-description/1","opaque":"owned native metadata","background":"0","codes":["9007199254740993","18446744073709551615"]})";
    if (fault(handle->options, "label-opaque"))
      description = R"({"schema":"unknown.authored/99","opaque":"schema parsing belongs to the native service"})";
    if (fault(handle->options, "label-valid-max")) {
      description = R"({"padding":")";
      description.append(65536 - description.size() - 2, 'x');
      description += "\"}";
    }
    if (fault(handle->options, "label-invalid-utf8"))
      description = std::string("\xed\xa0\x80", 3);
    if (fault(handle->options, "label-nul"))
      description = std::string("{}\0tail", 7);
    if (!text(*out, description))
      return QPX_READER_BUFFER_TOO_SMALL;
    if (fault(handle->options, "label-pointer"))
      out->data = nullptr;
    if (fault(handle->options, "label-capacity"))
      --out->capacity;
    if (fault(handle->options, "label-error-pointer"))
      error->data = nullptr;
    if (fault(handle->options, "label-error-capacity"))
      --error->capacity;
    if (fault(handle->options, "label-error-oversize"))
      error->size = 4097;
    if (fault(handle->options, "label-success-error"))
      text(*error, "Authored error on a success status");
    if (fault(handle->options, "label-failed-output")) {
      text(*error, "Authored failed call retained output size");
      return QPX_READER_IO_ERROR;
    }
    return QPX_READER_OK;
  });
}
}

#ifndef QPX_TEST_MISSING_LABEL
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_label_entry_v1(std::uint32_t abi, qpx_reader_label_api_v1 *out,
    qpx_reader_text_v1 *error) {
  return boundary(error, [&] {
    if (abi != 1 || !out || out->struct_size != sizeof(*out))
      return QPX_READER_INVALID_REQUEST;
    *out = {sizeof(*out), 1, label_description};
#ifdef QPX_TEST_BAD_LABEL
    out->abi_version = 2;
#endif
#ifdef QPX_TEST_SHORT_LABEL
    out->struct_size = 8;
#endif
    return QPX_READER_OK;
  });
}
#endif

extern "C" __declspec(dllexport) unsigned QPX_READER_CALL qpx_test_label_calls() {
  return label_calls.load();
}
extern "C" __declspec(dllexport) unsigned QPX_READER_CALL qpx_test_label_polls() {
  return label_polls.load();
}
