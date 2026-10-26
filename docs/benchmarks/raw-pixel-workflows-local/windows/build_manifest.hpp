#pragma once
#define QPX_ENV_GUI 1
#define QPX_ENV_PYTHON 1
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="700f1a1033155cfe23ba5f2a0bce29eac3a61977e3accd8663428a36e5e1677c";
inline constexpr char git_revision[]="7beb80bd0d8368249afcc1a6610b5d1a77c1b4b0";
inline constexpr char compiler[]="MSVC 19.44.35225.0";
inline constexpr char flags[]=R"qpx(base=/DWIN32 /D_WINDOWS /EHsc;Release=/O2 /Ob2 /DNDEBUG;Debug=/Zi /Ob0 /Od /RTC1;RelWithDebInfo=/Zi /O2 /Ob1 /DNDEBUG;MinSizeRel=/O1 /Ob1 /DNDEBUG)qpx";
inline constexpr char platform[]="Windows AMD64";
inline constexpr char cmake_version[]="4.4.4";
}
