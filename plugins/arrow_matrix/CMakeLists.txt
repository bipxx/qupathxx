# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_arrow_matrix_plugin VERSION 1.3.0 LANGUAGES C CXX)
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_target qpx_arrow_matrix_plugin)
set(qpx_provider_source arrow_matrix.cpp)
set(qpx_provider_id org.qupathxx.arrow-matrix)
set(qpx_format_id arrow-matrix)
set(qpx_provider_domain matrix)
set(qpx_provider_abi 2)
set(qpx_provider_name "Arrow IPC expression matrix")
set(qpx_provider_contract arrow-wide-expression/4)
include("../format-common/plugin_build.cmake")
target_compile_definitions(qpx_arrow_matrix_plugin PRIVATE
  QPX_COLUMNAR_MATRIX_VERSION="${qpx_provider_version}"
  QPX_COLUMNAR_MATRIX_CONTRACT="${qpx_provider_contract}")
include("../arrow_points/arrow_decoder.cmake")
