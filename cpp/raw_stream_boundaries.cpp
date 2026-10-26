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
RawBoundaryLabels label_raw_boundary_cells(const RawStreamBoundaries& network,
    const std::vector<RawBoundaryCell>& cells,std::size_t budget,const std::atomic<bool>* cancelled) {
    raster_boundary_checkpoint(cancelled);
    if(!budget || budget>512u*1024*1024 || cells.size()>1000000 || network.runs.size()>1000000 ||
       network.class_counts.empty() || network.class_counts.size()>256)
        throw std::invalid_argument("invalid raw boundary lookup limits");
    const auto& g=network.geometry;const auto& region=g.region;
    if(network.completed_samples!=g.samples || network.completed_windows!=g.windows)
        throw std::invalid_argument("raw boundary lookup requires a complete network");
    struct Event {std::int64_t row;std::size_t run;bool start;};
    std::size_t vertical=0;
    for(const auto& r:network.runs) {
        raster_boundary_checkpoint(cancelled);
        const auto lo=r.vertical?region.first_y:region.first_x;
        const auto hi=lo+(r.vertical?region.height:region.width);
        const auto fixed_lo=r.vertical?region.first_x:region.first_y;
        const auto fixed_hi=fixed_lo+(r.vertical?region.width:region.height);
        if(r.first<lo || r.first>=r.last || r.last>hi || r.fixed<fixed_lo || r.fixed>fixed_hi ||
           r.before==r.after || r.before>network.class_counts.size() || r.after>network.class_counts.size())
            throw std::invalid_argument("invalid raw boundary lookup run");
        vertical+=r.vertical;
    }
    // 512 bytes per possible active map node deliberately exceeds its payload.
    // Charge both event slots and every output/index slot before allocating.
    const auto work=base_charge+vertical*(2*sizeof(Event)+node_charge)+
        cells.size()*(sizeof(std::size_t)+sizeof(std::uint32_t));
    if(work>budget) throw std::length_error("raw boundary lookup work budget exceeded");
    for(const auto& cell:cells) {
        raster_boundary_checkpoint(cancelled);
        if(cell.x<region.first_x || cell.y<region.first_y ||
           cell.x>=region.first_x+region.width || cell.y>=region.first_y+region.height)
            throw std::invalid_argument("raw boundary lookup cell outside region");
    }
    if(cells.empty()) {RawBoundaryLabels empty;empty.estimated_work_bytes=work;return empty;}
    std::vector<Event> events;events.reserve(2*vertical);
    for(std::size_t i=0;i<network.runs.size();++i) if(network.runs[i].vertical) {
        const auto& r=network.runs[i];events.push_back({r.first,i,true});events.push_back({r.last,i,false});
    }
    std::sort(events.begin(),events.end(),[&](const auto& a,const auto& b) {
        raster_boundary_checkpoint(cancelled);return std::tie(a.row,a.start,a.run)<std::tie(b.row,b.start,b.run);
    });
    std::vector<std::size_t> order(cells.size());
    for(std::size_t i=0;i<order.size();++i) order[i]=i;
    std::sort(order.begin(),order.end(),[&](auto a,auto b) {
        raster_boundary_checkpoint(cancelled);return std::tie(cells[a].y,cells[a].x,a)<std::tie(cells[b].y,cells[b].x,b);
    });
    RawBoundaryLabels result;result.estimated_work_bytes=work;result.labels.resize(cells.size());
    std::map<std::int64_t,std::size_t> active;std::size_t next=0;
    for(const auto i:order) {
        raster_boundary_checkpoint(cancelled);const auto& cell=cells[i];
        while(next<events.size() && events[next].row<=cell.y) {
            raster_boundary_checkpoint(cancelled);const auto& event=events[next++];const auto& run=network.runs[event.run];
            if(event.start) {
                if(!active.emplace(run.fixed,event.run).second)
                    throw std::invalid_argument("overlapping raw boundary lookup runs");
                result.peak_active=std::max(result.peak_active,active.size());
            } else {
                const auto found=active.find(run.fixed);
                if(found==active.end() || found->second!=event.run)
                    throw std::invalid_argument("unmatched raw boundary lookup endpoint");
                active.erase(found);
            }
        }
        // Edges lie on integer cell borders; x==fixed belongs to the east side.
        const auto right=active.upper_bound(cell.x);
        const auto label=right==active.begin()?0:network.runs[std::prev(right)->second].after;
        const auto east_label=right==active.end()?0:network.runs[right->second].before;
        if(label!=east_label) throw std::invalid_argument("inconsistent raw boundary lookup class tags");
        result.labels[i]=label;
    }
    raster_boundary_checkpoint(cancelled);return result;
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
