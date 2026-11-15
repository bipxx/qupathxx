# SPDX-License-Identifier: AGPL-3.0-only
# Called by each independent provider with qpx_provider_target already set.
set(QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX "" CACHE PATH "Relocated FormatPluginKit prefix")
if(NOT IS_DIRECTORY "${QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX}")
  message(FATAL_ERROR "Set QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX to the matching kit; no dependency downloads")
endif()
set(qpx_format_kit "${QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX}/share/qupathxx/format-plugin")
include("${qpx_format_kit}/profile.cmake")
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "Typed TSV plugins require Windows x64 MSVC")
endif()
if(NOT MSVC_VERSION STREQUAL QUPATHXX_FORMAT_PLUGIN_MSVC_VERSION OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_FORMAT_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the C++ compiler recorded by the FormatPluginKit")
endif()
if(NOT QUPATHXX_FORMAT_PLUGIN_PROFILE_ID STREQUAL "windows-x64-msvc1944-release-md")
  message(FATAL_ERROR "Typed TSV plugins require the declared Windows x64 MSVC1944 profile")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "Typed TSV plugins require CMAKE_BUILD_TYPE=Release")
endif()
if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
  message(FATAL_ERROR "Typed TSV plugins require Release dynamic CRT (/MD)")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
configure_file("${CMAKE_CURRENT_LIST_DIR}/format_plugin_profile.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/format_plugin_profile.hpp" @ONLY)
add_library(${qpx_provider_target} MODULE "${qpx_provider_source}")
set_target_properties(${qpx_provider_target} PROPERTIES PREFIX "" MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_definitions(${qpx_provider_target} PRIVATE QPX_FORMAT_PLUGIN_EXPORTS _CRT_SECURE_NO_WARNINGS)
if(qpx_provider_domain STREQUAL "matrix")
  target_compile_definitions(${qpx_provider_target} PRIVATE QPX_FORMAT_PLUGIN_MATRIX_EXPORTS)
elseif(qpx_provider_domain STREQUAL "annotation")
  target_compile_definitions(${qpx_provider_target} PRIVATE QPX_FORMAT_PLUGIN_ANNOTATION_EXPORTS)
else()
  message(FATAL_ERROR "Unexpected typed TSV provider family")
endif()
target_compile_options(${qpx_provider_target} PRIVATE /utf-8 /W4 /permissive-)
target_include_directories(${qpx_provider_target} PRIVATE
  "${QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX}/include" "${CMAKE_CURRENT_BINARY_DIR}")
add_custom_command(TARGET ${qpx_provider_target} POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:${qpx_provider_target}>"
  "-DMODULE_NAME=${qpx_provider_target}.dll"
  "-DPROVIDER_ID=${qpx_provider_id}" "-DFORMAT_ID=${qpx_format_id}"
  "-DDOMAIN=${qpx_provider_domain}" "-DDISPLAY_NAME=${qpx_provider_name}"
  "-DCONTRACT=${qpx_provider_contract}" "-DPROFILE=${QUPATHXX_FORMAT_PLUGIN_PROFILE_ID}"
  "-DOUTPUT=$<TARGET_FILE_DIR:${qpx_provider_target}>/format-plugins.json"
  -P "${CMAKE_CURRENT_LIST_DIR}/write_manifest.cmake" VERBATIM)
install(TARGETS ${qpx_provider_target} RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:${qpx_provider_target}>/format-plugins.json" DESTINATION . CONFIGURATIONS Release)
install(FILES "${qpx_format_kit}/LICENSE" DESTINATION licenses/qupathxx)
# No SDK package discovery or core, Python, Java or codec link is required.
