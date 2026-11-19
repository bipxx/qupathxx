# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(qupathxx_counts_npz VERSION 1.0.0 LANGUAGES C CXX)
set(qpx_provider_target qpx_counts_npz_plugin)
set(qpx_provider_source counts_npz.cpp)
set(qpx_provider_id org.qupathxx.counts-npz)
set(qpx_format_id counts-npz)
set(qpx_provider_domain matrix)
set(qpx_provider_name "NumPy counts archive")
set(qpx_provider_contract npz-observation-feature-counts/1)
include("../format-common/plugin_build.cmake")

include("npz_zlib.cmake")
