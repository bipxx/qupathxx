# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_tsv_matrix_plugin VERSION 1.0.0 LANGUAGES CXX)
set(qpx_provider_target qpx_tsv_matrix_plugin)
set(qpx_provider_source tsv_matrix.cpp)
set(qpx_provider_id org.qupathxx.tsv-matrix)
set(qpx_format_id tsv-matrix)
set(qpx_provider_domain matrix)
set(qpx_provider_abi 2)
set(qpx_provider_name "TSV expression matrix")
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_contract tsv-wide-expression/1)
include("../format-common/plugin_build.cmake")
target_compile_definitions(qpx_tsv_matrix_plugin PRIVATE
  QPX_TSV_MATRIX_VERSION="${qpx_provider_version}"
  QPX_TSV_MATRIX_CONTRACT="${qpx_provider_contract}")
