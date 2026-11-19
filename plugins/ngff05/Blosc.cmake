# SPDX-License-Identifier: AGPL-3.0-only
set(QUPATHXX_NGFF05_BLOSC_ARCHIVE "" CACHE FILEPATH "Admitted C-Blosc 1.21.6 source archive")
set(qpx_blosc_lock_dir "${CMAKE_CURRENT_LIST_DIR}/../blosc-bytes")
file(READ "${qpx_blosc_lock_dir}/source-lock.json" qpx_blosc_lock)
string(JSON expected GET "${qpx_blosc_lock}" archive_sha256)
file(SHA256 "${QUPATHXX_NGFF05_BLOSC_ARCHIVE}" actual)
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "C-Blosc archive differs from its admitted source lock")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUPATHXX_NGFF05_BLOSC_ARCHIVE}")
set(qpx_blosc_inputs "${CMAKE_CURRENT_BINARY_DIR}/blosc-inputs")
file(ARCHIVE_EXTRACT INPUT "${QUPATHXX_NGFF05_BLOSC_ARCHIVE}" DESTINATION "${qpx_blosc_inputs}")
set(qpx_blosc_root "${qpx_blosc_inputs}/c-blosc-1.21.6")
string(JSON count LENGTH "${qpx_blosc_lock}" members)
math(EXPR last "${count}-1")
foreach(i RANGE 0 ${last})
  string(JSON member MEMBER "${qpx_blosc_lock}" members ${i})
  string(JSON expected GET "${qpx_blosc_lock}" members "${member}")
  file(SHA256 "${qpx_blosc_root}/${member}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "C-Blosc input differs from the source lock: ${member}")
  endif()
endforeach()
set(qpx_blosc_dir "${qpx_blosc_root}/blosc")
set(qpx_lz4_dir "${qpx_blosc_root}/internal-complibs/lz4-1.9.4")
include("${CMAKE_CURRENT_LIST_DIR}/BloscDecoder.cmake")
qpx_prepare_blosc_decoder("${qpx_blosc_dir}/blosc.c" "${CMAKE_CURRENT_BINARY_DIR}/ngff05_blosc.c")
add_library(ngff05_blosc STATIC
  "${CMAKE_CURRENT_BINARY_DIR}/ngff05_blosc.c" "${qpx_blosc_dir}/blosclz.c"
  "${qpx_blosc_dir}/fastcopy.c" "${qpx_blosc_dir}/shuffle-generic.c"
  "${qpx_blosc_dir}/bitshuffle-generic.c" "${qpx_blosc_dir}/shuffle.c"
  "${qpx_lz4_dir}/lz4.c" "${qpx_lz4_dir}/lz4hc.c")
target_include_directories(ngff05_blosc PUBLIC "${qpx_blosc_dir}"
  PRIVATE "${qpx_lz4_dir}" "${QUPATHXX_NGFF05_ZLIB_INCLUDE}")
target_compile_definitions(ngff05_blosc PRIVATE HAVE_LZ4 HAVE_ZSTD HAVE_ZLIB HAVE_SNAPPY _CRT_SECURE_NO_WARNINGS)
target_link_libraries(ngff05_blosc PRIVATE ngff05_zstd qupathxx::_zlib snappy)
target_link_libraries(qpx_ngff05_plugin PRIVATE ngff05_blosc)
install(FILES "${qpx_blosc_lock_dir}/source-lock.json" "${qpx_blosc_lock_dir}/THIRD_PARTY_NOTICES.txt"
  DESTINATION licenses/blosc)
