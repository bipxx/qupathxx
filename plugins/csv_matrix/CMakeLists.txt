# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_csv_matrix_plugin VERSION 1.0.1 LANGUAGES CXX)
set(qpx_provider_target qpx_csv_matrix_plugin)
set(qpx_provider_source csv_matrix.cpp)
set(qpx_provider_id org.qupathxx.csv-matrix)
set(qpx_format_id csv-matrix)
set(qpx_provider_domain matrix)
set(qpx_provider_abi 2)
set(qpx_provider_name "CSV expression matrix")
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_contract csv-wide-expression/1)
include("../format-common/plugin_build.cmake")
target_compile_definitions(qpx_csv_matrix_plugin PRIVATE
  QPX_CSV_MATRIX_VERSION="${qpx_provider_version}"
  QPX_CSV_MATRIX_CONTRACT="${qpx_provider_contract}")
