#pragma once
#define QPX_ENV_GUI 0
#define QPX_ENV_PYTHON 0
#define QPX_ENV_INTERCHANGE 0
namespace qpx::build_info {
inline constexpr char app_version[]="0.1.0";
inline constexpr char source_sha256[]="a35170fb6478bd0dccc756da12a8257b4b0c217dd7bd6029c25079ea77485cb4";
inline constexpr char git_revision[]="ab706d47bca3bb7fcbe05e1416b81c928e550ed4";
inline constexpr char compiler[]="GNU 13.3.0";
inline constexpr char flags[]=R"qpx(base=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all;Release=-O3 -DNDEBUG;Debug=-g;RelWithDebInfo=-O1 -g1 -DNDEBUG;MinSizeRel=-Os -DNDEBUG)qpx";
inline constexpr char platform[]="Linux x86_64";
inline constexpr char cmake_version[]="3.28.3";
}
