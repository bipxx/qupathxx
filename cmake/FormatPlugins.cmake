# Typed import APIs and receipt parsing remain available when dynamic providers
# are disabled. The loader/runtime implement explicit disabled-provider errors.
target_sources(qupathxx_core PRIVATE
  cpp/format_plugin_loader.cpp cpp/format_plugin_runtime.cpp cpp/format_import.cpp)

if(QUPATHXX_BUILD_FORMAT_PLUGINS)
  if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
     NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "Typed format plugins require Windows x64 MSVC Release /MD")
  endif()
  if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Typed format plugin configuration" FORCE)
  elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "Typed format plugins require CMAKE_BUILD_TYPE=Release")
  endif()
  if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL"
      AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    message(FATAL_ERROR "Typed format plugins require the dynamic CRT (/MD)")
  endif()
  set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  set_property(TARGET qupathxx_core PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  set_property(SOURCE cpp/format_plugin_loader.cpp cpp/format_plugin_runtime.cpp
    APPEND PROPERTY COMPILE_DEFINITIONS QUPATHXX_HAS_FORMAT_PLUGINS)

  # The independently buildable kit contains only C ABI headers, profile and
  # license. Native SDK installation continues to own the application *.hpp API.
  set(qpx_format_kit share/qupathxx/format-plugin)
  set(QUPATHXX_FORMAT_PLUGIN_MSVC_VERSION "${MSVC_VERSION}")
  set(QUPATHXX_FORMAT_PLUGIN_CXX_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
  set(QUPATHXX_FORMAT_PLUGIN_PROFILE_ID "windows-x64-msvc${MSVC_VERSION}-release-md")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/format-plugin-profile.cmake"
    "set(QUPATHXX_FORMAT_PLUGIN_MSVC_VERSION \"${MSVC_VERSION}\")\n"
    "set(QUPATHXX_FORMAT_PLUGIN_CXX_COMPILER_VERSION \"${CMAKE_CXX_COMPILER_VERSION}\")\n"
    "set(QUPATHXX_FORMAT_PLUGIN_ABI_VERSION 1)\n"
    "set(QUPATHXX_FORMAT_PLUGIN_PROFILE_ID \"${QUPATHXX_FORMAT_PLUGIN_PROFILE_ID}\")\n")
  install(FILES cpp/include/qupathxx/reader_plugin.h cpp/include/qupathxx/format_plugin.h
    DESTINATION include/qupathxx COMPONENT FormatPluginKit)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/format-plugin-profile.cmake"
    DESTINATION "${qpx_format_kit}" RENAME profile.cmake COMPONENT FormatPluginKit)
  install(FILES LICENSE DESTINATION "${qpx_format_kit}" COMPONENT FormatPluginKit)

  # An explicit relocated kit keeps these optional producer builds independent
  # from the core SDK/install tree. Root selects the matching prepared kit.
  set(QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX "" CACHE PATH "Matching standalone FormatPluginKit prefix")
  if(QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX)
    if(NOT IS_DIRECTORY "${QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX}")
      message(FATAL_ERROR "QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX must name a matching prepared FormatPluginKit")
    endif()
    add_subdirectory(plugins/counts_tsv "${CMAKE_CURRENT_BINARY_DIR}/format-plugins/counts_tsv" EXCLUDE_FROM_ALL)
    add_subdirectory(plugins/point_tsv "${CMAKE_CURRENT_BINARY_DIR}/format-plugins/point_tsv" EXCLUDE_FROM_ALL)
  endif()
endif()

function(qupathxx_format_plugin_tests)
  add_executable(qupathxx_format_plugin_c_header_tests cpp/tests/format_plugin_c_header_test.c)
  set_target_properties(qupathxx_format_plugin_c_header_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
  target_include_directories(qupathxx_format_plugin_c_header_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include")
  add_test(NAME format_plugin_c_header COMMAND qupathxx_format_plugin_c_header_tests)
  if(NOT QUPATHXX_BUILD_FORMAT_PLUGINS)
    return()
  endif()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/plugins/format-common/format_plugin_profile.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/format_plugin_profile.hpp" @ONLY)
  add_library(qupathxx_format_plugin_bad_module MODULE cpp/tests/format_plugin_bad_module.cpp)
  target_include_directories(qupathxx_format_plugin_bad_module PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_compile_features(qupathxx_format_plugin_bad_module PRIVATE cxx_std_17)
  target_compile_options(qupathxx_format_plugin_bad_module PRIVATE /utf-8 /permissive-)
  target_compile_definitions(qupathxx_format_plugin_bad_module PRIVATE
    QPX_FORMAT_PLUGIN_EXPORTS QPX_FORMAT_PLUGIN_MATRIX_EXPORTS QPX_FORMAT_PLUGIN_ANNOTATION_EXPORTS)
  set_target_properties(qupathxx_format_plugin_bad_module PROPERTIES
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL PREFIX "")

  add_executable(qupathxx_format_plugin_tests cpp/tests/format_plugin_tests.cpp)
  target_include_directories(qupathxx_format_plugin_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp")
  target_link_libraries(qupathxx_format_plugin_tests PRIVATE qupathxx_core nlohmann_json::nlohmann_json)
  target_compile_options(qupathxx_format_plugin_tests PRIVATE /utf-8 /permissive-)
  qupathxx_copy_runtime(qupathxx_format_plugin_tests)

  if(QUPATHXX_BUILD_PROJECT)
    add_executable(qupathxx_format_import_tests cpp/tests/format_import_tests.cpp)
    target_include_directories(qupathxx_format_import_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp")
    target_link_libraries(qupathxx_format_import_tests PRIVATE
      qupathxx_core qupathxx_sqlite nlohmann_json::nlohmann_json)
    target_compile_options(qupathxx_format_import_tests PRIVATE /utf-8 /permissive-)
    qupathxx_copy_runtime(qupathxx_format_import_tests)
  endif()
  # The independent controller first creates authored fixtures and hash-bound
  # manifests, then invokes each case in a fresh process. No bare CTest entry
  # can truthfully run these executables before that staging has occurred.
endfunction()
