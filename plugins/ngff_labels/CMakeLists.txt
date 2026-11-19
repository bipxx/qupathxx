cmake_minimum_required(VERSION 3.24)
project(qupathxx_ngff_labels_plugin VERSION 1.0.0 LANGUAGES CXX)

set(qpx_image_target qpx_ngff_labels_plugin)
set(qpx_image_source ngff_labels.cpp)
set(qpx_image_id org.qupathxx.ngff-labels)
set(qpx_image_name "OME NGFF integral labels")
set(qpx_image_contract ngff04-zarr2-integral-labels/1)
set(qpx_image_manifest_name ngff-label-reader-plugins.json)
include("${CMAKE_CURRENT_SOURCE_DIR}/../image-common/plugin_build.cmake")

find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
set(QUPATHXX_NGFF_LABELS_JSON_INCLUDE "" CACHE PATH "Exact admitted nlohmann-json3.12.0 include directory; no downloads")
if(NOT EXISTS "${QUPATHXX_NGFF_LABELS_JSON_INCLUDE}/nlohmann/json.hpp")
  message(FATAL_ERROR "Provide the matching admitted JSON3.12.0 headers through QUPATHXX_NGFF_LABELS_JSON_INCLUDE")
endif()
target_include_directories(qpx_ngff_labels_plugin PRIVATE "${QUPATHXX_NGFF_LABELS_JSON_INCLUDE}")
target_compile_definitions(qpx_ngff_labels_plugin PRIVATE
  QPX_READER_PLUGIN_SOURCE_STORE QPX_READER_PLUGIN_LABEL_DESCRIPTION NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(qpx_ngff_labels_plugin PRIVATE qupathxx::core)

# The native decoder's exact non-system PE import graph is a module dependency,
# not an undeclared lookup through PATH. Record only graph-reachable SDK DLLs.
get_target_property(qpx_ngff_runtime_files qupathxx::core QUPATHXX_RUNTIME_FILES)
if(NOT qpx_ngff_runtime_files)
  set(qpx_ngff_runtime_files "")
endif()
if(NOT CMAKE_DUMPBIN)
  get_filename_component(qpx_ngff_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(CMAKE_DUMPBIN NAMES dumpbin.exe dumpbin HINTS "${qpx_ngff_compiler_dir}" REQUIRED)
endif()
add_custom_command(TARGET qpx_ngff_labels_plugin POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:qpx_ngff_labels_plugin>"
  "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
  "-DSDK_RUNTIME_FILES=${qpx_ngff_runtime_files}"
  "-DDUMPBIN=${CMAKE_DUMPBIN}"
  "-DOUTPUT=$<TARGET_FILE_DIR:qpx_ngff_labels_plugin>/ngff-label-reader-plugins.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/write_manifest.cmake" VERBATIM)

# Required native DLLs are selected by the import graph. Packaging owns their
# exact inclusion, notices and corresponding source; none is relicensed here.
