#pragma once
#include "raw_pixel_stream_internal.hpp"
#include <memory>
#include <vector>

namespace qpx::detail {
struct RawBoundaryRun {
    bool vertical=false;
    std::int64_t fixed=0,first=0,last=0;
    // North/south for horizontal runs; west/east for vertical runs.
    std::uint32_t before=0,after=0;
    bool operator==(const RawBoundaryRun& r) const {
        return vertical==r.vertical && fixed==r.fixed && first==r.first && last==r.last && before==r.before && after==r.after;
    }
};
struct RawBoundaryLimits {
    std::size_t max_live_runs=1000000,work_bytes=64u*1024*1024;
};
// Integer topology only: no geometry, source verification or publication authority.
struct RawStreamBoundaries {
    RawPixelStreamGeometry geometry;
    std::vector<RawBoundaryRun> runs;
    std::vector<std::uint64_t> class_counts;
    std::uint64_t completed_windows=0,completed_samples=0;
    std::size_t peak_runs=0,estimated_work_bytes=0,retained_bytes=0;
    explicit RawStreamBoundaries(const RawPixelStreamGeometry& g):geometry(g) {}
};
// One worker owns this builder. Only the external cancellation flag may change
// concurrently. Input arrays are borrowed for append() and never retained.
// Work estimates charge map nodes and the final run vector, not process RSS or
// the producer's separately admitted prediction block/scratch allocations.
class RawStreamBoundaryBuilder {
public:
    RawStreamBoundaryBuilder(const RawPixelStreamGeometry&,std::size_t classes,RawBoundaryLimits={});
    ~RawStreamBoundaryBuilder();
    RawStreamBoundaryBuilder(const RawStreamBoundaryBuilder&)=delete;
    void append(const ScientificRawWindow&,const std::vector<std::uint32_t>&,
                const std::atomic<bool>* cancelled=nullptr);
    RawStreamBoundaries finish(const std::atomic<bool>* cancelled=nullptr);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
