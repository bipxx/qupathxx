#pragma once
#include <cstdint>
#include <vector>
namespace qpx::detail {
inline std::uint64_t pixel_training_mix(std::uint64_t x) {x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
struct PixelTrainingSample {std::uint64_t priority,id;std::int64_t x,y;std::vector<float> values;};
inline bool pixel_training_sample_less(const PixelTrainingSample& a,const PixelTrainingSample& b) {if(a.priority!=b.priority) return a.priority<b.priority;if(a.y!=b.y) return a.y<b.y;return a.x<b.x;}
}
