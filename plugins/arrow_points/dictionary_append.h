// SPDX-License-Identifier: AGPL-3.0-only
// Included by the patched nanoarrow translation unit after its dictionary
// types are declared. All buffers use the provider's private bounded allocator.
#include "decoder_memory.h"

static ArrowErrorCode qpx_append_dictionary_values(struct ArrowArray* out,
                                                   const struct ArrowArray* array,
                                                   enum ArrowType type,
                                                   struct ArrowError* error) {
  struct ArrowArrayView view;
  ArrowArrayViewInitFromType(&view, type);
  ArrowErrorCode result = ArrowArrayViewSetArray(&view, array, error);
  for (int64_t i = 0; result == NANOARROW_OK && i < view.length; ++i) {
    if (ArrowArrayViewIsNull(&view, i)) {
      result = ArrowArrayAppendNull(out, 1);
    } else {
      switch (type) {
        case NANOARROW_TYPE_STRING: case NANOARROW_TYPE_LARGE_STRING:
          result = ArrowArrayAppendString(out, ArrowArrayViewGetStringUnsafe(&view, i));
          break;
        case NANOARROW_TYPE_INT8: case NANOARROW_TYPE_INT16:
        case NANOARROW_TYPE_INT32: case NANOARROW_TYPE_INT64:
          result = ArrowArrayAppendInt(out, ArrowArrayViewGetIntUnsafe(&view, i));
          break;
        case NANOARROW_TYPE_UINT8: case NANOARROW_TYPE_UINT16:
        case NANOARROW_TYPE_UINT32: case NANOARROW_TYPE_UINT64:
          result = ArrowArrayAppendUInt(out, ArrowArrayViewGetUIntUnsafe(&view, i));
          break;
        case NANOARROW_TYPE_FLOAT: case NANOARROW_TYPE_DOUBLE:
          // All finite Float32 values, including subnormals and signed zero,
          // survive a round trip through binary64 exactly.
          result = ArrowArrayAppendDouble(out, ArrowArrayViewGetDoubleUnsafe(&view, i));
          break;
        default:
          result = ENOTSUP;
          break;
      }
    }
  }
  ArrowArrayViewReset(&view);
  return result;
}

static ArrowErrorCode qpx_append_dictionary_delta(struct ArrowIpcDictionary* dictionary,
                                                 struct ArrowArray* delta,
                                                 enum ArrowType type,
                                                 struct ArrowError* error) {
  const int64_t previous = dictionary->current_value.length;
  if (previous > QPX_ARROW_MAX_ROWS || delta->length > QPX_ARROW_MAX_ROWS - previous) {
    ArrowErrorSet(error, "Combined dictionary exceeds its entry limit");
    return EINVAL;
  }
  if (!previous)
    return ArrowIpcDictionaryReplace(dictionary, delta, error);
  if (!delta->length) {
    ArrowArrayRelease(delta);
    return NANOARROW_OK;
  }

  struct ArrowArray merged = {0};
  ArrowErrorCode result = ArrowArrayInitFromType(&merged, type);
  if (result == NANOARROW_OK)
    result = ArrowArrayStartAppending(&merged);
  if (result == NANOARROW_OK)
    result = ArrowArrayReserve(&merged, previous + delta->length);
  if (result == NANOARROW_OK)
    result = qpx_append_dictionary_values(&merged, &dictionary->current_value, type, error);
  if (result == NANOARROW_OK)
    result = qpx_append_dictionary_values(&merged, delta, type, error);
  if (result == NANOARROW_OK)
    result = ArrowArrayFinishBuildingDefault(&merged, error);
  if (result == NANOARROW_OK)
    result = ArrowIpcDictionaryReplace(dictionary, &merged, error);
  if (merged.release)
    ArrowArrayRelease(&merged);
  if (result == NANOARROW_OK)
    ArrowArrayRelease(delta);
  return result;
}
