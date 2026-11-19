# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_geoparquet_plugin VERSION 1.3.1 LANGUAGES C CXX)
set(qpx_provider_version "${PROJECT_VERSION}")
set(qpx_provider_target qpx_geoparquet_plugin)
set(qpx_provider_source geoparquet.cpp)
set(qpx_provider_id org.qupathxx.geoparquet)
set(qpx_format_id geoparquet)
set(qpx_provider_domain annotation)
set(qpx_provider_abi 3)
set(qpx_provider_name "GeoParquet image-pixel annotations")
set(qpx_provider_contract geoparquet-image-pixels/4)
include("../format-common/plugin_build.cmake")
find_package(Parquet 26.0.1 EXACT CONFIG REQUIRED)
target_compile_features(qpx_geoparquet_plugin PRIVATE cxx_std_20)
target_compile_definitions(qpx_geoparquet_plugin PRIVATE
  QPX_GEOPARQUET_VERSION="${qpx_provider_version}"
  QPX_GEOPARQUET_CONTRACT="${qpx_provider_contract}")
target_link_libraries(qpx_geoparquet_plugin PRIVATE Parquet::parquet_static)
