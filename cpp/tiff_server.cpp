#include "qupathxx/raw_image.hpp"
#include "reader_file_guard.hpp"
#include "reader_provenance.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#ifdef QUPATHXX_HAS_TIFF
#include <tiffio.h>
#include <tinyxml2.h>
#endif

namespace qpx {
bool has_tiff() {
#ifdef QUPATHXX_HAS_TIFF
    return true;
#else
    return false;
#endif
}
std::string tiff_library_version() {
#ifdef QUPATHXX_HAS_TIFF
    return TIFFGetVersion();
#else
    return "disabled";
#endif
}
#ifndef QUPATHXX_HAS_TIFF
std::shared_ptr<ImageServer> open_tiff(const std::string&,TiffOpenOptions,const std::atomic<bool>*) {throw std::runtime_error("TIFF reader requires QUPATHXX_BUILD_TIFF=ON");}
#else
namespace {
namespace fs=std::filesystem;
struct Ifd {
    std::uint64_t offset=0;
    std::uint32_t width=0,height=0,block_width=0,block_height=0;
    std::uint16_t bits=0,format=SAMPLEFORMAT_UINT,samples=1,planar=PLANARCONFIG_CONTIG,photo=PHOTOMETRIC_MINISBLACK,orientation=ORIENTATION_TOPLEFT,compression=COMPRESSION_NONE;
    bool tiled=false,alpha=false,associated_alpha=false;
    std::vector<std::uint64_t> sub;
    std::vector<Ifd> levels;
    SampleType type() const {
        if(format==SAMPLEFORMAT_IEEEFP) {if(bits==32) return SampleType::Float32;if(bits==64) return SampleType::Float64;}
        if(format==SAMPLEFORMAT_UINT || format==SAMPLEFORMAT_INT) {
            const bool s=format==SAMPLEFORMAT_INT;
            if(bits==8) return s?SampleType::Int8:SampleType::UInt8;if(bits==16) return s?SampleType::Int16:SampleType::UInt16;
            if(bits==32) return s?SampleType::Int32:SampleType::UInt32;if(bits==64) return s?SampleType::Int64:SampleType::UInt64;
        }throw std::invalid_argument("unsupported TIFF sample format/bit depth");
    }
    void validate() const {
        if(!TIFFIsCODECConfigured(compression)) throw std::invalid_argument("TIFF compression codec is not enabled in this reader");
        type();if(!width || !height || !samples || samples>1024 || !block_width || !block_height ||
          (planar!=PLANARCONFIG_CONTIG && planar!=PLANARCONFIG_SEPARATE) || orientation!=ORIENTATION_TOPLEFT ||
          (photo!=PHOTOMETRIC_MINISBLACK && photo!=PHOTOMETRIC_MINISWHITE && photo!=PHOTOMETRIC_RGB))
            throw std::invalid_argument("TIFF requires top-left byte-aligned grayscale or RGB samples");
        if(photo==PHOTOMETRIC_RGB && samples<3) throw std::invalid_argument("invalid TIFF RGB sample count");
    }
};
struct Mapping {std::uint32_t ifd=std::numeric_limits<std::uint32_t>::max();std::uint16_t sample=0;};
bool local(const tinyxml2::XMLElement* e,const char* expected) {if(!e) return false;const char* n=e->Name();const auto* colon=std::strrchr(n,':');return std::strcmp(colon?colon+1:n,expected)==0;}
const tinyxml2::XMLElement* child(const tinyxml2::XMLElement* e,const char* name) {for(auto* p=e?e->FirstChildElement():nullptr;p;p=p->NextSiblingElement()) if(local(p,name)) return p;return nullptr;}
std::uint32_t integer(const tinyxml2::XMLElement* e,const char* key,std::optional<std::uint32_t> fallback={}) {
    const auto* text=e->Attribute(key);if(!text) {if(fallback) return *fallback;throw std::invalid_argument(std::string("missing OME attribute ")+key);}
    std::uint32_t n=0;const auto* end=text+std::strlen(text);const auto result=std::from_chars(text,end,n);
    if(result.ec!=std::errc{} || result.ptr!=end) throw std::invalid_argument(std::string("invalid OME integer ")+key);return n;
}
std::optional<double> quantity(const tinyxml2::XMLElement* e,const char* key,const char* unit_key,bool time=false) {
    const auto* text=e->Attribute(key);if(!text) return {};double n=0;const auto* end=text+std::strlen(text);const auto parsed=std::from_chars(text,end,n);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end || !std::isfinite(n) || n<=0) return {};
    const auto* attr=e->Attribute(unit_key);const std::string unit=attr?attr:(time?"s":u8"\u00b5m");double scale=0;
    if(time) {if(unit=="s") scale=1;else if(unit=="ms") scale=.001;else if(unit==u8"\u00b5s") scale=.000001;else if(unit=="min") scale=60;else if(unit=="h") scale=3600;}
    else {if(unit==u8"\u00b5m" || unit=="um") scale=1;else if(unit=="nm") scale=.001;else if(unit=="mm") scale=1000;else if(unit=="cm") scale=10000;else if(unit=="m") scale=1000000;else if(unit==u8"\u00c5") scale=.0001;}
    const auto value=n*scale;return scale && std::isfinite(value) && value>0?std::optional<double>(value):std::nullopt;
}
std::string bounded(const char* text,std::size_t maximum) {if(!text) return {};std::size_t n=0;while(n<=maximum && text[n]) ++n;if(n>maximum) throw std::length_error("TIFF metadata text exceeds limit");return {text,n};}
std::int64_t floor_div(std::int64_t n,std::int64_t d) {return n/d-(n%d<0);}
bool same_file(const FileVersion& a,const FileVersion& b) {return a.identity_scheme==b.identity_scheme && a.identity==b.identity && a.bytes==b.bytes && a.stamp_scheme==b.stamp_scheme && a.modified_token==b.modified_token && a.change_token==b.change_token;}
std::pair<double,double> range(SampleType type) {
    switch(type) {case SampleType::UInt8:return {0,255};case SampleType::UInt16:return {0,65535};case SampleType::Int8:return {-128,127};case SampleType::Int16:return {-32768,32767};
    case SampleType::UInt32:return {0,4294967295.};case SampleType::Int32:return {-2147483648.,2147483647.};case SampleType::UInt64:return {0,18446744073709551616.};case SampleType::Int64:return {-9223372036854775808.,9223372036854775808.};default:return {0,1};}
}
class TiffData final {
    struct Block {std::vector<std::uint8_t> bytes;std::size_t charge() const {return bytes.capacity()+256;}};
    using Key=std::pair<std::uint64_t,std::uint32_t>;
    struct Cache {std::shared_ptr<Block> value;std::list<Key>::iterator position;};
    TIFF* handle_=nullptr;std::string path_;TiffOpenOptions options_;std::array<char,4096> error_{};const std::atomic<bool>* opening_cancel_=nullptr;
    std::vector<Ifd> roots_;std::vector<Mapping> mapping_;
    std::shared_ptr<RawImageMetadata> raw_=std::make_shared<RawImageMetadata>();
    std::shared_ptr<TiffView> view_;ImageMetadata display_;
    std::shared_ptr<const TiffOpenOptions> option_snapshot_;
    std::shared_ptr<const ReaderSource> source_;
    mutable std::mutex mutex_;bool closed_=false;
    std::map<Key,Cache> cache_;std::list<Key> lru_;TiffReadStats stats_;
    static int error_handler(TIFF*,void* data,const char* module,const char* format,va_list args) noexcept {
        auto& buffer=static_cast<TiffData*>(data)->error_;const auto prefix=std::snprintf(buffer.data(),buffer.size(),"%s: ",module?module:"TIFF");
        if(prefix>0 && std::size_t(prefix)<buffer.size()) std::vsnprintf(buffer.data()+prefix,buffer.size()-std::size_t(prefix),format,args);return 1;
    }
    static int warning_handler(TIFF*,void*,const char*,const char*,va_list) noexcept {return 1;}
    [[noreturn]] void failed(const char* context) const {throw std::runtime_error(std::string(context)+(error_[0]?": "+std::string(error_.data()):""));}
    void select(std::uint64_t offset) {detail::export_checkpoint(opening_cancel_);error_[0]=0;if(TIFFCurrentDirOffset(handle_)==offset) return;if(!TIFFSetSubDirectory(handle_,offset)) failed("cannot select TIFF directory");++stats_.directory_switches;}
    Ifd inspect() {
        Ifd d;d.offset=TIFFCurrentDirOffset(handle_);if(!TIFFGetField(handle_,TIFFTAG_IMAGEWIDTH,&d.width) || !TIFFGetField(handle_,TIFFTAG_IMAGELENGTH,&d.height)) failed("TIFF directory has no dimensions");
        TIFFGetFieldDefaulted(handle_,TIFFTAG_BITSPERSAMPLE,&d.bits);TIFFGetFieldDefaulted(handle_,TIFFTAG_SAMPLEFORMAT,&d.format);TIFFGetFieldDefaulted(handle_,TIFFTAG_SAMPLESPERPIXEL,&d.samples);TIFFGetFieldDefaulted(handle_,TIFFTAG_PLANARCONFIG,&d.planar);TIFFGetFieldDefaulted(handle_,TIFFTAG_PHOTOMETRIC,&d.photo);TIFFGetFieldDefaulted(handle_,TIFFTAG_ORIENTATION,&d.orientation);
        TIFFGetFieldDefaulted(handle_,TIFFTAG_COMPRESSION,&d.compression);
        std::uint32_t depth=1;TIFFGetFieldDefaulted(handle_,TIFFTAG_IMAGEDEPTH,&depth);if(depth!=1) throw std::invalid_argument("volumetric TIFF IFDs are unsupported; use OME plane IFDs");
        d.tiled=TIFFIsTiled(handle_)!=0;if(d.tiled) {TIFFGetField(handle_,TIFFTAG_TILEWIDTH,&d.block_width);TIFFGetField(handle_,TIFFTAG_TILELENGTH,&d.block_height);TIFFGetFieldDefaulted(handle_,TIFFTAG_TILEDEPTH,&depth);if(depth!=1) throw std::invalid_argument("volumetric TIFF tiles are unsupported");}
        else {d.block_width=d.width;TIFFGetFieldDefaulted(handle_,TIFFTAG_ROWSPERSTRIP,&d.block_height);d.block_height=std::min(d.block_height,d.height);}
        std::uint16_t count=0;std::uint64_t* offsets=nullptr;if(TIFFGetField(handle_,TIFFTAG_SUBIFD,&count,&offsets)) {if(count>64) throw std::length_error("TIFF subresolution count exceeds 64");d.sub.assign(offsets,offsets+count);}
        std::uint16_t extras=0;std::uint16_t* extra=nullptr;
        if(TIFFGetField(handle_,TIFFTAG_EXTRASAMPLES,&extras,&extra) && d.samples==(d.photo==PHOTOMETRIC_RGB?4:2) && extras==1) {d.alpha=extra[0]==EXTRASAMPLE_ASSOCALPHA || extra[0]==EXTRASAMPLE_UNASSALPHA;d.associated_alpha=extra[0]==EXTRASAMPLE_ASSOCALPHA;}
        return d;
    }
    void index(const std::string& xml) {
        raw_->series=options_.view.series;std::vector<std::uint32_t> groups;
        if(xml.find("<OME")==std::string::npos && xml.find(":OME")==std::string::npos) {
            if(raw_->series>=roots_.size()) throw std::out_of_range("TIFF page/series index is outside the file");const auto& d=roots_[raw_->series];
            raw_->width=d.width;raw_->height=d.height;raw_->channels=d.samples;raw_->series_count=static_cast<std::uint32_t>(roots_.size());raw_->dimension_order="XYC";
            mapping_.resize(d.samples);for(std::uint32_t c=0;c<d.samples;++c) {mapping_[c]={raw_->series,static_cast<std::uint16_t>(c)};raw_->channel_names.push_back("Channel "+std::to_string(c+1));}
            select(d.offset);std::uint16_t unit=RESUNIT_NONE;float x=0,y=0;TIFFGetFieldDefaulted(handle_,TIFFTAG_RESOLUTIONUNIT,&unit);TIFFGetField(handle_,TIFFTAG_XRESOLUTION,&x);TIFFGetField(handle_,TIFFTAG_YRESOLUTION,&y);
            const double scale=unit==RESUNIT_INCH?25400:unit==RESUNIT_CENTIMETER?10000:0;
            if(scale && std::isfinite(x) && x>0) raw_->pixel_width_um=scale/x;if(scale && std::isfinite(y) && y>0) raw_->pixel_height_um=scale/y;
            return;
        }
        if(xml.find("<!DOCTYPE")!=std::string::npos || xml.find("<!ENTITY")!=std::string::npos) throw std::invalid_argument("OME document declarations/entities are unsupported");
        tinyxml2::XMLDocument document;if(document.Parse(xml.data(),xml.size())!=tinyxml2::XML_SUCCESS) throw std::invalid_argument("malformed OME XML");
        const auto* root=document.RootElement();if(!local(root,"OME")) throw std::invalid_argument("OME XML root is not OME");
        const tinyxml2::XMLElement* image=nullptr;std::uint32_t count=0;
        for(auto* e=root->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"Image")) {if(count==raw_->series) image=e;if(++count>1024) throw std::length_error("OME series count exceeds 1024");}
        if(!image) throw std::out_of_range("OME series index is outside the file");raw_->series_count=count;const auto* p=child(image,"Pixels");if(!p) throw std::invalid_argument("OME Image has no Pixels");
        raw_->width=integer(p,"SizeX");raw_->height=integer(p,"SizeY");raw_->channels=integer(p,"SizeC");raw_->z_planes=integer(p,"SizeZ");raw_->timepoints=integer(p,"SizeT");
        if(!raw_->width || !raw_->height || !raw_->channels || raw_->channels>65536 || !raw_->z_planes || raw_->z_planes>65536 || !raw_->timepoints || raw_->timepoints>65536 ||
          std::uint64_t(raw_->channels)*raw_->z_planes*raw_->timepoints>1048576) throw std::length_error("OME channel/plane map exceeds limits");
        raw_->dimension_order=bounded(p->Attribute("DimensionOrder"),5);const auto order=raw_->dimension_order;
        if(order.size()!=5 || order.substr(0,2)!="XY" || std::set<char>(order.begin()+2,order.end())!=std::set<char>{'Z','C','T'}) throw std::invalid_argument("invalid OME dimension order");
        std::uint32_t first=0;std::size_t name_bytes=0;
        for(auto* e=p->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"Channel")) {
            const auto samples=integer(e,"SamplesPerPixel",1);if(!samples || samples>1024 || samples>raw_->channels-first) throw std::invalid_argument("invalid OME channel sample count");
            groups.push_back(first);auto name=bounded(e->Attribute("Name"),4096);if(name.empty()) name="Channel "+std::to_string(groups.size());
            for(std::uint32_t c=0;c<samples;++c) {auto label=name+(samples>1?" / "+std::to_string(c+1):"");detail::bounded_utf8(label,4110);if(label.size()>4*1024*1024-name_bytes) throw std::length_error("OME expanded channel names exceed 4 MiB");name_bytes+=label.size();raw_->channel_names.push_back(std::move(label));}first+=samples;
        }
        if(groups.empty()) {for(std::uint32_t c=0;c<raw_->channels;++c) {groups.push_back(c);raw_->channel_names.push_back("Channel "+std::to_string(c+1));}first=raw_->channels;}
        if(first!=raw_->channels) throw std::invalid_argument("OME Channel samples do not match SizeC");
        const std::uint64_t planes=std::uint64_t(groups.size())*raw_->z_planes*raw_->timepoints;if(planes>65536) throw std::length_error("OME plane count exceeds 65536");
        mapping_.resize(std::size_t(raw_->channels)*raw_->z_planes*raw_->timepoints);bool found=false;
        auto group_index=[&](std::uint32_t c) {const auto at=std::find(groups.begin(),groups.end(),c);if(at==groups.end()) throw std::invalid_argument("OME FirstC must identify a channel group");return std::uint32_t(at-groups.begin());};
        auto extent=[&](char axis)->std::uint32_t {return axis=='C'?std::uint32_t(groups.size()):axis=='Z'?raw_->z_planes:raw_->timepoints;};
        for(auto* e=p->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"TiffData")) {
            detail::export_checkpoint(opening_cancel_);found=true;if(const auto* uuid=child(e,"UUID")) {const auto filename=bounded(uuid->Attribute("FileName"),32768),value=bounded(uuid->GetText(),256),current=bounded(root->Attribute("UUID"),256);
                if((!filename.empty() && filename!=fs::u8path(path_).filename().u8string()) || current.empty() || value!=current) throw std::invalid_argument("multi-file OME references are unsupported");}
            std::array<std::uint32_t,3> position{};for(std::size_t i=0;i<3;++i) position[i]=order[i+2]=='C'?group_index(integer(e,"FirstC",0)):integer(e,order[i+2]=='Z'?"FirstZ":"FirstT",0);
            std::uint64_t linear=0,multiplier=1;for(std::size_t i=0;i<3;++i) {if(position[i]>=extent(order[i+2])) throw std::out_of_range("OME first plane exceeds dimensions");linear+=position[i]*multiplier;multiplier*=extent(order[i+2]);}
            const auto ifd=integer(e,"IFD",0),length=integer(e,"PlaneCount",e->Attribute("IFD")?1:std::uint32_t(roots_.size()));
            if(length>planes-linear || ifd>=roots_.size() || length>roots_.size()-ifd) throw std::out_of_range("OME TiffData exceeds plane or IFD bounds");
            for(std::uint32_t step=0;step<length;++step) {detail::export_checkpoint(opening_cancel_);auto value=linear+step;std::uint32_t c=0,z=0,t=0;
                for(std::size_t i=0;i<3;++i) {const auto coordinate=std::uint32_t(value%extent(order[i+2]));value/=extent(order[i+2]);if(order[i+2]=='C') c=coordinate;else if(order[i+2]=='Z') z=coordinate;else t=coordinate;}
                const auto start=groups[c],end=c+1<groups.size()?groups[c+1]:raw_->channels;const auto& d=roots_[ifd+step];if(d.samples!=end-start) throw std::invalid_argument("OME channel samples do not match TIFF IFD samples");
                for(std::uint32_t sample=start;sample<end;++sample) {auto& entry=mapping_[(std::size_t(t)*raw_->z_planes+z)*raw_->channels+sample];if(entry.ifd!=std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("duplicate OME plane mapping");entry={ifd+step,static_cast<std::uint16_t>(sample-start)};}
            }
        }
        if(!found || std::any_of(mapping_.begin(),mapping_.end(),[](auto m){return m.ifd==std::numeric_limits<std::uint32_t>::max();})) throw std::invalid_argument("OME plane mapping is incomplete");
        const auto declared=bounded(p->Attribute("Type"),32);auto actual=sample_name(roots_[mapping_.front().ifd].type());if(actual=="float32") actual="float";if(actual=="float64") actual="double";
        if(declared!=actual) throw std::invalid_argument("OME pixel type differs from TIFF samples");
        raw_->pixel_width_um=quantity(p,"PhysicalSizeX","PhysicalSizeXUnit");raw_->pixel_height_um=quantity(p,"PhysicalSizeY","PhysicalSizeYUnit");raw_->z_spacing_um=quantity(p,"PhysicalSizeZ","PhysicalSizeZUnit");raw_->time_interval_seconds=quantity(p,"TimeIncrement","TimeIncrementUnit",true);
    }
    void levels() {
        std::set<std::uint32_t> selected;for(auto m:mapping_) selected.insert(m.ifd);std::size_t count=roots_.size();
        for(auto index:selected) {auto& root=roots_[index];root.validate();Ifd base=root;base.sub.clear();root.levels.push_back(std::move(base));
            for(auto offset:root.sub) {if(++count>131072) throw std::length_error("TIFF directory catalog exceeds limit");select(offset);auto level=inspect();level.validate();
                if(!level.sub.empty() || level.samples!=root.samples || level.type()!=root.type() || level.photo!=root.photo) throw std::invalid_argument("incompatible TIFF subresolution samples");root.levels.push_back(std::move(level));}
        }
        const auto& reference=roots_[mapping_.front().ifd];raw_->sample_type=reference.type();
        for(std::size_t level=0;level<reference.levels.size();++level) {const auto& d=reference.levels[level];double scale=1;
            if(level) {scale=std::round(double(reference.width)/d.width);if(scale<=raw_->levels.back().downsample || scale<2 || (std::uint64_t(reference.width)+std::uint64_t(scale)-1)/std::uint64_t(scale)!=d.width || (std::uint64_t(reference.height)+std::uint64_t(scale)-1)/std::uint64_t(scale)!=d.height) throw std::invalid_argument("TIFF subresolutions require increasing integer isotropic scales");}
            raw_->levels.push_back({d.width,d.height,scale});
        }
        for(auto index:selected) {const auto& root=roots_[index];if(root.width!=raw_->width || root.height!=raw_->height || root.type()!=raw_->sample_type || root.levels.size()!=raw_->levels.size()) throw std::invalid_argument("OME plane geometry/type/pyramid differs");
            for(std::size_t i=0;i<root.levels.size();++i) if(root.levels[i].width!=raw_->levels[i].width || root.levels[i].height!=raw_->levels[i].height) throw std::invalid_argument("OME plane subresolution dimensions differ");}
    }
    std::shared_ptr<Block> block(const Ifd& d,std::uint32_t x,std::uint32_t y,std::uint16_t sample) {
        select(d.offset);const auto number=d.tiled?TIFFComputeTile(handle_,x,y,0,d.planar==PLANARCONFIG_SEPARATE?sample:0):TIFFComputeStrip(handle_,y,d.planar==PLANARCONFIG_SEPARATE?sample:0);const Key key{d.offset,number};
        if(auto found=cache_.find(key);found!=cache_.end()) {++stats_.cache_hits;lru_.splice(lru_.end(),lru_,found->second.position);return found->second.value;}
        const auto size=d.tiled?TIFFTileSize64(handle_):TIFFVStripSize64(handle_,std::min(d.block_height,d.height-y));
        const auto expected=std::uint64_t(d.block_width)*(d.tiled?d.block_height:std::min(d.block_height,d.height-y))*(d.bits/8)*(d.planar==PLANARCONFIG_SEPARATE?1:d.samples);
        if(!size || size!=expected || size>options_.decode_bytes || size>std::uint64_t(std::numeric_limits<tmsize_t>::max())) throw std::length_error("TIFF encoded block exceeds decode budget or has inconsistent geometry");
        auto value=std::make_shared<Block>();value->bytes.resize(std::size_t(size));error_[0]=0;
        const auto read=d.tiled?TIFFReadEncodedTile(handle_,number,value->bytes.data(),tmsize_t(size)):TIFFReadEncodedStrip(handle_,number,value->bytes.data(),tmsize_t(size));
        if(read<0 || std::uint64_t(read)!=size || error_[0]) failed("TIFF block decoding failed or returned incomplete samples");++stats_.decoded_blocks;stats_.decoded_bytes+=size;
        if(value->charge()<=options_.cache_bytes) {
            while(stats_.cache_bytes>options_.cache_bytes-value->charge() && !lru_.empty()) {auto old=cache_.find(lru_.front());stats_.cache_bytes-=old->second.value->charge();cache_.erase(old);lru_.pop_front();}
            lru_.push_back(key);try {cache_.emplace(key,Cache{value,std::prev(lru_.end())});}catch(...) {lru_.pop_back();throw;}stats_.cache_bytes+=value->charge();stats_.peak_cache_bytes=std::max(stats_.peak_cache_bytes,stats_.cache_bytes);
        }return value;
    }
    RawPixelBuffer raw_locked(const RawRegionRequest& r,const std::atomic<bool>* cancelled=nullptr) {
        detail::export_checkpoint(cancelled);if(closed_) throw std::runtime_error("TIFF image server is closed");r.validate(*raw_);detail::ReaderFileGuard guard(path_);if(!same_file(*source_->primary,*guard.version())) throw std::runtime_error("TIFF source changed; reopen it before reading");
        const auto& request=r.region;RawPixelBuffer output(request.width,request.height,r.channels,raw_->sample_type);const auto scale=std::int64_t(raw_->levels[request.level].downsample);
        const auto origin_x=floor_div(request.x,scale),origin_y=floor_div(request.y,scale);const auto width=raw_->levels[request.level].width,height=raw_->levels[request.level].height;
        const auto x0=std::max<std::int64_t>(0,origin_x),y0=std::max<std::int64_t>(0,origin_y),x1=std::min(width,origin_x+request.width),y1=std::min(height,origin_y+request.height);
        if(x0<x1 && y0<y1) {
            for(auto y=y0;y<y1;++y) std::fill_n(output.writable_valid().data()+std::size_t((y-origin_y)*request.width+x0-origin_x),std::size_t(x1-x0),std::uint8_t(1));
            std::visit([&](auto& values) {using T=typename std::decay_t<decltype(values)>::value_type;
                for(std::uint32_t c=0;c<r.channels;++c) {const auto m=mapping_[(std::size_t(r.t)*raw_->z_planes+r.z)*raw_->channels+r.first_channel+c];const auto& d=roots_[m.ifd].levels[request.level];
                    for(auto by=(y0/d.block_height)*d.block_height;by<y1;by+=d.block_height) for(auto bx=(x0/d.block_width)*d.block_width;bx<x1;bx+=d.block_width) {
                        detail::export_checkpoint(cancelled);
                        const auto decoded=block(d,std::uint32_t(bx),std::uint32_t(by),m.sample);const auto samples=d.planar==PLANARCONFIG_SEPARATE?1:d.samples;const auto selected=d.planar==PLANARCONFIG_SEPARATE?0:m.sample;
                        for(auto y=std::max(y0,by);y<std::min(y1,by+d.block_height);++y) for(auto x=std::max(x0,bx);x<std::min(x1,bx+d.block_width);++x) {
                            const auto source=(std::size_t(y-by)*d.block_width+std::size_t(x-bx))*samples+selected;const auto target=(std::size_t(y-origin_y)*std::size_t(request.width)+std::size_t(x-origin_x))*r.channels+c;
                            std::memcpy(&values[target],decoded->bytes.data()+source*sizeof(T),sizeof(T));
                        }
                    }
                }
            },output.writable_storage());
        }guard.verify();return output;
    }
public:
    TiffData(const std::string& path,TiffOpenOptions options,const std::atomic<bool>* cancelled):path_(fs::absolute(fs::u8path(path)).lexically_normal().u8string()),options_(std::move(options)),opening_cancel_(cancelled) {
        detail::export_checkpoint(cancelled);options_.validate();detail::ReaderFileGuard guard(path_);auto* open=TIFFOpenOptionsAlloc();if(!open) throw std::bad_alloc();
        TIFFOpenOptionsSetMaxSingleMemAlloc(open,tmsize_t(options_.decode_bytes));TIFFOpenOptionsSetMaxCumulatedMemAlloc(open,tmsize_t(options_.decode_bytes));TIFFOpenOptionsSetErrorHandlerExtR(open,error_handler,this);TIFFOpenOptionsSetWarningHandlerExtR(open,warning_handler,this);
#ifdef _WIN32
        handle_=TIFFOpenWExt(fs::u8path(path_).c_str(),"rm",open);
#else
        handle_=TIFFOpenExt(path_.c_str(),"rm",open);
#endif
        TIFFOpenOptionsFree(open);if(!handle_) failed("cannot open TIFF");
        try {
            char* description=nullptr;TIFFGetField(handle_,TIFFTAG_IMAGEDESCRIPTION,&description);const auto xml=bounded(description,1024*1024);std::set<std::uint64_t> offsets;
            do {detail::export_checkpoint(cancelled);if(roots_.size()>=65536 || !offsets.insert(TIFFCurrentDirOffset(handle_)).second) throw std::length_error("TIFF primary directory count/cycle limit exceeded");roots_.push_back(inspect());error_[0]=0;}while(TIFFReadDirectory(handle_));
            if(error_[0]) failed("cannot index TIFF directories");index(xml);levels();view_=std::make_shared<TiffView>(options_.view);
            if(view_->z>=raw_->z_planes || view_->t>=raw_->timepoints) throw std::out_of_range("TIFF display Z/time index is outside the image");
            if(view_->channels[0]==-2) {const bool rgb=roots_[mapping_.front().ifd].photo==PHOTOMETRIC_RGB;view_->channels=rgb?std::array<std::int32_t,3>{0,1,2}:std::array<std::int32_t,3>{0,0,0};}
            const auto limits=range(raw_->sample_type);for(std::size_t i=0;i<3;++i) {if(view_->channels[i]>=std::int32_t(raw_->channels)) throw std::out_of_range("TIFF display channel is outside the image");if(std::isnan(view_->lower[i])) {view_->lower[i]=limits.first;view_->upper[i]=limits.second;}}
            view_->validate(true);display_.width=raw_->width;display_.height=raw_->height;display_.levels=raw_->levels;display_.pixel_width_um=raw_->pixel_width_um;display_.pixel_height_um=raw_->pixel_height_um;display_.vendor="TIFF / "+sample_name(raw_->sample_type);
            guard.verify();detail::export_checkpoint(cancelled);auto descriptor=tiff_view_encode(*view_);descriptor.resize(descriptor.size()-64);source_=make_reader_source("tiff:"+descriptor,path_,"single_file",guard.version());options_.view=*view_;option_snapshot_=std::make_shared<const TiffOpenOptions>(options_);opening_cancel_=nullptr;
        }catch(...) {TIFFClose(handle_);handle_=nullptr;throw;}
    }
    ~TiffData() {if(handle_) TIFFClose(handle_);}
    TiffView resolve_view(TiffView view,const std::atomic<bool>* cancelled) const {
        detail::export_checkpoint(cancelled);view.validate();if(view.series!=raw_->series || view.z>=raw_->z_planes || view.t>=raw_->timepoints) throw std::out_of_range("TIFF display selection is outside this series");
        detail::ReaderFileGuard guard(path_);if(!same_file(*source_->primary,*guard.version())) throw std::runtime_error("TIFF source changed; relink/reopen it explicitly");
        if(view.channels[0]==-2) {const bool rgb=roots_[mapping_.front().ifd].photo==PHOTOMETRIC_RGB;view.channels=rgb?std::array<std::int32_t,3>{0,1,2}:std::array<std::int32_t,3>{0,0,0};}
        const auto limits=range(raw_->sample_type);for(std::size_t i=0;i<3;++i) {if(view.channels[i]>=std::int32_t(raw_->channels)) throw std::out_of_range("TIFF display channel is outside the image");if(std::isnan(view.lower[i])) {view.lower[i]=limits.first;view.upper[i]=limits.second;}}
        view.validate(true);guard.verify();detail::export_checkpoint(cancelled);return view;
    }
    const ImageMetadata& metadata() const {return display_;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled=nullptr) const {detail::export_checkpoint(cancelled);return raw_;}
    std::shared_ptr<const TiffView> tiff_view() const {return view_;}
    std::shared_ptr<const TiffOpenOptions> tiff_options() const {return option_snapshot_;}
    std::shared_ptr<const TiffReadStats> tiff_read_stats() const {std::lock_guard<std::mutex> lock(mutex_);return std::make_shared<const TiffReadStats>(stats_);}
    std::string reader_id() const {return "tiff";}std::string source_locator() const {return path_;}
    std::shared_ptr<const ReaderSource> reader_source() const {return source_;}
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>* cancelled=nullptr) const {std::lock_guard<std::mutex> lock(mutex_);return const_cast<TiffData*>(this)->raw_locked(r,cancelled);}
    PixelBuffer read_view(const RegionRequest& r,const TiffView& display_view) const {
        std::lock_guard<std::mutex> lock(mutex_);if(closed_) throw std::runtime_error("TIFF image server is closed");validate_region(display_,r);PixelBuffer output(r.width,r.height);auto& bytes=output.writable_bytes();
        std::vector<std::int32_t> channels;for(auto c:display_view.channels) if(c>=0 && std::find(channels.begin(),channels.end(),c)==channels.end()) channels.push_back(c);
        const auto plane=(std::size_t(display_view.t)*raw_->z_planes+display_view.z)*raw_->channels;const auto first=mapping_[plane+channels.front()];const auto& root=roots_[first.ifd].levels[r.level];std::optional<std::uint32_t> alpha;
        if(root.alpha && std::all_of(channels.begin(),channels.end(),[&](auto c){const auto m=mapping_[plane+c];return m.ifd==first.ifd && m.sample<root.samples-1;}))
            for(std::uint32_t c=0;c<raw_->channels;++c) if(mapping_[plane+c].ifd==first.ifd && mapping_[plane+c].sample==root.samples-1) {alpha=c;break;}
        const auto scale=std::int64_t(raw_->levels[r.level].downsample);
        // A large RGBA request never creates an equally large float64 working
        // image. Raw conversion uses fixed 256-square blocks plus decoder/cache.
        for(std::int64_t y=0;y<r.height;y+=256) for(std::int64_t x=0;x<r.width;x+=256) {
            RawRegionRequest request;request.region=RegionRequest(r.x+x*scale,r.y+y*scale,r.level,std::min<std::int64_t>(256,r.width-x),std::min<std::int64_t>(256,r.height-y));request.z=display_view.z;request.t=display_view.t;
            const auto n=std::size_t(request.region.width*request.region.height);std::vector<double> alphas(n,1);std::vector<std::uint8_t> valid(n,1);
            if(alpha) {request.first_channel=*alpha;const auto block=const_cast<TiffData*>(this)->raw_locked(request);const auto maximum=range(raw_->sample_type).second;
                std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i) {const auto v=double(values[i]);if(!block.valid()[i] || !std::isfinite(v)) valid[i]=0;else alphas[i]=std::clamp(v/maximum,0.,1.);}},block.storage());}
            for(auto c:channels) {request.first_channel=std::uint32_t(c);const auto block=const_cast<TiffData*>(this)->raw_locked(request);const auto m=mapping_[plane+c];const bool invert=roots_[m.ifd].photo==PHOTOMETRIC_MINISWHITE;
                std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i) {auto value=double(values[i]);if(!block.valid()[i] || !std::isfinite(value)) {valid[i]=0;continue;}
                    if(alpha && root.associated_alpha) value=alphas[i]>0?value/alphas[i]:0;
                    const auto target=(std::size_t(y+std::int64_t(i/std::size_t(request.region.width)))*std::size_t(r.width)+std::size_t(x)+i%std::size_t(request.region.width))*4;
                    for(std::size_t color=0;color<3;++color) if(display_view.channels[color]==c) {const auto lo=display_view.lower[color],hi=display_view.upper[color];double v=value<=lo?0:value>=hi?1:(value-lo)/(hi-lo);if(invert) v=1-v;bytes[target+color]=std::uint8_t(std::floor(v*255+.5));}
                }},block.storage());
            }
            for(std::size_t i=0;i<n;++i) {const auto target=(std::size_t(y+std::int64_t(i/std::size_t(request.region.width)))*std::size_t(r.width)+std::size_t(x)+i%std::size_t(request.region.width))*4;
                bytes[target+3]=valid[i]?std::uint8_t(std::floor(alphas[i]*255+.5)):0;if(!bytes[target+3]) bytes[target]=bytes[target+1]=bytes[target+2]=0;}
        }
        return output;
    }
    bool closed() const {std::lock_guard<std::mutex> lock(mutex_);return closed_;}
    void close() {std::lock_guard<std::mutex> lock(mutex_);if(handle_) TIFFClose(handle_);handle_=nullptr;closed_=true;cache_.clear();lru_.clear();stats_.cache_bytes=0;}
};
// Each logical view owns an independent close state and provenance descriptor.
// Native dataset/index/cache ownership is shared across immutable projections.
class TiffServer final:public ImageServer {
    mutable std::mutex mutex_;std::shared_ptr<TiffData> data_;std::atomic<bool> closed_{false};
    ImageMetadata metadata_;std::shared_ptr<const RawImageMetadata> raw_;
    std::shared_ptr<const TiffView> view_;std::shared_ptr<const TiffOpenOptions> options_;
    std::shared_ptr<const ReaderSource> source_;std::string path_;TiffReadStats final_stats_;
public:
    TiffServer(std::shared_ptr<TiffData> data,TiffView view,const std::atomic<bool>* cancelled):data_(std::move(data)) {
        view_=std::make_shared<const TiffView>(data_->resolve_view(view,cancelled));metadata_=data_->metadata();raw_=data_->raw_metadata(cancelled);path_=data_->source_locator();
        auto options=*data_->tiff_options();options.view=*view_;options_=std::make_shared<const TiffOpenOptions>(options);
        auto descriptor=tiff_view_encode(*view_);descriptor.resize(descriptor.size()-64);source_=make_reader_source("tiff:"+descriptor,path_,"single_file",data_->reader_source()->primary);
    }
    const ImageMetadata& metadata() const override {return metadata_;}
    std::string reader_id() const override {return "tiff";}std::string source_locator() const override {return path_;}
    std::shared_ptr<const ReaderSource> reader_source() const override {return source_;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled=nullptr) const override {detail::export_checkpoint(cancelled);return raw_;}
    std::shared_ptr<const TiffView> tiff_view() const override {return view_;}
    std::shared_ptr<const TiffOpenOptions> tiff_options() const override {return options_;}
    std::shared_ptr<const TiffReadStats> tiff_read_stats() const override {std::lock_guard<std::mutex> lock(mutex_);return data_?data_->tiff_read_stats():std::make_shared<const TiffReadStats>(final_stats_);}
    PixelBuffer read_region(const RegionRequest& r) const override {std::lock_guard<std::mutex> lock(mutex_);if(closed_) throw std::runtime_error("TIFF view is closed");return data_->read_view(r,*view_);}
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>* cancelled=nullptr) const override {std::lock_guard<std::mutex> lock(mutex_);if(closed_) throw std::runtime_error("TIFF view is closed");return data_->read_raw(r,cancelled);}
    std::shared_ptr<ImageServer> with_tiff_view(const TiffView& view,const std::atomic<bool>* cancelled=nullptr) const override {std::lock_guard<std::mutex> lock(mutex_);if(closed_) throw std::runtime_error("TIFF view is closed");return std::make_shared<TiffServer>(data_,view,cancelled);}
    bool closed() const override {return closed_.load(std::memory_order_relaxed);}
    void close() override {std::lock_guard<std::mutex> lock(mutex_);if(data_) {final_stats_=*data_->tiff_read_stats();final_stats_.cache_bytes=0;data_.reset();}closed_=true;}
};
}
std::shared_ptr<ImageServer> open_tiff(const std::string& path,TiffOpenOptions options,const std::atomic<bool>* cancelled) {
    auto data=std::make_shared<TiffData>(path,std::move(options),cancelled);return std::make_shared<TiffServer>(data,*data->tiff_view(),cancelled);
}
#endif
}
