# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_loom_matrix_host LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64" OR
   NOT MSVC_VERSION STREQUAL "1944" OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL "19.44.35225.0")
  message(FATAL_ERROR "Use the recorded Windows x64 MSVC1944/compiler19.44.35225.0 Release /MD profile")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Typed matrix configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "The typed matrix host requires Release /MD")
endif()

# Reuse the delivered application/directory/project/label addon and its exact
# frozen core. Never make the source overlay a dependency on live old targets:
# that would rebuild qualified scientific/application inputs during this packet.
find_package(qpx_ngff_labels CONFIG REQUIRED)
get_filename_component(source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(QUPATHXX_LOOM_MATRIX_JSON_INCLUDE "${source_root}/third_party/native-overlay/json/include" CACHE PATH "Matching nlohmann-json3.12.0 headers")
set(QUPATHXX_LOOM_MATRIX_SQLITE_INCLUDE "${source_root}/third_party/native-overlay/sqlite" CACHE PATH "Matching SQLite3.54.1 header")
foreach(header "${QUPATHXX_LOOM_MATRIX_JSON_INCLUDE}/nlohmann/json.hpp" "${QUPATHXX_LOOM_MATRIX_SQLITE_INCLUDE}/sqlite3.h")
  if(NOT EXISTS "${header}")
    message(FATAL_ERROR "Provide the recorded native dependency header: ${header}")
  endif()
endforeach()

# Six affected application units. All scientific, pixel/geometry, QMX query and
# project authorities outside these units remain in exact retained libraries.
add_library(qupathxx_loom_matrix_native STATIC
  "${source_root}/cpp/format_plugin_loader.cpp"
  "${source_root}/cpp/format_plugin_runtime.cpp"
  "${source_root}/cpp/format_import.cpp"
  "${source_root}/cpp/annotation_geometry_value.cpp"
  "${source_root}/cpp/expression_matrix_metadata.cpp"
  "${source_root}/cpp/expression_matrix_writer.cpp")
add_library(qupathxx::loom_matrix ALIAS qupathxx_loom_matrix_native)
target_compile_definitions(qupathxx_loom_matrix_native PRIVATE QUPATHXX_HAS_PROJECT QUPATHXX_HAS_FORMAT_PLUGINS)
target_compile_options(qupathxx_loom_matrix_native PRIVATE /utf-8 /W4 /permissive- /bigobj)
target_include_directories(qupathxx_loom_matrix_native BEFORE PUBLIC "${source_root}/cpp/include"
  PRIVATE "${source_root}/cpp" "${QUPATHXX_LOOM_MATRIX_JSON_INCLUDE}" "${QUPATHXX_LOOM_MATRIX_SQLITE_INCLUDE}")
target_link_libraries(qupathxx_loom_matrix_native PUBLIC qupathxx::ngff_labels)

add_executable(loom_format_cli "${source_root}/cpp/format_import_main.cpp")
set_target_properties(loom_format_cli PROPERTIES OUTPUT_NAME qupathxx_format_import
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_options(loom_format_cli PRIVATE /utf-8 /W4 /permissive-)
target_include_directories(loom_format_cli PRIVATE "${source_root}/cpp" "${QUPATHXX_LOOM_MATRIX_JSON_INCLUDE}")
target_link_libraries(loom_format_cli PRIVATE qupathxx::loom_matrix)
qupathxx_stage_runtime(loom_format_cli)
