#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="6ebd5bebf82660e4cd34f4bc7e84a6878b5b32cba4ee4879620a571dc14b981a";
inline constexpr char git_revision[]="5562c230a3f8cf368b401cf42142ef98a37d73da";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O2 -g -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
