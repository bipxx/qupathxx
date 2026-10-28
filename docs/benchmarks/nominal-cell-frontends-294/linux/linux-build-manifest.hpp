#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="0487b3a060e78aa7463deabe7d3988a22c5106e14382bb479451dc851d9dc6bd";
inline constexpr char git_revision[]="bb190eb7c467a74752ff87e8e6ab0491c995c493";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O1 -g1 -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
