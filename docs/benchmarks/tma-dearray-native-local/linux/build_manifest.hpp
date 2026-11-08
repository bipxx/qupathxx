#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="8049f863d07cb7997fc00e1675adacefba2638ce6aa9a4b2b8190b6fb7a12958";
inline constexpr char git_revision[]="ecbf83172ac3b44553d530e140e7109434426ee5";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O2 -g -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
