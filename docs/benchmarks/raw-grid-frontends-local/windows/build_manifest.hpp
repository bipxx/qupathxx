#pragma once
#define QPX_ENV_GUI 1
#define QPX_ENV_PYTHON 1
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="5297728aecff15fb76489a9dc9ab450eeb98d1378b37b36e4281979cabf2f951";
inline constexpr char git_revision[]="8ece80e52f1b93bd62e19f0c4cc5215d6357fd68";
inline constexpr char compiler[]="MSVC 19.44.35225.0";
inline constexpr char flags[]=R"qpx(base=/DWIN32 /D_WINDOWS /EHsc;Release=/O2 /Ob2 /DNDEBUG;Debug=/Zi /Ob0 /Od /RTC1;RelWithDebInfo=/Zi /O2 /Ob1 /DNDEBUG;MinSizeRel=/O1 /Ob1 /DNDEBUG)qpx";
inline constexpr char platform[]="Windows AMD64";
inline constexpr char cmake_version[]="4.4.4";
}
