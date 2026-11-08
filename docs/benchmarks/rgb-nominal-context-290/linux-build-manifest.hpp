#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="49693d7ed5cd83669b84800f727d18044370546111c9ae63ab7d4505c7fcb4ad";
inline constexpr char git_revision[]="65c03e4e714d84536f58d4bfe424041855a986fc";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O1 -g1 -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
