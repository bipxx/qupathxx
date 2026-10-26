#include "raw_stream_boundaries.hpp"
#include "raster_boundary_runs.hpp"
#include <array>
#include <iterator>
#include <map>
#include <tuple>

namespace qpx::detail {
namespace {
constexpr std::size_t base_charge=65536,node_charge=512;
bool same_axis(const ScientificRawWindowAxis& a,const ScientificRawWindowAxis& b) {
    return a.origin()==b.origin() && a.extent()==b.extent() && a.samples()==b.samples() &&
        a.context()==b.context() && a.first_core_index()==b.first_core_index() && a.core_samples()==b.core_samples();
}
struct LabelBlock {
    std::int64_t w,h;const std::vector<std::uint32_t>& data;
    auto width() const {return w;}auto height() const {return h;}
    const auto& labels() const {return data;}
};
}
struct RawStreamBoundaryBuilder::Impl {
    struct Key {
        bool vertical;std::int64_t fixed,first;
        bool operator<(const Key& k) const {return std::tie(vertical,fixed,first)<std::tie(k.vertical,k.fixed,k.first);}
    };
    struct Value {std::int64_t last;std::uint32_t before,after;};
    using Runs=std::map<Key,Value>;
    RawPixelStreamGeometry geometry;RawBoundaryLimits limits;
    std::vector<std::uint64_t> counts;Runs runs;
    std::uint64_t completed=0;std::size_t peak=0;
    bool failed=false,finished=false;
    Impl(const RawPixelStreamGeometry& g,std::size_t classes,RawBoundaryLimits l):
        geometry(g.lattice,g.region,g.options),limits(l) {
        if(classes<1 || classes>256 || !l.max_live_runs || l.max_live_runs>1000000 || !l.work_bytes || l.work_bytes>512u*1024*1024)
            throw std::invalid_argument("invalid raw boundary class/work limits");
        if(base_charge>l.work_bytes) throw std::length_error("raw boundary work budget exceeded");
        counts.resize(classes);
    }
    static bool line(const Key& a,const Key& b) {return a.vertical==b.vertical && a.fixed==b.fixed;}
    Runs::iterator insert(Key key,Value value) {
        if(runs.size()>=limits.max_live_runs || runs.size()+1>(limits.work_bytes-base_charge)/node_charge)
            throw std::length_error("raw boundary live-run/work limit exceeded");
        auto pair=runs.emplace(key,value);
        if(!pair.second) throw std::logic_error("duplicate raw boundary interval");
        peak=std::max(peak,runs.size());return pair.first;
    }
    Runs::iterator split(Runs::iterator it,std::int64_t at) {
        if(at==it->first.first) return it;
        auto key=it->first;key.first=at;
        auto right=insert(key,it->second);it->second.last=at;return right;
    }
    void coalesce(Runs::iterator it) {
        if(it!=runs.begin()) {
            auto prior=std::prev(it);
            if(line(prior->first,it->first) && prior->second.last==it->first.first &&
               prior->second.before==it->second.before && prior->second.after==it->second.after) {
                prior->second.last=it->second.last;runs.erase(it);it=prior;
            }
        }
        auto next=std::next(it);
        if(next!=runs.end() && line(it->first,next->first) && it->second.last==next->first.first &&
           it->second.before==next->second.before && it->second.after==next->second.after) {
            it->second.last=next->second.last;runs.erase(next);
        }
    }
    void overlay(bool vertical,std::int64_t fixed,std::int64_t first,std::int64_t last,std::uint32_t before,std::uint32_t after) {
        auto cursor=first;
        while(cursor<last) {
            Key key{vertical,fixed,cursor};auto it=runs.lower_bound(key);
            if(it!=runs.begin()) {
                auto prior=std::prev(it);
                if(line(key,prior->first) && prior->second.last>cursor) it=prior;
            }
            if(it!=runs.end() && line(key,it->first) && it->first.first<=cursor) {
                it=split(it,cursor);const auto stop=std::min(last,it->second.last);
                if(stop<it->second.last) split(it,stop);
                if((before && it->second.before) || (after && it->second.after))
                    throw std::invalid_argument("overlapping raw boundary sides");
                const auto a=before?before:it->second.before;
                const auto b=after?after:it->second.after;
                if(a==b) runs.erase(it);
                else {it->second.before=a;it->second.after=b;coalesce(it);}
                cursor=stop;
            } else {
                const auto stop=it!=runs.end() && line(key,it->first)?std::min(last,it->first.first):last;
                coalesce(insert(key,{stop,before,after}));cursor=stop;
            }
        }
    }
    void open() const {if(failed || finished) throw std::logic_error("raw boundary builder is closed");}
};

RawStreamBoundaryBuilder::RawStreamBoundaryBuilder(const RawPixelStreamGeometry& g,std::size_t classes,RawBoundaryLimits l):impl_(std::make_unique<Impl>(g,classes,l)) {}
RawStreamBoundaryBuilder::~RawStreamBoundaryBuilder()=default;
void RawStreamBoundaryBuilder::append(const ScientificRawWindow& window,const std::vector<std::uint32_t>& labels,const std::atomic<bool>* cancelled) {
    auto& s=*impl_;s.open();
    try {
        raster_boundary_checkpoint(cancelled);
        const auto expected=s.geometry.window(s.completed);
        if(window.image_width()!=expected.image_width() || window.image_height()!=expected.image_height() ||
           !same_axis(window.x(),expected.x()) || !same_axis(window.y(),expected.y()))
            throw std::invalid_argument("raw boundary block order or lattice mismatch");
        const auto width=window.core_width();const auto height=window.core_height();
        if(labels.size()!=std::uint64_t(width)*std::uint64_t(height)) throw std::invalid_argument("raw boundary block array dimensions differ");
        std::array<std::uint64_t,256> added{};
        for(std::size_t i=0;i<labels.size();++i) {
            if((i&4095)==0) raster_boundary_checkpoint(cancelled);
            const auto label=labels[i];if(label>s.counts.size()) throw std::invalid_argument("raw boundary class label exceeds model");
            if(label) ++added[label-1];
        }
        const auto fx=window.x().first_core_index();const auto fy=window.y().first_core_index();
        for_each_label_boundary_run(LabelBlock{width,height,labels},[&](auto x0,auto y0,auto x1,auto y1,auto a,auto b) {
            raster_boundary_checkpoint(cancelled);
            if(x0==x1) s.overlay(true,fx+x0,fy+y0,fy+y1,a,b);
            else s.overlay(false,fy+y0,fx+x0,fx+x1,a,b);
        },cancelled);
        for(std::size_t i=0;i<s.counts.size();++i) s.counts[i]+=added[i];
        ++s.completed;
    } catch(...) {s.failed=true;s.runs.clear();throw;}
}
RawStreamBoundaries RawStreamBoundaryBuilder::finish(const std::atomic<bool>* cancelled) {
    auto& s=*impl_;s.open();
    try {
        raster_boundary_checkpoint(cancelled);
        if(s.completed!=s.geometry.windows) throw std::logic_error("raw boundary stream is incomplete");
        RawStreamBoundaries out(s.geometry);out.class_counts=s.counts;out.runs.reserve(s.runs.size());
        for(const auto& entry:s.runs) {
            raster_boundary_checkpoint(cancelled);
            const auto& k=entry.first;const auto& v=entry.second;
            out.runs.push_back({k.vertical,k.fixed,k.first,v.last,v.before,v.after});
        }
        out.completed_windows=s.completed;out.completed_samples=s.geometry.samples;
        out.peak_runs=s.peak;out.estimated_work_bytes=base_charge+s.peak*node_charge;
        out.retained_bytes=sizeof(out)+out.runs.capacity()*sizeof(RawBoundaryRun)+out.class_counts.capacity()*sizeof(std::uint64_t);
        if(out.retained_bytes>s.limits.work_bytes) throw std::length_error("raw boundary retained output exceeds work budget");
        s.runs.clear();s.finished=true;return out;
    } catch(...) {s.failed=true;s.runs.clear();throw;}
}
}
