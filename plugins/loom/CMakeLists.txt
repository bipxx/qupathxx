cmake_minimum_required(VERSION 3.24)
project(qupathxx_loom_plugin VERSION 1.0.0 LANGUAGES CXX)

set(qpx_provider_target qpx_loom_plugin)
set(qpx_provider_source loom.cpp)
set(qpx_provider_id org.qupathxx.loom)
set(qpx_format_id loom)
set(qpx_provider_domain matrix)
set(qpx_provider_name "Loom typed expression matrix")
set(qpx_provider_contract loom3-dense-typed/1)
set(qpx_provider_abi 2)
set(qpx_provider_manifest_name loom-format-plugins.json)
include("${CMAKE_CURRENT_SOURCE_DIR}/../format-common/plugin_build.cmake")

# No HDF5/zlib discovery, downloads or dependency rebuilds: use the exact
# separately retained frozen SDK closure with its corresponding source/notices.
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(qpx_loom_core qupathxx::core IMPORTED_LOCATION_RELEASE)
if(NOT EXISTS "${qpx_loom_core}")
  message(FATAL_ERROR "Loom needs the exact retained FP04 native SDK/core")
endif()
file(SHA256 "${qpx_loom_core}" qpx_loom_core_sha)
if(NOT qpx_loom_core_sha STREQUAL "4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb")
  message(FATAL_ERROR "Loom requires the frozen FP04 core; qualify rebuilds separately")
endif()
get_filename_component(qpx_loom_libdir "${qpx_loom_core}" DIRECTORY)
get_filename_component(qpx_loom_sdk "${qpx_loom_libdir}/../.." ABSOLUTE)
foreach(component hdf5 zlib)
  set(qpx_loom_library "${qpx_loom_libdir}/deps/qpx-${component}.lib")
  if(NOT EXISTS "${qpx_loom_library}")
    message(FATAL_ERROR "Missing frozen SDK ${component} static library")
  endif()
  file(SHA256 "${qpx_loom_library}" qpx_loom_library_sha)
  if(component STREQUAL "hdf5")
    set(qpx_loom_expected "ef5a6cfebd2fd9d5bf2b5f449980fcf8676cb64fc42048e952f3c9f3b272f43c")
  else()
    set(qpx_loom_expected "4ec525ee88a54ccf5d33ef1a5c8105533ad19f0ac946b71b52c8a824cbefb63d")
  endif()
  if(NOT qpx_loom_library_sha STREQUAL qpx_loom_expected)
    message(FATAL_ERROR "Frozen ${component} bytes differ from the selected source profile")
  endif()
endforeach()

set(QUPATHXX_LOOM_HDF5_INCLUDE "" CACHE PATH "Pinned HDF5 2.2.0 source src headers; no downloads")
set(QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE "" CACHE PATH "Matching frozen HDF5 generated src headers")
set(QUPATHXX_LOOM_ZLIB_INCLUDE "" CACHE PATH "Matching prefixed zlib 1.3.2 paired qpx-include")
if(NOT EXISTS "${QUPATHXX_LOOM_HDF5_INCLUDE}/hdf5.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_HDF5_INCLUDE}/H5Dpublic.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_HDF5_INCLUDE}/H5FDsubfiling/H5FDsubfiling.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_HDF5_INCLUDE}/H5FDsubfiling/H5FDioc.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE}/H5pubconf.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_ZLIB_INCLUDE}/zlib.h" OR
   NOT EXISTS "${QUPATHXX_LOOM_ZLIB_INCLUDE}/zconf.h")
  message(FATAL_ERROR "Provide the exact frozen HDF5 source/generated and paired zlib header roots")
endif()
file(SHA256 "${QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE}/H5pubconf.h" qpx_loom_config_sha)
if(NOT qpx_loom_config_sha STREQUAL "e980d2826a23809a3640549fecd587868644c494b1ea6e0ddc4640eed7562f77")
  message(FATAL_ERROR "HDF5 generated configuration differs from the frozen static instance")
endif()
file(READ "${QUPATHXX_LOOM_ZLIB_INCLUDE}/zconf.h" qpx_loom_zconf)
if(NOT qpx_loom_zconf MATCHES "#define Z_PREFIX 1")
  message(FATAL_ERROR "Use the frozen paired prefixed zlib headers")
endif()
target_include_directories(qpx_loom_plugin PRIVATE
  "${QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE}"
  "${QUPATHXX_LOOM_HDF5_INCLUDE}"
  "${QUPATHXX_LOOM_HDF5_INCLUDE}/H5FDsubfiling"
  "${QUPATHXX_LOOM_ZLIB_INCLUDE}")
target_compile_definitions(qpx_loom_plugin PRIVATE H5_BUILT_AS_STATIC_LIB NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(qpx_loom_plugin PRIVATE qupathxx::core)

# These full original notices accompany the static components. Packaging also
# binds exact matching component source archives and the application license.
install(FILES "${qpx_loom_sdk}/share/qupathxx/licenses/hdf5/LICENSE"
  DESTINATION licenses/hdf5)
install(FILES "${qpx_loom_sdk}/share/qupathxx/licenses/tiff-zlib/LICENSE"
  DESTINATION licenses/zlib)
