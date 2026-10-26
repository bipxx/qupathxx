#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 1
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="afa72c42bb0440dffbae5c2471485410043b8e480ed01eedebf7e87b777683fd";
inline constexpr char git_revision[]="ed2bf409107479d039e5ec2f5171565b2924ef7d";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O2 -g -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
