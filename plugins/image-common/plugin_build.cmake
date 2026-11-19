# SPDX-License-Identifier: AGPL-3.0-only
# Shared build policy, not a format registry or automatic dispatcher.
set(QUPATHXX_READER_PLUGIN_KIT_PREFIX "" CACHE PATH "Relocated matching ReaderPluginKit")
if(NOT IS_DIRECTORY "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}")
  message(FATAL_ERROR "Set QUPATHXX_READER_PLUGIN_KIT_PREFIX; no dependency downloads")
endif()
set(qpx_image_kit "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/share/qupathxx/reader-plugin")
include("${qpx_image_kit}/profile.cmake")
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
  message(FATAL_ERROR "This image provider requires Windows x64 MSVC")
endif()
if(NOT MSVC_VERSION STREQUAL QUPATHXX_READER_PLUGIN_MSVC_VERSION OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION)
  message(FATAL_ERROR "Use the exact compiler recorded by the ReaderPluginKit")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "This image provider requires CMAKE_BUILD_TYPE=Release")
endif()
if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
  message(FATAL_ERROR "This image provider requires Release dynamic CRT (/MD)")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
configure_file("${CMAKE_CURRENT_LIST_DIR}/reader_plugin_profile.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/reader_plugin_profile.hpp" @ONLY)
add_library(${qpx_image_target} MODULE "${qpx_image_source}")
set_target_properties(${qpx_image_target} PROPERTIES PREFIX "")
target_compile_definitions(${qpx_image_target} PRIVATE QPX_READER_PLUGIN_EXPORTS _CRT_SECURE_NO_WARNINGS)
target_compile_options(${qpx_image_target} PRIVATE /utf-8 /W4 /permissive-)
target_include_directories(${qpx_image_target} PRIVATE
  "${QUPATHXX_READER_PLUGIN_KIT_PREFIX}/include" "${CMAKE_CURRENT_BINARY_DIR}")
if(NOT DEFINED qpx_image_manifest_name)
  set(qpx_image_manifest_name reader-plugins.json)
endif()
if(NOT qpx_image_manifest_name MATCHES "^[A-Za-z0-9_.-]+\\.json$")
  message(FATAL_ERROR "Image plugin manifest must be a plain JSON basename")
endif()
add_custom_command(TARGET ${qpx_image_target} POST_BUILD COMMAND "${CMAKE_COMMAND}"
  "-DMODULE_FILE=$<TARGET_FILE:${qpx_image_target}>" "-DMODULE_NAME=${qpx_image_target}.dll"
  "-DPROVIDER_ID=${qpx_image_id}" "-DDISPLAY_NAME=${qpx_image_name}"
  "-DCONTRACT=${qpx_image_contract}" "-DVERSION=${PROJECT_VERSION}"
  "-DSOURCE_STORE=${qpx_image_source_store}"
  "-DSOURCE_FILES=${qpx_image_source_files}"
  "-DSDK_RUNTIME_FILES=${qpx_image_sdk_runtime_files}" "-DDUMPBIN=${CMAKE_DUMPBIN}"
  "-DPROFILE=windows-x64-msvc${QUPATHXX_READER_PLUGIN_MSVC_VERSION}-release-md"
  "-DOUTPUT=$<TARGET_FILE_DIR:${qpx_image_target}>/${qpx_image_manifest_name}"
  -P "${CMAKE_CURRENT_LIST_DIR}/write_manifest.cmake" VERBATIM)
install(TARGETS ${qpx_image_target} RUNTIME DESTINATION . LIBRARY DESTINATION . CONFIGURATIONS Release)
install(FILES "$<TARGET_FILE_DIR:${qpx_image_target}>/${qpx_image_manifest_name}" DESTINATION . CONFIGURATIONS Release)
if(qpx_image_sdk_runtime_files)
  # Read the generated manifest so installation matches its inspected PE graph.
  install(CODE "
    set(module_dir \"$<TARGET_FILE_DIR:${qpx_image_target}>\")
    file(READ \"\${module_dir}/${qpx_image_manifest_name}\" manifest)
    string(JSON count LENGTH \"\${manifest}\" plugins 0 dependencies)
    if(count GREATER 0)
      math(EXPR last \"\${count} - 1\")
      foreach(i RANGE 0 \"\${last}\")
        string(JSON name GET \"\${manifest}\" plugins 0 dependencies \"\${i}\" path)
        file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}\" TYPE FILE FILES \"\${module_dir}/\${name}\")
      endforeach()
    endif()" CONFIGURATIONS Release)
endif()
install(FILES "${qpx_image_kit}/LICENSE" DESTINATION licenses/qupathxx)
