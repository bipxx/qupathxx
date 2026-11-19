# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_pcx_reader_plugin VERSION 1.1.0 LANGUAGES CXX)

set(qpx_image_target qpx_pcx_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/pcx_reader.cpp")
set(qpx_image_id org.qupathxx.pcx)
set(qpx_image_name PCX)
set(qpx_image_contract pcx-native-samples/1)
include("../image-common/plugin_build.cmake")
target_compile_definitions(qpx_pcx_plugin PRIVATE QPX_PCX_VERSION="${PROJECT_VERSION}")
