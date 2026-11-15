cmake_minimum_required(VERSION 3.24)
project(qupathxx_bmp_reader_plugin VERSION 1.0.0 LANGUAGES CXX)

set(QUPATHXX_READER_PLUGIN_KIT_PREFIX "" CACHE PATH "Relocated ReaderPluginKit prefix")
option(QUPATHXX_BMP_BUILD_TESTS "Build independent ABI and installed-SDK checks" OFF)
if(NOT IS_DIRECTORY "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}")
  message(FATAL_ERROR "Set QUPATHXX_READER_PLUGIN_KIT_PREFIX to the existing matching kit; no dependency downloads")
endif()
set(qpx_kit "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/share/qupathxx/reader-plugin")
include("${qpx_kit}/profile.cmake")
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "This plugin requires Windows x64 MSVC")
endif()
if(NOT MSVC_VERSION STREQUAL QUPATHXX_READER_PLUGIN_MSVC_VERSION OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the C++ compiler recorded by the ReaderPluginKit")
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
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
configure_file(reader_plugin_profile.hpp.in reader_plugin_profile.hpp @ONLY)
add_library(qpx_bmp_plugin MODULE bmp_reader.cpp)
set_target_properties(qpx_bmp_plugin PROPERTIES PREFIX "")
target_compile_definitions(qpx_bmp_plugin PRIVATE QPX_READER_PLUGIN_EXPORTS _CRT_SECURE_NO_WARNINGS)
target_compile_options(qpx_bmp_plugin PRIVATE /utf-8 /W4 /permissive-)
target_include_directories(qpx_bmp_plugin PRIVATE
  "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include" "${CMAKE_CURRENT_BINARY_DIR}")
add_custom_command(TARGET qpx_bmp_plugin POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:qpx_bmp_plugin>"
  "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
  "-DOUTPUT=$<TARGET_FILE_DIR:qpx_bmp_plugin>/reader-plugins.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/write_manifest.cmake" VERBATIM)
install(TARGETS qpx_bmp_plugin RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:qpx_bmp_plugin>/reader-plugins.json" DESTINATION . CONFIGURATIONS Release)
install(FILES "${qpx_kit}/LICENSE" DESTINATION licenses/qupathxx)

if(QUPATHXX_BMP_BUILD_TESTS)
  add_executable(bmp_plugin_tests tests/bmp_plugin_tests.cpp)
  target_include_directories(bmp_plugin_tests PRIVATE "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include")
  target_compile_options(bmp_plugin_tests PRIVATE /utf-8 /permissive-)
  find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
  add_executable(bmp_project_consumer tests/bmp_project_consumer.cpp)
  target_compile_options(bmp_project_consumer PRIVATE /utf-8 /permissive-)
  target_link_libraries(bmp_project_consumer PRIVATE qupathxx::core)
  qupathxx_stage_runtime(bmp_project_consumer)
endif()

# The independently built module has no codec, core or Python runtime dependency.
