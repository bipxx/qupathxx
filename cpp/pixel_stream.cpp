#include "qupathxx/pixel_stream.hpp"
#include "pixel_workflow_snapshot.hpp"
#include "pixel_prediction_grid.hpp"
#include "pixel_stream_digest.hpp"
#include "qupathxx/input_digest.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
namespace qpx {
namespace {
std::int64_t edge(double x,const ImageLevel& level,bool vertical=false) {
    const auto maximum=vertical?level.height:level.width;
    const auto v=std::ceil(static_cast<long double>(x)/level.downsample-.5L);
    return v<=0?0:v>=maximum?maximum:static_cast<std::int64_t>(v);
}
enum class State {Ready,Producing,Complete,Failed};
}
void PixelStreamOptions::validate() const {
    prediction.validate();if(block_size<16 || block_size>512 || !max_pixels || max_pixels>1000000000000000ULL || !max_blocks || max_blocks>100000000) throw std::invalid_argument("invalid pixel stream limits");
}
struct PixelPredictionStream::Impl {
    std::optional<PixelPredictionRequest> prototype;
    std::int64_t x=0,y=0,w=0,h=0;std::uint64_t columns=0,blocks=0;std::size_t block_size=0,work=0;bool windowed=false;
    std::atomic<std::uint64_t> completed{0},valid{0},classified{0};std::array<std::atomic<std::uint64_t>,256> counts{};
    std::atomic<bool> cancelled{false},hashing{false};std::atomic<State> state{State::Ready};std::mutex consumer,publication;
};
PixelPredictionStream::PixelPredictionStream(const Document& doc,std::uint64_t id,const PixelClassifier& model,PixelStreamOptions options,std::optional<Rect> window):impl_(std::make_unique<Impl>()) {
    options.validate();auto& s=*impl_;const auto selected=pixel_classifier_level(doc,model);const auto& metadata=doc.metadata();const auto& grid=metadata.levels.at(selected);
    const auto b=detail::pixel_prediction_window(doc.annotation(id).geometry.bounds(),window);
    s.x=edge(b.x,grid);s.y=edge(b.y,grid,true);s.w=edge(b.x+b.width,grid)-s.x;s.h=edge(b.y+b.height,grid,true)-s.y;
    if(s.w<=0 || s.h<=0) throw std::invalid_argument("pixel stream contains no sampled pixel centers");
    if(std::uint64_t(s.w)>options.max_pixels/std::uint64_t(s.h)) throw std::length_error("pixel stream exceeds scanned-pixel limit");
    s.columns=(std::uint64_t(s.w)+options.block_size-1)/options.block_size;const auto rows=(std::uint64_t(s.h)+options.block_size-1)/options.block_size;
    if(s.columns>options.max_blocks/rows) throw std::length_error("pixel stream exceeds block limit");s.blocks=s.columns*rows;s.block_size=options.block_size;s.windowed=window.has_value();
    // The prototype keeps source/authority ownership and the worst-sized output
    // grid. Reserve the second immutable Data snapshot and producer bookkeeping
    // before letting the existing kernel calculate its remaining allocation cap.
    const auto reserve=sizeof(Impl)+sizeof(PixelPredictionRequest::Data)+metadata.levels.size()*sizeof(ImageLevel)+doc.annotation(id).class_name.size()+65536;
    if(options.prediction.work_bytes<=reserve+1024*1024) throw std::length_error("pixel stream bookkeeping exceeds memory budget");
    auto prediction=options.prediction;prediction.work_bytes-=reserve;
    const auto width=std::min<std::int64_t>(s.w,options.block_size),height=std::min<std::int64_t>(s.h,options.block_size);
    s.prototype.emplace(doc,id,model,prediction,Rect{s.x*grid.downsample,s.y*grid.downsample,width*grid.downsample,height*grid.downsample});
    const auto actual=s.prototype->level_bounds();if(actual.x!=s.x || actual.y!=s.y || actual.width!=width || actual.height!=height) throw std::invalid_argument("pixel stream grid is not representable at source coordinates");
    s.work=reserve+s.prototype->estimated_work_bytes();
    for(auto& count:s.counts) count.store(0);
}
PixelPredictionStream::~PixelPredictionStream()=default;
std::string PixelPredictionStream::input_sha256(const std::atomic<bool>* cancelled) const {
    auto& s=*impl_;std::unique_lock<std::mutex> lock(s.consumer,std::try_to_lock);if(!lock.owns_lock()) throw std::logic_error("pixel stream already has an active consumer");
    auto stop=[&] {if(s.cancelled.load() || (cancelled && cancelled->load())) throw std::runtime_error("pixel stream input digest cancelled");};stop();if(s.state.load()==State::Failed) throw std::logic_error("pixel stream has failed");if(s.prototype->source_->closed()) throw std::runtime_error("pixel stream source is closed");
    struct Reset {std::atomic<bool>& flag;~Reset(){flag.store(false);}} reset{s.hashing};s.hashing.store(true);Sha256 hash;detail::pixel_stream_input_header(hash,s.prototype->data_->model.sha256(),s.blocks);
    for(std::uint64_t i=0;i<s.blocks;++i) {
        stop();auto request=*s.prototype;auto data=std::make_shared<PixelPredictionRequest::Data>(*request.data_);
        data->x=s.x+static_cast<std::int64_t>(i%s.columns)*s.block_size;data->y=s.y+static_cast<std::int64_t>(i/s.columns)*s.block_size;
        data->w=std::min<std::int64_t>(s.block_size,s.x+s.w-data->x);data->h=std::min<std::int64_t>(s.block_size,s.y+s.h-data->y);data->windowed=true;
        data->tiles=((std::uint64_t(data->w)+data->options.tile_size-1)/data->options.tile_size)*((std::uint64_t(data->h)+data->options.tile_size-1)/data->options.tile_size);request.data_=std::move(data);
        const auto input=qpx::input_sha256(request,cancelled?cancelled:&s.cancelled);detail::pixel_stream_input_block(hash,i,input);
    }
    stop();return hash.finish();
}
std::shared_ptr<const PixelPredictionResult> PixelPredictionStream::next() {
    auto& s=*impl_;std::unique_lock<std::mutex> lock(s.consumer,std::try_to_lock);if(!lock.owns_lock()) throw std::logic_error("pixel stream already has an active consumer");
    if(s.cancelled.load()) throw std::runtime_error("pixel stream cancelled");
    if(s.state.load()==State::Failed) throw std::logic_error("pixel stream has failed");
    const auto index=s.completed.load();if(index==s.blocks) return {};
    s.state.store(State::Producing);
    try {
        if(s.prototype->source_->closed()) throw std::runtime_error("pixel stream source is closed");
        auto request=*s.prototype;auto data=std::make_shared<PixelPredictionRequest::Data>(*request.data_);
        data->x=s.x+static_cast<std::int64_t>(index%s.columns)*s.block_size;data->y=s.y+static_cast<std::int64_t>(index/s.columns)*s.block_size;
        data->w=std::min<std::int64_t>(s.block_size,s.x+s.w-data->x);data->h=std::min<std::int64_t>(s.block_size,s.y+s.h-data->y);data->windowed=true;
        data->tiles=((std::uint64_t(data->w)+data->options.tile_size-1)/data->options.tile_size)*((std::uint64_t(data->h)+data->options.tile_size-1)/data->options.tile_size);request.data_=std::move(data);
        auto result=predict_pixel_region(request,&s.cancelled);std::lock_guard<std::mutex> publication(s.publication);if(s.cancelled.load()) throw std::runtime_error("pixel stream cancelled");
        s.valid.fetch_add(result->valid_pixels());s.classified.fetch_add(result->classified_pixels());for(std::size_t i=0;i<result->class_counts().size();++i) s.counts[i].fetch_add(result->class_counts()[i]);
        s.completed.store(index+1);s.state.store(index+1==s.blocks?State::Complete:State::Ready);return result;
    }catch(...) {s.state.store(State::Failed);throw;}
}
void PixelPredictionStream::cancel() {std::lock_guard<std::mutex> publication(impl_->publication);if(impl_->state.load()!=State::Complete) impl_->cancelled.store(true);}
bool PixelPredictionStream::busy() const {return impl_->state.load()==State::Producing || impl_->hashing.load();}
bool PixelPredictionStream::matches(const Document& doc) const {return impl_->prototype->matches(doc);}
std::string PixelPredictionStream::state() const {if(impl_->cancelled.load()) return busy()?"cancelling":"cancelled";if(impl_->hashing.load()) return "hashing";switch(impl_->state.load()) {case State::Ready:return "ready";case State::Producing:return "producing";case State::Complete:return "complete";default:return "failed";}}
Rect PixelPredictionStream::level_bounds() const {const auto& s=*impl_;return {double(s.x),double(s.y),double(s.w),double(s.h)};}
std::int32_t PixelPredictionStream::level() const {return impl_->prototype->level();}
double PixelPredictionStream::downsample() const {return impl_->prototype->downsample();}
bool PixelPredictionStream::windowed() const {return impl_->windowed;}
std::uint64_t PixelPredictionStream::object_id() const {return impl_->prototype->object_id();}
std::uint64_t PixelPredictionStream::document_revision() const {return impl_->prototype->document_revision();}
std::uint64_t PixelPredictionStream::total_blocks() const {return impl_->blocks;}
std::uint64_t PixelPredictionStream::completed_blocks() const {return impl_->completed.load();}
std::uint64_t PixelPredictionStream::valid_pixels() const {return impl_->valid.load();}
std::uint64_t PixelPredictionStream::classified_pixels() const {return impl_->classified.load();}
std::vector<std::uint64_t> PixelPredictionStream::class_counts() const {std::vector<std::uint64_t> out(impl_->prototype->data_->model.classes().size());for(std::size_t i=0;i<out.size();++i) out[i]=impl_->counts[i].load();return out;}
std::size_t PixelPredictionStream::estimated_work_bytes() const {return impl_->work;}
}
