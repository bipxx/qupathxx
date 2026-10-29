#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="2b9b9525cac85cafc01506e492aec04821c880bcd56d3bf4378f1555372ac2a5";
inline constexpr char git_revision[]="2b4786ed0c5c6c5f77f66c7af1ba31afeccddc59";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O1 -g1 -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
