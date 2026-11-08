#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="66d35562d555c233b4b415bd13a773deec4171dd59b77a76a58c3738d3c20518";
inline constexpr char git_revision[]="125afbb493bfec9df9ea879d12f0eeff9def6a9b";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O2 -g -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
