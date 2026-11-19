# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_ngff05_plugin VERSION 1.17.0 LANGUAGES C CXX)

set(qpx_image_target qpx_ngff05_plugin)
set(qpx_image_source ngff05.cpp)
set(qpx_image_id org.qupathxx.ngff05)
set(qpx_image_name "OME-Zarr NGFF 0.5")
set(qpx_image_contract ngff05-zarr3-image/18)
set(qpx_image_manifest_name ngff05-reader-plugins.json)
set(qpx_image_source_store ON)
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(qpx_image_sdk_runtime_files qupathxx::core QUPATHXX_RUNTIME_FILES)
if(NOT CMAKE_DUMPBIN)
  get_filename_component(qpx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(CMAKE_DUMPBIN NAMES dumpbin.exe dumpbin HINTS "${qpx_compiler_dir}" REQUIRED)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../image-common/plugin_build.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Zstd.cmake")

set(QUPATHXX_NGFF05_JSON_INCLUDE "" CACHE PATH "Admitted nlohmann-json include directory")
set(QUPATHXX_NGFF05_ZLIB_INCLUDE "" CACHE PATH "Matching SDK prefixed zlib headers")
if(NOT EXISTS "${QUPATHXX_NGFF05_JSON_INCLUDE}/nlohmann/json.hpp" OR
   NOT EXISTS "${QUPATHXX_NGFF05_ZLIB_INCLUDE}/zlib.h")
  message(FATAL_ERROR "Provide the admitted JSON headers and matching SDK zlib headers")
endif()

set(qpx_native "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp")
target_sources(qpx_ngff05_plugin PRIVATE
  blosc_zstd.cpp
  "${qpx_native}/directory_inventory.cpp"
  "${qpx_native}/ome_zarr.cpp"
  "${qpx_native}/ome_zarr_metadata.cpp"
  "${qpx_native}/ome_zarr_server.cpp"
  "${qpx_native}/ome_zarr_store.cpp")
target_include_directories(qpx_ngff05_plugin PRIVATE
  "${qpx_native}/include" "${qpx_native}" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${QUPATHXX_NGFF05_JSON_INCLUDE}" "${QUPATHXX_NGFF05_ZLIB_INCLUDE}")
target_compile_definitions(qpx_ngff05_plugin PRIVATE
  QUPATHXX_HAS_OME_ZARR QUPATHXX_HAS_NGFF05 QPX_READER_PLUGIN_SOURCE_STORE
  QPX_NGFF05_VERSION="${PROJECT_VERSION}" QPX_NGFF05_CONTRACT="${qpx_image_contract}")
target_compile_options(qpx_ngff05_plugin PRIVATE /bigobj)
target_link_libraries(qpx_ngff05_plugin PRIVATE qupathxx::core ngff05_zstd)

include("${CMAKE_CURRENT_SOURCE_DIR}/Snappy.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/Blosc.cmake")
