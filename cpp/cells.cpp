#include "qupathxx/cells.hpp"
#include "cell_contours.hpp"
#include "cell_numeric.hpp"
#include "sha256.hpp"
#include <chrono>
#include <cstring>
#include <future>
#include <map>
#include <numeric>
#include <tuple>
namespace qpx {
namespace {
using cell_detail::checkpoint;
std::size_t estimate(const TissueRequest& request,const CellParameters& p) {
    const auto b=request.level_bounds();const auto n=static_cast<std::uint64_t>(b.width)*static_cast<std::uint64_t>(b.height);
    const auto bytes=80ull*n+40ull*static_cast<std::uint64_t>(std::max(b.width,b.height))+1024ull*std::min<std::uint64_t>(n,p.max_cells)+16ull*p.tile_size*p.tile_size+65536+(has_geos()?2ull*p.contours.work_bytes:0);
    if(bytes>p.work_bytes) throw std::length_error("cell raster/contours exceed work budget; select a coarser level or smaller ROI");return static_cast<std::size_t>(bytes);
}
TissueRequest choose(const Document& doc,std::uint64_t id,const StainTransform& transform,const CellParameters& p) {
    p.validate();TissueParameters t;t.feature=p.feature;t.threshold=p.threshold;t.tile_size=p.tile_size;t.max_components=p.max_cells;t.work_bytes=p.work_bytes;
    if(p.level>=0) {t.level=p.level;TissueRequest r(doc,id,transform,t);estimate(r,p);return r;}
    std::optional<TissueRequest> best;
    for(std::size_t i=0;i<doc.metadata().levels.size();++i) {
        t.level=static_cast<std::int32_t>(i);
        try {TissueRequest r(doc,id,transform,t);estimate(r,p);if(!best || r.downsample()<best->downsample()) best=std::move(r);}
        catch(const std::length_error&) {} // Try a coarser native level, never arbitrary resampling.
        catch(const std::invalid_argument& e) {if(std::string(e.what()).find("ROI contains no pixel centers")==std::string::npos) throw;}
    }
    if(!best) throw std::length_error("no native cell sampling grid fits the work budget and ROI");return *best;
}
template<class F> void neighbors(std::size_t i,std::size_t width,std::size_t height,int connectivity,F f) {
    const auto x=static_cast<std::int64_t>(i%width),y=static_cast<std::int64_t>(i/width);
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if((!dx && !dy) || (connectivity==4 && dx && dy)) continue;
        const auto xx=x+dx,yy=y+dy;if(xx>=0 && yy>=0 && xx<std::int64_t(width) && yy<std::int64_t(height)) f(static_cast<std::size_t>(yy)*width+static_cast<std::size_t>(xx));
    }
}
struct Peak {std::uint32_t index;double height;};
struct Node {double cost;std::uint64_t order;std::uint32_t index;};
struct Later {bool operator()(const Node& a,const Node& b) const {return std::tie(a.cost,a.order)>std::tie(b.cost,b.order);}};
std::vector<std::uint32_t> watershed(const std::vector<std::uint8_t>& mask,const std::vector<double>& distance,
    std::size_t width,std::size_t height,const CellParameters& p,std::vector<CellMarker>& markers,std::size_t& raw_peaks,const std::atomic<bool>* cancelled) {
    const auto n=mask.size();std::vector<Peak> peaks;peaks.reserve(std::min(n,p.max_cells));
    {
        std::vector<std::uint8_t> visited(n);std::vector<std::uint32_t> queue;queue.reserve(n);
        for(std::size_t i=0;i<n;++i) {
            if((i&4095)==0) checkpoint(cancelled);if(!mask[i] || visited[i]) continue;
            queue.clear();queue.push_back(static_cast<std::uint32_t>(i));visited[i]=1;bool higher=false;
            for(std::size_t at=0;at<queue.size();++at) {
                if((at&4095)==0) checkpoint(cancelled);
                neighbors(queue[at],width,height,p.connectivity,[&](std::size_t j){
                    if(!mask[j]) return;if(distance[j]>distance[i]) higher=true;
                    if(distance[j]==distance[i] && !visited[j]) {visited[j]=1;queue.push_back(static_cast<std::uint32_t>(j));}
                });
            }
            if(!higher && distance[i]>=p.min_peak_radius*p.min_peak_radius) {
                if(peaks.size()>=p.max_cells) throw std::length_error("cell regional-maximum candidate limit exceeded");
                peaks.push_back({static_cast<std::uint32_t>(i),distance[i]});
            }
        }
    }
    raw_peaks=peaks.size();std::sort(peaks.begin(),peaks.end(),[](const Peak& a,const Peak& b){return a.height!=b.height?a.height>b.height:a.index<b.index;});
    std::map<std::pair<std::int64_t,std::int64_t>,std::vector<Peak>> bins;std::vector<Peak> kept;kept.reserve(peaks.size());
    const auto spacing=std::max(1.,p.min_seed_distance);
    for(const auto& peak:peaks) {
        checkpoint(cancelled);const auto x=double(peak.index%width),y=double(peak.index/width);
        const auto bx=static_cast<std::int64_t>(std::floor(x/spacing)),by=static_cast<std::int64_t>(std::floor(y/spacing));bool close=false;
        if(p.min_seed_distance>0) for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            const auto found=bins.find({bx+dx,by+dy});if(found==bins.end()) continue;
            for(const auto& old:found->second) {const auto xx=x-double(old.index%width),yy=y-double(old.index/width);if(xx*xx+yy*yy<p.min_seed_distance*p.min_seed_distance) close=true;}
        }
        if(!close) {bins[{bx,by}].push_back(peak);kept.push_back(peak);}
    }
    std::sort(kept.begin(),kept.end(),[](const Peak& a,const Peak& b){return a.index<b.index;});
    std::vector<std::uint32_t> labels(n);std::vector<Node> heap;heap.reserve(n);std::uint64_t order=0;
    for(const auto& peak:kept) {
        const auto label=static_cast<std::uint32_t>(markers.size()+1);markers.push_back({label,0,peak.index,{},std::sqrt(peak.height)});labels[peak.index]=label;
        heap.push_back({-peak.height,order++,peak.index});std::push_heap(heap.begin(),heap.end(),Later{});
    }
    std::size_t processed=0;
    while(!heap.empty()) {
        if((processed++&4095)==0) checkpoint(cancelled);std::pop_heap(heap.begin(),heap.end(),Later{});const auto next=heap.back();heap.pop_back();
        neighbors(next.index,width,height,p.connectivity,[&](std::size_t j){if(mask[j] && !labels[j]) {
            labels[j]=labels[next.index];heap.push_back({std::max(next.cost,-distance[j]),order++,static_cast<std::uint32_t>(j)});std::push_heap(heap.begin(),heap.end(),Later{});
        }});
    }
    std::vector<std::uint64_t> counts(markers.size()+1);for(auto label:labels) ++counts[label];std::uint32_t dense=0;
    for(auto& marker:markers) {const auto count=counts[marker.label];if(count>=p.min_nucleus_pixels && (!p.max_nucleus_pixels || count<=p.max_nucleus_pixels)) marker.result_label=++dense;}
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);if(labels[i]) labels[i]=markers[labels[i]-1].result_label;}
    return labels;
}
}
void CellParameters::validate() const {
    TissueParameters t;t.feature=feature;t.threshold=threshold;t.level=level;t.tile_size=tile_size;t.work_bytes=work_bytes;t.max_components=max_cells;t.connectivity=connectivity;t.validate();
    if(!std::isfinite(sigma) || sigma<0 || sigma>16 || !std::isfinite(min_peak_radius) || min_peak_radius<0 || min_peak_radius>1000000 ||
       !std::isfinite(min_seed_distance) || min_seed_distance<0 || min_seed_distance>1000000 || !std::isfinite(cell_expansion) || cell_expansion<0 || cell_expansion>1000000)
        throw std::invalid_argument("invalid cell Gaussian/radius/spacing parameters");
    if(!min_nucleus_pixels || (max_nucleus_pixels && max_nucleus_pixels<min_nucleus_pixels)) throw std::invalid_argument("invalid nucleus size range");
    contours.validate();if(!contours.clip_to_roi) throw std::invalid_argument("cell contours must clip to their source ROI");
}
CellRequest::CellRequest(const Document& doc,std::uint64_t id,const StainTransform& transform,CellParameters p):
    source_(choose(doc,id,transform,p)),parameters_(p),reader_(doc.server()),transform_(transform),object_revision_(doc.annotation(id).revision),document_revision_(doc.revision()) {work_=estimate(source_,p);}
std::size_t CellResult::retained_bytes() const {return sizeof(CellResult)+markers_.capacity()*sizeof(CellMarker)+features_.capacity()*sizeof(CellFeature)+(nuclei_?nuclei_->retained_bytes():0)+(cells_?cells_->retained_bytes():0)+(nucleus_contours_?nucleus_contours_->retained_bytes():0)+(cell_contours_?cell_contours_->retained_bytes():0);}
struct CellKernel {
    static std::shared_ptr<const TissueResult> raster(const CellRequest& request,std::vector<std::uint32_t> labels,std::size_t count,const std::string& digest,std::uint64_t samples,std::uint64_t threshold,bool body,const std::atomic<bool>* cancelled) {
        auto r=std::shared_ptr<TissueResult>(new TissueResult());r->provenance_=request.source_.provenance();r->algorithm_=body?"qpx.cell-bodies/1":"qpx.cell-nuclei/1";r->input_sha256_=digest;
        r->bounds_=request.bounds();r->level_bounds_=request.level_bounds();r->level_=request.level();r->downsample_=request.downsample();r->roi_=request.roi();
        r->image_bounds_={0,0,double(request.reader_->metadata().width),double(request.reader_->metadata().height)};r->parameters_=request.source_.parameters();
        r->white_=request.transform_.white();r->stains_=request.transform_.stains();r->object_id_=request.object_id();r->object_revision_=request.object_revision_;r->document_revision_=request.document_revision_;
        r->samples_=samples;r->threshold_pixels_=threshold;r->cleaned_pixels_=threshold;r->raw_components_=count;r->estimated_work_bytes_=request.work_;r->labels_=std::move(labels);
        struct Bounds {std::uint64_t n=0,x=0,y=0;std::size_t x0=std::numeric_limits<std::size_t>::max(),y0=std::numeric_limits<std::size_t>::max(),x1=0,y1=0;};
        std::vector<Bounds> stats(count);const auto width=static_cast<std::size_t>(r->width());
        for(std::size_t i=0;i<r->labels_.size();++i) {if((i&4095)==0) checkpoint(cancelled);const auto label=r->labels_[i];if(!label) continue;auto& s=stats.at(label-1);const auto x=i%width,y=i/width;++s.n;s.x+=x;s.y+=y;s.x0=std::min(s.x0,x);s.y0=std::min(s.y0,y);s.x1=std::max(s.x1,x);s.y1=std::max(s.y1,y);}
        r->components_.reserve(count);const auto scale=r->downsample_;const auto b=r->level_bounds_;
        for(std::size_t i=0;i<count;++i) {const auto& s=stats[i];if(!s.n) throw std::logic_error("empty retained cell label");r->foreground_pixels_+=s.n;
            r->components_.push_back({static_cast<std::uint32_t>(i+1),s.n,{(b.x+s.x0)*scale,(b.y+s.y0)*scale,double(s.x1-s.x0+1)*scale,double(s.y1-s.y0+1)*scale},{(b.x+double(s.x)/double(s.n)+.5)*scale,(b.y+double(s.y)/double(s.n)+.5)*scale}});
        }
        if(body) r->threshold_pixels_=r->cleaned_pixels_=r->foreground_pixels_;
        return r;
    }
    static void contours(CellResult& result,const std::atomic<bool>* cancelled) {
        const auto& p=result.parameters_;const auto count=result.features_.size();
        if(has_geos()) {
            result.nucleus_contours_=trace_tissue(*result.nuclei_,p.contours,cancelled);result.cell_contours_=trace_tissue(*result.cells_,p.contours,cancelled);
            if(result.nucleus_contours_->components().size()!=count || result.cell_contours_->components().size()!=count) throw std::runtime_error("cell contours did not preserve every label");
            for(std::size_t i=0;i<count;++i) {checkpoint(cancelled);const auto& body=result.cell_contours_->components()[i];const auto& nucleus=result.nucleus_contours_->components()[i];
                if(body.label!=i+1 || nucleus.label!=i+1) throw std::runtime_error("cell contour label validation failed");
                if(!body.geometry.covers(nucleus.geometry)) {
                    // Independent ROI intersections can leave zero-area boundary
                    // remnants that fail GEOS Covers. Normalize this fresh body
                    // before publication, without relaxing containment predicates.
                    if(nucleus.geometry.boolean_operation(body.geometry,BooleanOp::Difference)) throw std::runtime_error("cell nucleus extends outside its body contour");
                    auto united=body.geometry.boolean_operation(nucleus.geometry,BooleanOp::Union);checkpoint(cancelled);
                    if(!united || !united->covers(body.geometry) || !united->covers(nucleus.geometry) || !result.nuclei_->roi().covers(*united)) throw std::runtime_error("cell contour boundary normalization failed");
                    std::size_t before=0,after=0;for(const auto& path:body.geometry.paths()) before+=path.size();for(const auto& path:united->paths()) after+=path.size();
                    // trace_tissue allocated mutable native storage; this result
                    // has not escaped the worker or acquired any external owner.
                    auto repaired=std::const_pointer_cast<TissueContours>(result.cell_contours_);
                    const auto vertices=repaired->vertices_-before+after,retained=repaired->retained_-body.geometry.payload_bytes()+united->payload_bytes();
                    if(vertices>p.contours.max_vertices || retained>p.contours.work_bytes) throw std::length_error("cell contour normalization exceeds geometry budget");
                    repaired->components_[i].geometry=std::move(*united);repaired->vertices_=vertices;repaired->retained_=retained;
                }
            }
        }
    }
    static std::shared_ptr<const CellResult> run(const CellRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr,std::atomic<int>* phase=nullptr) {
        checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto& p=request.parameters_;const auto grid=request.level_bounds();
        const auto width=static_cast<std::size_t>(grid.width),height=static_cast<std::size_t>(grid.height),n=width*height;const auto scale=request.downsample();
        auto result=std::shared_ptr<CellResult>(new CellResult());result->parameters_=p;result->ticket_=request.ticket_;
        std::vector<float> channels(n*3);std::vector<std::uint8_t> rgb(n*3),domain(n),mask(n);std::vector<double> smooth(n);
        Sha256 digest;const std::string format=request.level()==0?"qpx.rgba-tiles/1":"qpx.rgba-level-tiles/1";digest.update(format.data(),format.size());const auto& metadata=request.reader_->metadata();
        digest.integer(metadata.width);digest.integer(metadata.height);
        if(request.level()!=0) {std::uint64_t bits;std::memcpy(&bits,&scale,8);const auto& l=metadata.levels[request.level()];digest.integer(request.level());digest.integer(l.width);digest.integer(l.height);digest.integer(bits);}
        const auto x0=static_cast<std::int64_t>(grid.x),y0=static_cast<std::int64_t>(grid.y),x1=x0+static_cast<std::int64_t>(width),y1=y0+static_cast<std::int64_t>(height);
        for(auto v:{x0,y0,x1,y1}) digest.integer(v);digest.integer(p.tile_size);std::uint64_t tiles=0,samples=0,threshold=0;
        if(phase) phase->store(1);
        for(auto y=y0;y<y1;) {const auto h=std::min<std::int64_t>(p.tile_size,y1-y);
            for(auto x=x0;x<x1;) {checkpoint(cancelled);const auto w=std::min<std::int64_t>(p.tile_size,x1-x);
                const auto rx=static_cast<std::int64_t>(std::round(static_cast<long double>(x)*scale)),ry=static_cast<std::int64_t>(std::round(static_cast<long double>(y)*scale));
                const auto pixels=request.reader_->read_region({rx,ry,request.level(),w,h});if(pixels.width()!=w || pixels.height()!=h) throw std::runtime_error("reader returned incorrect cell tile dimensions");
                if(request.level()!=0) {digest.integer(x);digest.integer(y);}for(auto v:{rx,ry,w,h}) digest.integer(v);digest.update(pixels.bytes().data(),pixels.bytes().size());
                const auto values=p.feature==TissueFeature::OpticalDensitySum?request.transform_.optical_densities(pixels,cancelled):request.transform_.deconvolve(pixels,cancelled);
                for(std::int64_t row=0;row<h;++row) {checkpoint(cancelled);for(std::int64_t col=0;col<w;++col) {
                    const auto src=static_cast<std::size_t>(row*w+col),dst=static_cast<std::size_t>(y-y0+row)*width+static_cast<std::size_t>(x-x0+col);
                    if(!std::isfinite(values.values()[src*3]) || !request.roi().contains({(double(x+col)+.5)*scale,(double(y+row)+.5)*scale})) continue;
                    domain[dst]=1;++samples;for(std::size_t c=0;c<3;++c) {channels[dst*3+c]=values.values()[src*3+c];rgb[dst*3+c]=pixels.bytes()[src*4+c];}
                    smooth[dst]=p.feature==TissueFeature::OpticalDensitySum?double(channels[dst*3])+double(channels[dst*3+1])+double(channels[dst*3+2]):channels[dst*3+static_cast<int>(p.feature)-1];
                }}
                ++tiles;if(completed) completed->store(tiles,std::memory_order_relaxed);x+=w;
            }y+=h;
        }
        if(!samples) throw std::invalid_argument("cell ROI contains no opaque pixel centers");const auto sha=digest.finish();
        if(phase) phase->store(2);cell_detail::gaussian(smooth,domain,width,height,p.sigma,cancelled);
        for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);mask[i]=domain[i] && smooth[i]>=p.threshold;threshold+=mask[i]!=0;}
        std::vector<double>().swap(smooth);std::vector<double> distance(n);for(std::size_t i=0;i<n;++i) distance[i]=mask[i]?cell_detail::infinity:0;
        if(phase) phase->store(3);cell_detail::distance(distance,width,height,true,nullptr,cancelled);
        if(phase) phase->store(4);auto nuclei=watershed(mask,distance,width,height,p,result->markers_,result->raw_peaks_,cancelled);
        std::size_t count=0;for(auto& marker:result->markers_) {marker.center={(grid.x+marker.pixel_index%width+.5)*scale,(grid.y+marker.pixel_index/width+.5)*scale};count=std::max<std::size_t>(count,marker.result_label);}
        std::vector<std::uint8_t>().swap(mask);std::vector<std::uint32_t> bodies(n);
        if(phase) phase->store(5);
        if(count && p.cell_expansion>0) {
            for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);distance[i]=nuclei[i]?0:cell_detail::infinity;}
            std::vector<std::uint32_t> nearest;cell_detail::distance(distance,width,height,false,&nearest,cancelled);
            const auto radius=p.cell_expansion*p.cell_expansion;
            for(std::size_t i=0;i<n;++i) {if((i&4095)==0) checkpoint(cancelled);if(domain[i] && distance[i]<=radius && nearest[i]!=std::numeric_limits<std::uint32_t>::max()) bodies[i]=nuclei[nearest[i]];}
        } else bodies=nuclei;
        std::vector<double>().swap(distance);
        result->features_.resize(count);std::vector<ColorVector> nrgb(count),brgb(count),nch(count),bch(count),cytoch(count),ncorr(count),bcorr(count),cytocorr(count);
        auto add=[](double v,double& sum,double& correction) {const auto adjusted=v-correction,next=sum+adjusted;correction=(next-sum)-adjusted;sum=next;};
        for(std::size_t i=0;i<n;++i) {
            if((i&4095)==0) checkpoint(cancelled);
            const auto a=nuclei[i],b=bodies[i];if(a && a!=b) throw std::logic_error("cell expansion lost a nucleus pixel");
            if(a) {auto& f=result->features_[a-1];++f.nucleus_pixels;for(int c=0;c<3;++c) {nrgb[a-1][c]+=rgb[i*3+c];add(channels[i*3+c],nch[a-1][c],ncorr[a-1][c]);}}
            if(b) {auto& f=result->features_[b-1];++f.cell_pixels;for(int c=0;c<3;++c) {brgb[b-1][c]+=rgb[i*3+c];add(channels[i*3+c],bch[b-1][c],bcorr[b-1][c]);if(!a) add(channels[i*3+c],cytoch[b-1][c],cytocorr[b-1][c]);}}
        }
        for(std::size_t i=0;i<count;++i) {auto& f=result->features_[i];f.label=static_cast<std::uint32_t>(i+1);const auto cyto=f.cell_pixels-f.nucleus_pixels;
            if(cyto) {f.cytoplasm_rgb=ColorVector{};f.cytoplasm_channels=ColorVector{};}
            for(int c=0;c<3;++c) {f.nucleus_rgb[c]=nrgb[i][c]/double(f.nucleus_pixels);f.cell_rgb[c]=brgb[i][c]/double(f.cell_pixels);f.nucleus_channels[c]=nch[i][c]/double(f.nucleus_pixels);f.cell_channels[c]=bch[i][c]/double(f.cell_pixels);
                if(cyto) {(*f.cytoplasm_rgb)[c]=(brgb[i][c]-nrgb[i][c])/double(cyto);(*f.cytoplasm_channels)[c]=cytoch[i][c]/double(cyto);}}
        }
        std::vector<float>().swap(channels);std::vector<std::uint8_t>().swap(rgb);std::vector<std::uint8_t>().swap(domain);
        result->nuclei_=raster(request,std::move(nuclei),count,sha,samples,threshold,false,cancelled);
        result->cells_=raster(request,std::move(bodies),count,sha,samples,samples,true,cancelled);
        if(phase) phase->store(6);contours(*result,cancelled);
        checkpoint(cancelled);result->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();if(phase) phase->store(7);return result;
    }
};
void restore_cell_contours(CellResult& result,const std::atomic<bool>* cancelled) {CellKernel::contours(result,cancelled);}
std::shared_ptr<const CellResult> detect_cells(const CellRequest& r,const std::atomic<bool>* c) {return CellKernel::run(r,c);}
struct CellJob::Impl {
    struct Completion {std::shared_ptr<const CellResult> result;std::string error;double elapsed=0;};
    std::optional<CellRequest> request;std::future<Completion> future;std::atomic<bool> cancelled{false};std::atomic<std::uint64_t> completed{0};std::atomic<int> phase{0};
    CellStats stats;std::uint64_t verified_record=0;bool stale=false,published=false;std::shared_ptr<const CellResult> result;
};
CellJob::CellJob():impl_(std::make_unique<Impl>()) {} CellJob::~CellJob() {cancel();if(busy()) impl_->future.wait();}
bool CellJob::busy() const {return impl_->future.valid();}std::shared_ptr<const CellResult> CellJob::result() const {return impl_->result;}bool CellJob::published() const {return impl_->published;}
void CellJob::cancel() {impl_->verified_record=0;impl_->cancelled.store(true);impl_->result.reset();impl_->published=false;if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";if(!busy()) impl_->request.reset();}
bool CellJob::request(const CellRequest& request) {
    if(busy()) return false;if(impl_->stats.generation==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("cell generation exhausted");
    impl_->verified_record=0;impl_->request=request;impl_->result.reset();impl_->cancelled.store(false);impl_->completed.store(0);impl_->phase.store(0);impl_->stale=impl_->published=false;
    ++impl_->stats.generation;impl_->stats.state="running";impl_->stats.error.clear();impl_->stats.elapsed_ms=0;impl_->stats.total_tiles=request.total_tiles();impl_->stats.completed_tiles=0;
    try {impl_->future=std::async(std::launch::async,[state=impl_.get()] {Impl::Completion c;const auto begin=std::chrono::steady_clock::now();try {c.result=CellKernel::run(*state->request,&state->cancelled,&state->completed,&state->phase);} catch(const std::exception& e) {c.error=e.what();}c.elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();return c;});}
    catch(...) {impl_->request.reset();impl_->stats.state="failed";impl_->stats.error="could not start cell worker";throw;}return true;
}
CellStats CellJob::poll(const Document& doc,std::uint64_t id) {
    if(impl_->request && !impl_->request->matches(doc,id)) {impl_->stale=true;impl_->cancelled.store(true);impl_->result.reset();impl_->published=false;impl_->stats.state=busy()?"cancelling":"stale";}
    impl_->stats.completed_tiles=impl_->completed.load();constexpr const char* phases[]={"idle","reading","smoothing","distance","watershed","expansion","contours","complete"};impl_->stats.phase=phases[impl_->phase.load()];
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto c=impl_->future.get();impl_->stats.elapsed_ms=c.elapsed;impl_->stats.completed_tiles=impl_->completed.load();
        if(impl_->stale) impl_->stats.state="stale";else if(impl_->cancelled.load()) impl_->stats.state="cancelled";else if(!c.result) {impl_->stats.state="failed";impl_->stats.error=std::move(c.error);}else {impl_->result=std::move(c.result);impl_->stats.state="ready";impl_->stats.phase="complete";}} catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=e.what();}}
    if(!busy() && !impl_->result) impl_->request.reset();return impl_->stats;
}
CellStats CellJob::wait(const Document& doc,std::uint64_t id) {poll(doc,id);if(busy()) impl_->future.wait();return poll(doc,id);}
std::vector<std::uint64_t> CellJob::keep_result(Document& doc,std::uint64_t id,std::string prefix,std::string classification) {
    if(poll(doc,id).state!="ready" || !impl_->result) throw std::logic_error("no current cells to publish");if(impl_->published) throw std::logic_error("cells already published; use Undo/Redo");
    auto ids=impl_->verified_record?doc.publish_cell_record(impl_->verified_record,std::move(prefix),std::move(classification)):
        doc.publish_cells(*impl_->request,impl_->result,std::move(prefix),std::move(classification));impl_->published=true;return ids;
}
std::uint64_t CellJob::keep_record(Document& doc,std::uint64_t id) {
    if(poll(doc,id).state!="ready" || !impl_->result || !impl_->request) throw std::logic_error("no current cell run to retain");
    return doc.keep_cells(*impl_->request,impl_->result);
}
bool CellJob::verify_record(Document& doc,std::uint64_t id) {
    const auto record=doc.cell_record(id);
    if(poll(doc,record->result->nuclei()->object_id()).state!="ready" || !impl_->result || !impl_->request) throw std::logic_error("no current cell run to verify");
    const bool verified=doc.verify_cells(id,*impl_->request,*impl_->result);impl_->verified_record=verified?id:0;return verified;
}
}
