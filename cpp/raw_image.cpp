#include "qupathxx/raw_image.hpp"
#include "qupathxx/geometry.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/source_identity.hpp"
#include "atomic_export.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace qpx {
bool is_ome_tiff_path(const std::string& path) {
    auto name=std::filesystem::u8path(path).filename().u8string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
    const auto ext=std::filesystem::u8path(name).extension().u8string();return name.find(".ome.")!=std::string::npos && (ext==".tif" || ext==".tiff" || ext==".tf2" || ext==".tf8" || ext==".btf");
}
namespace {
std::size_t pixels(std::int64_t w,std::int64_t h,std::uint32_t c,SampleType t) {
    constexpr std::uint64_t maximum=256*1024*1024;
    const auto bytes=sample_bytes(t);
    if(w<=0 || h<=0 || !c || c>65536 || std::uint64_t(w)>maximum/(std::uint64_t(c)*bytes+1) ||
       std::uint64_t(h)>maximum/(std::uint64_t(w)*(std::uint64_t(c)*bytes+1))) throw std::length_error("raw region exceeds the 256 MiB samples/validity limit");
    return static_cast<std::size_t>(w*h);
}
void coordinates(const RawImageMetadata& m,const RawRegionRequest& r) {
    if(!r.channels || r.first_channel>=m.channels || r.channels>m.channels-r.first_channel || r.z>=m.z_planes || r.t>=m.timepoints)
        throw std::out_of_range("raw channel/Z/time selection is outside the image");
    const auto& p=r.region;if(p.width<=0 || p.height<=0 || p.level<0 || std::size_t(p.level)>=m.levels.size()) throw std::invalid_argument("invalid raw region dimensions or level");
    const auto d=m.levels[p.level].downsample;if(!std::isfinite(d) || d<1) throw std::invalid_argument("invalid raw pyramid scale");const auto limit=static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    const auto ex=std::ceil(static_cast<long double>(p.width)*d),ey=std::ceil(static_cast<long double>(p.height)*d);
    if(!std::isfinite(ex) || !std::isfinite(ey) || ex>=limit || ey>=limit || p.x>std::numeric_limits<std::int64_t>::max()-std::int64_t(ex) || p.y>std::numeric_limits<std::int64_t>::max()-std::int64_t(ey)) throw std::invalid_argument("raw region coordinate overflow");
}
}
std::size_t sample_bytes(SampleType type) {
    switch(type) {case SampleType::UInt8:case SampleType::Int8:return 1;case SampleType::UInt16:case SampleType::Int16:return 2;
    case SampleType::UInt32:case SampleType::Int32:case SampleType::Float32:return 4;case SampleType::UInt64:case SampleType::Int64:case SampleType::Float64:return 8;}
    throw std::invalid_argument("invalid raw sample type");
}
std::string sample_name(SampleType type) {
    static const char* names[]={"uint8","uint16","int8","int16","uint32","int32","uint64","int64","float32","float64"};
    sample_bytes(type);return names[static_cast<unsigned>(type)];
}
std::size_t RawImageMetadata::payload_bytes() const {auto bytes=sizeof(*this)+64+dimension_order.capacity()+levels.capacity()*sizeof(ImageLevel)+channel_names.capacity()*sizeof(std::string);for(const auto& name:channel_names) bytes+=name.capacity();return bytes;}
void RawRegionRequest::validate(const RawImageMetadata& m) const {coordinates(m,*this);pixels(region.width,region.height,channels,m.sample_type);}
RawPixelBuffer::RawPixelBuffer(std::int64_t w,std::int64_t h,std::uint32_t c,SampleType t):width_(w),height_(h),channels_(c),type_(t) {
    const auto n=pixels(w,h,c,t);valid_.resize(n);const auto count=n*c;
    switch(t) {
#define QPX_RAW_ALLOC(code,T) case SampleType::code:values_=std::vector<T>(count);break
    QPX_RAW_ALLOC(UInt8,std::uint8_t);QPX_RAW_ALLOC(UInt16,std::uint16_t);QPX_RAW_ALLOC(Int8,std::int8_t);QPX_RAW_ALLOC(Int16,std::int16_t);
    QPX_RAW_ALLOC(UInt32,std::uint32_t);QPX_RAW_ALLOC(Int32,std::int32_t);QPX_RAW_ALLOC(UInt64,std::uint64_t);QPX_RAW_ALLOC(Int64,std::int64_t);
    QPX_RAW_ALLOC(Float32,float);QPX_RAW_ALLOC(Float64,double);
#undef QPX_RAW_ALLOC
    }
}
std::size_t RawPixelBuffer::payload_bytes() const {return valid_.capacity()+std::visit([](const auto& v){return v.capacity()*sizeof(typename std::decay_t<decltype(v)>::value_type);},values_);}
RawPixelBuffer ImageServer::read_raw(const RawRegionRequest&,const std::atomic<bool>*) const {throw std::runtime_error("image reader does not expose raw quantitative samples");}
std::shared_ptr<ImageServer> ImageServer::with_tiff_view(const TiffView&,const std::atomic<bool>*) const {throw std::runtime_error("image reader does not expose TIFF display views");}
void Document::adopt_tiff_source(std::shared_ptr<ImageServer> next) {
    const auto previous_view=server_->tiff_view();const auto next_view=next?next->tiff_view():nullptr;
    if(!previous_view || !next_view || next->reader_id()!="tiff" || next->source_locator()!=server_->source_locator()) throw std::invalid_argument("TIFF view changes must retain the same source file");
    if(previous_view->series!=next_view->series) throw std::invalid_argument("open another TIFF series as a separate image/document");
    const auto& a=metadata();const auto& b=next->metadata();if(a.width!=b.width || a.height!=b.height || a.levels.size()!=b.levels.size()) throw std::invalid_argument("TIFF view source dimensions changed");
    for(std::size_t i=0;i<a.levels.size();++i) if(a.levels[i].width!=b.levels[i].width || a.levels[i].height!=b.levels[i].height || a.levels[i].downsample!=b.levels[i].downsample) throw std::invalid_argument("TIFF view source pyramid changed");
    const auto old=server_->reader_source(),fresh=next->reader_source();
    if(old && old->primary && fresh && fresh->primary) {const auto& x=*old->primary;const auto& y=*fresh->primary;
        if(x.identity_scheme!=y.identity_scheme || x.identity!=y.identity || x.bytes!=y.bytes || x.modified_token!=y.modified_token || x.change_token!=y.change_token || x.stamp_scheme!=y.stamp_scheme) throw std::runtime_error("TIFF source changed; relink/reopen the image explicitly");}
    if(tiff_view_encode(*previous_view)==tiff_view_encode(*next_view)) return;
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");
    server_=std::move(next);++revision_;render_revision_=revision_;
}
void Document::set_tiff_view(const TiffView& view) {
    if(!server_->tiff_view()) throw std::invalid_argument("document does not use the TIFF quantitative reader");
    adopt_tiff_source(server_->with_tiff_view(view));
}
void TiffView::validate(bool resolved) const {
    if(series>=65536 || z>=65536 || t>=65536) throw std::out_of_range("TIFF series/Z/time index exceeds limits");
    bool automatic=false,explicit_channel=false;
    for(std::size_t i=0;i<3;++i) {
        if(channels[i]==-2) automatic=true;else explicit_channel=true;
        if(channels[i]<(resolved?-1:-2) || channels[i]>=65536) throw std::invalid_argument("invalid TIFF display channel");
        if(!resolved && std::isnan(lower[i]) && std::isnan(upper[i])) continue;
        if(!std::isfinite(lower[i]) || !std::isfinite(upper[i]) || lower[i]>=upper[i] || !std::isfinite(upper[i]-lower[i])) throw std::invalid_argument("TIFF display range/span must be finite and increasing");
    }
    if(automatic && explicit_channel) throw std::invalid_argument("automatic TIFF channel selection must apply to all colors");
    if(resolved && std::all_of(channels.begin(),channels.end(),[](auto c){return c<0;})) throw std::invalid_argument("TIFF display requires an enabled channel");
}
void TiffOpenOptions::validate() const {view.validate();if(decode_bytes<65536 || decode_bytes>256*1024*1024 || cache_bytes>256*1024*1024) throw std::invalid_argument("TIFF decode/cache budgets must be 64 KiB..256 MiB / 0..256 MiB");}
std::string tiff_view_encode(const TiffView& view) {
    view.validate(true);std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<"qpx.tiff-view/1\n"<<view.series<<' '<<view.z<<' '<<view.t<<'\n';
    for(std::size_t i=0;i<3;++i) out<<view.channels[i]<<' '<<view.lower[i]<<' '<<view.upper[i]<<'\n';
    const auto text=out.str();Sha256 hash;hash.update(text.data(),text.size());return text+hash.finish();
}
TiffView tiff_view_decode(const std::string& text) {
    if(text.size()<80 || text.size()>4096) throw std::invalid_argument("invalid TIFF view payload length");const auto n=text.size()-64;
    Sha256 hash;hash.update(text.data(),n);if(hash.finish()!=text.substr(n)) throw std::invalid_argument("TIFF view checksum mismatch");
    std::istringstream input(text.substr(0,n));input.imbue(std::locale::classic());std::string header;std::getline(input,header);TiffView view;
    if(header!="qpx.tiff-view/1" || !(input>>view.series>>view.z>>view.t)) throw std::invalid_argument("invalid TIFF view header");
    for(std::size_t i=0;i<3;++i) if(!(input>>view.channels[i]>>view.lower[i]>>view.upper[i])) throw std::invalid_argument("invalid TIFF view channel");
    input>>std::ws;if(!input.eof()) throw std::invalid_argument("trailing TIFF view data");view.validate(true);if(tiff_view_encode(view)!=text) throw std::invalid_argument("noncanonical TIFF view payload");return view;
}
void save_raw_numpy(const RawPixelBuffer& buffer,const std::string& path,bool replace,const std::atomic<bool>* cancelled) {
    const auto type=buffer.sample_type();const auto size=sample_bytes(type);const std::uint16_t one=1;const bool little=*reinterpret_cast<const unsigned char*>(&one)==1;
    const bool floating=type==SampleType::Float32 || type==SampleType::Float64;
    const bool signed_value=type==SampleType::Int8 || type==SampleType::Int16 || type==SampleType::Int32 || type==SampleType::Int64;
    std::string descriptor(1,size==1?'|':little?'<':'>');descriptor+=floating?'f':signed_value?'i':'u';descriptor+=std::to_string(size);
    std::string header="{'descr': '"+descriptor+"', 'fortran_order': False, 'shape': ("+std::to_string(buffer.height())+", "+std::to_string(buffer.width())+", "+std::to_string(buffer.channels())+"), }";
    header.append((64-(10+header.size()+1)%64)%64,' ');header+='\n';
    detail::AtomicExportFile file(detail::export_path(path),buffer.payload_bytes()+4096,cancelled);const unsigned char magic[]={0x93,'N','U','M','P','Y',1,0,static_cast<unsigned char>(header.size()),static_cast<unsigned char>(header.size()>>8)};
    file.write(reinterpret_cast<const char*>(magic),sizeof(magic));file.write(header.data(),header.size());
    std::visit([&](const auto& values){const auto* p=reinterpret_cast<const char*>(values.data());auto n=values.size()*sizeof(typename std::decay_t<decltype(values)>::value_type);while(n) {const auto step=std::min<std::size_t>(n,65536);file.write(p,step);p+=step;n-=step;}},buffer.storage());file.commit(replace);
}
RawRegionStatistics raw_region_statistics(const ImageServer& server,const RawRegionRequest& r,std::size_t tile,const Roi* mask,std::uint64_t maximum,const std::atomic<bool>* cancelled) {
    const auto metadata=server.raw_metadata(cancelled);if(!metadata) throw std::invalid_argument("image has no raw quantitative samples");coordinates(*metadata,r);
    if(tile<16 || tile>1024 || !maximum || maximum>1000000000000ULL || std::uint64_t(r.region.width)>maximum/std::uint64_t(r.region.height)) throw std::length_error("raw statistics exceeds pixel/tile limits");
    const auto d=metadata->levels[r.region.level].downsample;if(d!=std::floor(d)) throw std::invalid_argument("raw statistics requires an integer natural pyramid scale");
    RawRegionStatistics result;result.pixels=std::uint64_t(r.region.width)*r.region.height;result.finite_samples.resize(r.channels);result.nonfinite_samples.resize(r.channels);result.means.resize(r.channels);result.minima.resize(r.channels);result.maxima.resize(r.channels);
    for(std::int64_t y=0;y<r.region.height;y+=tile) for(std::int64_t x=0;x<r.region.width;x+=tile) {
        detail::export_checkpoint(cancelled);auto next=r;next.region=RegionRequest(r.region.x+x*std::int64_t(d),r.region.y+y*std::int64_t(d),r.region.level,std::min<std::int64_t>(tile,r.region.width-x),std::min<std::int64_t>(tile,r.region.height-y));const auto block=server.read_raw(next,cancelled);
        std::visit([&](const auto& values) {for(std::size_t i=0;i<block.valid().size();++i) {
            if(!block.valid()[i]) continue;const auto bx=i%std::size_t(block.width()),by=i/std::size_t(block.width());
            const auto grid_x=next.region.x/std::int64_t(d)-(next.region.x%std::int64_t(d)<0),grid_y=next.region.y/std::int64_t(d)-(next.region.y%std::int64_t(d)<0);
            if(mask && !mask->contains({(grid_x+double(bx)+.5)*d,(grid_y+double(by)+.5)*d})) continue;++result.valid_pixels;
            for(std::size_t c=0;c<r.channels;++c) {const auto value=static_cast<double>(values[i*r.channels+c]);if(!std::isfinite(value)) {++result.nonfinite_samples[c];continue;}
                const auto count=++result.finite_samples[c];const auto previous=result.means[c].value_or(0);const auto delta=value-previous;
                result.means[c]=std::isfinite(delta)?previous+delta/count:previous*(1-1.0/count)+value/count;
                result.minima[c]=result.minima[c]?std::min(*result.minima[c],value):value;result.maxima[c]=result.maxima[c]?std::max(*result.maxima[c],value):value;
            }
        }},block.storage());
    }return result;
}
}
