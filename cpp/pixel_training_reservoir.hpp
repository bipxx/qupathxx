#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <atomic>
#include <stdexcept>
namespace qpx::detail {
inline std::uint64_t pixel_training_mix(std::uint64_t x) {x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
struct PixelTrainingSample {std::uint64_t priority,id;std::int64_t x,y;std::vector<float> values;};
inline bool pixel_training_sample_less(const PixelTrainingSample& a,const PixelTrainingSample& b) {if(a.priority!=b.priority) return a.priority<b.priority;if(a.y!=b.y) return a.y<b.y;return a.x<b.x;}
struct PixelTrainingSelection {
    std::vector<float> values;
    std::vector<std::uint32_t> labels;
    std::vector<std::int64_t> coordinates;
    std::vector<std::uint64_t> ids,eligible,counts;
    std::uint64_t ambiguous=0;
};
// One reservoir for the complete traversal, independent of window boundaries.
class PixelTrainingReservoir {
    std::vector<std::vector<PixelTrainingSample>> heaps_;
    std::vector<std::uint64_t> eligible_;
    std::size_t capacity_,columns_;std::uint64_t seed_,ambiguous_=0;
public:
    PixelTrainingReservoir(std::size_t classes,std::size_t capacity,std::size_t columns,std::uint64_t seed):
        heaps_(classes),eligible_(classes),capacity_(capacity),columns_(columns),seed_(seed) {
        if(!classes || capacity<2 || !columns) throw std::invalid_argument("invalid pixel training reservoir shape");
        for(auto& heap:heaps_) heap.reserve(capacity);
    }
    void observe(std::uint32_t token,std::uint64_t owner,std::int64_t x,std::int64_t y,const float* values) {
        if(token==UINT32_MAX) {++ambiguous_;return;}if(!token) return;
        if(token>heaps_.size()) throw std::invalid_argument("pixel training class outside reservoir");
        auto& heap=heaps_[token-1];++eligible_[token-1];
        PixelTrainingSample sample{pixel_training_mix(seed_^pixel_training_mix(std::uint64_t(x))^pixel_training_mix(std::uint64_t(y)^0x6a09e667f3bcc909ULL)),owner,x,y,{}};
        if(heap.size()<capacity_ || pixel_training_sample_less(sample,heap.front())) {
            sample.values.assign(values,values+columns_);
            if(heap.size()==capacity_) {std::pop_heap(heap.begin(),heap.end(),pixel_training_sample_less);heap.back()=std::move(sample);}
            else heap.push_back(std::move(sample));
            std::push_heap(heap.begin(),heap.end(),pixel_training_sample_less);
        }
    }
    PixelTrainingSelection finish(bool balanced,const std::atomic<bool>* cancelled) {
        std::size_t minimum=capacity_,total=0;
        for(const auto& heap:heaps_) {if(heap.size()<2) throw std::invalid_argument("each raw pixel class requires two valid unambiguous sample centers");minimum=std::min(minimum,heap.size());}
        for(const auto& heap:heaps_) total+=balanced?minimum:heap.size();
        PixelTrainingSelection out;out.eligible=eligible_;out.ambiguous=ambiguous_;out.counts.resize(heaps_.size());
        out.values.reserve(total*columns_);out.labels.reserve(total);out.ids.reserve(total);out.coordinates.reserve(total*2);
        for(std::size_t label=0;label<heaps_.size();++label) {
            auto& heap=heaps_[label];std::sort(heap.begin(),heap.end(),pixel_training_sample_less);
            const auto count=balanced?minimum:heap.size();out.counts[label]=count;
            for(std::size_t i=0;i<count;++i) {
                if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("pixel training selection cancelled");
                const auto& row=heap[i];out.values.insert(out.values.end(),row.values.begin(),row.values.end());out.labels.push_back(std::uint32_t(label));out.ids.push_back(row.id);out.coordinates.push_back(row.x);out.coordinates.push_back(row.y);
            }
        }
        return out;
    }
};
}
