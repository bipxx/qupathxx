# SPDX-License-Identifier: AGPL-3.0-only

function(qpx_prepare_blosc_decoder input output)
  file(READ "${input}" source)
  string(FIND "${source}" "static int zstd_wrap_compress(" start)
  string(FIND "${source}" "static int zstd_wrap_decompress(" end)
  if(start LESS 0 OR end LESS_EQUAL start)
    message(FATAL_ERROR "Cannot isolate the pinned C-Blosc Zstandard writer")
  endif()
  math(EXPR length "${end}-${start}")
  string(SUBSTRING "${source}" ${start} ${length} writer)
  string(REPLACE "${writer}"
    "size_t qpx_ngff05_blosc_zstd(const void*, size_t, void*, size_t);\n\n"
    source "${source}")
  set(compress_branch [=[
    #if defined(HAVE_ZSTD)
    else if (context->compcode == BLOSC_ZSTD) {
      cbytes = zstd_wrap_compress((char*)_tmp + j * neblock, (size_t)neblock,
                                  (char*)dest, (size_t)maxout, context->clevel);
    }
    #endif /* HAVE_ZSTD */
]=])
  string(FIND "${source}" "${compress_branch}" at)
  if(at LESS 0)
    message(FATAL_ERROR "Cannot isolate pinned C-Blosc Zstandard compression dispatch")
  endif()
  string(REPLACE "${compress_branch}" "" source "${source}")
  set(old [=[
  code = ZSTD_decompress(
      (void*)output, maxout, (void*)input, compressed_length);
]=])
  string(FIND "${source}" "${old}" at)
  if(at LESS 0)
    message(FATAL_ERROR "Cannot locate pinned C-Blosc Zstandard decode call")
  endif()
  string(REPLACE "${old}"
    "  code = qpx_ngff05_blosc_zstd(input, (size_t)compressed_length, output, (size_t)maxout);\n"
    source "${source}")
  set(old [=[
  status = uncompress(
             (Bytef*)output, &ul, (Bytef*)input, (uLong)compressed_length);
  if (status != Z_OK){
]=])
  string(FIND "${source}" "${old}" at)
  if(at LESS 0)
    message(FATAL_ERROR "Cannot locate pinned C-Blosc zlib decode call")
  endif()
  set(strict [=[
  uLong consumed = (uLong)compressed_length;
  status = uncompress2((Bytef*)output, &ul, (const Bytef*)input, &consumed);
  if (status != Z_OK || consumed != (uLong)compressed_length){
]=])
  string(REPLACE "${old}" "${strict}" source "${source}")
  file(CONFIGURE OUTPUT "${output}" CONTENT "${source}" @ONLY NEWLINE_STYLE LF)
endfunction()
