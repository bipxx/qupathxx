# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_parquet_points_plugin VERSION 1.1.0 LANGUAGES C CXX)
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_target qpx_parquet_points_plugin)
set(qpx_provider_source parquet_points.cpp)
set(qpx_provider_id org.qupathxx.parquet-points)
set(qpx_format_id parquet-points)
set(qpx_provider_domain annotation)
set(qpx_provider_abi 3)
set(qpx_provider_name "Parquet image-pixel points")
set(qpx_provider_contract parquet-level-zero-points/2)
include("../format-common/plugin_build.cmake")
find_package(Parquet 26.0.1 EXACT CONFIG REQUIRED)
target_compile_features(qpx_parquet_points_plugin PRIVATE cxx_std_20)
target_compile_definitions(qpx_parquet_points_plugin PRIVATE
  QPX_PARQUET_POINTS_VERSION="${qpx_provider_version}"
  QPX_PARQUET_POINTS_CONTRACT="${qpx_provider_contract}")
target_link_libraries(qpx_parquet_points_plugin PRIVATE Parquet::parquet_static)
