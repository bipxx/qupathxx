#include "tissue_archive.hpp"
#include "tissue_cleanup.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
void stopped(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("tissue archive cancelled");}
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','T','M',1};
    void u32(std::uint32_t v) {for(int i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void u64(std::uint64_t v) {for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void real(double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);u64(bits);}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint8_t byte() {if(!left) throw std::invalid_argument("truncated tissue mask blob");--left;return *p++;}
    std::uint32_t u32() {std::uint32_t v=0;for(int i=0;i<4;++i) v|=std::uint32_t(byte())<<(8*i);return v;}
    std::uint64_t u64() {std::uint64_t v=0;for(int i=0;i<8;++i) v|=std::uint64_t(byte())<<(8*i);return v;}
    double real() {auto bits=u64();double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite tissue mask number");return v;}
    std::uint64_t limited(std::uint64_t limit) {auto v=u64();if(v>limit) throw std::invalid_argument("tissue archive integer exceeds limit");return v;}
};
std::string digest(const std::uint8_t* data,std::size_t size,const Roi& roi,const std::atomic<bool>* cancelled) {
    Sha256 hash;const auto geometry=roi.wkt();hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());
    for(std::size_t at=0;at<size;) {stopped(cancelled);const auto n=std::min<std::size_t>(65536,size-at);hash.update(data+at,n);at+=n;}
    return hash.finish();
}
bool hex(const std::string& s) {return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});}
std::uint64_t edge(double coordinate,double scale,std::int64_t limit) {
    const auto v=std::ceil(static_cast<long double>(coordinate)/scale-.5L);
    return static_cast<std::uint64_t>(std::max<long double>(0,std::min<long double>(static_cast<long double>(limit),v)));
}
}
std::vector<std::uint8_t> TissueArchiveCodec::encode(const TissueResult& r,const std::atomic<bool>* cancelled) {
    if(!r.input_format_override_.empty()) throw std::invalid_argument("cell detector masks require their versioned cell archive");
    stopped(cancelled);Writer w;const auto& p=r.parameters_;
    p.validate();
    if(p.sampling_policy==RoiSamplingPolicy::PolygonWinding) {w.bytes[3]=2;w.u64(1);}
    for(auto v:{r.object_id_,r.object_revision_,r.document_revision_}) w.u64(v);
    w.u64(r.nucleus_);w.u64(static_cast<std::uint64_t>(p.feature));w.real(p.threshold);
    w.u64(p.connectivity);w.u64(static_cast<std::uint64_t>(std::int64_t(p.level)+1));w.u64(p.open_radius);w.u64(p.close_radius);w.u64(p.fill_holes);
    for(auto v:{p.max_hole_pixels,p.min_pixels,p.max_pixels,std::uint64_t(p.tile_size),std::uint64_t(p.max_components),std::uint64_t(p.work_bytes)}) w.u64(v);
    w.u64(r.level_);w.real(r.downsample_);
    const auto grid=r.level_bounds_;for(auto v:{grid.x,grid.y,grid.width,grid.height}) w.u64(static_cast<std::uint64_t>(v));
    for(auto v:r.white_) w.real(v);for(const auto& vector:r.stains_) for(auto v:vector) w.real(v);
    for(auto v:{r.samples_,r.threshold_pixels_,r.cleaned_pixels_,r.filled_pixels_,r.foreground_pixels_,r.raw_components_}) w.u64(v);
    w.real(r.cleanup_ms_);w.real(r.elapsed_ms_);w.u64(r.estimated_work_bytes_);w.u64(r.components_.size());
    w.bytes.insert(w.bytes.end(),r.input_sha256_.begin(),r.input_sha256_.end());
    std::size_t runs=0;std::uint32_t prior=0;
    for(std::size_t i=0;i<r.labels_.size();++i) {if((i&4095)==0) stopped(cancelled);if(!i || r.labels_[i]!=prior) ++runs;prior=r.labels_[i];}
    const bool compressed=runs*8<r.labels_.size()*4;w.u64(compressed);w.u64(compressed?runs:r.labels_.size());
    const auto payload=compressed?runs*8:r.labels_.size()*4;
    w.bytes.reserve(w.bytes.size()+payload+64);
    if(compressed) {
        std::size_t at=0;
        while(at<r.labels_.size()) {
            stopped(cancelled);const auto value=r.labels_[at];std::size_t end=at+1;
            while(end<r.labels_.size() && r.labels_[end]==value) {if((end&4095)==0) stopped(cancelled);++end;}
            w.u32(static_cast<std::uint32_t>(end-at));w.u32(value);at=end;
        }
    } else for(std::size_t i=0;i<r.labels_.size();++i) {if((i&4095)==0) stopped(cancelled);w.u32(r.labels_[i]);}
    const auto checksum=digest(w.bytes.data(),w.bytes.size(),r.roi_,cancelled);w.bytes.insert(w.bytes.end(),checksum.begin(),checksum.end());return std::move(w.bytes);
}
std::shared_ptr<const TissueResult> TissueArchiveCodec::decode(const std::uint8_t* data,std::size_t size,Roi roi,const ImageMetadata& metadata,const std::atomic<bool>* cancelled) {
    stopped(cancelled);
    if(!data || size<128 || size>256u*1024u*1024u) throw std::invalid_argument("invalid tissue mask blob size");
    const std::string checksum(reinterpret_cast<const char*>(data+size-64),64);
    if(!hex(checksum) || digest(data,size-64,roi,cancelled)!=checksum) throw std::invalid_argument("tissue mask checksum mismatch");
    Reader d{data,size-64};if(d.byte()!='Q' || d.byte()!='T' || d.byte()!='M') throw std::invalid_argument("unsupported tissue mask encoding");
    const auto version=d.byte();if(version!=1 && version!=2) throw std::invalid_argument("unsupported tissue mask encoding");
    const auto policy=version==2?d.u64():0;
    if((version==2 && policy!=1)) throw std::invalid_argument("unsupported tissue sampling policy");
    auto r=std::shared_ptr<TissueResult>(new TissueResult());r->roi_=std::move(roi);r->image_bounds_={0,0,double(metadata.width),double(metadata.height)};
    r->object_id_=d.u64();r->object_revision_=d.u64();r->document_revision_=d.u64();r->nucleus_=d.limited(1)!=0;
    if(!r->object_id_ || r->object_revision_>r->document_revision_) throw std::invalid_argument("invalid tissue mask identity");
    auto& p=r->parameters_;p.sampling_policy=static_cast<RoiSamplingPolicy>(policy);p.feature=static_cast<TissueFeature>(d.limited(3));p.threshold=d.real();p.connectivity=static_cast<int>(d.limited(8));
    p.level=static_cast<std::int32_t>(std::int64_t(d.limited(std::uint64_t(std::numeric_limits<std::int32_t>::max())+1))-1);
    p.open_radius=static_cast<int>(d.limited(256));p.close_radius=static_cast<int>(d.limited(256));p.fill_holes=d.limited(1)!=0;
    p.max_hole_pixels=d.u64();p.min_pixels=d.u64();p.max_pixels=d.u64();p.tile_size=static_cast<std::size_t>(d.limited(1024));p.max_components=static_cast<std::size_t>(d.limited(1000000));p.work_bytes=static_cast<std::size_t>(d.limited(512u*1024u*1024u));p.validate();
    r->level_=static_cast<std::int32_t>(d.limited(std::numeric_limits<std::int32_t>::max()));r->downsample_=d.real();
    if(static_cast<std::size_t>(r->level_)>=metadata.levels.size()) throw std::invalid_argument("tissue mask level exceeds source pyramid");
    const auto& level=metadata.levels[r->level_];
    if(r->downsample_!=level.downsample || (p.level>=0 && p.level!=r->level_) || !r->roi_.is_area()) throw std::invalid_argument("tissue mask pyramid or ROI mismatch");
    const auto b=r->roi_.bounds();
    if(b.x<0 || b.y<0 || static_cast<long double>(b.x+b.width)>metadata.width || static_cast<long double>(b.y+b.height)>metadata.height) throw std::invalid_argument("tissue mask ROI exceeds source");
    const auto x=d.limited(level.width),y=d.limited(level.height),width=d.limited(level.width),height=d.limited(level.height);
    if(!width || !height || x+width>static_cast<std::uint64_t>(level.width) || y+height>static_cast<std::uint64_t>(level.height) ||
       x!=edge(b.x,r->downsample_,level.width) || y!=edge(b.y,r->downsample_,level.height) ||
       x+width!=edge(b.x+b.width,r->downsample_,level.width) || y+height!=edge(b.y+b.height,r->downsample_,level.height) ||
       width>std::numeric_limits<std::uint32_t>::max()/height) throw std::invalid_argument("invalid tissue mask sample grid");
    const auto count=width*height;r->level_bounds_={double(x),double(y),double(width),double(height)};
    r->bounds_={double(x)*r->downsample_,double(y)*r->downsample_,double(width)*r->downsample_,double(height)*r->downsample_};
    for(auto& v:r->white_) v=d.real();for(auto& vector:r->stains_) for(auto& v:vector) v=d.real();StainTransform::validate_normalized(r->stains_,r->white_);
    r->samples_=d.limited(count);r->threshold_pixels_=d.limited(r->samples_);r->cleaned_pixels_=d.limited(r->samples_);r->filled_pixels_=d.limited(r->cleaned_pixels_);r->foreground_pixels_=d.limited(r->cleaned_pixels_);r->raw_components_=d.limited(p.max_components);
    r->cleanup_ms_=d.real();r->elapsed_ms_=d.real();r->estimated_work_bytes_=static_cast<std::size_t>(d.limited(p.work_bytes));
    const auto components=d.limited(r->raw_components_);r->algorithm_=tissue_detail::algorithm(p,r->level_);
    if(r->cleanup_ms_<0 || r->elapsed_ms_<r->cleanup_ms_ || (!tissue_detail::enabled(p) && (r->cleaned_pixels_!=r->threshold_pixels_ || r->filled_pixels_ || r->cleanup_ms_)) ||
       (!p.fill_holes && r->filled_pixels_) || (components==0)!=(r->foreground_pixels_==0)) throw std::invalid_argument("invalid tissue mask statistics");
    const auto capacity=std::min<std::uint64_t>(count,p.max_components);
    const auto estimate=count*(tissue_detail::enabled(p)?9:8)+capacity*sizeof(TissueComponent)+16ull*p.tile_size*p.tile_size+65536;
    if(estimate!=r->estimated_work_bytes_ || count*4+components*sizeof(TissueComponent)+r->roi_.payload_bytes()>256u*1024u*1024u) throw std::invalid_argument("tissue mask exceeds its decoded budget");
    for(int i=0;i<64;++i) r->input_sha256_.push_back(static_cast<char>(d.byte()));if(!hex(r->input_sha256_)) throw std::invalid_argument("invalid tissue input digest");
    const bool compressed=d.limited(1)!=0;const auto entries=d.limited(count);
    if((!compressed && entries!=count) || (compressed && !entries) || entries*(compressed?8:4)!=d.left) throw std::invalid_argument("invalid tissue mask payload length");
    r->labels_.resize(static_cast<std::size_t>(count));std::size_t at=0;std::uint32_t prior=std::numeric_limits<std::uint32_t>::max();
    for(std::uint64_t i=0;i<entries;++i) {
        if((i&4095)==0) stopped(cancelled);const auto n=compressed?d.u32():1,value=d.u32();
        if(!n || n>count-at || value>components || (compressed && value==prior)) throw std::invalid_argument("invalid tissue mask label/run");
        const auto end=at+n;
        while(at<end) {stopped(cancelled);const auto chunk=std::min<std::size_t>(65536,end-at);std::fill_n(r->labels_.begin()+at,chunk,value);at+=chunk;}
        prior=value;
    }
    if(at!=count) throw std::invalid_argument("tissue mask runs do not fill grid");
    struct Stats {std::uint64_t n=0,sx=0,sy=0,minx=std::numeric_limits<std::uint64_t>::max(),miny=std::numeric_limits<std::uint64_t>::max(),maxx=0,maxy=0;};
    std::vector<Stats> stats(static_cast<std::size_t>(components));std::uint64_t total=0,next=1;
    for(std::uint64_t i=0;i<count;++i) {
        if((i&4095)==0) stopped(cancelled);const auto label=r->labels_[static_cast<std::size_t>(i)];if(!label) continue;
        auto& s=stats[label-1];if(!s.n && label!=next++) throw std::invalid_argument("tissue labels are not in dense raster order");
        const auto xx=i%width,yy=i/width;++s.n;++total;s.sx+=xx;s.sy+=yy;s.minx=std::min(s.minx,xx);s.miny=std::min(s.miny,yy);s.maxx=std::max(s.maxx,xx);s.maxy=std::max(s.maxy,yy);
    }
    if(total!=r->foreground_pixels_ || next!=components+1) throw std::invalid_argument("tissue mask component counts disagree");
    r->components_.reserve(static_cast<std::size_t>(components));const auto scale=r->downsample_;
    for(std::size_t i=0;i<stats.size();++i) {
        stopped(cancelled);const auto& s=stats[i];if(s.n<p.min_pixels || (p.max_pixels && s.n>p.max_pixels)) throw std::invalid_argument("tissue mask component size violates parameters");
        r->components_.push_back({static_cast<std::uint32_t>(i+1),s.n,{double(x+s.minx)*scale,double(y+s.miny)*scale,double(s.maxx-s.minx+1)*scale,double(s.maxy-s.miny+1)*scale},
            {(double(x)+double(s.sx)/double(s.n)+.5)*scale,(double(y)+double(s.sy)/double(s.n)+.5)*scale}});
    }
    return r;
}
}
