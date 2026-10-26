#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="1d87072234f7931583df8378551b80f46c00dec4b5bb01303b1fca9c0a907ed0";
inline constexpr char git_revision[]="cc6fab40cbc54aa382e146bb5d98c22d13ddd01e";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O2 -g -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
