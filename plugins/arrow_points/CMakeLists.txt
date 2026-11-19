# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_arrow_points_plugin VERSION 1.6.0 LANGUAGES C CXX)
set(qpx_provider_target qpx_arrow_points_plugin)
set(qpx_provider_source arrow_points.cpp)
set(qpx_provider_id org.qupathxx.arrow-points)
set(qpx_format_id arrow-points)
set(qpx_provider_domain annotation)
set(qpx_provider_abi 3)
set(qpx_provider_name "Arrow IPC image-pixel points")
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_contract arrow-file-level-zero-points/7)
include("../format-common/plugin_build.cmake")
target_compile_definitions(qpx_arrow_points_plugin PRIVATE
  QPX_ARROW_POINTS_VERSION="${qpx_provider_version}"
  QPX_ARROW_POINTS_CONTRACT="${qpx_provider_contract}")

include("arrow_decoder.cmake")
