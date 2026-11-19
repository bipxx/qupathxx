# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_apng_reader VERSION 1.0.0 LANGUAGES C CXX)
set(qpx_image_target qpx_apng_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/apng_reader.cpp")
set(qpx_image_id org.qupathxx.apng)
set(qpx_image_name APNG)
set(qpx_image_contract apng-rendered-frame/1)
include("../image-common/plugin_build.cmake")

# Reuse the locked static PNG decoder and its private zlib build. The static
# PNG plugin itself is excluded from both the default build and installation.
set(QUPATHXX_PNG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../png" "${CMAKE_CURRENT_BINARY_DIR}/png" EXCLUDE_FROM_ALL)
target_link_libraries(qpx_apng_plugin PRIVATE qpx_png_codec)
target_compile_definitions(qpx_apng_plugin PRIVATE QPX_APNG_VERSION="${PROJECT_VERSION}")
install(FILES "${QUPATHXX_PNG_SOURCE_DIR}/LICENSE" DESTINATION licenses/libpng)
install(FILES "${QUPATHXX_PNG_ZLIB_SOURCE_DIR}/LICENSE" DESTINATION licenses/zlib)
