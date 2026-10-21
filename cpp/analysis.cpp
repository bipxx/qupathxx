#include "qupathxx/analysis.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void check_cancelled(const std::atomic<bool>* c) {
    if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("analysis cancelled");
}
}
AnalysisRequest::AnalysisRequest(const Document& document,std::uint64_t id,const StainTransform& transform,
    AnalysisKind kind,bool nucleus,std::size_t tile_size):AnalysisRequest(document,id,transform,kind,nucleus,tile_size,false) {}
AnalysisRequest::AnalysisRequest(const Document& document,std::uint64_t id,const StainTransform& transform,
    AnalysisKind kind,bool nucleus,std::size_t tile_size,bool allow_empty_grid):identity_(document.analysis_identity_.token),source_(document.server()),
    object_id_(id),object_revision_(0),document_revision_(document.revision()),total_tiles_(0),kind_(kind),nucleus_(nucleus),
    tile_size_(tile_size),transform_(transform) {
    if(kind!=AnalysisKind::OpticalDensityMeans && kind!=AnalysisKind::StainMeans) throw std::invalid_argument("invalid analysis kind");
    if(tile_size<16 || tile_size>1024) throw std::invalid_argument("analysis tile size must be 16..1024");
    if(!source_ || source_->closed()) throw std::invalid_argument("analysis requires an open source");
    const auto a=document.annotation(id); object_revision_=a.revision;
    if(nucleus && !a.nucleus) throw std::invalid_argument("object has no nucleus");
    roi_=nucleus?*a.nucleus:a.geometry;
    if(!roi_.is_area()) throw std::invalid_argument("analysis requires an area ROI");
    const auto b=roi_.bounds(); const auto& metadata=source_->metadata();
    if(b.x<0 || b.y<0 || b.x+b.width>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
       b.y+b.height>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
       static_cast<long double>(b.x+b.width)>metadata.width || static_cast<long double>(b.y+b.height)>metadata.height)
        throw std::invalid_argument("analysis ROI must lie inside the image");
    x0_=static_cast<std::int64_t>(std::ceil(b.x-.5)); y0_=static_cast<std::int64_t>(std::ceil(b.y-.5));
    x1_=static_cast<std::int64_t>(std::ceil(b.x+b.width-.5)); y1_=static_cast<std::int64_t>(std::ceil(b.y+b.height-.5));
    if(x0_>=x1_ || y0_>=y1_) {
        if(allow_empty_grid) return;
        throw std::invalid_argument("ROI contains no pixel centers");
    }
    const auto w=static_cast<std::uint64_t>(x1_-x0_),h=static_cast<std::uint64_t>(y1_-y0_);
    if(w>std::numeric_limits<std::uint64_t>::max()/h) throw std::invalid_argument("analysis sample count overflow");
    total_tiles_=((w+tile_size-1)/tile_size)*((h+tile_size-1)/tile_size);
}
bool AnalysisRequest::matches(const Document& document,std::uint64_t selected_id) const {
    if(selected_id!=object_id_ || identity_!=document.analysis_identity_.token || source_.get()!=document.server().get() || source_->closed()) return false;
    const auto it=document.objects().find(object_id_);
    return it!=document.objects().end() && it->second.revision==object_revision_;
}
struct AnalysisKernel {
    static AnalysisResult run(const AnalysisRequest& request,const std::atomic<bool>* cancelled,std::atomic<std::uint64_t>* completed=nullptr) {
        const auto started=std::chrono::steady_clock::now(); check_cancelled(cancelled);
        AnalysisResult result;
        result.object_id=request.object_id_; result.object_revision=request.object_revision_; result.document_revision=request.document_revision_;
        result.kind=request.kind_; result.nucleus=request.nucleus_; result.white=request.transform_.white(); result.stains=request.transform_.stains();
        ColorVector sum{},correction{};
        Sha256 digest;
        constexpr char format[]="qpx.rgba-tiles/1";digest.update(format,sizeof(format)-1);
        digest.integer(static_cast<std::uint64_t>(request.source_->metadata().width));
        digest.integer(static_cast<std::uint64_t>(request.source_->metadata().height));
        for(auto value:{request.x0_,request.y0_,request.x1_,request.y1_}) digest.integer(static_cast<std::uint64_t>(value));
        digest.integer(request.tile_size_);
        const auto step=static_cast<std::int64_t>(request.tile_size_);
        for(auto y=request.y0_;y<request.y1_;) {
            const auto h=std::min(step,request.y1_-y);
            for(auto x=request.x0_;x<request.x1_;) {
                const auto w=std::min(step,request.x1_-x); check_cancelled(cancelled);
                const auto pixels=request.source_->read_region({x,y,0,w,h});
                if(pixels.width()!=w || pixels.height()!=h) throw std::runtime_error("reader returned incorrect analysis tile dimensions");
                for(auto value:{x,y,w,h}) digest.integer(static_cast<std::uint64_t>(value));
                digest.update(pixels.bytes().data(),pixels.bytes().size());
                const auto values=request.kind_==AnalysisKind::StainMeans?request.transform_.deconvolve(pixels,cancelled):request.transform_.optical_densities(pixels,cancelled);
                const auto& data=values.values();
                for(std::int64_t row=0;row<h;++row) {
                    check_cancelled(cancelled);
                    for(std::int64_t col=0;col<w;++col) {
                        const auto i=static_cast<std::size_t>((row*w+col)*3);
                        if(!std::isfinite(data[i])) continue;
                        if(request.roi_.kind()!=RoiKind::Rectangle && !request.roi_.contains({static_cast<double>(x+col)+.5,static_cast<double>(y+row)+.5})) continue;
                        for(std::size_t c=0;c<3;++c) {
                            const double v=data[i+c]-correction[c],next=sum[c]+v;
                            correction[c]=(next-sum[c])-v; sum[c]=next;
                        }
                        ++result.samples;
                    }
                }
                ++result.tiles; if(completed) completed->store(result.tiles,std::memory_order_relaxed);
                x+=w;
            }
            y+=h;
        }
        check_cancelled(cancelled);
        if(!result.samples) throw std::invalid_argument("ROI contains no opaque pixel centers");
        for(std::size_t c=0;c<3;++c) result.means[c]=sum[c]/static_cast<double>(result.samples);
        result.input_sha256=digest.finish();
        result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        return result;
    }
};
AnalysisResult analyze(const AnalysisRequest& request,const std::atomic<bool>* cancelled) {return AnalysisKernel::run(request,cancelled);}
struct AnalysisJob::Impl {
    struct Completion {std::optional<AnalysisResult> result;std::string error;double elapsed_ms=0;};
    std::optional<AnalysisRequest> request;
    std::future<Completion> future;
    std::atomic<bool> cancelled{false};
    std::atomic<std::uint64_t> completed{0};
    bool stale=false;
    AnalysisStats stats;
    std::optional<AnalysisResult> result;
};
AnalysisJob::AnalysisJob():impl_(std::make_unique<Impl>()) {}
AnalysisJob::~AnalysisJob() {cancel();if(busy()) impl_->future.wait();}
bool AnalysisJob::busy() const {return impl_->future.valid();}
std::optional<AnalysisResult> AnalysisJob::result() const {return impl_->result;}
void AnalysisJob::cancel() {
    impl_->cancelled.store(true); impl_->result.reset();
    if(impl_->request) impl_->stats.state=busy()?"cancelling":"cancelled";
    if(!busy()) impl_->request.reset();
}
bool AnalysisJob::request(const AnalysisRequest& request) {
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
            try {completion.result=AnalysisKernel::run(*state->request,&state->cancelled,&state->completed);}
            catch(const std::exception& e) {completion.error=e.what();}
            completion.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            return completion;
        });
    } catch(...) {impl_->request.reset();impl_->stats.state="failed";impl_->stats.error="could not start analysis worker";throw;}
    return true;
}
AnalysisStats AnalysisJob::poll(const Document& document,std::uint64_t selected_id) {
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
AnalysisStats AnalysisJob::wait(const Document& document,std::uint64_t selected_id) {
    poll(document,selected_id); if(busy()) impl_->future.wait(); return poll(document,selected_id);
}
std::uint64_t AnalysisJob::keep_result(Document& document,std::uint64_t selected_id) {
    if(poll(document,selected_id).state!="ready" || !impl_->request || !impl_->result) throw std::logic_error("no current analysis result to keep");
    return document.keep_analysis(*impl_->request,*impl_->result);
}
bool AnalysisJob::verify_result(Document& document,std::uint64_t id) {
    const auto record=document.analysis_record(id);
    if(poll(document,record.result.object_id).state!="ready" || !impl_->request || !impl_->result) throw std::logic_error("no current analysis result to verify");
    return document.verify_analysis(id,*impl_->request,*impl_->result);
}
AnalysisRequest AnalysisRequest::replay(const Document& document,std::uint64_t id) {
    const auto record=document.analysis_record(id);
    AnalysisRequest request(document,record.result.object_id,StainTransform::from_normalized(record.result.stains,record.result.white),record.result.kind,record.result.nucleus,record.tile_size);
    if(request.object_revision()!=record.result.object_revision || !request.roi().equals(record.roi)) throw std::invalid_argument("saved analysis object has changed");
    return request;
}
}
