if(QUPATHXX_BUILD_READER_PLUGINS)
  if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "Reader plugin pilot requires Windows x64 MSVC Release /MD")
  endif()
  if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL"
      AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    message(FATAL_ERROR "Reader plugin pilot requires the dynamic CRT (/MD)")
  endif()
  qupathxx_enable_json()
  target_sources(qupathxx_core PRIVATE cpp/reader_plugin_loader.cpp cpp/reader_plugin_bridge.cpp)
  set_property(SOURCE cpp/reader_plugin_catalog.cpp APPEND PROPERTY COMPILE_DEFINITIONS QUPATHXX_HAS_READER_PLUGINS)
  target_link_libraries(qupathxx_core PRIVATE nlohmann_json::nlohmann_json)

  # The development kit has no application-library dependency. Install the C
  # header explicitly: the existing SDK's header directory installs *.hpp only.
  set(qpx_plugin_kit share/qupathxx/reader-plugin)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/reader-plugin-profile.cmake"
    "set(QUPATHXX_READER_PLUGIN_MSVC_VERSION \"${MSVC_VERSION}\")\n"
    "set(QUPATHXX_READER_PLUGIN_CXX_COMPILER_VERSION \"${CMAKE_CXX_COMPILER_VERSION}\")\n"
    "set(QUPATHXX_READER_PLUGIN_ABI_VERSION 1)\n")
  install(FILES cpp/include/qupathxx/reader_plugin.h DESTINATION include/qupathxx COMPONENT ReaderPluginKit)
  install(FILES cpp/ppm_decoder.hpp cpp/ppm_decoder.cpp DESTINATION "${qpx_plugin_kit}/decoder" COMPONENT ReaderPluginKit)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/reader-plugin-profile.cmake"
    DESTINATION "${qpx_plugin_kit}" RENAME profile.cmake COMPONENT ReaderPluginKit)
  install(DIRECTORY examples/reader_plugin/ DESTINATION "${qpx_plugin_kit}/example" COMPONENT ReaderPluginKit)
  install(FILES LICENSE DESTINATION "${qpx_plugin_kit}" COMPONENT ReaderPluginKit)
endif()

function(qupathxx_reader_plugin_host target)
  if(NOT QUPATHXX_BUILD_READER_PLUGINS)
    return()
  endif()
  # Minimal local development host stage. Existing release staging/admission
  # remains authoritative for an eventual distributable plugin release.
  install(TARGETS ${target} RUNTIME DESTINATION qupathxx CONFIGURATIONS Release COMPONENT ReaderPluginHost)
  install(FILES LICENSE DESTINATION qupathxx/licenses/qupathxx COMPONENT ReaderPluginHost)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/reader-plugin-host-source.json"
    DESTINATION share/qupathxx/reader-plugin COMPONENT ReaderPluginHost)
  if(QUPATHXX_BUILD_GEOS)
    install(FILES "$<TARGET_FILE:GEOS::geos_c>" "$<TARGET_FILE:GEOS::geos>"
      DESTINATION qupathxx CONFIGURATIONS Release COMPONENT ReaderPluginHost)
    install(FILES "${GEOS_LICENSE_FILE}" DESTINATION qupathxx/licenses/geos RENAME COPYING COMPONENT ReaderPluginHost)
  endif()
  if(QUPATHXX_BUILD_OPENSLIDE)
    install(FILES "${OpenSlide_RUNTIME}" DESTINATION qupathxx CONFIGURATIONS Release COMPONENT ReaderPluginHost)
    install(DIRECTORY "${OpenSlide_ROOT}/licenses/" DESTINATION qupathxx/licenses/openslide-sdk COMPONENT ReaderPluginHost)
  endif()
endfunction()

function(qupathxx_reader_plugin_tests)
  add_executable(qupathxx_reader_plugin_c_header_tests cpp/tests/reader_plugin_c_header_test.c)
  set_target_properties(qupathxx_reader_plugin_c_header_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
  target_include_directories(qupathxx_reader_plugin_c_header_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include")
  add_test(NAME reader_plugin_c_header COMMAND qupathxx_reader_plugin_c_header_tests)
  if(QUPATHXX_BUILD_PROJECT)
    add_executable(qupathxx_reader_plugin_offline_tests cpp/tests/reader_plugin_offline_tests.cpp)
    target_link_libraries(qupathxx_reader_plugin_offline_tests PRIVATE qupathxx_core qupathxx_sqlite)
    target_compile_definitions(qupathxx_reader_plugin_offline_tests PRIVATE QUPATHXX_HAS_PROJECT)
    qupathxx_copy_runtime(qupathxx_reader_plugin_offline_tests)
    add_test(NAME reader_plugin_offline COMMAND qupathxx_reader_plugin_offline_tests)
  endif()
  if(NOT QUPATHXX_BUILD_READER_PLUGINS)
    return()
  endif()
  add_library(qupathxx_reader_plugin_test_helper SHARED cpp/tests/reader_plugin_test_dependency.cpp)
  add_library(qupathxx_reader_plugin_test_module MODULE cpp/tests/reader_plugin_test_module.cpp)
  add_library(qupathxx_reader_plugin_test_dependent_module MODULE cpp/tests/reader_plugin_test_module.cpp)
  add_library(qupathxx_reader_plugin_test_compound_module MODULE cpp/tests/reader_plugin_test_module.cpp)
  foreach(target qupathxx_reader_plugin_test_helper qupathxx_reader_plugin_test_module qupathxx_reader_plugin_test_dependent_module qupathxx_reader_plugin_test_compound_module)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include")
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target} PRIVATE /utf-8)
    set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL PREFIX "")
  endforeach()
  target_compile_definitions(qupathxx_reader_plugin_test_dependent_module PRIVATE QPX_READER_TEST_WITH_DEPENDENCY=1)
  target_compile_definitions(qupathxx_reader_plugin_test_compound_module PRIVATE QPX_READER_TEST_COMPOUND_SOURCE=1 QPX_READER_PLUGIN_SOURCE_FILES=1)
  target_link_libraries(qupathxx_reader_plugin_test_dependent_module PRIVATE qupathxx_reader_plugin_test_helper)
  add_executable(qupathxx_reader_plugin_tests cpp/tests/reader_plugin_tests.cpp)
  target_compile_options(qupathxx_reader_plugin_tests PRIVATE /utf-8)
  target_link_libraries(qupathxx_reader_plugin_tests PRIVATE qupathxx_core)
  if(QUPATHXX_BUILD_PROJECT)
    target_compile_definitions(qupathxx_reader_plugin_tests PRIVATE QUPATHXX_HAS_PROJECT)
  endif()
  target_compile_definitions(qupathxx_reader_plugin_tests PRIVATE
    "QPX_READER_TEST_MODULE=\"$<TARGET_FILE:qupathxx_reader_plugin_test_module>\""
    "QPX_READER_TEST_DEPENDENT_MODULE=\"$<TARGET_FILE:qupathxx_reader_plugin_test_dependent_module>\""
    "QPX_READER_TEST_HELPER=\"$<TARGET_FILE:qupathxx_reader_plugin_test_helper>\"")
  qupathxx_copy_runtime(qupathxx_reader_plugin_tests)
  add_dependencies(qupathxx_reader_plugin_tests qupathxx_reader_plugin_test_module qupathxx_reader_plugin_test_dependent_module)
  add_test(NAME reader_plugin COMMAND qupathxx_reader_plugin_tests)
  add_test(NAME reader_plugin_loader COMMAND ${CMAKE_COMMAND}
    "-DTEST=$<TARGET_FILE:qupathxx_reader_plugin_tests>"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/reader-plugin-loader-cases"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/check_reader_plugin_cases.cmake")
  set_tests_properties(reader_plugin reader_plugin_loader PROPERTIES TIMEOUT 60)
  if(QUPATHXX_BUILD_PROJECT)
    add_executable(qupathxx_reader_plugin_project_tests cpp/tests/reader_plugin_project_tests.cpp)
    target_compile_options(qupathxx_reader_plugin_project_tests PRIVATE /utf-8)
    target_link_libraries(qupathxx_reader_plugin_project_tests PRIVATE qupathxx_core qupathxx_sqlite nlohmann_json::nlohmann_json)
    target_compile_definitions(qupathxx_reader_plugin_project_tests PRIVATE
      QUPATHXX_HAS_PROJECT
      "QPX_READER_TEST_MODULE=\"$<TARGET_FILE:qupathxx_reader_plugin_test_module>\""
      "QPX_READER_TEST_DEPENDENT_MODULE=\"$<TARGET_FILE:qupathxx_reader_plugin_test_dependent_module>\""
      "QPX_READER_TEST_COMPOUND_MODULE=\"$<TARGET_FILE:qupathxx_reader_plugin_test_compound_module>\""
      "QPX_READER_TEST_HELPER=\"$<TARGET_FILE:qupathxx_reader_plugin_test_helper>\"")
    qupathxx_copy_runtime(qupathxx_reader_plugin_project_tests)
    add_test(NAME reader_plugin_project COMMAND ${CMAKE_COMMAND}
      "-DTEST=$<TARGET_FILE:qupathxx_reader_plugin_project_tests>"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/reader-plugin-project-cases"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/check_reader_plugin_project.cmake")
    set_tests_properties(reader_plugin_project PROPERTIES TIMEOUT 90)
  endif()
endfunction()
