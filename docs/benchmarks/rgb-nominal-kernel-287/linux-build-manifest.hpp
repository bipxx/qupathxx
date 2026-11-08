#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="2e255d036f21c60eaca8f3a733f8bb4af852635b5601db94bbc9fa00008bdd6d";
inline constexpr char git_revision[]="23edd7ed1f15c87438602ec3b22e5506793a3a59";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O1 -g1 -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
