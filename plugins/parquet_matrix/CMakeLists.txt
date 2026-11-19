# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_parquet_matrix_plugin VERSION 1.1.0 LANGUAGES C CXX)
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_target qpx_parquet_matrix_plugin)
set(qpx_provider_source parquet_matrix.cpp)
set(qpx_provider_id org.qupathxx.parquet-matrix)
set(qpx_format_id parquet-matrix)
set(qpx_provider_domain matrix)
set(qpx_provider_abi 2)
set(qpx_provider_name "Parquet expression matrix")
set(qpx_provider_contract parquet-wide-expression/2)
include("../format-common/plugin_build.cmake")
target_compile_definitions(qpx_parquet_matrix_plugin PRIVATE
  QPX_COLUMNAR_MATRIX_VERSION="${qpx_provider_version}"
  QPX_COLUMNAR_MATRIX_CONTRACT="${qpx_provider_contract}")
find_package(Parquet 26.0.1 EXACT CONFIG REQUIRED)
target_compile_features(qpx_parquet_matrix_plugin PRIVATE cxx_std_20)
target_link_libraries(qpx_parquet_matrix_plugin PRIVATE Parquet::parquet_static)
