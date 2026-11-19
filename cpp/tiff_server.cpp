#include "qupathxx/raw_image.hpp"
#include "reader_file_guard.hpp"
#include "reader_provenance.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include "decimal_quantity.hpp"
#include "filesystem_access.hpp"
#include "sha256.hpp"
#include "tiff_metadata_export.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <tuple>
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
detail::OmeTiffXmlExport detail::export_ome_tiff_xml(const std::string&,const std::string&,bool,const std::atomic<bool>*) {
    throw std::runtime_error("OME-TIFF XML export requires QUPATHXX_BUILD_TIFF=ON");
}
#else
namespace {
namespace fs=std::filesystem;
struct Ifd {
    std::uint64_t offset=0;
    std::uint32_t file=0;
    std::uint32_t width=0,height=0,block_width=0,block_height=0;
    std::uint16_t bits=0,format=SAMPLEFORMAT_UINT,samples=1,planar=PLANARCONFIG_CONTIG,photo=PHOTOMETRIC_MINISBLACK,orientation=ORIENTATION_TOPLEFT,compression=COMPRESSION_NONE;
    std::uint16_t fill_order=FILLORDER_MSB2LSB;
    bool tiled=false,alpha=false,associated_alpha=false;
    std::vector<std::uint64_t> sub;
    std::vector<Ifd> levels;
    bool packed_palette() const {return photo==PHOTOMETRIC_PALETTE && bits==4;}
    bool packed_gray() const {
        return (bits==1 || bits==4) && (photo==PHOTOMETRIC_MINISBLACK || photo==PHOTOMETRIC_MINISWHITE);
    }
    bool packed_samples() const {return packed_gray() || packed_palette();}
    SampleType type() const {
        if(packed_samples() && format==SAMPLEFORMAT_UINT) return SampleType::UInt8;
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
          (planar!=PLANARCONFIG_CONTIG && planar!=PLANARCONFIG_SEPARATE) ||
          orientation<ORIENTATION_TOPLEFT || orientation>ORIENTATION_LEFTBOT ||
          (photo!=PHOTOMETRIC_MINISBLACK && photo!=PHOTOMETRIC_MINISWHITE &&
           photo!=PHOTOMETRIC_RGB && photo!=PHOTOMETRIC_PALETTE))
            throw std::invalid_argument("TIFF requires supported grayscale, RGB or palette samples and a valid orientation");
        if(photo==PHOTOMETRIC_RGB && samples<3) throw std::invalid_argument("invalid TIFF RGB sample count");
        const bool lossless= compression==COMPRESSION_NONE || compression==COMPRESSION_LZW ||
            compression==COMPRESSION_PACKBITS || compression==COMPRESSION_ADOBE_DEFLATE ||
            compression==COMPRESSION_DEFLATE;
        if(photo==PHOTOMETRIC_PALETTE &&
           (format!=SAMPLEFORMAT_UINT || (bits!=4 && bits!=8 && bits!=16) || samples!=1 || !lossless))
            throw std::invalid_argument("TIFF palette requires one lossless unsigned 4-, 8- or 16-bit index");
        if(packed_palette() && fill_order!=FILLORDER_MSB2LSB)
            throw std::invalid_argument("TIFF 4-bit palette requires most-significant-bit FillOrder");
        if(packed_gray() && (samples!=1 || (fill_order!=FILLORDER_MSB2LSB && fill_order!=FILLORDER_LSB2MSB)))
            throw std::invalid_argument("Packed grayscale TIFF requires one unsigned sample and a valid FillOrder");
        if(packed_gray() && bits==4 && (fill_order!=FILLORDER_MSB2LSB || !lossless))
            throw std::invalid_argument("TIFF 4-bit grayscale requires lossless compression and most-significant-bit FillOrder");
    }
};
struct RasterAxes {
    bool transpose,reverse_x,reverse_y;
    std::int64_t last_x,last_y;
    explicit RasterAxes(const Ifd& d):
        transpose(d.orientation>=ORIENTATION_LEFTTOP),
        reverse_x(d.orientation==ORIENTATION_TOPRIGHT || d.orientation==ORIENTATION_BOTRIGHT ||
                  d.orientation==ORIENTATION_RIGHTTOP || d.orientation==ORIENTATION_RIGHTBOT),
        reverse_y(d.orientation==ORIENTATION_BOTRIGHT || d.orientation==ORIENTATION_BOTLEFT ||
                  d.orientation==ORIENTATION_RIGHTBOT || d.orientation==ORIENTATION_LEFTBOT),
        last_x(std::int64_t(transpose?d.height:d.width)-1),last_y(std::int64_t(transpose?d.width:d.height)-1) {}
    std::pair<std::int64_t,std::int64_t> stored(std::int64_t x,std::int64_t y) const {
        if(reverse_x) x=last_x-x;
        if(reverse_y) y=last_y-y;
        return transpose?std::make_pair(y,x):std::make_pair(x,y);
    }
    std::pair<std::int64_t,std::int64_t> canvas(std::int64_t x,std::int64_t y) const {
        if(transpose) std::swap(x,y);
        return {reverse_x?last_x-x:x,reverse_y?last_y-y:y};
    }
    std::int64_t output_step(std::int64_t width) const {
        return transpose?(reverse_y?-width:width):(reverse_x?-1:1);
    }
};
struct Mapping {std::uint32_t ifd=std::numeric_limits<std::uint32_t>::max();std::uint16_t sample=0;};
bool local(const tinyxml2::XMLElement* e,const char* expected) {if(!e) return false;const char* n=e->Name();const auto* colon=std::strrchr(n,':');return std::strcmp(colon?colon+1:n,expected)==0;}
const tinyxml2::XMLElement* child(const tinyxml2::XMLElement* e,const char* name) {for(auto* p=e?e->FirstChildElement():nullptr;p;p=p->NextSiblingElement()) if(local(p,name)) return p;return nullptr;}
std::optional<std::uint32_t> unsigned_decimal(const char* text) {
    if(!text)return {};
    std::uint32_t value=0;
    const auto end=text+std::strlen(text);
    const auto parsed=std::from_chars(text,end,value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=end)return {};
    return value;
}
std::uint32_t integer(const tinyxml2::XMLElement* e,const char* key,std::optional<std::uint32_t> fallback={}) {
    const auto* text=e->Attribute(key);if(!text) {if(fallback) return *fallback;throw std::invalid_argument(std::string("missing OME attribute ")+key);}
    const auto value=unsigned_decimal(text);
    if(!value)throw std::invalid_argument(std::string("invalid OME integer ")+key);
    return *value;
}
double ome_unit_scale(std::string_view unit,bool time) {
    double scale=0;
    // OME 2016-06 SI range. Store final factors directly so the existing
    // conversions keep their binary64 arithmetic (no pow or extra scaling).
    struct Prefix {const char* symbol;double micrometres,seconds;};
    static constexpr Prefix prefixes[]={
        {"Y",1e30,1e24},{"Z",1e27,1e21},{"E",1e24,1e18},{"P",1e21,1e15},
        {"T",1e18,1e12},{"G",1e15,1e9},{"M",1e12,1e6},{"k",1e9,1e3},
        {"h",1e8,1e2},{"da",1e7,1e1},{"",1e6,1},{"d",1e5,1e-1},
        {"c",1e4,1e-2},{"m",1e3,1e-3},{u8"\u00b5",1,1e-6},
        {"n",1e-3,1e-9},{"p",1e-6,1e-12},{"f",1e-9,1e-15},
        {"a",1e-12,1e-18},{"z",1e-15,1e-21},{"y",1e-18,1e-24}};
    if(time && unit=="min") scale=60;
    else if(time && unit=="h") scale=3600;
    else if(time && unit=="d") scale=86400;
    else if(!time && unit=="um") scale=1;
    else if(!time && unit==u8"\u00c5") scale=.0001;
    else if(!unit.empty() && unit.back()==(time?'s':'m')) {
        const auto prefix=unit.substr(0,unit.size()-1);
        for(const auto& entry:prefixes) if(prefix==entry.symbol) {
            scale=time?entry.seconds:entry.micrometres;break;
        }
    }
    if(!time && !scale) {
        // Non-SI OME length units, including its approximate parsec factor.
        // Final micrometre factors leave the legacy/SI arithmetic untouched.
        static constexpr std::pair<const char*,double> lengths[]={
            {"in",25400.},{"thou",25.4},{"li",25400./12.},{"ft",304800.},
            {"yd",914400.},{"mi",1609344000.},{"ua",1.495978707e17},
            {"ly",9.4607304725808e21},{"pc",3.0856776e22},{"pt",25400./72.}};
        for(const auto& entry:lengths) if(unit==entry.first) {scale=entry.second;break;}
    }
    return scale;
}
std::optional<double> quantity(const tinyxml2::XMLElement* e,const char* key,const char* unit_key,bool time=false) {
    const auto parsed=detail::positive_decimal_quantity(e->Attribute(key));if(!parsed)return {};
    const auto* attr=e->Attribute(unit_key);
    const auto scale=ome_unit_scale(attr?attr:(time?"s":u8"\u00b5m"),time);
    const auto value=*parsed*scale;
    return scale && std::isfinite(value) && value>0?std::optional<double>(value):std::nullopt;
}
enum class PlaneAxis { Z, Time };
std::optional<double> plane_coordinate(const tinyxml2::XMLElement& plane,PlaneAxis axis) {
    const bool time=axis==PlaneAxis::Time;
    const auto* text=plane.Attribute(time?"DeltaT":"PositionZ");
    if(!text || std::strlen(text)>128)return {};
    const auto value=detail::finite_decimal_number(text);
    const auto* unit=plane.Attribute(time?"DeltaTUnit":"PositionZUnit");
    const auto scale=ome_unit_scale(unit?unit:(time?"s":"reference frame"),time);
    if(!value || *value<0 || !scale)return {};
    const auto coordinate=*value*scale;
    if(!std::isfinite(coordinate) || (*value!=0 && coordinate==0))return {};
    return coordinate;
}
std::optional<double> regular_plane_step(const tinyxml2::XMLElement& pixels,const RawImageMetadata& metadata,
                                             const std::vector<std::uint32_t>& groups,
                                             PlaneAxis axis,const std::atomic<bool>* cancelled) {
    const bool time=axis==PlaneAxis::Time;
    const auto extent=time?metadata.timepoints:metadata.z_planes;
    if(extent<2 || pixels.Attribute(time?"TimeIncrement":"PhysicalSizeZ"))return {};
    std::vector<bool> seen(std::size_t(metadata.timepoints)*metadata.z_planes*groups.size());
    std::vector<std::optional<double>> coordinates(extent);
    std::size_t count=0;
    for(auto* plane=pixels.FirstChildElement();plane;plane=plane->NextSiblingElement()) {
        if(!local(plane,"Plane"))continue;
        detail::export_checkpoint(cancelled);
        const auto c=unsigned_decimal(plane->Attribute("TheC"));
        const auto z=unsigned_decimal(plane->Attribute("TheZ"));
        const auto t=unsigned_decimal(plane->Attribute("TheT"));
        if(!c || !z || !t || *z>=metadata.z_planes || *t>=metadata.timepoints)return {};
        const auto group=std::lower_bound(groups.begin(),groups.end(),*c);
        if(group==groups.end() || *group!=*c)return {};
        const auto index=(std::size_t(*t)*metadata.z_planes+*z)*groups.size()+
                         std::size_t(group-groups.begin());
        const auto value=plane_coordinate(*plane,axis);
        auto& coordinate=coordinates[time?*t:*z];
        if(seen[index] || !value || (coordinate && coordinate!=value))return {};
        seen[index]=true;
        coordinate=value;
        ++count;
    }
    if(count!=seen.size() || *coordinates[0]!=0 || *coordinates[1]<=0)return {};
    const auto step=*coordinates[1];
    for(std::size_t i=2;i<coordinates.size();++i) {
        const auto expected=step*i;
        // Decimal-to-binary64 unit conversion can put a regular endpoint one code away.
        if(*coordinates[i]<=*coordinates[i-1] || !std::isfinite(expected) ||
           *coordinates[i]<std::nextafter(expected,0.) ||
           *coordinates[i]>std::nextafter(expected,std::numeric_limits<double>::infinity()))return {};
    }
    return step;
}
struct PlaneCalibration {
    std::optional<double> z_spacing_um,time_interval_seconds;
};
std::string bounded(const char* text,std::size_t maximum) {if(!text) return {};std::size_t n=0;while(n<=maximum && text[n]) ++n;if(n>maximum) throw std::length_error("TIFF metadata text exceeds limit");return {text,n};}
std::int64_t floor_div(std::int64_t n,std::int64_t d) {return n/d-(n%d<0);}
bool same_file(const FileVersion& a,const FileVersion& b) {return a.identity_scheme==b.identity_scheme && a.identity==b.identity && a.bytes==b.bytes && a.stamp_scheme==b.stamp_scheme && a.modified_token==b.modified_token && a.change_token==b.change_token;}
std::pair<double,double> range(SampleType type) {
    switch(type) {case SampleType::UInt8:return {0,255};case SampleType::UInt16:return {0,65535};case SampleType::Int8:return {-128,127};case SampleType::Int16:return {-32768,32767};
    case SampleType::UInt32:return {0,4294967295.};case SampleType::Int32:return {-2147483648.,2147483647.};case SampleType::UInt64:return {0,18446744073709551616.};case SampleType::Int64:return {-9223372036854775808.,9223372036854775808.};default:return {0,1};}
}
class TiffData final {
    struct Block {std::vector<std::uint8_t> bytes;std::size_t charge() const {return bytes.capacity()+256;}};
    using Key=std::tuple<std::uint32_t,std::uint64_t,std::uint32_t>;
    struct Cache {std::shared_ptr<Block> value;std::list<Key>::iterator position;};
    TIFF* handle_=nullptr;std::string path_;TiffOpenOptions options_;std::array<char,4096> error_{};const std::atomic<bool>* opening_cancel_=nullptr;
    struct File {
        std::string path,uuid,xml;std::uint32_t first=0,count=0,metadata_file=UINT32_MAX;
        bool metadata_only=false;
        std::shared_ptr<const FileVersion> version;
        std::unique_ptr<detail::ReaderFileGuard> opening_guard;
    };
    std::vector<std::unique_ptr<File>> files_;std::uint32_t active_file_=UINT32_MAX;
    std::size_t xml_bytes_=0,catalog_entries_=0;
    std::vector<Ifd> roots_;std::vector<Mapping> mapping_;
    bool ome_has_roi_references_=false,ome_bit_=false;
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
    void activate(std::uint32_t file) {
        detail::export_checkpoint(opening_cancel_);
        if(handle_ && active_file_==file) return;
        if(handle_) TIFFClose(handle_);handle_=nullptr;active_file_=UINT32_MAX;error_[0]=0;
        auto* open=TIFFOpenOptionsAlloc();if(!open) throw std::bad_alloc();
        TIFFOpenOptionsSetMaxSingleMemAlloc(open,tmsize_t(options_.decode_bytes));TIFFOpenOptionsSetMaxCumulatedMemAlloc(open,tmsize_t(options_.decode_bytes));
        TIFFOpenOptionsSetErrorHandlerExtR(open,error_handler,this);TIFFOpenOptionsSetWarningHandlerExtR(open,warning_handler,this);
#ifdef _WIN32
        handle_=TIFFOpenWExt(fs::u8path(files_.at(file)->path).c_str(),"rm",open);
#else
        handle_=TIFFOpenExt(files_.at(file)->path.c_str(),"rm",open);
#endif
        TIFFOpenOptionsFree(open);if(!handle_) failed("cannot open TIFF");active_file_=file;
    }
    void select(std::uint32_t file,std::uint64_t offset) {activate(file);error_[0]=0;if(TIFFCurrentDirOffset(handle_)==offset) return;if(!TIFFSetSubDirectory(handle_,offset)) failed("cannot select TIFF directory");++stats_.directory_switches;}
    void select(const Ifd& d) {select(d.file,d.offset);}
    bool declared_palette(const Ifd& d) {
        select(d);
        // LibTIFF can replace a PALETTE declaration with grayscale or RGB
        // when ColorMap is missing. Inspect only the original inline tag.
        const bool big=TIFFIsBigTIFF(handle_),big_endian=TIFFIsBigEndian(handle_);
        const std::size_t count_bytes=big?8:2,entry_bytes=big?20:12,value_offset=big?12:8;
        const auto word=[&](const std::uint8_t* bytes,std::size_t size) {
            std::uint64_t value=0;
            for(std::size_t i=0;i<size;++i) value=(value<<8)|bytes[big_endian?i:size-1-i];
            return value;
        };
        const auto seek=TIFFGetSeekProc(handle_);
        const auto read=TIFFGetReadProc(handle_);
        const auto client=TIFFClientdata(handle_);
        const auto position=seek(client,0,SEEK_CUR);
        const auto size=files_[d.file]->version->bytes;
        std::array<std::uint8_t,20*64> bytes;
        if(position==toff_t(-1) || d.offset>size || count_bytes>size-d.offset ||
           seek(client,d.offset,SEEK_SET)!=d.offset || read(client,bytes.data(),tmsize_t(count_bytes))!=tmsize_t(count_bytes))
            throw std::invalid_argument("Cannot read stored TIFF directory count");
        const auto entries=word(bytes.data(),count_bytes);
        if(entries>4096 || entries>(size-d.offset-count_bytes)/entry_bytes)
            throw std::invalid_argument("Stored TIFF directory exceeds bounds");
        bool palette=false;
        for(std::uint64_t left=entries;left && !palette;) {
            detail::export_checkpoint(opening_cancel_);
            const auto count=std::min<std::uint64_t>(left,64);
            const auto chunk=tmsize_t(count*entry_bytes);
            if(read(client,bytes.data(),chunk)!=chunk) throw std::invalid_argument("Cannot read stored TIFF directory tags");
            for(std::size_t i=0;i<count;++i) {
                const auto* entry=bytes.data()+i*entry_bytes;
                if(word(entry,2)==TIFFTAG_PHOTOMETRIC && word(entry+2,2)==TIFF_SHORT &&
                   word(entry+4,value_offset-4)==1 && word(entry+value_offset,2)==PHOTOMETRIC_PALETTE) {
                    palette=true;
                    break;
                }
            }
            left-=count;
        }
        if(seek(client,position,SEEK_SET)!=position)
            throw std::runtime_error("Cannot restore TIFF decoder position");
        return palette;
    }

    std::array<std::uint16_t*,3> color_map(const Ifd& d) {
        select(d);
        std::array<std::uint16_t*,3> map{};
        if(!TIFFGetField(handle_,TIFFTAG_COLORMAP,&map[0],&map[1],&map[2]) ||
           !map[0] || !map[1] || !map[2])
            throw std::invalid_argument("TIFF palette requires its three ColorMap tables");
        return map;
    }
    void validate_color_map(const Ifd& d) {
        if(d.photo==PHOTOMETRIC_PALETTE) {
            if(raw_->dimension_order!="XYC") throw std::invalid_argument("OME-TIFF palette display is unsupported");
            color_map(d);
        } else if((d.photo==PHOTOMETRIC_MINISBLACK || d.photo==PHOTOMETRIC_RGB) && declared_palette(d)) {
            throw std::invalid_argument("TIFF palette requires its three ColorMap tables");
        }
    }
    static void parse_ome(const std::string& xml,tinyxml2::XMLDocument& document) {
        if(xml.find("<!DOCTYPE")!=std::string::npos || xml.find("<!ENTITY")!=std::string::npos) throw std::invalid_argument("OME document declarations/entities are unsupported");
        if(document.Parse(xml.data(),xml.size())!=tinyxml2::XML_SUCCESS || !local(document.RootElement(),"OME")) throw std::invalid_argument("malformed OME XML or non-OME companion");
    }
    std::uint32_t observe_file(const std::string& path) {
        const auto absolute=fs::absolute(fs::u8path(path)).lexically_normal().u8string();
        for(std::uint32_t i=0;i<files_.size();++i) if(files_[i]->path==absolute) return i;
        if(files_.size()>=64) throw std::length_error("TIFF fileset exceeds 64 files");
        auto file=std::make_unique<File>();file->path=absolute;file->opening_guard=std::make_unique<detail::ReaderFileGuard>(absolute);file->version=file->opening_guard->version();
        for(const auto& other:files_) if(other->version->identity_scheme==file->version->identity_scheme && other->version->identity==file->version->identity) throw std::invalid_argument("duplicate TIFF physical file under different paths");
        const auto ordinal=std::uint32_t(files_.size());files_.push_back(std::move(file));return ordinal;
    }
    void set_xml(std::uint32_t ordinal,std::string xml) {
        auto& entry=*files_[ordinal];entry.xml=std::move(xml);
        if(entry.xml.size()>4*1024*1024-xml_bytes_) throw std::length_error("TIFF fileset XML exceeds 4 MiB");xml_bytes_+=entry.xml.size();
        if(entry.xml.find("<OME")!=std::string::npos || entry.xml.find(":OME")!=std::string::npos) {
            tinyxml2::XMLDocument document;parse_ome(entry.xml,document);entry.uuid=bounded(document.RootElement()->Attribute("UUID"),256);
            for(std::uint32_t i=0;i<files_.size();++i) if(i!=ordinal && !entry.uuid.empty() && files_[i]->uuid==entry.uuid) throw std::invalid_argument("duplicate OME UUID across files");
        }
    }
    std::string embedded_xml() const {
        char* description=nullptr;
        TIFFGetField(handle_,TIFFTAG_IMAGEDESCRIPTION,&description);
        return bounded(description,1024*1024);
    }
    void load_tiff_description(std::uint32_t ordinal) {
        activate(ordinal);
        set_xml(ordinal,embedded_xml());
    }
    std::uint32_t load_file(const std::string& path) {
        const auto ordinal=observe_file(path);auto& entry=*files_[ordinal];
        if(entry.metadata_only) throw std::invalid_argument("OME TiffData references an XML metadata file");
        if(entry.count) return ordinal;
        load_tiff_description(ordinal);
        entry.first=std::uint32_t(roots_.size());std::set<std::uint64_t> offsets;
        do {detail::export_checkpoint(opening_cancel_);if(roots_.size()>=65536 || !offsets.insert(TIFFCurrentDirOffset(handle_)).second) throw std::length_error("TIFF primary directory count/cycle limit exceeded");auto directory=inspect();catalog_entries_+=1+directory.sub.size();if(catalog_entries_>131072) throw std::length_error("TIFF directory catalog exceeds limit");roots_.push_back(std::move(directory));error_[0]=0;}while(TIFFReadDirectory(handle_));
        if(error_[0]) failed("cannot index TIFF directories");entry.count=std::uint32_t(roots_.size())-entry.first;return ordinal;
    }
    std::uint32_t load_metadata_file(const std::string& path,bool index_tiff) {
        detail::export_checkpoint(opening_cancel_);const auto ordinal=observe_file(path);auto& entry=*files_[ordinal];
        if(entry.count || entry.metadata_only) return ordinal;
        std::ifstream input(detail::access_path(fs::u8path(entry.path)),std::ios::binary);
        if(!input) throw std::runtime_error("cannot read OME metadata file");
        std::array<unsigned char,4> prefix{};input.read(reinterpret_cast<char*>(prefix.data()),prefix.size());
        const bool little=prefix[0]=='I' && prefix[1]=='I' && (prefix[2]==42 || prefix[2]==43) && prefix[3]==0;
        const bool big=prefix[0]=='M' && prefix[1]=='M' && prefix[2]==0 && (prefix[3]==42 || prefix[3]==43);
        if(input.gcount()==4 && (little || big)) {
            input.close();entry.opening_guard->verify();
            if(index_tiff)return load_file(entry.path);
            load_tiff_description(ordinal);entry.metadata_only=true;
            return ordinal;
        }
        if(!entry.version->bytes || entry.version->bytes>1024*1024 || entry.version->bytes>4*1024*1024-xml_bytes_)
            throw std::length_error("OME metadata XML is empty or exceeds XML limits");
        input.clear();input.seekg(0);std::string xml(std::size_t(entry.version->bytes),'\0');
        for(std::size_t at=0;at<xml.size();) {
            detail::export_checkpoint(opening_cancel_);const auto count=std::min<std::size_t>(65536,xml.size()-at);
            input.read(xml.data()+at,std::streamsize(count));if(input.gcount()!=std::streamsize(count)) throw std::runtime_error("incomplete OME metadata read");at+=count;
        }
        if(input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("OME metadata grew during reading");
        entry.opening_guard->verify();detail::export_checkpoint(opening_cancel_);set_xml(ordinal,std::move(xml));entry.metadata_only=true;return ordinal;
    }
    std::uint32_t resolve_metadata(std::uint32_t file,bool index_tiff=true) {
        auto& entry=*files_[file];if(entry.metadata_file!=UINT32_MAX) return entry.metadata_file;
        if(entry.xml.find("<OME")==std::string::npos && entry.xml.find(":OME")==std::string::npos) return entry.metadata_file=file;
        tinyxml2::XMLDocument document;parse_ome(entry.xml,document);const auto* root=document.RootElement();const auto* binary=child(root,"BinaryOnly");
        if(!binary) return entry.metadata_file=file;
        if(child(root,"Image")) throw std::invalid_argument("OME BinaryOnly cannot also contain Image metadata");
        std::size_t count=0;for(auto* e=root->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"BinaryOnly")) ++count;
        if(count!=1) throw std::invalid_argument("OME requires one BinaryOnly metadata reference");
        const auto name=bounded(binary->Attribute("MetadataFile"),32768),uuid=bounded(binary->Attribute("UUID"),256);
        if(name.empty() || uuid.empty() || entry.uuid.empty()) throw std::invalid_argument("OME BinaryOnly requires MetadataFile and root/target UUIDs");
        detail::bounded_utf8(name,32768);
        const auto metadata=load_metadata_file((fs::u8path(entry.path).parent_path()/fs::u8path(name)).u8string(),index_tiff);
        if(files_[metadata]->uuid!=uuid) throw std::invalid_argument("OME metadata file UUID differs");
        tinyxml2::XMLDocument master;parse_ome(files_[metadata]->xml,master);
        if(child(master.RootElement(),"BinaryOnly") || !child(master.RootElement(),"Image"))
            throw std::invalid_argument("OME metadata master must contain full Image metadata");
        files_[metadata]->metadata_file=metadata;return entry.metadata_file=metadata;
    }
    static std::string ome_pixel_type(const tinyxml2::XMLElement* pixels) {
        auto type=bounded(pixels->Attribute("Type"),32);
        if(type=="bit" && integer(pixels,"SignificantBits",1)!=1)
            throw std::invalid_argument("OME bit pixels require SignificantBits of one");
        return type;
    }
    // Compare selected-series semantics; file UUIDs and XML formatting differ.
    static auto signature(const tinyxml2::XMLElement* p) {
        std::array<std::uint32_t,5> sizes{integer(p,"SizeX"),integer(p,"SizeY"),integer(p,"SizeC"),integer(p,"SizeZ"),integer(p,"SizeT")};
        std::vector<std::pair<std::uint32_t,std::string>> channels;std::uint32_t total=0;std::size_t bytes=0;
        if(!sizes[2] || sizes[2]>65536) throw std::length_error("OME channel count exceeds limits");
        for(auto* e=p->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"Channel")) {
            const auto n=integer(e,"SamplesPerPixel",1);if(!n || n>1024 || n>sizes[2]-total) throw std::invalid_argument("invalid OME channel sample count");
            auto name=bounded(e->Attribute("Name"),4096);if(name.empty()) name="Channel "+std::to_string(channels.size()+1);
            bytes+=name.size();if(bytes>4*1024*1024) throw std::length_error("OME channel names exceed limit");channels.emplace_back(n,std::move(name));total+=n;
        }
        if(channels.empty()) {for(std::uint32_t c=0;c<sizes[2];++c) channels.emplace_back(1,"Channel "+std::to_string(c+1));total=sizes[2];}
        if(total!=sizes[2]) throw std::invalid_argument("OME Channel samples do not match SizeC");
        return std::make_tuple(sizes,bounded(p->Attribute("DimensionOrder"),5),ome_pixel_type(p),channels,
            std::array<std::optional<double>,4>{quantity(p,"PhysicalSizeX","PhysicalSizeXUnit"),quantity(p,"PhysicalSizeY","PhysicalSizeYUnit"),quantity(p,"PhysicalSizeZ","PhysicalSizeZUnit"),quantity(p,"TimeIncrement","TimeIncrementUnit",true)});
    }
    void validate_companion_metadata(std::uint32_t metadata,const tinyxml2::XMLElement* primary,
                                     const std::vector<std::uint32_t>& groups,PlaneCalibration& calibration) {
        tinyxml2::XMLDocument document;parse_ome(files_[metadata]->xml,document);
        const tinyxml2::XMLElement* selected=nullptr;std::uint32_t count=0;
        for(auto* e=document.RootElement()->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"Image")) {if(count==raw_->series) selected=e;if(++count>1024) throw std::length_error("OME series count exceeds 1024");}
        const auto* pixels=child(selected,"Pixels");if(count!=raw_->series_count || !pixels || signature(pixels)!=signature(primary)) throw std::invalid_argument("OME companion series metadata differs");
        ome_has_roi_references_|=child(selected,"ROIRef")!=nullptr;
        if(calibration.z_spacing_um &&
           regular_plane_step(*pixels,*raw_,groups,PlaneAxis::Z,opening_cancel_)!=calibration.z_spacing_um)
            calibration.z_spacing_um.reset();
        if(calibration.time_interval_seconds &&
           regular_plane_step(*pixels,*raw_,groups,PlaneAxis::Time,opening_cancel_)!=calibration.time_interval_seconds)
            calibration.time_interval_seconds.reset();
    }
    using Guards=std::vector<std::unique_ptr<detail::ReaderFileGuard>>;
    Guards guard_files(const std::atomic<bool>* cancelled=nullptr) const {
        Guards guards;guards.reserve(files_.size());for(const auto& file:files_) {
            detail::export_checkpoint(cancelled);auto guard=std::make_unique<detail::ReaderFileGuard>(file->path);
            if(!same_file(*file->version,*guard->version())) throw std::runtime_error("TIFF source changed; reopen it before reading");guards.push_back(std::move(guard));
        }return guards;
    }
    static void verify_guards(const Guards& guards) {for(const auto& guard:guards) guard->verify();}
    Ifd inspect() {
        Ifd d;d.file=active_file_;d.offset=TIFFCurrentDirOffset(handle_);if(!TIFFGetField(handle_,TIFFTAG_IMAGEWIDTH,&d.width) || !TIFFGetField(handle_,TIFFTAG_IMAGELENGTH,&d.height)) failed("TIFF directory has no dimensions");
        TIFFGetFieldDefaulted(handle_,TIFFTAG_BITSPERSAMPLE,&d.bits);TIFFGetFieldDefaulted(handle_,TIFFTAG_SAMPLEFORMAT,&d.format);TIFFGetFieldDefaulted(handle_,TIFFTAG_SAMPLESPERPIXEL,&d.samples);TIFFGetFieldDefaulted(handle_,TIFFTAG_PLANARCONFIG,&d.planar);TIFFGetFieldDefaulted(handle_,TIFFTAG_PHOTOMETRIC,&d.photo);TIFFGetFieldDefaulted(handle_,TIFFTAG_ORIENTATION,&d.orientation);
        TIFFGetFieldDefaulted(handle_,TIFFTAG_COMPRESSION,&d.compression);
        if(d.packed_samples()) TIFFGetFieldDefaulted(handle_,TIFFTAG_FILLORDER,&d.fill_order);
        std::uint32_t depth=1;TIFFGetFieldDefaulted(handle_,TIFFTAG_IMAGEDEPTH,&depth);if(depth!=1) throw std::invalid_argument("volumetric TIFF IFDs are unsupported; use OME plane IFDs");
        d.tiled=TIFFIsTiled(handle_)!=0;if(d.tiled) {TIFFGetField(handle_,TIFFTAG_TILEWIDTH,&d.block_width);TIFFGetField(handle_,TIFFTAG_TILELENGTH,&d.block_height);TIFFGetFieldDefaulted(handle_,TIFFTAG_TILEDEPTH,&depth);if(depth!=1) throw std::invalid_argument("volumetric TIFF tiles are unsupported");}
        else {d.block_width=d.width;TIFFGetFieldDefaulted(handle_,TIFFTAG_ROWSPERSTRIP,&d.block_height);d.block_height=std::min(d.block_height,d.height);}
        std::uint16_t count=0;std::uint64_t* offsets=nullptr;if(TIFFGetField(handle_,TIFFTAG_SUBIFD,&count,&offsets)) {if(count>64) throw std::length_error("TIFF subresolution count exceeds 64");d.sub.assign(offsets,offsets+count);}
        std::uint16_t extras=0;std::uint16_t* extra=nullptr;
        if(TIFFGetField(handle_,TIFFTAG_EXTRASAMPLES,&extras,&extra) && d.samples==(d.photo==PHOTOMETRIC_RGB?4:2) && extras==1) {d.alpha=extra[0]==EXTRASAMPLE_ASSOCALPHA || extra[0]==EXTRASAMPLE_UNASSALPHA;d.associated_alpha=extra[0]==EXTRASAMPLE_ASSOCALPHA;}
        return d;
    }
    void index(std::uint32_t metadata_file) {
        const auto& xml=files_[metadata_file]->xml;
        raw_->series=options_.view.series;std::vector<std::uint32_t> groups;
        if(xml.find("<OME")==std::string::npos && xml.find(":OME")==std::string::npos) {
            if(raw_->series>=roots_.size()) throw std::out_of_range("TIFF page/series index is outside the file");const auto& d=roots_[raw_->series];
            raw_->width=d.width;raw_->height=d.height;raw_->channels=d.samples;raw_->series_count=static_cast<std::uint32_t>(roots_.size());raw_->dimension_order="XYC";
            mapping_.resize(d.samples);for(std::uint32_t c=0;c<d.samples;++c) {mapping_[c]={raw_->series,static_cast<std::uint16_t>(c)};raw_->channel_names.push_back(d.photo==PHOTOMETRIC_PALETTE?"Palette index":"Channel "+std::to_string(c+1));}
            select(d);std::uint16_t unit=RESUNIT_NONE;float x=0,y=0;TIFFGetFieldDefaulted(handle_,TIFFTAG_RESOLUTIONUNIT,&unit);TIFFGetField(handle_,TIFFTAG_XRESOLUTION,&x);TIFFGetField(handle_,TIFFTAG_YRESOLUTION,&y);
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
        ome_has_roi_references_=child(image,"ROIRef")!=nullptr;
        // Validate OME XY metadata in the stored raster axes before normalization.
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
        PlaneCalibration calibration{
            regular_plane_step(*p,*raw_,groups,PlaneAxis::Z,opening_cancel_),
            regular_plane_step(*p,*raw_,groups,PlaneAxis::Time,opening_cancel_)};
        mapping_.resize(std::size_t(raw_->channels)*raw_->z_planes*raw_->timepoints);bool found=false;
        auto group_index=[&](std::uint32_t c) {const auto at=std::find(groups.begin(),groups.end(),c);if(at==groups.end()) throw std::invalid_argument("OME FirstC must identify a channel group");return std::uint32_t(at-groups.begin());};
        std::set<std::uint32_t> validated_metadata{metadata_file};
        auto extent=[&](char axis)->std::uint32_t {return axis=='C'?std::uint32_t(groups.size()):axis=='Z'?raw_->z_planes:raw_->timepoints;};
        for(auto* e=p->FirstChildElement();e;e=e->NextSiblingElement()) if(local(e,"TiffData")) {
            detail::export_checkpoint(opening_cancel_);found=true;std::uint32_t file=metadata_file;
            if(const auto* uuid=child(e,"UUID")) {
                const auto filename=bounded(uuid->Attribute("FileName"),32768),value=bounded(uuid->GetText(),256);
                if(value.empty() || files_[0]->uuid.empty()) throw std::invalid_argument("OME file reference requires root and target UUIDs");
                if(!filename.empty()) file=load_file((fs::u8path(files_[metadata_file]->path).parent_path()/fs::u8path(filename)).u8string());
                else {const auto at=std::find_if(files_.begin(),files_.end(),[&](const auto& f){return f->uuid==value;});if(at==files_.end()) throw std::invalid_argument("unresolved OME UUID without FileName");file=std::uint32_t(at-files_.begin());}
                if(files_[file]->uuid!=value) throw std::invalid_argument("OME referenced file UUID differs");
                const auto metadata=resolve_metadata(file);
                if(validated_metadata.insert(metadata).second)
                    validate_companion_metadata(metadata,p,groups,calibration);
            }
            if(files_[file]->metadata_only) throw std::invalid_argument("OME TiffData requires an explicit pixel file UUID");
            const auto& catalog=*files_[file];
            std::array<std::uint32_t,3> position{};for(std::size_t i=0;i<3;++i) position[i]=order[i+2]=='C'?group_index(integer(e,"FirstC",0)):integer(e,order[i+2]=='Z'?"FirstZ":"FirstT",0);
            std::uint64_t linear=0,multiplier=1;for(std::size_t i=0;i<3;++i) {if(position[i]>=extent(order[i+2])) throw std::out_of_range("OME first plane exceeds dimensions");linear+=position[i]*multiplier;multiplier*=extent(order[i+2]);}
            const auto ifd=integer(e,"IFD",0);
            // The default mapping ignores trailing IFDs outside the OME lattice.
            // An explicit PlaneCount still has to fit both the lattice and file.
            const auto inferred=e->Attribute("IFD")?1u:std::min(catalog.count,std::uint32_t(planes-linear));
            const auto length=integer(e,"PlaneCount",inferred);
            if(length>planes-linear || ifd>=catalog.count || length>catalog.count-ifd) throw std::out_of_range("OME TiffData exceeds plane or IFD bounds");
            for(std::uint32_t step=0;step<length;++step) {detail::export_checkpoint(opening_cancel_);auto value=linear+step;std::uint32_t c=0,z=0,t=0;
                for(std::size_t i=0;i<3;++i) {const auto coordinate=std::uint32_t(value%extent(order[i+2]));value/=extent(order[i+2]);if(order[i+2]=='C') c=coordinate;else if(order[i+2]=='Z') z=coordinate;else t=coordinate;}
                const auto start=groups[c],end=c+1<groups.size()?groups[c+1]:raw_->channels;const auto global=catalog.first+ifd+step;const auto& d=roots_[global];if(d.samples!=end-start) throw std::invalid_argument("OME channel samples do not match TIFF IFD samples");
                for(std::uint32_t sample=start;sample<end;++sample) {auto& entry=mapping_[(std::size_t(t)*raw_->z_planes+z)*raw_->channels+sample];if(entry.ifd!=std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("duplicate OME plane mapping");entry={global,static_cast<std::uint16_t>(sample-start)};}
            }
        }
        if(!found || std::any_of(mapping_.begin(),mapping_.end(),[](auto m){return m.ifd==std::numeric_limits<std::uint32_t>::max();})) throw std::invalid_argument("OME plane mapping is incomplete");
        const auto declared=ome_pixel_type(p);ome_bit_=declared=="bit";
        if(!ome_bit_) {
            auto actual=sample_name(roots_[mapping_.front().ifd].type());
            if(actual=="float32") actual="float";if(actual=="float64") actual="double";
            if(declared!=actual) throw std::invalid_argument("OME pixel type differs from TIFF samples");
        }
        raw_->pixel_width_um=quantity(p,"PhysicalSizeX","PhysicalSizeXUnit");raw_->pixel_height_um=quantity(p,"PhysicalSizeY","PhysicalSizeYUnit");raw_->z_spacing_um=quantity(p,"PhysicalSizeZ","PhysicalSizeZUnit");raw_->time_interval_seconds=quantity(p,"TimeIncrement","TimeIncrementUnit",true);
        if(!raw_->z_spacing_um) raw_->z_spacing_um=calibration.z_spacing_um;
        if(!raw_->time_interval_seconds) raw_->time_interval_seconds=calibration.time_interval_seconds;
    }
    void levels() {
        std::set<std::uint32_t> selected;for(auto m:mapping_) selected.insert(m.ifd);std::size_t count=roots_.size();
        const auto orientation=roots_[mapping_.front().ifd].orientation;
        if(ome_has_roi_references_ && orientation!=ORIENTATION_TOPLEFT)
            throw std::invalid_argument("Non-top-left OME-TIFF with referenced ROI geometry is unsupported");
        for(auto index:selected) {auto& root=roots_[index];root.validate();
            if(root.orientation!=orientation)
                throw std::invalid_argument("OME-TIFF plane orientations differ");
            if(ome_bit_ && (!root.packed_gray() || root.bits!=1))
                throw std::invalid_argument("OME bit pixels require one-bit grayscale TIFF storage");
            if(!ome_bit_ && raw_->dimension_order!="XYC" && root.packed_gray())
                throw std::invalid_argument("Packed grayscale OME-TIFF is outside the native pixel type profile");
            validate_color_map(root);
            Ifd base=root;base.sub.clear();root.levels.push_back(std::move(base));
            for(auto offset:root.sub) {if(++count>131072) throw std::length_error("TIFF directory catalog exceeds limit");select(root.file,offset);auto level=inspect();level.validate();
                if(level.orientation!=root.orientation) throw std::invalid_argument("TIFF pyramid orientations differ");
                if(!level.sub.empty() || level.samples!=root.samples || level.type()!=root.type() ||
                   level.photo!=root.photo ||
                   ((level.packed_gray() || root.packed_gray()) && level.bits!=root.bits))
                    throw std::invalid_argument("incompatible TIFF subresolution samples");
                validate_color_map(level);
                root.levels.push_back(std::move(level));}
        }
        const auto& reference=roots_[mapping_.front().ifd];raw_->sample_type=reference.type();
        for(std::size_t level=0;level<reference.levels.size();++level) {const auto& d=reference.levels[level];double scale=1;
            if(level) {scale=std::round(double(reference.width)/d.width);if(scale<=raw_->levels.back().downsample || scale<2 || (std::uint64_t(reference.width)+std::uint64_t(scale)-1)/std::uint64_t(scale)!=d.width || (std::uint64_t(reference.height)+std::uint64_t(scale)-1)/std::uint64_t(scale)!=d.height) throw std::invalid_argument("TIFF subresolutions require increasing integer isotropic scales");}
            raw_->levels.push_back({d.width,d.height,scale});
        }
        for(auto index:selected) {const auto& root=roots_[index];if(root.width!=raw_->width || root.height!=raw_->height || root.type()!=raw_->sample_type || root.levels.size()!=raw_->levels.size()) throw std::invalid_argument("OME plane geometry/type/pyramid differs");
            for(std::size_t i=0;i<root.levels.size();++i) if(root.levels[i].width!=raw_->levels[i].width || root.levels[i].height!=raw_->levels[i].height) throw std::invalid_argument("OME plane subresolution dimensions differ");}
        if(RasterAxes(reference).transpose) {
            std::swap(raw_->width,raw_->height);
            std::swap(raw_->pixel_width_um,raw_->pixel_height_um);
            for(auto& level:raw_->levels) std::swap(level.width,level.height);
        }
    }
    std::shared_ptr<Block> block(const Ifd& d,std::uint32_t x,std::uint32_t y,std::uint16_t sample) {
        select(d);const auto number=d.tiled?TIFFComputeTile(handle_,x,y,0,d.planar==PLANARCONFIG_SEPARATE?sample:0):TIFFComputeStrip(handle_,y,d.planar==PLANARCONFIG_SEPARATE?sample:0);const Key key{d.file,d.offset,number};
        if(d.packed_samples() && !TIFFIsMSB2LSB(handle_))
            throw std::runtime_error("TIFF decoder did not return MSB-first packed samples");
        if(auto found=cache_.find(key);found!=cache_.end()) {++stats_.cache_hits;lru_.splice(lru_.end(),lru_,found->second.position);return found->second.value;}
        const auto size=d.tiled?TIFFTileSize64(handle_):TIFFVStripSize64(handle_,std::min(d.block_height,d.height-y));
        const auto samples=d.planar==PLANARCONFIG_SEPARATE?1:d.samples;
        const auto rows=d.tiled?d.block_height:std::min(d.block_height,d.height-y);
        const auto row_bytes=(std::uint64_t(d.block_width)*d.bits*samples+7)/8;
        if(!rows || row_bytes>options_.decode_bytes/rows) throw std::length_error("TIFF decoded block exceeds decode budget");
        const auto expected=row_bytes*rows;
        if(!size || size!=expected || size>options_.decode_bytes || size>std::uint64_t(std::numeric_limits<tmsize_t>::max())) throw std::length_error("TIFF encoded block exceeds decode budget or has inconsistent geometry");
        auto value=std::make_shared<Block>();value->bytes.resize(std::size_t(size));error_[0]=0;
        const auto read=d.tiled?TIFFReadEncodedTile(handle_,number,value->bytes.data(),tmsize_t(size)):TIFFReadEncodedStrip(handle_,number,value->bytes.data(),tmsize_t(size));
        if(read<0 || std::uint64_t(read)!=size || error_[0]) failed("TIFF block decoding failed or returned incomplete samples");++stats_.decoded_blocks;stats_.decoded_bytes+=size;
        if(value->charge()<=options_.cache_bytes) {
            while(stats_.cache_bytes>options_.cache_bytes-value->charge() && !lru_.empty()) {auto old=cache_.find(lru_.front());stats_.cache_bytes-=old->second.value->charge();cache_.erase(old);lru_.pop_front();}
            lru_.push_back(key);try {cache_.emplace(key,Cache{value,std::prev(lru_.end())});}catch(...) {lru_.pop_back();throw;}stats_.cache_bytes+=value->charge();stats_.peak_cache_bytes=std::max(stats_.peak_cache_bytes,stats_.cache_bytes);
        }return value;
    }
    RawPixelBuffer raw_locked(const RawRegionRequest& r,const std::atomic<bool>* cancelled=nullptr,bool guarded=false) {
        detail::export_checkpoint(cancelled);if(closed_) throw std::runtime_error("TIFF image server is closed");r.validate(*raw_);auto guards=guarded?Guards{}:guard_files(cancelled);
        const auto& request=r.region;RawPixelBuffer output(request.width,request.height,r.channels,raw_->sample_type);const auto scale=std::int64_t(raw_->levels[request.level].downsample);
        const auto origin_x=floor_div(request.x,scale),origin_y=floor_div(request.y,scale);const auto width=raw_->levels[request.level].width,height=raw_->levels[request.level].height;
        const auto x0=std::max<std::int64_t>(0,origin_x),y0=std::max<std::int64_t>(0,origin_y),x1=std::min(width,origin_x+request.width),y1=std::min(height,origin_y+request.height);
        if(x0<x1 && y0<y1) {
            for(auto y=y0;y<y1;++y) std::fill_n(output.writable_valid().data()+std::size_t((y-origin_y)*request.width+x0-origin_x),std::size_t(x1-x0),std::uint8_t(1));
            std::visit([&](auto& values) {using T=typename std::decay_t<decltype(values)>::value_type;
                const auto first=(std::size_t(r.t)*raw_->z_planes+r.z)*raw_->channels+r.first_channel;
                for(std::uint32_t begin=0;begin<r.channels;) {
                    const auto m=mapping_[first+begin];const auto& d=roots_[m.ifd].levels[request.level];
                    auto end=begin+1;
                    // Interleaved channels in one IFD share physical blocks.
                    // Retain one decoded block locally even when the LRU is off
                    // or too small; separate planes still need separate reads.
                    if(d.planar==PLANARCONFIG_CONTIG)
                        while(end<r.channels && mapping_[first+end].ifd==m.ifd) ++end;
                    const auto samples=d.planar==PLANARCONFIG_SEPARATE?1:d.samples;
                    const RasterAxes axes(d);
                    const auto [ax,ay]=axes.stored(x0,y0);
                    const auto [zx,zy]=axes.stored(x1-1,y1-1);
                    const auto sx0=std::min(ax,zx),sy0=std::min(ay,zy);
                    const auto sx1=std::max(ax,zx)+1,sy1=std::max(ay,zy)+1;
                    const auto step=axes.output_step(request.width);
                    for(auto by=(sy0/d.block_height)*d.block_height;by<sy1;by+=d.block_height) for(auto bx=(sx0/d.block_width)*d.block_width;bx<sx1;bx+=d.block_width) {
                        detail::export_checkpoint(cancelled);
                        const auto decoded=block(d,std::uint32_t(bx),std::uint32_t(by),m.sample);
                        for(auto y=std::max(sy0,by);y<std::min(sy1,by+d.block_height);++y) {
                            detail::export_checkpoint(cancelled);
                            const auto first_x=std::max(sx0,bx);
                            const auto [cx,cy]=axes.canvas(first_x,y);
                            auto target=(cy-origin_y)*request.width+cx-origin_x;
                            if(d.packed_samples()) {
                                const auto per_byte=8/d.bits;
                                const auto mask=(1u<<d.bits)-1;
                                const auto row_bytes=(std::size_t(d.block_width)*d.bits+7)/8;
                                const auto* row=decoded->bytes.data()+std::size_t(y-by)*row_bytes;
                                for(auto x=first_x;x<std::min(sx1,bx+d.block_width);++x,target+=step) {
                                    const auto column=std::size_t(x-bx);
                                    const auto packed=row[column/per_byte];
                                    const auto shift=8-d.bits*(1+column%per_byte);
                                    values[std::size_t(target)*r.channels+begin]=static_cast<T>(
                                        (packed>>shift)&mask);
                                }
                                continue;
                            }
                            for(auto x=first_x;x<std::min(sx1,bx+d.block_width);++x,target+=step) {
                                const auto source=(std::size_t(y-by)*d.block_width+std::size_t(x-bx))*samples;
                                for(auto c=begin;c<end;++c) {
                                    const auto selected=d.planar==PLANARCONFIG_SEPARATE?0:mapping_[first+c].sample;
                                    std::memcpy(&values[std::size_t(target)*r.channels+c],decoded->bytes.data()+(source+selected)*sizeof(T),sizeof(T));
                                }
                            }
                        }
                    }
                    begin=end;
                }
            },output.writable_storage());
        }verify_guards(guards);detail::export_checkpoint(cancelled);return output;
    }
    std::pair<double,double> display_range() const {
        const auto& root=roots_[mapping_.front().ifd];
        if(root.packed_gray()) return {0.,double((1u<<root.bits)-1)};
        return range(raw_->sample_type);
    }
    struct OpenInput {};
    TiffData(const std::string& path,TiffOpenOptions options,const std::atomic<bool>* cancelled,OpenInput)
        :path_(detail::export_path(path)),options_(std::move(options)),opening_cancel_(cancelled) {
        detail::export_checkpoint(cancelled);options_.validate();
    }
public:
    TiffData(const std::string& path,TiffOpenOptions options,const std::atomic<bool>* cancelled)
        :TiffData(path,std::move(options),cancelled,OpenInput{}) {
        try {
            load_file(path_);index(resolve_metadata(0));levels();view_=std::make_shared<TiffView>(options_.view);
            if(view_->z>=raw_->z_planes || view_->t>=raw_->timepoints) throw std::out_of_range("TIFF display Z/time index is outside the image");
            if(view_->channels[0]==-2) {const bool rgb=roots_[mapping_.front().ifd].photo==PHOTOMETRIC_RGB;view_->channels=rgb?std::array<std::int32_t,3>{0,1,2}:std::array<std::int32_t,3>{0,0,0};}
            const auto limits=display_range();for(std::size_t i=0;i<3;++i) {if(view_->channels[i]>=std::int32_t(raw_->channels)) throw std::out_of_range("TIFF display channel is outside the image");if(std::isnan(view_->lower[i])) {view_->lower[i]=limits.first;view_->upper[i]=limits.second;}}
            view_->validate(true);display_.width=raw_->width;display_.height=raw_->height;display_.levels=raw_->levels;display_.pixel_width_um=raw_->pixel_width_um;display_.pixel_height_um=raw_->pixel_height_um;display_.vendor="TIFF / "+sample_name(raw_->sample_type);
            for(const auto& file:files_) file->opening_guard->verify();detail::export_checkpoint(cancelled);
            auto descriptor=tiff_view_encode(*view_);descriptor.resize(descriptor.size()-64);
            if(files_.size()==1) source_=make_reader_source("tiff:"+descriptor,path_,"single_file",files_[0]->version);
            else {std::vector<FileVersion> companions;for(std::size_t i=1;i<files_.size();++i) companions.push_back(*files_[i]->version);source_=make_compound_reader_source("tiff:"+descriptor,path_,files_[0]->version,std::move(companions));}
            for(auto& file:files_) {file->opening_guard.reset();std::string{}.swap(file->xml);}options_.view=*view_;option_snapshot_=std::make_shared<const TiffOpenOptions>(options_);opening_cancel_=nullptr;
        }catch(...) {if(handle_) TIFFClose(handle_);handle_=nullptr;throw;}
    }
    ~TiffData() {if(handle_) TIFFClose(handle_);}
    static detail::OmeTiffXmlExport export_xml(const std::string& input,const std::string& output,bool resolve_companion,
                                             const std::atomic<bool>* cancelled) {
        TiffData data(input,{},cancelled,OpenInput{});
        const auto ordinal=data.observe_file(data.path_);
        const auto& file=*data.files_[ordinal];
        const auto destination=detail::export_path(output);
        detail::guard_file_output(file.path,file.version->identity,destination);
        data.load_tiff_description(ordinal);
        data.files_[ordinal]->metadata_only=true;
        const auto metadata=resolve_companion?data.resolve_metadata(ordinal,false):ordinal;
        const auto verify_inputs=[&] {
            for(const auto& member:data.files_) {
                detail::export_checkpoint(cancelled);
                member->opening_guard->verify();
                detail::guard_file_output(member->path,member->version->identity,destination);
            }
        };
        verify_inputs();
        if(fs::exists(fs::symlink_status(fs::u8path(destination))))throw WorkflowOutputExists("OME XML destination already exists");
        const auto& selected=*data.files_[metadata];
        const auto& xml=selected.xml;
        detail::bounded_utf8(xml,1024*1024);
        tinyxml2::XMLDocument parsed;
        parse_ome(xml,parsed);
        detail::AtomicExportFile target(destination,1024*1024,cancelled);
        target.write(xml.data(),xml.size());
        Sha256 digest;digest.update(xml.data(),xml.size());
        auto result=detail::OmeTiffXmlExport{destination,selected.path,xml.size(),digest.finish()};
        verify_inputs();
        target.commit(false);
        return result;
    }
    TiffView resolve_view(TiffView view,const std::atomic<bool>* cancelled) const {
        detail::export_checkpoint(cancelled);view.validate();if(view.series!=raw_->series || view.z>=raw_->z_planes || view.t>=raw_->timepoints) throw std::out_of_range("TIFF display selection is outside this series");
        auto guards=guard_files(cancelled);
        if(view.channels[0]==-2) {const bool rgb=roots_[mapping_.front().ifd].photo==PHOTOMETRIC_RGB;view.channels=rgb?std::array<std::int32_t,3>{0,1,2}:std::array<std::int32_t,3>{0,0,0};}
        const auto limits=display_range();for(std::size_t i=0;i<3;++i) {if(view.channels[i]>=std::int32_t(raw_->channels)) throw std::out_of_range("TIFF display channel is outside the image");if(std::isnan(view.lower[i])) {view.lower[i]=limits.first;view.upper[i]=limits.second;}}
        view.validate(true);verify_guards(guards);detail::export_checkpoint(cancelled);return view;
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
        std::lock_guard<std::mutex> lock(mutex_);if(closed_) throw std::runtime_error("TIFF image server is closed");validate_region(display_,r);auto guards=guard_files();PixelBuffer output(r.width,r.height);auto& bytes=output.writable_bytes();
        std::vector<std::int32_t> channels;for(auto c:display_view.channels) if(c>=0 && std::find(channels.begin(),channels.end(),c)==channels.end()) channels.push_back(c);
        const auto plane=(std::size_t(display_view.t)*raw_->z_planes+display_view.z)*raw_->channels;const auto first=mapping_[plane+channels.front()];const auto& root=roots_[first.ifd].levels[r.level];std::optional<std::uint32_t> alpha;
        const auto maximum_index=range(raw_->sample_type).second;
        const bool palette=root.photo==PHOTOMETRIC_PALETTE &&
            display_view.channels==std::array<std::int32_t,3>{0,0,0} &&
            display_view.lower==std::array<double,3>{0,0,0} &&
            display_view.upper==std::array<double,3>{maximum_index,maximum_index,maximum_index};
        if(root.alpha && std::all_of(channels.begin(),channels.end(),[&](auto c){const auto m=mapping_[plane+c];return m.ifd==first.ifd && m.sample<root.samples-1;}))
            for(std::uint32_t c=0;c<raw_->channels;++c) if(mapping_[plane+c].ifd==first.ifd && mapping_[plane+c].sample==root.samples-1) {alpha=c;break;}
        const auto scale=std::int64_t(raw_->levels[r.level].downsample);
        // A large RGBA request never creates an equally large float64 working
        // image. Raw conversion uses fixed 256-square blocks plus decoder/cache.
        for(std::int64_t y=0;y<r.height;y+=256) for(std::int64_t x=0;x<r.width;x+=256) {
            RawRegionRequest request;request.region=RegionRequest(r.x+x*scale,r.y+y*scale,r.level,std::min<std::int64_t>(256,r.width-x),std::min<std::int64_t>(256,r.height-y));request.z=display_view.z;request.t=display_view.t;
            if(palette) {
                auto& self=*const_cast<TiffData*>(this);
                const auto indices=self.raw_locked(request,nullptr,true);
                const auto map=self.color_map(root);
                const auto render=[&](const auto& values) {
                    for(std::int64_t row=0;row<indices.height();++row) {
                        auto* rgba=bytes.data()+4*((y+row)*r.width+x);
                        for(std::int64_t column=0;column<indices.width();++column,rgba+=4) {
                            const auto index=std::size_t(row*indices.width()+column);
                            if(!indices.valid()[index]) continue;
                            for(std::size_t c=0;c<map.size();++c)
                                rgba[c]=std::uint8_t((std::uint32_t(map[c][values[index]])*255+32767)/65535);
                            rgba[3]=255;
                        }
                    }
                };
                if(root.bits<16) render(std::get<std::vector<std::uint8_t>>(indices.storage()));
                else render(std::get<std::vector<std::uint16_t>>(indices.storage()));
                continue;
            }
            const auto n=std::size_t(request.region.width*request.region.height);std::vector<double> alphas(n,1);std::vector<std::uint8_t> valid(n,1);
            if(alpha) {request.first_channel=*alpha;const auto block=const_cast<TiffData*>(this)->raw_locked(request,nullptr,true);const auto maximum=range(raw_->sample_type).second;
                std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i) {const auto v=double(values[i]);if(!block.valid()[i] || !std::isfinite(v)) valid[i]=0;else alphas[i]=std::clamp(v/maximum,0.,1.);}},block.storage());}
            for(auto c:channels) {request.first_channel=std::uint32_t(c);const auto block=const_cast<TiffData*>(this)->raw_locked(request,nullptr,true);const auto m=mapping_[plane+c];const bool invert=roots_[m.ifd].photo==PHOTOMETRIC_MINISWHITE;
                std::visit([&](const auto& values){for(std::size_t i=0;i<n;++i) {auto value=double(values[i]);if(!block.valid()[i] || !std::isfinite(value)) {valid[i]=0;continue;}
                    if(alpha && root.associated_alpha) value=alphas[i]>0?value/alphas[i]:0;
                    const auto target=(std::size_t(y+std::int64_t(i/std::size_t(request.region.width)))*std::size_t(r.width)+std::size_t(x)+i%std::size_t(request.region.width))*4;
                    for(std::size_t color=0;color<3;++color) if(display_view.channels[color]==c) {const auto lo=display_view.lower[color],hi=display_view.upper[color];double v=value<=lo?0:value>=hi?1:(value-lo)/(hi-lo);if(invert) v=1-v;bytes[target+color]=std::uint8_t(std::floor(v*255+.5));}
                }},block.storage());
            }
            for(std::size_t i=0;i<n;++i) {const auto target=(std::size_t(y+std::int64_t(i/std::size_t(request.region.width)))*std::size_t(r.width)+std::size_t(x)+i%std::size_t(request.region.width))*4;
                bytes[target+3]=valid[i]?std::uint8_t(std::floor(alphas[i]*255+.5)):0;if(!bytes[target+3]) bytes[target]=bytes[target+1]=bytes[target+2]=0;}
        }
        verify_guards(guards);return output;
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
        auto descriptor=tiff_view_encode(*view_);descriptor.resize(descriptor.size()-64);const auto source=data_->reader_source();source_=source->companions.empty()?make_reader_source("tiff:"+descriptor,path_,"single_file",source->primary):make_compound_reader_source("tiff:"+descriptor,path_,source->primary,source->companions);
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
detail::OmeTiffXmlExport detail::export_ome_tiff_xml(const std::string& input,const std::string& output,bool resolve_companion,
                                                   const std::atomic<bool>* cancelled) {
    return TiffData::export_xml(input,output,resolve_companion,cancelled);
}
#endif
}
