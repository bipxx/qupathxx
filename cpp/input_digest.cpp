#include "qupathxx/input_digest.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
struct InputDigestKernel {
    static std::int64_t origin(std::int64_t index,double scale) {const auto n=std::round(static_cast<long double>(index)*scale);if(!std::isfinite(n) || n<0 || n>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("input digest origin overflow");return static_cast<std::int64_t>(n);}
    static std::string run(const AnalysisRequest& source,std::int32_t level,double scale,std::int64_t x0,std::int64_t y0,std::int64_t x1,std::int64_t y1,const std::atomic<bool>* cancelled) {
        auto check=[&]{if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("input digest cancelled");};check();
        const auto& m=source.source_->metadata();Sha256 digest;const std::string format=level==0?"qpx.rgba-tiles/1":"qpx.rgba-level-tiles/1";digest.update(format.data(),format.size());digest.integer(m.width);digest.integer(m.height);
        if(level) {std::uint64_t bits;static_assert(sizeof(bits)==sizeof(scale));std::memcpy(&bits,&scale,8);digest.integer(level);digest.integer(m.levels[level].width);digest.integer(m.levels[level].height);digest.integer(bits);}
        for(auto n:{x0,y0,x1,y1}) digest.integer(n);digest.integer(source.tile_size());
        for(auto y=y0;y<y1;) {const auto h=std::min<std::int64_t>(source.tile_size(),y1-y);
            for(auto x=x0;x<x1;) {check();const auto w=std::min<std::int64_t>(source.tile_size(),x1-x),rx=origin(x,scale),ry=origin(y,scale);const auto pixels=source.source_->read_region({rx,ry,level,w,h});
                if(pixels.width()!=w || pixels.height()!=h || pixels.bytes().size()!=region_byte_count(w,h)) throw std::runtime_error("reader returned incorrect input digest tile");
                if(level) {digest.integer(x);digest.integer(y);}for(auto n:{rx,ry,w,h}) digest.integer(n);digest.update(pixels.bytes().data(),pixels.bytes().size());x+=w;
            }y+=h;
        }check();return digest.finish();
    }
    static std::string run(const AnalysisRequest& r,const std::atomic<bool>* c) {return run(r,0,1,r.x0_,r.y0_,r.x1_,r.y1_,c);}
    static std::string run(const TissueRequest& r,const std::atomic<bool>* c) {return run(r.source_,r.level_,r.downsample_,r.x0_,r.y0_,r.x1_,r.y1_,c);}
    static std::string run(const CellRequest& r,const std::atomic<bool>* c) {return run(r.source_,c);}
};
std::string input_sha256(const AnalysisRequest& r,const std::atomic<bool>* c) {return InputDigestKernel::run(r,c);}
std::string input_sha256(const TissueRequest& r,const std::atomic<bool>* c) {return InputDigestKernel::run(r,c);}
std::string input_sha256(const CellRequest& r,const std::atomic<bool>* c) {return InputDigestKernel::run(r,c);}
}
