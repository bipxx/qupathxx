# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_bdv_hdf5_plugin VERSION 1.4.0 LANGUAGES CXX)

set(qpx_image_target qpx_bdv_hdf5_plugin)
set(qpx_image_source bdv_hdf5.cpp)
set(qpx_image_id org.qupathxx.bdv-hdf5)
set(qpx_image_name "BigDataViewer HDF5 UInt16")
set(qpx_image_contract bdv-hdf5-uint16/5)
set(qpx_image_manifest_name bdv-hdf5-reader-plugins.json)
set(qpx_image_source_store ON)
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(qpx_image_sdk_runtime_files qupathxx::core QUPATHXX_RUNTIME_FILES)
if(NOT CMAKE_DUMPBIN)
  get_filename_component(qpx_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(CMAKE_DUMPBIN NAMES dumpbin.exe dumpbin HINTS "${qpx_compiler_dir}" REQUIRED)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../image-common/plugin_build.cmake")

set(QUPATHXX_BDV_HDF5_INCLUDE "" CACHE PATH "Admitted HDF5 2.2.0 src headers")
set(QUPATHXX_BDV_HDF5_GENERATED_INCLUDE "" CACHE PATH "Matching SDK-generated HDF5 headers")
set(QUPATHXX_BDV_ZLIB_INCLUDE "" CACHE PATH "Matching SDK prefixed zlib headers")
set(QUPATHXX_BDV_XML_INCLUDE "" CACHE PATH "Matching SDK TinyXML2 headers")
if(NOT EXISTS "${QUPATHXX_BDV_HDF5_INCLUDE}/hdf5.h" OR
   NOT EXISTS "${QUPATHXX_BDV_HDF5_GENERATED_INCLUDE}/H5pubconf.h" OR
   NOT EXISTS "${QUPATHXX_BDV_ZLIB_INCLUDE}/zlib.h" OR
   NOT EXISTS "${QUPATHXX_BDV_XML_INCLUDE}/tinyxml2.h")
  message(FATAL_ERROR "Provide the admitted HDF5, zlib and TinyXML2 headers")
endif()

get_target_property(qpx_hdf5_core qupathxx::core IMPORTED_LOCATION_RELEASE)
get_filename_component(qpx_hdf5_libdir "${qpx_hdf5_core}" DIRECTORY)
file(SHA256 "${qpx_hdf5_libdir}/deps/qpx-hdf5.lib" qpx_hdf5_sha)
file(SHA256 "${QUPATHXX_BDV_HDF5_GENERATED_INCLUDE}/H5pubconf.h" qpx_hdf5_config_sha)
if(NOT qpx_hdf5_sha STREQUAL "ef5a6cfebd2fd9d5bf2b5f449980fcf8676cb64fc42048e952f3c9f3b272f43c" OR
   NOT qpx_hdf5_config_sha STREQUAL "e980d2826a23809a3640549fecd587868644c494b1ea6e0ddc4640eed7562f77")
  message(FATAL_ERROR "Use the exact retained FP04 SDK HDF5 library and matching configuration")
endif()

set(qpx_native "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp")
target_sources(qpx_bdv_hdf5_plugin PRIVATE
  bdv_hdf5_source.cpp
  "${CMAKE_CURRENT_SOURCE_DIR}/../bdv_n5/bdv_xml.cpp"
  "${qpx_native}/directory_inventory.cpp"
  "${qpx_native}/directory_store.cpp")
target_include_directories(qpx_bdv_hdf5_plugin PRIVATE
  "${qpx_native}/include" "${qpx_native}"
  "${QUPATHXX_BDV_HDF5_GENERATED_INCLUDE}" "${QUPATHXX_BDV_HDF5_INCLUDE}"
  "${QUPATHXX_BDV_HDF5_INCLUDE}/H5FDsubfiling"
  "${QUPATHXX_BDV_ZLIB_INCLUDE}" "${QUPATHXX_BDV_XML_INCLUDE}")
target_compile_definitions(qpx_bdv_hdf5_plugin PRIVATE
  QPX_READER_PLUGIN_SOURCE_STORE H5_BUILT_AS_STATIC_LIB NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(qpx_bdv_hdf5_plugin PRIVATE /bigobj)
target_link_libraries(qpx_bdv_hdf5_plugin PRIVATE qupathxx::core)
