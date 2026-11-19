# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_jpeg2000_reader_plugin VERSION 1.13.0 LANGUAGES C CXX)

set(qpx_image_target qpx_jpeg2000_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/jpeg2000_reader.cpp")
set(qpx_image_id org.qupathxx.jpeg2000)
set(qpx_image_name "JPEG 2000")
set(qpx_image_contract jpeg2000-native-samples/2)
include("../image-common/plugin_build.cmake")
target_compile_definitions(qpx_jpeg2000_plugin PRIVATE QPX_JPEG2000_VERSION="${PROJECT_VERSION}")

if(NOT CMAKE_C_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the C compiler recorded by the ReaderPluginKit")
endif()
set(QUPATHXX_JPEG2000_JASPER_SOURCE "" CACHE PATH "Exact local JasPer 4.2.9 source subset")
if(NOT IS_DIRECTORY "${QUPATHXX_JPEG2000_JASPER_SOURCE}")
  message(FATAL_ERROR "Provide the pinned JasPer source subset. No dependency downloads are enabled.")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/jasper-sources.sha256" source_records)
set(expected_files "")
foreach(record IN LISTS source_records)
  if(NOT record MATCHES "^([a-f0-9]+)  ([A-Za-z0-9_./-]+)$")
    message(FATAL_ERROR "Malformed JasPer source lock")
  endif()
  set(expected_hash "${CMAKE_MATCH_1}")
  set(relative_path "${CMAKE_MATCH_2}")
  file(SHA256 "${QUPATHXX_JPEG2000_JASPER_SOURCE}/${relative_path}" source_hash)
  if(NOT source_hash STREQUAL expected_hash)
    message(FATAL_ERROR "JasPer source lock mismatch for ${relative_path}")
  endif()
  list(APPEND expected_files "${relative_path}")
endforeach()
file(GLOB_RECURSE actual_files LIST_DIRECTORIES false
  RELATIVE "${QUPATHXX_JPEG2000_JASPER_SOURCE}" "${QUPATHXX_JPEG2000_JASPER_SOURCE}/*")
list(SORT expected_files)
list(SORT actual_files)
if(NOT actual_files STREQUAL expected_files)
  message(FATAL_ERROR "JasPer source subset membership differs from the lock")
endif()

foreach(option IN ITEMS
    JAS_ENABLE_SHARED JAS_ENABLE_LIBJPEG JAS_ENABLE_LIBHEIF JAS_ENABLE_OPENGL
    JAS_ENABLE_DOC JAS_ENABLE_PROGRAMS JAS_ENABLE_CXX JAS_ENABLE_MULTITHREADING_SUPPORT
    JAS_ENABLE_CMAKE_PACKAGE_CONFIG JAS_ENABLE_CONFORMANCE_TESTS JAS_PACKAGING
    JAS_INCLUDE_BMP_CODEC JAS_INCLUDE_JP2_CODEC JAS_INCLUDE_JPG_CODEC JAS_INCLUDE_HEIC_CODEC
    JAS_INCLUDE_MIF_CODEC JAS_INCLUDE_PGX_CODEC JAS_INCLUDE_PNM_CODEC JAS_INCLUDE_RAS_CODEC
    BUILD_TESTING)
  set(${option} OFF CACHE BOOL "Disabled in the private JPEG 2000 build" FORCE)
endforeach()
set(JAS_INCLUDE_JPC_CODEC ON CACHE BOOL "Private JPEG 2000 codestream codec" FORCE)
add_subdirectory("${QUPATHXX_JPEG2000_JASPER_SOURCE}" jasper EXCLUDE_FROM_ALL)

# Require the tile's packet iterator to be exhausted at its final part,
# even if the next marker is present. Other parts resume the same iterator.
# Keep the verified upstream source intact for corresponding-source delivery.
set(jpc_source_dir "${QUPATHXX_JPEG2000_JASPER_SOURCE}/src/libjasper/jpc")
set(packet_source "${jpc_source_dir}/jpc_t2dec.c")
file(READ "${packet_source}" packet_code)
set(packet_boundary "\t\t\tcase JPC_MS_EOC:\n\t\t\tcase JPC_MS_SOT:\n\t\t\t\treturn 0;")
string(FIND "${packet_code}" "${packet_boundary}" boundary_offset)
if(boundary_offset LESS 0)
  message(FATAL_ERROR "The private JasPer packet completeness patch cannot be applied")
endif()
string(REPLACE "${packet_boundary}"
  "\t\t\tcase JPC_MS_EOC:\n\t\t\tcase JPC_MS_SOT:\n\t\t\t\treturn tile->partno + 1 < tile->numparts || jpc_pi_next(pi) == 1 ? 0 : -1;"
  packet_code "${packet_code}")
set(patched_packets "${CMAKE_CURRENT_BINARY_DIR}/jpc_t2dec_complete.c")
file(CONFIGURE OUTPUT "${patched_packets}" CONTENT "@packet_code@" @ONLY)
set_property(SOURCE "${packet_source}" TARGET_DIRECTORY libjasper PROPERTY HEADER_FILE_ONLY TRUE)
target_sources(libjasper PRIVATE "${patched_packets}")
target_include_directories(libjasper PRIVATE "${jpc_source_dir}")

target_include_directories(qpx_jpeg2000_plugin SYSTEM PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/jasper/src/libjasper/include"
  "${QUPATHXX_JPEG2000_JASPER_SOURCE}/src/libjasper/include")
target_link_libraries(qpx_jpeg2000_plugin PRIVATE libjasper)
install(FILES "${QUPATHXX_JPEG2000_JASPER_SOURCE}/LICENSE.txt"
              "${QUPATHXX_JPEG2000_JASPER_SOURCE}/COPYRIGHT.txt"
        DESTINATION licenses/jasper)

option(QUPATHXX_JPEG2000_BUILD_TESTS "Build the independent JPEG 2000 ABI check" OFF)
if(QUPATHXX_JPEG2000_BUILD_TESTS)
  add_executable(jpeg2000_abi_test tests/jpeg2000_abi_test.cpp)
  target_include_directories(jpeg2000_abi_test PRIVATE "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include")
  target_compile_options(jpeg2000_abi_test PRIVATE /utf-8 /W4 /permissive-)
endif()
