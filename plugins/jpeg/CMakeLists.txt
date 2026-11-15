cmake_minimum_required(VERSION 3.24)
project(qupathxx_jpeg_reader_plugin VERSION 1.0.0 LANGUAGES C CXX)

set(QUPATHXX_READER_PLUGIN_KIT_PREFIX "" CACHE PATH "Relocated ReaderPluginKit prefix")
set(QUPATHXX_JPEG_SOURCE_DIR "" CACHE PATH "Verified libjpeg-turbo3.1.3 code selection")
option(QUPATHXX_JPEG_BUILD_TESTS "Build independent ABI and installed-SDK checks" OFF)
foreach(name QUPATHXX_READER_PLUGIN_KIT_PREFIX QUPATHXX_JPEG_SOURCE_DIR)
  if(NOT IS_DIRECTORY "${${name}}")
    message(FATAL_ERROR "Set ${name} to an existing admitted input directory; this build never downloads")
  endif()
endforeach()
set(qpx_kit "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/share/qupathxx/reader-plugin")
include("${qpx_kit}/profile.cmake")
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "This plugin requires Windows x64 MSVC")
endif()
if(NOT MSVC_VERSION STREQUAL QUPATHXX_READER_PLUGIN_MSVC_VERSION OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION OR
   NOT CMAKE_C_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the C/C++ compiler recorded by the ReaderPluginKit")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "This plugin requires CMAKE_BUILD_TYPE=Release")
endif()
if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
  message(FATAL_ERROR "This plugin requires the Release dynamic CRT (/MD)")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/libjpeg-source-lock.json" jpeg_lock)
string(JSON jpeg_version GET "${jpeg_lock}" version)
string(JSON jpeg_archive GET "${jpeg_lock}" archive_sha256)
if(NOT jpeg_version STREQUAL "3.1.3" OR NOT jpeg_archive STREQUAL
   "3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5")
  message(FATAL_ERROR "Unexpected libjpeg-turbo source identity")
endif()
string(JSON jpeg_count LENGTH "${jpeg_lock}" members)
math(EXPR jpeg_last "${jpeg_count}-1")
foreach(index RANGE 0 ${jpeg_last})
  string(JSON member MEMBER "${jpeg_lock}" members ${index})
  string(JSON expected GET "${jpeg_lock}" members "${member}" sha256)
  if(NOT EXISTS "${QUPATHXX_JPEG_SOURCE_DIR}/${member}")
    message(FATAL_ERROR "Missing libjpeg-turbo source: ${member}")
  endif()
  file(SHA256 "${QUPATHXX_JPEG_SOURCE_DIR}/${member}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Changed libjpeg-turbo source: ${member}")
  endif()
endforeach()

# Upstream explicitly requires a standalone build, not add_subdirectory().
# No SIMD assembler, tools, tests, assets, Java, network or shared runtime codec.
include(ExternalProject)
set(jpeg_binary "${CMAKE_CURRENT_BINARY_DIR}/codec")
if(CMAKE_CONFIGURATION_TYPES)
  set(jpeg_library "${jpeg_binary}/Release/jpeg-static.lib")
else()
  set(jpeg_library "${jpeg_binary}/jpeg-static.lib")
endif()
ExternalProject_Add(qpx_jpeg_codec_build
  SOURCE_DIR "${QUPATHXX_JPEG_SOURCE_DIR}" BINARY_DIR "${jpeg_binary}"
  DOWNLOAD_COMMAND "" UPDATE_COMMAND "" INSTALL_COMMAND ""
  CMAKE_ARGS
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"
    "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_CONFIGURATION_TYPES=Release"
    "-DENABLE_SHARED=OFF" "-DENABLE_STATIC=ON" "-DWITH_SIMD=OFF"
    "-DWITH_CRT_DLL=ON" "-DWITH_JPEG7=OFF" "-DWITH_JPEG8=OFF"
    "-DWITH_TURBOJPEG=OFF" "-DWITH_TOOLS=OFF" "-DWITH_TESTS=OFF"
    "-DWITH_JAVA=OFF" "-DWITH_FUZZ=OFF"
    "-DWITH_ARITH_ENC=OFF" "-DWITH_ARITH_DEC=OFF" "-DBUILD=20261114"
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config Release --target jpeg-static --parallel 4
  BUILD_BYPRODUCTS "${jpeg_library}")
add_library(qpx_jpeg_codec STATIC IMPORTED)
set_target_properties(qpx_jpeg_codec PROPERTIES IMPORTED_LOCATION "${jpeg_library}")
add_dependencies(qpx_jpeg_codec qpx_jpeg_codec_build)

configure_file(reader_plugin_profile.hpp.in reader_plugin_profile.hpp @ONLY)
add_library(qpx_jpeg_plugin MODULE jpeg_reader.cpp jpeg_decoder.c jpeg_memory_backend.c)
set_target_properties(qpx_jpeg_plugin PROPERTIES PREFIX "")
target_compile_definitions(qpx_jpeg_plugin PRIVATE QPX_READER_PLUGIN_EXPORTS _CRT_SECURE_NO_WARNINGS)
target_compile_options(qpx_jpeg_plugin PRIVATE /utf-8 /W4 $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
target_include_directories(qpx_jpeg_plugin PRIVATE
  "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include" "${CMAKE_CURRENT_BINARY_DIR}"
  "${QUPATHXX_JPEG_SOURCE_DIR}/src" "${jpeg_binary}")
target_link_libraries(qpx_jpeg_plugin PRIVATE qpx_jpeg_codec)
add_custom_command(TARGET qpx_jpeg_plugin POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:qpx_jpeg_plugin>"
  "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
  "-DOUTPUT=$<TARGET_FILE_DIR:qpx_jpeg_plugin>/reader-plugins.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/write_manifest.cmake" VERBATIM)
install(TARGETS qpx_jpeg_plugin RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:qpx_jpeg_plugin>/reader-plugins.json" DESTINATION . CONFIGURATIONS Release)
install(FILES "${QUPATHXX_JPEG_SOURCE_DIR}/LICENSE.md" "${QUPATHXX_JPEG_SOURCE_DIR}/README.ijg"
  DESTINATION licenses/libjpeg-turbo)
install(FILES "${qpx_kit}/LICENSE" DESTINATION licenses/qupathxx)

if(QUPATHXX_JPEG_BUILD_TESTS)
  add_executable(jpeg_plugin_tests tests/jpeg_plugin_tests.cpp)
  target_include_directories(jpeg_plugin_tests PRIVATE "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include")
  target_compile_options(jpeg_plugin_tests PRIVATE /utf-8 /permissive-)
  find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
  add_executable(jpeg_project_consumer tests/jpeg_project_consumer.cpp)
  target_compile_options(jpeg_project_consumer PRIVATE /utf-8 /permissive-)
  target_link_libraries(jpeg_project_consumer PRIVATE qupathxx::core)
  qupathxx_stage_runtime(jpeg_project_consumer)
endif()
