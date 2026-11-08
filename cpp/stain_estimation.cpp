#include "qupathxx/stain_estimation.hpp"
#include "sha256.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
namespace qpx {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
void checkpoint(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("stain estimation cancelled");}
StainEstimateParameters checked(StainEstimateParameters p) {p.validate();return p;}
double dot(const ColorVector& a,const ColorVector& b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
ColorVector unit(ColorVector a) {const auto n=std::hypot(a[0],a[1],a[2]);if(!std::isfinite(n) || n<1e-12) throw std::invalid_argument("stain plane is degenerate");for(auto& v:a) v/=n;return a;}
ColorVector cross(const ColorVector& a,const ColorVector& b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
std::int64_t edge(double value,double scale,std::int64_t limit) {const auto n=std::ceil(static_cast<long double>(value)/scale-.5L);return n<=0?0:n>=static_cast<long double>(limit)?limit:static_cast<std::int64_t>(n);}
std::int64_t origin(std::int64_t index,double scale) {const auto n=std::round(static_cast<long double>(index)*scale);if(!std::isfinite(n) || n<0 || n>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("stain sample origin overflow");return static_cast<std::int64_t>(n);}
std::uint64_t draw(std::uint64_t& state,std::uint64_t bound) {
    // SplitMix64 plus rejection: deterministic, unbiased bounded draws.
    const auto threshold=(std::uint64_t(0)-bound)%bound;
    for(;;) {auto z=(state+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;z^=z>>31;if(z>=threshold) return z%bound;}
}
using Matrix=std::array<ColorVector,3>;
struct Eigen {ColorVector values;Matrix vectors;};
Eigen eigen(Matrix a) {
    Matrix v{{{1,0,0},{0,1,0},{0,0,1}}};
    for(int step=0;step<64;++step) {
        int p=0,q=1;for(int i=0;i<3;++i) for(int j=i+1;j<3;++j) if(std::abs(a[i][j])>std::abs(a[p][q])) {p=i;q=j;}
        if(std::abs(a[p][q])<=1e-15*std::max(1.,std::abs(a[0][0])+std::abs(a[1][1])+std::abs(a[2][2]))) break;
        const auto phi=.5*std::atan2(2*a[p][q],a[q][q]-a[p][p]),c=std::cos(phi),s=std::sin(phi);
        const auto pp=a[p][p],qq=a[q][q],pq=a[p][q];a[p][p]=c*c*pp-2*s*c*pq+s*s*qq;a[q][q]=s*s*pp+2*s*c*pq+c*c*qq;a[p][q]=a[q][p]=0;
        for(int k=0;k<3;++k) {if(k!=p && k!=q) {const auto kp=a[k][p],kq=a[k][q];a[k][p]=a[p][k]=c*kp-s*kq;a[k][q]=a[q][k]=s*kp+c*kq;}const auto vp=v[k][p],vq=v[k][q];v[k][p]=c*vp-s*vq;v[k][q]=s*vp+c*vq;}
    }
    std::array<int,3> order={0,1,2};std::stable_sort(order.begin(),order.end(),[&](int i,int j){return a[i][i]>a[j][j];});Eigen out;
    for(int i=0;i<3;++i) {out.values[i]=std::max(0.,a[order[i]][order[i]]);for(int j=0;j<3;++j) out.vectors[i][j]=v[j][order[i]];}
    return out;
}
double quantile(const std::vector<double>& sorted,double q) {const auto index=q*(sorted.size()-1);const auto low=static_cast<std::size_t>(index),high=std::min(low+1,sorted.size()-1);return sorted[low]+(sorted[high]-sorted[low])*(index-low);}
bool hex(const std::string& s) {return s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos;}
}
void StainEstimateParameters::validate() const {
    if(!std::isfinite(min_od) || min_od<0 || min_od>3 || !std::isfinite(trim_quantile) || trim_quantile<0 || trim_quantile>.25 || !std::isfinite(min_angle_degrees) || min_angle_degrees<.1 || min_angle_degrees>89 || !std::isfinite(max_residual_fraction) || max_residual_fraction<0 || max_residual_fraction>.25) throw std::invalid_argument("invalid stain estimation thresholds");
    if(min_samples<8 || sample_limit<min_samples || sample_limit>1000000 || tile_size<16 || tile_size>1024 || !work_bytes || work_bytes>256u*1024u*1024u || !max_read_pixels || max_read_pixels>(1ULL<<40) || level< -1) throw std::invalid_argument("invalid stain estimation resource limits");
}
StainEstimateRequest::StainEstimateRequest(const Document& doc,std::uint64_t id,StainEstimateParameters p,ColorVector white,bool nucleus):parameters_(checked(p)),source_(doc,id,StainTransform({{1,0,0},{0,1,0}},white),AnalysisKind::OpticalDensityMeans,nucleus,p.tile_size,true) {
    const auto& m=source_.source_->metadata();const auto b=source_.roi().bounds();if(p.level>=0 && std::size_t(p.level)>=m.levels.size()) throw std::out_of_range("invalid stain estimation pyramid level");bool found=false;
    for(std::size_t i=0;i<m.levels.size();++i) {
        const auto& l=m.levels[i];if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1) throw std::invalid_argument("invalid stain estimation pyramid");
        if(p.level>=0 && i!=std::size_t(p.level)) continue;
        const auto x0=edge(b.x,l.downsample,l.width),y0=edge(b.y,l.downsample,l.height),x1=edge(b.x+b.width,l.downsample,l.width),y1=edge(b.y+b.height,l.downsample,l.height);
        if(x0>=x1 || y0>=y1) continue;const auto w=std::uint64_t(x1-x0),h=std::uint64_t(y1-y0);if(w>p.max_read_pixels/h) continue;
        const auto count=w*h,cap=std::min<std::uint64_t>(count,p.sample_limit),bytes=65536+cap*(sizeof(ColorVector)+sizeof(double))+8*std::min<std::uint64_t>(w,p.tile_size)*std::min<std::uint64_t>(h,p.tile_size);
        if(cap<p.min_samples || bytes>p.work_bytes || (found && l.downsample>=downsample_)) continue;
        const auto step=static_cast<std::int64_t>(p.tile_size);
        const auto lastx=x0+((x1-x0-1)/step)*step,lasty=y0+((y1-y0-1)/step)*step;
        validate_region(m,{origin(lastx,l.downsample),origin(lasty,l.downsample),static_cast<std::int32_t>(i),x1-lastx,y1-lasty});
        found=true;level_=static_cast<std::int32_t>(i);downsample_=l.downsample;x0_=x0;y0_=y0;x1_=x1;y1_=y1;capacity_=std::size_t(cap);estimated_work_bytes_=std::size_t(bytes);total_tiles_=((w+p.tile_size-1)/p.tile_size)*((h+p.tile_size-1)/p.tile_size);
    }
    if(!found) throw std::length_error("stain ROI exceeds read/work limits or has too few grid samples; choose a smaller ROI, coarser level or larger limits");
}
std::string stain_model_sha256(const StainTransform& transform) {
    static_assert(sizeof(double)==sizeof(std::uint64_t));Sha256 hash;const std::string schema="qpx.stain-model/1";hash.update(schema.data(),schema.size());
    auto real=[&](double v) {std::uint64_t bits=0;if(v!=0) std::memcpy(&bits,&v,8);hash.integer(bits);};for(auto v:transform.white()) real(v);for(const auto& stain:transform.stains()) for(auto v:stain) real(v);return hash.finish();
}
struct StainEstimateKernel {
    static StainEstimateResult run(const StainEstimateRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr) {
        const auto start=std::chrono::steady_clock::now();checkpoint(cancelled);const auto& p=request.parameters_;const auto& source=request.source_;const auto& m=source.source_->metadata();
        StainEstimateResult out;out.parameters=p;out.provenance=source.provenance();out.roi=source.roi();out.white=source.transform().white();out.level=request.level_;out.downsample=request.downsample_;out.estimated_work_bytes=request.estimated_work_bytes_;
        std::array<std::array<double,256>,3> lookup;for(int c=0;c<3;++c) for(int value=0;value<256;++value) lookup[c][value]=optical_density(value,out.white[c]);
        std::vector<ColorVector> samples;samples.reserve(request.capacity_);auto state=p.seed;Sha256 digest;const std::string format=out.level==0?"qpx.rgba-tiles/1":"qpx.rgba-level-tiles/1";digest.update(format.data(),format.size());digest.integer(m.width);digest.integer(m.height);
        if(out.level) {const auto& l=m.levels[out.level];std::uint64_t bits;std::memcpy(&bits,&out.downsample,8);digest.integer(out.level);digest.integer(l.width);digest.integer(l.height);digest.integer(bits);}
        for(auto v:{request.x0_,request.y0_,request.x1_,request.y1_}) digest.integer(v);digest.integer(p.tile_size);
        for(auto y=request.y0_;y<request.y1_;) {const auto h=std::min<std::int64_t>(p.tile_size,request.y1_-y);
            for(auto x=request.x0_;x<request.x1_;) {checkpoint(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,request.x1_-x),rx=origin(x,out.downsample),ry=origin(y,out.downsample);const auto pixels=source.source_->read_region({rx,ry,out.level,w,h});
                if(pixels.width()!=w || pixels.height()!=h || pixels.bytes().size()!=region_byte_count(w,h)) throw std::runtime_error("reader returned incorrect stain tile dimensions");if(out.level) {digest.integer(x);digest.integer(y);}for(auto v:{rx,ry,w,h}) digest.integer(v);digest.update(pixels.bytes().data(),pixels.bytes().size());
                const auto& data=pixels.bytes();for(std::int64_t row=0;row<h;++row) {checkpoint(cancelled);for(std::int64_t col=0;col<w;++col) {const auto i=std::size_t((row*w+col)*4);if(data[i+3]!=255 || (out.roi.kind()!=RoiKind::Rectangle && !out.roi.contains({(x+col+.5)*out.downsample,(y+row+.5)*out.downsample}))) continue;
                    ++out.samples;ColorVector od={lookup[0][data[i]],lookup[1][data[i+1]],lookup[2][data[i+2]]};if(od[0]<p.min_od || od[1]<p.min_od || od[2]<p.min_od || dot(od,od)==0) continue;++out.eligible;
                    if(samples.size()<request.capacity_) samples.push_back(od);else {const auto index=draw(state,out.eligible);if(index<samples.size()) samples[std::size_t(index)]=od;}
                }}++out.tiles;if(completed) completed->store(out.tiles,std::memory_order_relaxed);x+=w;
            }y+=h;
        }
        checkpoint(cancelled);if(samples.size()<p.min_samples) throw std::invalid_argument("too few foreground samples for stain estimation");out.sampled=samples.size();Matrix moment{},correction{};ColorVector mean{},mean_correction{};
        for(std::size_t i=0;i<samples.size();++i) {if(i%1024==0) checkpoint(cancelled);const auto& od=samples[i];for(int r=0;r<3;++r) {const auto d=od[r]-mean_correction[r],n=mean[r]+d;mean_correction[r]=(n-mean[r])-d;mean[r]=n;for(int c=0;c<3;++c) {const auto delta=od[r]*od[c]-correction[r][c],next=moment[r][c]+delta;correction[r][c]=(next-moment[r][c])-delta;moment[r][c]=next;}}}
        for(auto& row:moment) for(auto& v:row) v/=double(samples.size());const auto fit=eigen(moment);out.eigenvalues=fit.values;const auto total=fit.values[0]+fit.values[1]+fit.values[2];
        if(fit.values[0]<=0 || fit.values[1]<=fit.values[0]*1e-6 || fit.values[1]-fit.values[2]<=fit.values[0]*1e-6) throw std::invalid_argument("stain samples do not define a stable two-dimensional plane");
        out.residual_fraction=fit.values[2]/total;if(out.residual_fraction>p.max_residual_fraction) throw std::invalid_argument("stain ROI is not sufficiently described by two stain directions");
        auto normal=unit(cross(fit.vectors[0],fit.vectors[1]));const auto largest=std::max_element(normal.begin(),normal.end(),[](double a,double b){return std::abs(a)<std::abs(b);});if(*largest<0) for(auto& v:normal) v=-v;
        ColorVector first{};for(int i=0;i<2;++i) {const auto weight=dot(mean,fit.vectors[i]);for(int c=0;c<3;++c) first[c]+=weight*fit.vectors[i][c];}first=unit(first);const auto second=unit(cross(normal,first));std::vector<double> angles;angles.reserve(samples.size());
        for(std::size_t i=0;i<samples.size();++i) {if(i%1024==0) checkpoint(cancelled);const auto a=dot(samples[i],first),b=dot(samples[i],second);if(a<=0) throw std::invalid_argument("stain sample angles do not lie in a common positive half-plane");angles.push_back(std::atan2(b,a));}
        checkpoint(cancelled);std::sort(angles.begin(),angles.end());checkpoint(cancelled);const auto low=quantile(angles,p.trim_quantile),high=quantile(angles,1-p.trim_quantile);std::vector<ColorVector> stains;
        for(auto angle:{low,high}) {ColorVector v{};for(int c=0;c<3;++c) {v[c]=first[c]*std::cos(angle)+second[c]*std::sin(angle);if(v[c]< -1e-10) throw std::invalid_argument("estimated stain has negative OD components; choose a representative two-stain ROI");v[c]=std::max(0.,v[c]);}stains.push_back(unit(v));}
        if(stains[0]<stains[1]) std::swap(stains[0],stains[1]);out.angle_degrees=std::acos(std::clamp(dot(stains[0],stains[1]),-1.,1.))*180/pi;if(out.angle_degrees<p.min_angle_degrees) throw std::invalid_argument("estimated stain directions are too similar");
        const StainTransform transform(stains,out.white);out.stains=transform.stains();out.model_sha256=stain_model_sha256(transform);out.input_sha256=digest.finish();out.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();out.validate();checkpoint(cancelled);return out;
    }
};
StainEstimateResult estimate_stains(const StainEstimateRequest& request,const std::atomic<bool>* cancelled) {return StainEstimateKernel::run(request,cancelled);}
StainTransform StainEstimateResult::transform() const {return StainTransform::from_normalized(stains,white);}
void StainEstimateResult::validate() const {
    parameters.validate();if(algorithm!="qpx.od-plane-stains/1" || !provenance || !hex(input_sha256) || !hex(model_sha256) || !roi.is_area() || !tiles || sampled<parameters.min_samples || sampled!=std::min<std::uint64_t>(eligible,parameters.sample_limit) || samples<eligible || samples>parameters.max_read_pixels || estimated_work_bytes>parameters.work_bytes || level<0 || std::size_t(level)>=provenance->source.levels.size() || downsample!=provenance->source.levels[level].downsample || !std::isfinite(angle_degrees) || angle_degrees<parameters.min_angle_degrees || angle_degrees>180 || !std::isfinite(residual_fraction) || residual_fraction<0 || residual_fraction>parameters.max_residual_fraction || !std::isfinite(elapsed_ms) || elapsed_ms<0) throw std::invalid_argument("invalid stain estimate metadata");
    provenance->validate();for(auto v:eigenvalues) if(!std::isfinite(v) || v<0) throw std::invalid_argument("invalid stain estimate spectrum");const auto model=transform();if(stain_model_sha256(model)!=model_sha256) throw std::invalid_argument("stain model digest mismatch");
}
std::string stain_estimate_json(const StainEstimateResult& r) {
    r.validate();std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);const auto& p=r.parameters;
    out<<"{\"schema\":\"qpx.stain-estimate/1\",\"algorithm\":"<<std::quoted(r.algorithm)<<",\"input_sha256\":"<<std::quoted(r.input_sha256)<<",\"model_sha256\":"<<std::quoted(r.model_sha256)<<",\"level\":"<<r.level<<",\"downsample\":"<<r.downsample<<",\"roi_wkt\":"<<std::quoted(r.roi.wkt())<<",\"samples\":"<<r.samples<<",\"eligible\":"<<r.eligible<<",\"sampled\":"<<r.sampled<<",\"tiles\":"<<r.tiles<<",\"estimated_work_bytes\":"<<r.estimated_work_bytes<<",\"angle_degrees\":"<<r.angle_degrees<<",\"residual_fraction\":"<<r.residual_fraction<<",\"elapsed_ms\":"<<r.elapsed_ms;
    auto vector=[&](const ColorVector& v) {out<<'['<<v[0]<<','<<v[1]<<','<<v[2]<<']';};out<<",\"white\":";vector(r.white);out<<",\"eigenvalues\":";vector(r.eigenvalues);out<<",\"stains\":[";for(int i=0;i<3;++i) {if(i) out<<',';vector(r.stains[i]);}
    out<<"],\"parameters\":{\"min_od\":"<<p.min_od<<",\"trim_quantile\":"<<p.trim_quantile<<",\"min_angle_degrees\":"<<p.min_angle_degrees<<",\"max_residual_fraction\":"<<p.max_residual_fraction<<",\"sample_limit\":"<<p.sample_limit<<",\"min_samples\":"<<p.min_samples<<",\"tile_size\":"<<p.tile_size<<",\"work_bytes\":"<<p.work_bytes<<",\"seed\":"<<p.seed<<",\"max_read_pixels\":"<<p.max_read_pixels<<",\"level\":"<<p.level<<"},\"provenance\":"<<provenance_json(*r.provenance)<<"}\n";
    auto text=out.str();if(text.size()>16*1024*1024) throw std::length_error("stain estimate JSON exceeds 16 MiB");return text;
}
void save_stain_estimate(const StainEstimateResult& result,const std::string& path) {
    const auto text=stain_estimate_json(result),destination=detail::export_path(path);
    auto guard=[&] {if(!result.provenance->locator.empty()) {const auto& source=result.provenance->reader_source;detail::guard_file_output(result.provenance->locator,source && source->primary?source->primary->identity:std::string{},destination);}};
    guard();detail::AtomicExportFile file(destination,16*1024*1024);file.write(text.data(),text.size());guard();file.commit();
}
struct StainEstimateJob::Impl {
    struct Completion {std::optional<StainEstimateResult> result;std::string error;double elapsed_ms=0;};
    std::optional<StainEstimateRequest> request;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> completed{0};
    bool stale=false;
    AnalysisStats stats;
    std::optional<StainEstimateResult> result;
};
StainEstimateJob::StainEstimateJob():impl_(std::make_unique<Impl>()) {}
StainEstimateJob::~StainEstimateJob() {cancel();if(busy()) impl_->future.wait();}
bool StainEstimateJob::busy() const {return impl_->future.valid();}
std::optional<StainEstimateResult> StainEstimateJob::result() const {return impl_->result;}
void StainEstimateJob::cancel() {
    impl_->cancelled.store(true); impl_->result.reset();
    if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";
    if(!busy()) impl_->request.reset();
}
bool StainEstimateJob::request(const StainEstimateRequest& request) {
    if(busy()) return false;
    if(impl_->stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("analysis generation exhausted");
    // The worker captures no shared source/Document/Python ownership. This
    // immutable request is released only on the owner thread after joining.
    impl_->request=request; impl_->result.reset(); impl_->stale=false;
    impl_->cancelled.store(false); impl_->completed.store(0);
    ++impl_->stats.generation; impl_->stats.state="running"; impl_->stats.error.clear();
    impl_->stats.elapsed_ms=0; impl_->stats.completed_tiles=0; impl_->stats.total_tiles=request.total_tiles();
    try {
        impl_->future=std::async(std::launch::async,[state=impl_.get()] {
            Impl::Completion completion; const auto started=std::chrono::steady_clock::now();
            try {completion.result=StainEstimateKernel::run(*state->request,&state->cancelled,&state->completed);}
            catch(const std::exception& e) {completion.error=e.what();}
            completion.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            return completion;
        });
    } catch(...) {impl_->request.reset();impl_->stats.state="failed";impl_->stats.error="could not start stain estimation worker";throw;}
    return true;
}
AnalysisStats StainEstimateJob::poll(const Document& document,std::uint64_t selected_id) {
    if(impl_->request && !impl_->request->matches(document,selected_id)) {
        impl_->stale=true; impl_->cancelled.store(true); impl_->result.reset();
        impl_->stats.state=busy()?"cancelling":"stale";
    }
    impl_->stats.completed_tiles=impl_->completed.load(std::memory_order_relaxed);
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto completion=impl_->future.get(); impl_->stats.elapsed_ms=completion.elapsed_ms;
            impl_->stats.completed_tiles=impl_->completed.load(std::memory_order_relaxed);
            if(impl_->stale) impl_->stats.state="stale";
            else if(impl_->cancelled.load()) impl_->stats.state="cancelled";
            else if(!completion.result) {impl_->stats.state="failed";impl_->stats.error=std::move(completion.error);}
            else {impl_->result=std::move(completion.result);impl_->stats.state="ready";}
        } catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=e.what();}
    }
    if(!busy() && !impl_->result) impl_->request.reset();
    return impl_->stats;
}
void StainEstimateJob::await_completion() {if(impl_->future.valid()) impl_->future.wait();}
AnalysisStats StainEstimateJob::wait(const Document& document,std::uint64_t selected_id) {
    poll(document,selected_id); await_completion(); return poll(document,selected_id);
}
}
