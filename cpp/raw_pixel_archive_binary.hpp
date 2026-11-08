#pragma once
#include "raw_pixel_workflow_snapshot.hpp"
#include "provenance_codec.hpp"
#include "geometry_archive.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
namespace qpx::detail::raw_archive {
constexpr std::size_t limit=128u*1024*1024,record_limit=limit+4096;
inline void stop(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("raw pixel archive cancelled");}
inline std::string digest(const std::uint8_t* p,std::size_t n,const std::atomic<bool>* c) {Sha256 h;for(std::size_t at=0;at<n;) {stop(c);const auto count=std::min<std::size_t>(65536,n-at);h.update(p+at,count);at+=count;}stop(c);return h.finish();}
inline void hash_check(const std::string& value) {if(value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid raw pixel archive hash");}
struct Writer {
    std::vector<std::uint8_t> bytes;const std::atomic<bool>* cancelled;std::size_t maximum;
    Writer(const char* magic,const std::atomic<bool>* c,std::size_t max=limit):bytes(magic,magic+4),cancelled(c),maximum(max) {}
    void raw(const void* p,std::size_t size) {if(size>maximum-bytes.size()) throw std::length_error("raw pixel archive exceeds size limit");const auto* data=static_cast<const std::uint8_t*>(p);for(std::size_t at=0;at<size;) {stop(cancelled);const auto count=std::min<std::size_t>(65536,size-at);bytes.insert(bytes.end(),data+at,data+at+count);at+=count;}}
    void integer(std::uint64_t v) {std::uint8_t data[8];for(int i=0;i<8;++i) data[i]=std::uint8_t(v>>(8*i));raw(data,8);}
    void signed_integer(std::int64_t v) {std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
    void real(double v) {if(!std::isfinite(v)) throw std::invalid_argument("nonfinite raw pixel archive parameter");std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
    void optional(std::optional<double> v) {integer(bool(v));if(v) real(*v);}
    void blob(const std::vector<std::uint8_t>& v) {integer(v.size());raw(v.data(),v.size());}
    void text(const std::string& v) {integer(v.size());raw(v.data(),v.size());}
    void u32(std::uint32_t v) {std::uint8_t b[4];for(int i=0;i<4;++i) b[i]=std::uint8_t(v>>(8*i));raw(b,4);}
    void finish() {const auto sha=digest(bytes.data(),bytes.size(),cancelled);raw(sha.data(),sha.size());bytes.shrink_to_fit();}
};
struct Reader {
    const std::uint8_t* data;std::size_t left;const std::atomic<bool>* cancelled;
    Reader(const std::uint8_t* p,std::size_t n,const char* magic,const std::atomic<bool>* c,std::size_t maximum=limit):data(p),left(n>=68?n-68:0),cancelled(c) {
        stop(c);if(!p || n<68 || n>maximum || std::memcmp(p,magic,4) || digest(p,n-64,c)!=std::string(reinterpret_cast<const char*>(p+n-64),64)) throw std::invalid_argument("invalid raw pixel archive header/checksum");data+=4;
    }
    std::uint64_t integer(std::uint64_t max=UINT64_MAX) {if(left<8) throw std::invalid_argument("truncated raw pixel archive integer");std::uint64_t v=0;for(int i=0;i<8;++i) v|=std::uint64_t(data[i])<<(8*i);data+=8;left-=8;if(v>max) throw std::invalid_argument("raw pixel archive integer exceeds limit");return v;}
    std::int64_t signed_integer() {const auto bits=integer();std::int64_t value;std::memcpy(&value,&bits,8);return value;}
    double real() {const auto bits=integer();double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite raw pixel archive parameter");return v;}
    std::optional<double> optional() {if(!integer(1)) return {};return real();}
    std::pair<const std::uint8_t*,std::size_t> view(std::size_t maximum=limit) {const auto n=integer(maximum);if(n>left) throw std::invalid_argument("truncated raw pixel archive blob");auto result=std::make_pair(data,std::size_t(n));data+=n;left-=n;return result;}
    std::vector<std::uint8_t> blob(std::size_t maximum=limit) {const auto v=view(maximum);stop(cancelled);return {v.first,v.first+v.second};}
    std::string text(std::size_t maximum) {const auto v=view(maximum);std::string s(reinterpret_cast<const char*>(v.first),v.second);detail::bounded_utf8(s,maximum);return s;}
    std::uint32_t u32() {if(left<4) throw std::invalid_argument("truncated raw pixel feature array");std::uint32_t v=0;for(int i=0;i<4;++i) v|=std::uint32_t(data[i])<<(8*i);data+=4;left-=4;return v;}
    void finish() {if(left) throw std::invalid_argument("trailing raw pixel archive data");}
};
inline std::uint32_t float_bits(float value) {std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
inline float float_value(std::uint32_t bits) {float value;std::memcpy(&value,&bits,4);return value;}
template<class Grid> void grid(Writer& w,const Grid& g) {for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.x().extent(),g.x().samples(),g.x().context(),g.y().origin(),g.y().extent(),g.y().samples(),g.y().context()}) w.signed_integer(v);}
inline ScientificGrid grid(Reader& r) {
    const auto width=r.signed_integer(),height=r.signed_integer(),x=r.signed_integer(),extent_x=r.signed_integer(),samples_x=r.signed_integer(),context_x=r.signed_integer(),y=r.signed_integer(),extent_y=r.signed_integer(),samples_y=r.signed_integer(),context_y=r.signed_integer();
    return {width,height,{x,extent_x,samples_x,context_x},{y,extent_y,samples_y,context_y},1024ULL*1024*1024};
}
inline void source(Writer& w,const RawPixelSourceSnapshot& s) {
    if(!s.provenance) throw std::invalid_argument("raw pixel archive has no provenance");w.blob(ProvenanceCodec::encode(*s.provenance,true));const auto& m=s.metadata;
    w.signed_integer(m.width);w.signed_integer(m.height);for(auto v:{m.channels,m.z_planes,m.timepoints,m.series,m.series_count,std::uint32_t(m.sample_type)}) w.integer(v);
    w.integer(m.levels.size());for(const auto& level:m.levels) {w.signed_integer(level.width);w.signed_integer(level.height);w.real(level.downsample);}
    w.integer(m.channel_names.size());std::size_t names=0;for(const auto& name:m.channel_names) {detail::bounded_utf8(name,65536);names+=name.size();if(names>1024*1024) throw std::length_error("raw pixel metadata labels exceed limit");w.text(name);}detail::bounded_utf8(m.dimension_order,64);w.text(m.dimension_order);w.optional(m.pixel_width_um);w.optional(m.pixel_height_um);w.optional(m.z_spacing_um);w.optional(m.time_interval_seconds);w.text(s.epoch);
}
template<class Grid> RawPixelSourceSnapshot source(Reader& r,const Grid& g) {
    RawPixelSourceSnapshot s;const auto p=r.view(16u*1024*1024);s.provenance=ProvenanceCodec::decode(p.first,p.second,true);auto& m=s.metadata;m.width=r.signed_integer();m.height=r.signed_integer();m.channels=std::uint32_t(r.integer(65536));m.z_planes=std::uint32_t(r.integer(65536));m.timepoints=std::uint32_t(r.integer(65536));m.series=std::uint32_t(r.integer(UINT32_MAX));m.series_count=std::uint32_t(r.integer(UINT32_MAX));m.sample_type=SampleType(r.integer(9));
    const auto levels=r.integer(65536);for(std::uint64_t i=0;i<levels;++i) {const auto width=r.signed_integer(),height=r.signed_integer();const auto scale=r.real();if(width<=0 || height<=0 || scale<1) throw std::invalid_argument("invalid raw pixel source level");m.levels.push_back({width,height,scale});}
    const auto channels=r.integer(65536);std::size_t bytes=0;for(std::uint64_t i=0;i<channels;++i) {auto name=r.text(65536);bytes+=name.size();if(bytes>1024*1024) throw std::length_error("raw pixel metadata labels exceed limit");m.channel_names.push_back(std::move(name));}
    m.dimension_order=r.text(64);m.pixel_width_um=r.optional();m.pixel_height_um=r.optional();m.z_spacing_um=r.optional();m.time_interval_seconds=r.optional();s.epoch=r.text(64);hash_check(s.epoch);
    s.reader=s.provenance->reader;s.observed=s.provenance->reader_source;s.calibration={s.provenance->effective_pixel_width_um,s.provenance->effective_pixel_height_um};
    if(s.provenance->source.width!=g.image_width() || s.provenance->source.height!=g.image_height() || s.epoch!=raw_pixel_source_metadata_epoch(m,g) || (!m.channel_names.empty() && m.channel_names.size()!=m.channels)) throw std::invalid_argument("raw pixel source metadata/provenance mismatch");return s;
}
inline void options(Writer& w,const RawPixelTrainingOptions& o) {w.integer(o.max_samples);w.integer(o.max_samples_per_class);w.integer(o.work_bytes);w.integer(o.seed);w.integer(o.max_scanned_samples);w.integer(o.balanced);}
inline RawPixelTrainingOptions options(Reader& r) {RawPixelTrainingOptions o;o.max_samples=std::size_t(r.integer(100000));o.max_samples_per_class=std::size_t(r.integer(50000));o.work_bytes=std::size_t(r.integer(1024ULL*1024*1024));o.seed=r.integer();o.max_scanned_samples=r.integer(100000000);o.balanced=r.integer(1)!=0;o.validate();return o;}
template<class Table> std::string training_hash(const Table& table) {
    Sha256 h;const std::string domain="qpx.forest.training/1";h.update(domain.data(),domain.size());h.integer(table.row_count());const auto schema=table.recipe().feature_schema();h.integer(schema.size());h.integer(table.classes().size());
    auto text=[&](const std::string& s) {h.integer(s.size());h.update(s.data(),s.size());};for(const auto& f:schema) {text(f.name);text(f.unit);}for(const auto& name:table.classes()) text(name);for(auto value:table.values()) h.integer(float_bits(value));for(auto label:table.labels()) h.integer(label);return h.finish();
}
template<class Grid> bool supported_footprint(const Grid& g,std::int64_t x,std::int64_t y) {return g.x().edge(x).floor()>=0 && g.y().edge(y).floor()>=0 && g.x().edge(x+1).ceil()<=g.image_width() && g.y().edge(y+1).ceil()<=g.image_height();}
template<class Grid> void recipe_matches(const RawPixelRecipe& recipe,const RawPixelSourceSnapshot& source,const Grid& g) {
    auto m=source.metadata;m.pixel_width_um=source.calibration.x;m.pixel_height_um=source.calibration.y;if(!recipe.matches(m,g,recipe.selection())) throw std::invalid_argument("raw pixel archived recipe/source mismatch");
    auto selection=recipe.selection();selection.work_bytes=1024ULL*1024*1024;if constexpr(std::is_same_v<Grid,ScientificGrid>) scientific_raw_work_bytes(source.metadata,g,selection);
}
inline ScientificRawLattice lattice(Reader& r) {
    const auto width=r.signed_integer(),height=r.signed_integer(),x=r.signed_integer(),ex=r.signed_integer(),nx=r.signed_integer(),cx=r.signed_integer(),y=r.signed_integer(),ey=r.signed_integer(),ny=r.signed_integer(),cy=r.signed_integer();return {width,height,{x,ex,nx,cx},{y,ey,ny,cy}};
}

}
