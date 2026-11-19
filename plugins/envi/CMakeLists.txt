# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_envi_reader_plugin VERSION 1.5.0 LANGUAGES CXX)
set(qpx_image_target qpx_envi_plugin)
set(qpx_image_source "${CMAKE_CURRENT_SOURCE_DIR}/envi_reader.cpp")
set(qpx_image_id org.qupathxx.envi)
set(qpx_image_name "ENVI Standard")
set(qpx_image_contract envi-standard-raster/5)
set(qpx_image_source_files ON)
include("../image-common/plugin_build.cmake")
target_compile_definitions(qpx_envi_plugin PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
  QPX_READER_PLUGIN_SOURCE_FILES QPX_ENVI_VERSION="${PROJECT_VERSION}"
  QPX_ENVI_CONTRACT="${qpx_image_contract}")
