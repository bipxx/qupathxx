#pragma once
#define QPX_ENV_GUI 1
#define QPX_ENV_PYTHON 1
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="ecb8b7c0d502e88a89d1391a97cce7e21b15c39104594199e402c69e227639ae";
inline constexpr char git_revision[]="a7c8e4f456273eb8e4e0c7925371e8c273fa200e";
inline constexpr char compiler[]="MSVC 19.44.35225.0";
inline constexpr char flags[]=R"qpx(base=/DWIN32 /D_WINDOWS /EHsc;Release=/O2 /Ob2 /DNDEBUG;Debug=/Zi /Ob0 /Od /RTC1;RelWithDebInfo=/Zi /O2 /Ob1 /DNDEBUG;MinSizeRel=/O1 /Ob1 /DNDEBUG)qpx";
inline constexpr char platform[]="Windows AMD64";
inline constexpr char cmake_version[]="4.4.4";
}
