# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(apng_project_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
find_package(qupathxx CONFIG REQUIRED COMPONENTS core)
add_executable(apng_project_probe project_probe.cpp)
target_link_libraries(apng_project_probe PRIVATE qupathxx::core)
qupathxx_stage_runtime(apng_project_probe)
