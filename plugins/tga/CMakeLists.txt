# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_tga_reader_plugin VERSION 1.1.0 LANGUAGES CXX)

set(qpx_image_target qpx_tga_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/tga_reader.cpp")
set(qpx_image_id org.qupathxx.tga)
set(qpx_image_name TGA)
set(qpx_image_contract tga-native-samples/1)
include("../image-common/plugin_build.cmake")
target_compile_definitions(qpx_tga_plugin PRIVATE QPX_TGA_VERSION="${PROJECT_VERSION}")
