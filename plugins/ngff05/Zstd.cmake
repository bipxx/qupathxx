# SPDX-License-Identifier: AGPL-3.0-only
# The unmodified upstream decoder selects the BSD-3-Clause option.
set(QUPATHXX_NGFF05_ZSTD_ARCHIVE "" CACHE FILEPATH "Verified upstream zstd-1.5.7.tar.gz")
set(qpx_ngff05_zstd_sha256 "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3")
if(NOT EXISTS "${QUPATHXX_NGFF05_ZSTD_ARCHIVE}" OR IS_DIRECTORY "${QUPATHXX_NGFF05_ZSTD_ARCHIVE}")
  message(FATAL_ERROR "Set QUPATHXX_NGFF05_ZSTD_ARCHIVE to the pinned upstream archive")
endif()
file(SHA256 "${QUPATHXX_NGFF05_ZSTD_ARCHIVE}" qpx_ngff05_zstd_actual_sha256)
if(NOT qpx_ngff05_zstd_actual_sha256 STREQUAL qpx_ngff05_zstd_sha256)
  message(FATAL_ERROR "Zstandard 1.5.7 archive SHA256 differs from the upstream release")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUPATHXX_NGFF05_ZSTD_ARCHIVE}")

set(qpx_ngff05_zstd_inputs "${CMAKE_CURRENT_BINARY_DIR}/zstd-inputs")
file(ARCHIVE_EXTRACT INPUT "${QUPATHXX_NGFF05_ZSTD_ARCHIVE}" DESTINATION "${qpx_ngff05_zstd_inputs}"
  PATTERNS "zstd-1.5.7/LICENSE" "zstd-1.5.7/lib/*")
set(qpx_ngff05_zstd_source "${qpx_ngff05_zstd_inputs}/zstd-1.5.7")
set(qpx_ngff05_zstd_lib "${qpx_ngff05_zstd_source}/lib")

add_library(ngff05_zstd STATIC
  "${qpx_ngff05_zstd_lib}/common/debug.c"
  "${qpx_ngff05_zstd_lib}/common/entropy_common.c"
  "${qpx_ngff05_zstd_lib}/common/error_private.c"
  "${qpx_ngff05_zstd_lib}/common/fse_decompress.c"
  "${qpx_ngff05_zstd_lib}/common/xxhash.c"
  "${qpx_ngff05_zstd_lib}/common/zstd_common.c"
  "${qpx_ngff05_zstd_lib}/decompress/huf_decompress.c"
  "${qpx_ngff05_zstd_lib}/decompress/zstd_ddict.c"
  "${qpx_ngff05_zstd_lib}/decompress/zstd_decompress_block.c"
  "${qpx_ngff05_zstd_lib}/decompress/zstd_decompress.c")
target_include_directories(ngff05_zstd SYSTEM PUBLIC "${qpx_ngff05_zstd_lib}")
target_compile_definitions(ngff05_zstd PRIVATE ZSTD_LEGACY_SUPPORT=0 ZSTD_DISABLE_ASM)
install(FILES "${qpx_ngff05_zstd_source}/LICENSE" DESTINATION licenses/zstd)
