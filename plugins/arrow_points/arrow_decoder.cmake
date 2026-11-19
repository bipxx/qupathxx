# SPDX-License-Identifier: AGPL-3.0-only
set(QUPATHXX_NANOARROW_ARCHIVE "" CACHE FILEPATH "Apache Arrow nanoarrow 0.9.0 pinned library source ZIP")
set(QUPATHXX_ARROW_LZ4_ARCHIVE "" CACHE FILEPATH "LZ4 v1.10.0 pinned library source ZIP")
set(QUPATHXX_ARROW_ZSTD_ARCHIVE "" CACHE FILEPATH "Admitted Zstandard 1.5.7 source archive")
function(extract_pinned archive expected)
  file(SHA256 "${archive}" digest)
  if(NOT digest STREQUAL expected)
    message(FATAL_ERROR "Use the exact pinned archive: ${archive}")
  endif()
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/libraries" ${ARGN})
endfunction()
extract_pinned("${QUPATHXX_NANOARROW_ARCHIVE}"
  6935c5b84e0906b1843b5c93eb20ffab6aab9c06311ebb3a9a5b97fbb151d60f)
extract_pinned("${QUPATHXX_ARROW_LZ4_ARCHIVE}"
  e9fb04130b7fc33727fa9e8e747ed067a8a0e7bef4b3769d689e2953a245d9c0)
extract_pinned("${QUPATHXX_ARROW_ZSTD_ARCHIVE}"
  eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
  PATTERNS "zstd-1.5.7/LICENSE" "zstd-1.5.7/lib/*")

set(arrow "${CMAKE_CURRENT_BINARY_DIR}/libraries/apache-arrow-nanoarrow-0.9.0")
set(nano "${arrow}/src/nanoarrow")
set(flat "${arrow}/thirdparty/flatcc")
set(lz4 "${CMAKE_CURRENT_BINARY_DIR}/libraries/lz4-1.10.0/lib")
set(zstd_root "${CMAKE_CURRENT_BINARY_DIR}/libraries/zstd-1.5.7")
set(zstd "${zstd_root}/lib")
set(NANOARROW_VERSION 0.9.0)
set(NANOARROW_VERSION_MAJOR 0)
set(NANOARROW_VERSION_MINOR 9)
set(NANOARROW_VERSION_PATCH 0)
set(NANOARROW_NAMESPACE_DEFINE "#define NANOARROW_NAMESPACE QpxArrow")
configure_file("${nano}/nanoarrow_config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/nanoarrow/nanoarrow_config.h" @ONLY)

include("${CMAKE_CURRENT_LIST_DIR}/nanoarrow_dictionary.cmake")
add_library(qpx_arrow_points_decoder STATIC
  "${nano}/common/array.c" "${nano}/common/array_stream.c" "${nano}/common/schema.c" "${nano}/common/utils.c"
  "${CMAKE_CURRENT_BINARY_DIR}/patched-decoder.c" "${CMAKE_CURRENT_BINARY_DIR}/patched-codecs.c" "${nano}/ipc/encoder.c"
  "${flat}/src/runtime/verifier.c" "${flat}/src/runtime/builder.c"
  "${flat}/src/runtime/emitter.c" "${flat}/src/runtime/refmap.c"
  "${lz4}/lz4.c" "${lz4}/lz4hc.c" "${lz4}/lz4frame.c" "${lz4}/xxhash.c")
target_include_directories(qpx_arrow_points_decoder SYSTEM PUBLIC "${arrow}/src"
  "${CMAKE_CURRENT_BINARY_DIR}" "${lz4}" PRIVATE "${flat}/include" "${nano}/ipc")
target_include_directories(qpx_arrow_points_decoder PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(qpx_arrow_points_decoder PRIVATE NANOARROW_IPC_WITH_LZ4 NANOARROW_IPC_WITH_ZSTD)
# These replacements apply only to the compiled dependency sources.
target_compile_options(qpx_arrow_points_decoder PRIVATE
  "/FI${CMAKE_CURRENT_LIST_DIR}/decoder_allocators.h")
add_library(qpx_arrow_points_zstd STATIC
  "${zstd}/common/debug.c" "${zstd}/common/entropy_common.c"
  "${zstd}/common/error_private.c" "${zstd}/common/fse_decompress.c"
  "${zstd}/common/xxhash.c" "${zstd}/common/zstd_common.c"
  "${zstd}/decompress/huf_decompress.c" "${zstd}/decompress/zstd_ddict.c"
  "${zstd}/decompress/zstd_decompress_block.c" "${zstd}/decompress/zstd_decompress.c")
target_include_directories(qpx_arrow_points_zstd SYSTEM PUBLIC "${zstd}")
target_compile_definitions(qpx_arrow_points_zstd PRIVATE ZSTD_LEGACY_SUPPORT=0 ZSTD_DISABLE_ASM ZSTD_HEAPMODE=1)
target_compile_options(qpx_arrow_points_zstd PRIVATE "/FI${CMAKE_CURRENT_LIST_DIR}/decoder_allocators.h")
target_link_libraries(qpx_arrow_points_decoder PRIVATE qpx_arrow_points_zstd)
target_sources(${qpx_provider_target} PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/decoder_memory.c" "${CMAKE_CURRENT_LIST_DIR}/ipc_bounds.c"
  "${CMAKE_CURRENT_LIST_DIR}/arrow_file.cpp")
target_include_directories(${qpx_provider_target} SYSTEM PRIVATE "${flat}/include")
target_link_libraries(${qpx_provider_target} PRIVATE qpx_arrow_points_decoder)
install(FILES "${arrow}/LICENSE.txt" "${arrow}/NOTICE.txt" DESTINATION licenses/nanoarrow)
install(FILES "${flat}/LICENSE" DESTINATION licenses/flatcc)
install(FILES "${lz4}/LICENSE" DESTINATION licenses/lz4)
install(FILES "${zstd_root}/LICENSE" DESTINATION licenses/zstd)
