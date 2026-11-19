# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_webp_animation_reader VERSION 1.0.0 LANGUAGES C CXX)
set(qpx_image_target qpx_webp_animation_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/animation_reader.cpp")
set(qpx_image_id org.qupathxx.webp-animation)
set(qpx_image_name "Animated WebP")
set(qpx_image_contract webp-rendered-frame/1)
include("../image-common/plugin_build.cmake")

set(QUPATHXX_WEBP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../webp" "${CMAKE_CURRENT_BINARY_DIR}/webp" EXCLUDE_FROM_ALL)
target_sources(qpx_webp_animation_plugin PRIVATE "../webp/webp_memory_backend.c")
target_include_directories(qpx_webp_animation_plugin PRIVATE "${QUPATHXX_WEBP_SOURCE_DIR}/src")
target_link_libraries(qpx_webp_animation_plugin PRIVATE qpx_webp_codec)
target_compile_definitions(qpx_webp_animation_plugin PRIVATE QPX_WEBP_ANIMATION_VERSION="${PROJECT_VERSION}")
install(FILES "${QUPATHXX_WEBP_SOURCE_DIR}/COPYING" "${QUPATHXX_WEBP_SOURCE_DIR}/PATENTS"
  "${QUPATHXX_WEBP_SOURCE_DIR}/AUTHORS" DESTINATION licenses/libwebp)
