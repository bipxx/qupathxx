# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(svg_annotation_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
find_package(qupathxx CONFIG REQUIRED COMPONENTS core)
add_executable(svg_annotation_probe project_probe.cpp)
target_link_libraries(svg_annotation_probe PRIVATE qupathxx::core)
qupathxx_stage_runtime(svg_annotation_probe)
