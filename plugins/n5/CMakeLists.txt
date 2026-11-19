# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_n5_plugin VERSION 1.0.0 LANGUAGES CXX)

set(qpx_image_target qpx_n5_plugin)
set(qpx_image_source n5.cpp)
set(qpx_image_id org.qupathxx.n5)
set(qpx_image_name "N5 TensorStore UInt16")
set(qpx_image_contract n5-tensorstore-uint16/1)
set(qpx_image_manifest_name n5-reader-plugins.json)
set(qpx_image_source_store ON)
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(qpx_image_sdk_runtime_files qupathxx::core QUPATHXX_RUNTIME_FILES)
if(NOT CMAKE_DUMPBIN)
  get_filename_component(qpx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(CMAKE_DUMPBIN NAMES dumpbin.exe dumpbin HINTS "${qpx_compiler_dir}" REQUIRED)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../image-common/plugin_build.cmake")

set(QUPATHXX_N5_JSON_INCLUDE "" CACHE PATH "Admitted nlohmann-json include directory")
set(QUPATHXX_N5_ZLIB_INCLUDE "" CACHE PATH "Matching SDK prefixed zlib headers")
if(NOT EXISTS "${QUPATHXX_N5_JSON_INCLUDE}/nlohmann/json.hpp" OR
   NOT EXISTS "${QUPATHXX_N5_ZLIB_INCLUDE}/zlib.h")
  message(FATAL_ERROR "Provide the admitted JSON headers and matching SDK zlib headers")
endif()

set(qpx_native "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp")
target_sources(qpx_n5_plugin PRIVATE
  n5_source.cpp
  "${qpx_native}/directory_inventory.cpp"
  "${qpx_native}/directory_store.cpp")
target_include_directories(qpx_n5_plugin PRIVATE
  "${qpx_native}/include" "${qpx_native}" "${QUPATHXX_N5_JSON_INCLUDE}" "${QUPATHXX_N5_ZLIB_INCLUDE}")
target_compile_definitions(qpx_n5_plugin PRIVATE QPX_READER_PLUGIN_SOURCE_STORE)
target_compile_options(qpx_n5_plugin PRIVATE /bigobj)
target_link_libraries(qpx_n5_plugin PRIVATE qupathxx::core)
