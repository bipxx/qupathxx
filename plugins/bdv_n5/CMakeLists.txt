# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_bdv_n5_plugin VERSION 1.3.0 LANGUAGES CXX)

set(qpx_image_target qpx_bdv_n5_plugin)
set(qpx_image_source bdv_n5.cpp)
set(qpx_image_id org.qupathxx.bdv-n5)
set(qpx_image_name "BigDataViewer N5 UInt16")
set(qpx_image_contract bdv-n5-uint16/4)
set(qpx_image_manifest_name bdv-n5-reader-plugins.json)
set(qpx_image_source_store ON)
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(qpx_image_sdk_runtime_files qupathxx::core QUPATHXX_RUNTIME_FILES)
if(NOT CMAKE_DUMPBIN)
  get_filename_component(qpx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(CMAKE_DUMPBIN NAMES dumpbin.exe dumpbin HINTS "${qpx_compiler_dir}" REQUIRED)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../image-common/plugin_build.cmake")

set(QUPATHXX_BDV_JSON_INCLUDE "" CACHE PATH "Admitted nlohmann-json include directory")
set(QUPATHXX_BDV_ZLIB_INCLUDE "" CACHE PATH "Matching SDK prefixed zlib headers")
set(QUPATHXX_BDV_XML_INCLUDE "" CACHE PATH "Matching SDK TinyXML2 headers")
if(NOT EXISTS "${QUPATHXX_BDV_JSON_INCLUDE}/nlohmann/json.hpp" OR
   NOT EXISTS "${QUPATHXX_BDV_ZLIB_INCLUDE}/zlib.h" OR
   NOT EXISTS "${QUPATHXX_BDV_XML_INCLUDE}/tinyxml2.h")
  message(FATAL_ERROR "Provide the admitted JSON headers and exact SDK zlib and TinyXML2 headers")
endif()

set(qpx_native "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp")
target_sources(qpx_bdv_n5_plugin PRIVATE
  bdv_xml.cpp
  "${CMAKE_CURRENT_SOURCE_DIR}/../n5/n5_source.cpp"
  "${qpx_native}/directory_inventory.cpp"
  "${qpx_native}/directory_store.cpp")
target_include_directories(qpx_bdv_n5_plugin PRIVATE
  "${qpx_native}/include" "${qpx_native}" "${QUPATHXX_BDV_JSON_INCLUDE}"
  "${QUPATHXX_BDV_ZLIB_INCLUDE}" "${QUPATHXX_BDV_XML_INCLUDE}")
target_compile_definitions(qpx_bdv_n5_plugin PRIVATE QPX_READER_PLUGIN_SOURCE_STORE QUPATHXX_HAS_BDV_N5)
target_compile_options(qpx_bdv_n5_plugin PRIVATE /bigobj)
target_link_libraries(qpx_bdv_n5_plugin PRIVATE qupathxx::core)
