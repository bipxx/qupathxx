#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 1
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="a748067e7301b332e7144c21fd1e393097bf697e3ea70a1d253a8cb985782516";
inline constexpr char git_revision[]="4bdfa986265c35a756c4114f49a862750f2bda8f";
inline constexpr char compiler[]="MSVC 19.44.35225.0";
inline constexpr char flags[]=R"qpx(base=/DWIN32 /D_WINDOWS /EHsc;Release=/O2 /Ob2 /DNDEBUG;Debug=/Zi /Ob0 /Od /RTC1;RelWithDebInfo=/Zi /O2 /Ob1 /DNDEBUG;MinSizeRel=/O1 /Ob1 /DNDEBUG)qpx";
inline constexpr char platform[]="Windows AMD64";
inline constexpr char cmake_version[]="4.4.4";
}
