cmake_minimum_required(VERSION 3.24)
project(qupathxx_webp_reader_plugin VERSION 1.0.0 LANGUAGES C CXX)

set(QUPATHXX_READER_PLUGIN_KIT_PREFIX "" CACHE PATH "Relocated ReaderPluginKit prefix")
option(QUPATHXX_WEBP_BUILD_TESTS "Build independent ABI and installed-SDK checks" OFF)
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
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION OR
   NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
   NOT CMAKE_C_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
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
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(QUPATHXX_WEBP_SOURCE_DIR "" CACHE PATH "Exact prepared libwebp source selection")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/libwebp-source-lock.json" webp_lock)
string(JSON webp_commit GET "${webp_lock}" commit)
if(NOT webp_commit STREQUAL "4fa21912338357f89e4fd51cf2368325b59e9bd9")
  message(FATAL_ERROR "Unexpected libwebp source identity")
endif()
string(JSON webp_count LENGTH "${webp_lock}" members)
math(EXPR webp_last "${webp_count}-1")
foreach(index RANGE 0 ${webp_last})
  string(JSON member MEMBER "${webp_lock}" members ${index})
  string(JSON expected GET "${webp_lock}" members "${member}" sha256)
  if(NOT EXISTS "${QUPATHXX_WEBP_SOURCE_DIR}/${member}")
    message(FATAL_ERROR "Missing locked libwebp member: ${member}")
  endif()
  file(SHA256 "${QUPATHXX_WEBP_SOURCE_DIR}/${member}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Changed locked libwebp member: ${member}")
  endif()
endforeach()
string(JSON webp_sources LENGTH "${webp_lock}" compile_sources)
math(EXPR webp_sources_last "${webp_sources}-1")
set(codec_sources)
foreach(index RANGE 0 ${webp_sources_last})
  string(JSON member GET "${webp_lock}" compile_sources ${index})
  list(APPEND codec_sources "${QUPATHXX_WEBP_SOURCE_DIR}/${member}")
endforeach()
# A generated feature configuration selects generic C and synchronous workers.
# No upstream encoder, SIMD, tools, assets, mux/demux or dependency discovery.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/webp")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/src/webp/config.h"
  CONTENT "/* qupathxx selected generic synchronous decoder configuration. */\n" @ONLY)
add_library(qpx_webp_codec STATIC ${codec_sources})
target_compile_definitions(qpx_webp_codec PRIVATE HAVE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
target_include_directories(qpx_webp_codec PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}" "${QUPATHXX_WEBP_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include")
target_compile_options(qpx_webp_codec PRIVATE /utf-8)
# Allocation/poll interception is limited to the identified codec translation units.
# Their locked upstream bytes stay unmodified.
set_source_files_properties("${QUPATHXX_WEBP_SOURCE_DIR}/src/utils/utils.c" PROPERTIES
  COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/webp_memory_backend.h"
  COMPILE_DEFINITIONS QPX_WEBP_ALLOCATOR_INTERPOSE=1)
configure_file(reader_plugin_profile.hpp.in reader_plugin_profile.hpp @ONLY)
add_library(qpx_webp_plugin MODULE webp_reader.cpp webp_memory_backend.c)
set_target_properties(qpx_webp_plugin PROPERTIES PREFIX "")
target_compile_definitions(qpx_webp_plugin PRIVATE QPX_READER_PLUGIN_EXPORTS _CRT_SECURE_NO_WARNINGS)
target_compile_options(qpx_webp_plugin PRIVATE /utf-8 /W4 $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
target_include_directories(qpx_webp_plugin PRIVATE
  "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include" "${CMAKE_CURRENT_BINARY_DIR}"
  "${QUPATHXX_WEBP_SOURCE_DIR}" "${QUPATHXX_WEBP_SOURCE_DIR}/src")
target_link_libraries(qpx_webp_plugin PRIVATE qpx_webp_codec)
add_custom_command(TARGET qpx_webp_plugin POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:qpx_webp_plugin>"
  "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
  "-DOUTPUT=$<TARGET_FILE_DIR:qpx_webp_plugin>/reader-plugins.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/write_manifest.cmake" VERBATIM)
install(TARGETS qpx_webp_plugin RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:qpx_webp_plugin>/reader-plugins.json" DESTINATION . CONFIGURATIONS Release)
install(FILES "${QUPATHXX_WEBP_SOURCE_DIR}/COPYING" "${QUPATHXX_WEBP_SOURCE_DIR}/PATENTS" "${QUPATHXX_WEBP_SOURCE_DIR}/AUTHORS" DESTINATION licenses/libwebp)
install(FILES "${qpx_kit}/LICENSE" DESTINATION licenses/qupathxx)

if(QUPATHXX_WEBP_BUILD_TESTS)
  add_executable(webp_plugin_tests tests/webp_plugin_tests.cpp)
  target_include_directories(webp_plugin_tests PRIVATE "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include")
  target_compile_options(webp_plugin_tests PRIVATE /utf-8 /permissive-)
  find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
  add_executable(webp_project_consumer tests/webp_project_consumer.cpp)
  target_compile_options(webp_project_consumer PRIVATE /utf-8 /permissive-)
  target_link_libraries(webp_project_consumer PRIVATE qupathxx::core)
  qupathxx_stage_runtime(webp_project_consumer)
endif()

# The independently built module has a private static codec and no core/Python dependency.
