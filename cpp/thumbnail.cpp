#include "qupathxx/thumbnail.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
constexpr std::uint64_t max_source=256ULL*1024*1024/4,max_output=16ULL*1024*1024;
void checkpoint(const std::atomic<bool>* cancel) {if(cancel && cancel->load()) throw std::runtime_error("thumbnail cancelled");}
// Exact a*b/d without a*b overflow, with b bounded to 4096. Used only where
// the result is bounded by the requested output dimension.
std::int64_t scaled_floor(std::int64_t a,std::int32_t b,std::int64_t d) {
    auto result=(a/d)*b;const auto remainder=a%d;std::int64_t accumulated=0;
    for(std::int32_t i=0;i<b;++i) {
        if(accumulated>=d-remainder) {accumulated-=d-remainder;++result;}
        else accumulated+=remainder;
    }
    return result;
}
}
void ThumbnailOptions::validate() const {
    if(max_width<1 || max_height<1 || max_width>4096 || max_height>4096)
        throw std::invalid_argument("thumbnail bounds must be 1..4096 pixels");
    if(!source_pixel_limit || source_pixel_limit>max_source || !output_pixel_limit || output_pixel_limit>max_output)
        throw std::invalid_argument("thumbnail source/output pixel limits must be positive and at most 67108864/16777216");
}
ThumbnailPlan plan_thumbnail(const ImageMetadata& metadata,ThumbnailOptions options) {
    options.validate();
    if(metadata.width<1 || metadata.height<1 || metadata.levels.empty() || metadata.levels.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
       metadata.pixel_type!=PixelType::UInt8 || metadata.layout!=PixelLayout::RGBA)
        throw std::invalid_argument("thumbnail requires positive dimensions and an RGBA8 pyramid");
    if(metadata.levels.front().width!=metadata.width || metadata.levels.front().height!=metadata.height || metadata.levels.front().downsample!=1)
        throw std::invalid_argument("thumbnail level zero must match base dimensions and downsample one");
    ThumbnailPlan plan;
    const auto w=metadata.width,h=metadata.height;
    if(w<=options.max_width && h<=options.max_height) {plan.width=w;plan.height=h;}
    else {
        const auto qw=w/options.max_width,qh=h/options.max_height;
        const bool width_limited=qw>qh || (qw==qh && (w%options.max_width)*options.max_height>=(h%options.max_height)*options.max_width);
        if(width_limited) {plan.width=options.max_width;plan.height=std::max<std::int64_t>(1,scaled_floor(h,options.max_width,w));}
        else {plan.height=options.max_height;plan.width=std::max<std::int64_t>(1,scaled_floor(w,options.max_height,h));}
    }
    plan.output_pixels=static_cast<std::uint64_t>(plan.width)*plan.height;
    if(plan.output_pixels>options.output_pixel_limit) throw std::length_error("thumbnail exceeds output pixel limit");
    bool found=false;
    for(std::size_t i=0;i<metadata.levels.size();++i) {
        const auto& level=metadata.levels[i];
        if(level.width<1 || level.height<1 || !std::isfinite(level.downsample) || level.downsample<1)
            throw std::invalid_argument("invalid thumbnail pyramid level");
        if(level.width<plan.width || level.height<plan.height || static_cast<std::uint64_t>(level.width)>options.source_pixel_limit/static_cast<std::uint64_t>(level.height)) continue;
        const auto pixels=static_cast<std::uint64_t>(level.width)*level.height;
        if(!found || pixels<plan.source_pixels) {
            found=true;plan.level=static_cast<std::int32_t>(i);plan.source_width=level.width;plan.source_height=level.height;
            plan.source_pixels=pixels;plan.level_downsample=level.downsample;
        }
    }
    if(!found) throw std::length_error("no adequate natural pyramid level fits the thumbnail source pixel limit");
    plan.downsample_x=static_cast<double>(w)/plan.width;plan.downsample_y=static_cast<double>(h)/plan.height;
    // Validate the exact request, including downstream level-zero extent limits.
    validate_region(metadata,{0,0,plan.level,plan.source_width,plan.source_height});
    region_byte_count(plan.width,plan.height);
    return plan;
}
std::shared_ptr<const ThumbnailResult> read_thumbnail(const ImageServer& server,ThumbnailOptions options,const std::atomic<bool>* cancel) {
    const auto start=Clock::now();checkpoint(cancel);const auto plan=plan_thumbnail(server.metadata(),options);checkpoint(cancel);
    auto source=server.read_region({0,0,plan.level,plan.source_width,plan.source_height});checkpoint(cancel);
    if(source.width()!=plan.source_width || source.height()!=plan.source_height || source.bytes().size()!=region_byte_count(plan.source_width,plan.source_height)) throw std::runtime_error("thumbnail reader returned unexpected dimensions or byte count");
    PixelBuffer output(plan.width,plan.height);const auto& input=source.bytes();auto& pixels=output.writable_bytes();
    // Integer overlap units avoid floating edge decisions. Per-output total
    // weight is source_width*source_height; even 255*255*weight fits uint64.
    const auto total_weight=plan.source_pixels;std::uint64_t visited=0;
    for(std::int64_t y=0;y<plan.height;++y) {
        checkpoint(cancel);const auto top=y*plan.source_height,bottom=(y+1)*plan.source_height;
        const auto first_y=top/plan.height,last_y=(bottom+plan.height-1)/plan.height;
        for(std::int64_t x=0;x<plan.width;++x) {
            const auto left=x*plan.source_width,right=(x+1)*plan.source_width;
            const auto first_x=left/plan.width,last_x=(right+plan.width-1)/plan.width;
            std::uint64_t alpha=0,channels[3]={0,0,0};
            for(auto sy=first_y;sy<last_y;++sy) {
                const auto wy=std::min(bottom,(sy+1)*plan.height)-std::max(top,sy*plan.height);
                for(auto sx=first_x;sx<last_x;++sx) {
                    if((++visited&4095)==0) checkpoint(cancel);
                    const auto wx=std::min(right,(sx+1)*plan.width)-std::max(left,sx*plan.width);
                    const auto weight=static_cast<std::uint64_t>(wx)*wy;
                    const auto index=static_cast<std::size_t>((sy*plan.source_width+sx)*4);
                    const auto weighted_alpha=weight*input[index+3];alpha+=weighted_alpha;
                    for(unsigned c=0;c<3;++c) channels[c]+=weighted_alpha*input[index+c];
                }
            }
            const auto index=static_cast<std::size_t>((y*plan.width+x)*4);
            pixels[index+3]=static_cast<std::uint8_t>((alpha+total_weight/2)/total_weight);
            for(unsigned c=0;c<3;++c) pixels[index+c]=pixels[index+3]?static_cast<std::uint8_t>((channels[c]+alpha/2)/alpha):0;
        }
    }
    checkpoint(cancel);const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    return std::make_shared<const ThumbnailResult>(plan,std::move(output),ms);
}
struct ThumbnailJob::Impl {
    struct Completion {std::shared_ptr<const ThumbnailResult> result;std::string error;double ms=0;};
    std::future<Completion> future;std::shared_ptr<std::atomic<bool>> cancelled;
    std::shared_ptr<const ThumbnailResult> result;ThumbnailStats stats;
};
ThumbnailJob::ThumbnailJob():impl_(std::make_unique<Impl>()) {}
ThumbnailJob::~ThumbnailJob() {cancel();if(busy()) impl_->future.wait();}
bool ThumbnailJob::busy() const {return impl_->future.valid();}
void ThumbnailJob::cancel() {if(busy()) impl_->cancelled->store(true);}
std::shared_ptr<const ThumbnailResult> ThumbnailJob::result() const {return impl_->result;}
bool ThumbnailJob::request(std::shared_ptr<ImageServer> server,ThumbnailOptions options) {
    if(busy()) return false;
    if(!server) throw std::invalid_argument("thumbnail requires a server");
    const auto plan=plan_thumbnail(server->metadata(),options);
    if(impl_->stats.completed==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("thumbnail completion counter exhausted");
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    auto future=std::async(std::launch::async,[server=std::move(server),options,cancelled] {
        Impl::Completion completion;const auto start=Clock::now();
        try {completion.result=read_thumbnail(*server,options,cancelled.get());}
        catch(const std::exception& error) {completion.error=error.what();}
        completion.ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();return completion;
    });
    const auto completed=impl_->stats.completed;impl_->stats={};impl_->stats.completed=completed;
    impl_->stats.source_pixels=plan.source_pixels;impl_->stats.output_pixels=plan.output_pixels;impl_->stats.state="running";
    impl_->result.reset();impl_->cancelled=std::move(cancelled);impl_->future=std::move(future);return true;
}
ThumbnailStats ThumbnailJob::poll() {
    if(busy() && impl_->cancelled->load()) impl_->stats.state="cancelling";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto completion=impl_->future.get();impl_->stats.elapsed_ms=completion.ms;
        if(impl_->cancelled->load()) {impl_->stats.state="cancelled";impl_->stats.error="thumbnail cancelled";}
        else if(completion.result) {impl_->result=std::move(completion.result);impl_->stats.state="succeeded";++impl_->stats.completed;}
        else {impl_->stats.state="failed";impl_->stats.error=std::move(completion.error);}
    }
    return impl_->stats;
}
ThumbnailStats ThumbnailJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
