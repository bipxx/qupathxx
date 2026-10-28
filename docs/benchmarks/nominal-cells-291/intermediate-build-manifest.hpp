#pragma once
#define QPX_ENV_GUI 1
#define QPX_ENV_PYTHON 1
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="a4ef715c7827a6eda14d30c3c2b50c5ac6ef8eaf200e2eebc7664e1ad4c7c2da";
inline constexpr char git_revision[]="b05cf2c9f3788dd8a539e56e2d0bd177af76b1ed";
inline constexpr char compiler[]="MSVC 19.44.35225.0";
inline constexpr char flags[]=R"qpx(base=/DWIN32 /D_WINDOWS /EHsc;Release=/O2 /Ob2 /DNDEBUG;Debug=/Zi /Ob0 /Od /RTC1;RelWithDebInfo=/Zi /O2 /Ob1 /DNDEBUG;MinSizeRel=/O1 /Ob1 /DNDEBUG)qpx";
inline constexpr char platform[]="Windows AMD64";
inline constexpr char cmake_version[]="4.4.4";
}
