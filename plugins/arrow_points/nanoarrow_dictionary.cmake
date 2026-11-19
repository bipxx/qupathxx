# SPDX-License-Identifier: AGPL-3.0-only
# Adapt the pinned 0.9.0 dictionary decoder to validate and decode the nested
# RecordBatch using its IPC version and compression.
function(replace_nanoarrow old new)
  string(FIND "${source}" "${old}" found)
  if(found LESS 0)
    message(FATAL_ERROR "Missing nanoarrow 0.9.0 patch context")
  endif()
  string(REPLACE "${old}" "${new}" source "${source}")
  set(source "${source}" PARENT_SCOPE)
endfunction()

function(write_patched_nanoarrow file)
  set(content "${source}\n/* Modified by qupathxx for strict Arrow IPC dictionary decoding. */\n")
  set(destination "${CMAKE_CURRENT_BINARY_DIR}/patched-${file}")
  if(EXISTS "${destination}")
    file(READ "${destination}" previous)
  endif()
  if(NOT content STREQUAL previous)
    file(WRITE "${destination}" "${content}")
  endif()
endfunction()

file(READ "${nano}/ipc/decoder.c" source)
replace_nanoarrow(
  [=[  dictionary->decoder.message_type = NANOARROW_IPC_MESSAGE_TYPE_RECORD_BATCH;
  dictionary_decoder_private_data->last_message = record_batch;]=]
  [=[  if (record_batch == NULL) {
    ArrowErrorSet(error, "DictionaryBatch is missing its RecordBatch");
    return EINVAL;
  }
  dictionary->decoder.message_type = NANOARROW_IPC_MESSAGE_TYPE_RECORD_BATCH;
  dictionary->decoder.metadata_version = decoder->metadata_version;
  NANOARROW_RETURN_NOT_OK(
      ArrowIpcDecoderDecodeRecordBatchHeader(&dictionary->decoder, record_batch, error));
  dictionary_decoder_private_data->last_message = record_batch;]=])

replace_nanoarrow(
  [=[static ArrowErrorCode ArrowIpcDictionaryAppend(struct ArrowIpcDictionary* dictionary,
                                               struct ArrowArray* value,
                                               struct ArrowError* error) {
  if (dictionary->current_value.release != NULL &&
      dictionary->current_value.length != 0) {
    ArrowErrorSet(error, "Dictionary concatenation is not yet supported");
    return ENOTSUP;
  }

  NANOARROW_RETURN_NOT_OK(ArrowIpcDictionaryReplace(dictionary, value, error));
  return NANOARROW_OK;
}]=]
  [=[#include "dictionary_append.h"]=])
replace_nanoarrow(
  [=[result = ArrowIpcDictionaryAppend(dictionary, &tmp, error);]=]
  [=[result = qpx_append_dictionary_delta(dictionary, &tmp, array_view->storage_type, error);]=])
write_patched_nanoarrow(decoder.c)

file(READ "${nano}/ipc/codecs.c" source)
replace_nanoarrow(
  [=[#include <zstd.h>]=]
  [=[#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include "decoder_memory.h"]=])
replace_nanoarrow(
  [=[  size_t code =
      ZSTD_decompress((void*)dst, (size_t)dst_size, src.data.data, src.size_bytes);]=]
  [=[  const size_t src_size = (size_t)src.size_bytes;
  ZSTD_frameHeader frame;
  if (ZSTD_getFrameHeader(&frame, src.data.data, src_size) != 0 ||
      frame.frameType != ZSTD_frame || frame.dictID != 0 ||
      frame.windowSize > QPX_ARROW_DECODER_WORK ||
      (frame.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN &&
       frame.frameContentSize != (unsigned long long)dst_size) ||
      ZSTD_findFrameCompressedSize(src.data.data, src_size) != src_size) {
    ArrowErrorSet(error, "Expected one bounded self-contained Zstandard frame");
    return EIO;
  }
  size_t code = ZSTD_decompress(dst, (size_t)dst_size, src.data.data, src_size);]=])
replace_nanoarrow(
  [=[  if (ret != 0) {]=]
  [=[  if (ret != 0 || src_size != (size_t)src.size_bytes) {]=])
write_patched_nanoarrow(codecs.c)
