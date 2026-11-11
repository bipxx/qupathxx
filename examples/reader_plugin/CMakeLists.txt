cmake_minimum_required(VERSION 3.24)
project(qupathxx_p6_reader_plugin LANGUAGES CXX)

set(QUPATHXX_READER_PLUGIN_KIT_PREFIX "" CACHE PATH "Installed qupathxx reader-plugin kit prefix")
if(NOT QUPATHXX_READER_PLUGIN_KIT_PREFIX)
  message(FATAL_ERROR "Set QUPATHXX_READER_PLUGIN_KIT_PREFIX to the installed kit prefix")
endif()
get_filename_component(qpx_kit_prefix "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}" ABSOLUTE)
set(qpx_kit "${qpx_kit_prefix}/share/qupathxx/reader-plugin")
foreach(member
    "${qpx_kit}/profile.cmake"
    "${qpx_kit_prefix}/include/qupathxx/reader_plugin.h"
    "${qpx_kit}/decoder/ppm_decoder.hpp"
    "${qpx_kit}/decoder/ppm_decoder.cpp")
  if(NOT EXISTS "${member}")
    message(FATAL_ERROR "Incomplete reader-plugin kit: ${member}")
  endif()
endforeach()
include("${qpx_kit}/profile.cmake")

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
    NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "This reader-plugin kit requires Windows x64 MSVC")
endif()
if(NOT DEFINED QUPATHXX_READER_PLUGIN_MSVC_VERSION OR
    NOT DEFINED QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "The reader-plugin kit has no recorded compiler profile")
endif()
if(NOT MSVC_VERSION STREQUAL QUPATHXX_READER_PLUGIN_MSVC_VERSION OR
    NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the MSVC compiler recorded by the installed reader-plugin kit")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported reader-plugin configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "This reader-plugin kit requires CMAKE_BUILD_TYPE=Release")
endif()
if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
  message(FATAL_ERROR "This reader-plugin kit requires Release dynamic CRT (/MD)")
endif()

configure_file(reader_plugin_profile.hpp.in reader_plugin_profile.hpp @ONLY)
add_library(qpx_p6_plugin MODULE p6_plugin.cpp "${qpx_kit}/decoder/ppm_decoder.cpp")
set_target_properties(qpx_p6_plugin PROPERTIES
  PREFIX ""
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)
target_include_directories(qpx_p6_plugin PRIVATE
  "${qpx_kit_prefix}/include"
  "${qpx_kit}/decoder"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(qpx_p6_plugin PRIVATE /utf-8 /permissive-)
target_compile_definitions(qpx_p6_plugin PRIVATE QPX_READER_PLUGIN_EXPORTS)
add_custom_command(TARGET qpx_p6_plugin POST_BUILD
  COMMAND "${CMAKE_COMMAND}"
    "-DMODULE_FILE=$<TARGET_FILE:qpx_p6_plugin>"
    "-DMODULE_NAME=$<TARGET_FILE_NAME:qpx_p6_plugin>"
    "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
    "-DOUTPUT=$<TARGET_FILE_DIR:qpx_p6_plugin>/reader-plugins.json"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/write_manifest.cmake"
  VERBATIM)
install(TARGETS qpx_p6_plugin RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:qpx_p6_plugin>/reader-plugins.json" DESTINATION . CONFIGURATIONS Release)

# There is intentionally no qupathxx package discovery or core-library link.
