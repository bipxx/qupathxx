# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(jpegxl_project_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
find_package(qupathxx CONFIG REQUIRED COMPONENTS core)
add_executable(jpegxl_project_probe project_probe.cpp)
target_link_libraries(jpegxl_project_probe PRIVATE qupathxx::core)
qupathxx_stage_runtime(jpegxl_project_probe)

set(QUPATHXX_JPEGXL_TEST_DECODER_CACHE "" CACHE PATH "Optional matching decoder build for direct container reference")
if(QUPATHXX_JPEGXL_TEST_DECODER_CACHE)
  set(decoder "${QUPATHXX_JPEGXL_TEST_DECODER_CACHE}/jpegxl")
  add_executable(jpegxl_fragment_reference fragment_reference.cpp)
  target_compile_definitions(jpegxl_fragment_reference PRIVATE JXL_STATIC_DEFINE)
  target_include_directories(jpegxl_fragment_reference PRIVATE
    "${QUPATHXX_JPEGXL_TEST_DECODER_CACHE}/codec-inputs/libjxl-0.13.0/lib/include"
    "${decoder}/lib/include")
  target_link_libraries(jpegxl_fragment_reference PRIVATE
    "${decoder}/lib/Release/jxl_dec-internal.lib" "${decoder}/third_party/highway/Release/hwy.lib")
endif()
