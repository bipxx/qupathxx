# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_anndata_zarr_host LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64" OR
   NOT MSVC_VERSION STREQUAL "1944" OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL "19.44.35225.0")
  message(FATAL_ERROR "Use Windows x64 MSVC1944/compiler19.44.35225.0 Release /MD")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Native store configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "The native matrix store host requires Release /MD")
endif()
find_package(qpx_ngff_labels CONFIG REQUIRED)
get_filename_component(source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(QUPATHXX_ANNDATA_ZARR_MATRIX_LIBRARY "${source_root}/build/gui-aggregation/catalogue-closure/loom/package-final/native/qupathxx_loom_matrix_native.lib" CACHE FILEPATH "Exact retained M05 application archive")
file(SHA256 "${QUPATHXX_ANNDATA_ZARR_MATRIX_LIBRARY}" retained_matrix_sha)
if(NOT retained_matrix_sha STREQUAL "42f41eb92220038324424435a0963b93c4e16ad3420596d603658d6623176fb0")
  message(FATAL_ERROR "Use the exact retained M05 archive; never a live old build target")
endif()
add_library(qpx_anndata_retained_matrix STATIC IMPORTED)
set_target_properties(qpx_anndata_retained_matrix PROPERTIES
  IMPORTED_CONFIGURATIONS RELEASE
  IMPORTED_LOCATION_RELEASE "${QUPATHXX_ANNDATA_ZARR_MATRIX_LIBRARY}"
  INTERFACE_LINK_LIBRARIES "qupathxx::ngff_labels")
set(QUPATHXX_ANNDATA_ZARR_JSON_INCLUDE "${source_root}/third_party/native-overlay/json/include" CACHE PATH "Matching JSON3.12 headers")
set(QUPATHXX_ANNDATA_ZARR_SQLITE_INCLUDE "${source_root}/third_party/native-overlay/sqlite" CACHE PATH "Matching SQLite3.54.1 header")
foreach(header "${QUPATHXX_ANNDATA_ZARR_JSON_INCLUDE}/nlohmann/json.hpp" "${QUPATHXX_ANNDATA_ZARR_SQLITE_INCLUDE}/sqlite3.h")
  if(NOT EXISTS "${header}")
    message(FATAL_ERROR "Provide the exact retained native interface: ${header}")
  endif()
endforeach()
add_library(qupathxx_anndata_zarr_native STATIC
  "${source_root}/cpp/directory_store.cpp"
  "${source_root}/cpp/format_plugin_loader.cpp"
  "${source_root}/cpp/format_plugin_runtime.cpp"
  "${source_root}/cpp/format_import.cpp"
  "${source_root}/cpp/expression_matrix_attach.cpp"
  "${source_root}/cpp/expression_matrix_metadata.cpp"
  "${source_root}/cpp/expression_matrix_writer.cpp")
add_library(qupathxx::anndata_zarr ALIAS qupathxx_anndata_zarr_native)
target_include_directories(qupathxx_anndata_zarr_native BEFORE PUBLIC "${source_root}/cpp/include"
  PRIVATE "${source_root}/cpp" "${QUPATHXX_ANNDATA_ZARR_JSON_INCLUDE}" "${QUPATHXX_ANNDATA_ZARR_SQLITE_INCLUDE}")
target_compile_definitions(qupathxx_anndata_zarr_native PRIVATE QUPATHXX_HAS_PROJECT QUPATHXX_HAS_FORMAT_PLUGINS)
target_compile_options(qupathxx_anndata_zarr_native PRIVATE /utf-8 /W4 /permissive- /bigobj)
target_link_libraries(qupathxx_anndata_zarr_native PUBLIC qpx_anndata_retained_matrix)
add_executable(anndata_format_cli "${source_root}/cpp/format_import_main.cpp")
set_target_properties(anndata_format_cli PROPERTIES OUTPUT_NAME qupathxx_format_import)
target_include_directories(anndata_format_cli PRIVATE "${source_root}/cpp" "${QUPATHXX_ANNDATA_ZARR_JSON_INCLUDE}")
target_compile_options(anndata_format_cli PRIVATE /utf-8 /W4 /permissive- /bigobj)
target_link_libraries(anndata_format_cli PRIVATE qupathxx::anndata_zarr)
qupathxx_stage_runtime(anndata_format_cli)
