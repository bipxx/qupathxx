#include "qupathxx/scientific_raw_sampling.hpp"
#include "exact_weighted_mean.hpp"
#include "sha256.hpp"
#include <array>
#include <cstring>
#include <optional>
#include <type_traits>

namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* c) {
    if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("scientific raw sampling cancelled");
}
RawRegionRequest request_for(const ScientificGrid& grid,const ScientificRawOptions& o) {
    const auto b=grid.clipped_read_bounds();
    return {{b.x,b.y,0,std::max<std::int64_t>(1,b.width),std::max<std::int64_t>(1,b.height)},o.first_channel,o.channels,o.z,o.t};
}
void text(Sha256& hash,const std::string& value) {hash.integer(value.size());hash.update(value.data(),value.size());}
void real(Sha256& hash,double value) {std::uint64_t raw;std::memcpy(&raw,&value,8);hash.integer(raw);}
void optional(Sha256& hash,std::optional<double> value) {hash.integer(value.has_value());if(value) real(hash,*value);}
template<class T> auto raw_bits(const T& value) {
    using U=std::conditional_t<sizeof(T)==8,std::uint64_t,std::conditional_t<sizeof(T)==4,std::uint32_t,
            std::conditional_t<sizeof(T)==2,std::uint16_t,std::uint8_t>>>;
    U out;std::memcpy(&out,&value,sizeof(T));return out;
}
template<class T> bool finite(const T& value) {
    if constexpr(std::is_same_v<T,float>) return ((raw_bits(value)>>23)&255)!=255;
    else if constexpr(std::is_same_v<T,double>) return ((raw_bits(value)>>52)&2047)!=2047;
    else return true;
}
}
void ScientificRawOptions::validate() const {
    if(!channels || channels>65536 || first_channel>=65536 || channels>65536-first_channel || z>=65536 || t>=65536)
        throw std::out_of_range("scientific raw channel/Z/time selection exceeds limits");
    if(!work_bytes || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid scientific raw work budget");
}
namespace {
std::size_t work_bytes(const RawImageMetadata& m,const ScientificGrid& grid,ScientificRawOptions o,bool materialize) {
    o.validate();sample_bytes(m.sample_type);
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || m.levels.empty() ||
       m.levels[0].width!=m.width || m.levels[0].height!=m.height || m.levels[0].downsample!=1)
        throw std::invalid_argument("scientific raw grid requires matching unit-scale raw level zero");
    if(!m.series_count || m.series>=m.series_count) throw std::invalid_argument("scientific raw source has invalid series metadata");
    const auto b=grid.clipped_read_bounds();
    auto request=request_for(grid,o);request.validate(m);
    const auto elements=std::uint64_t(grid.width())*grid.height()*o.channels;
    constexpr std::uint64_t visits=1ULL<<28;
    // Account for a per-channel exact-division pass as well as source visits.
    if(elements>visits/68 || grid.overlap_visits()>(visits-68*elements)/(std::uint64_t(o.channels)+1))
        throw std::length_error("scientific raw grid exceeds numeric traversal limit");
    if(std::uint64_t(grid.x().extent())>detail::ExactWeightedMean::maximum_weight/std::uint64_t(grid.y().extent()))
        throw std::length_error("scientific raw footprint exceeds exact weight bound");
    const auto metadata=m.payload_bytes();
    if(metadata>o.work_bytes/4) throw std::length_error("scientific raw metadata exceeds work budget");
    const auto source=std::uint64_t(b.width)*b.height*(std::uint64_t(o.channels)*sample_bytes(m.sample_type)+1);
    const auto bytes=source+(materialize?10*elements:0)+4*metadata+4*65536+8192+sizeof(ScientificRawGrid);
    if(bytes>o.work_bytes) throw std::length_error("scientific raw grid exceeds work budget");
    return std::size_t(bytes);
}
}
std::size_t scientific_raw_work_bytes(const RawImageMetadata& m,const ScientificGrid& grid,ScientificRawOptions o) {
    return work_bytes(m,grid,o,true);
}
ScientificRawGrid::ScientificRawGrid(ScientificGrid grid,RawImageMetadata metadata,ScientificRawOptions o,std::size_t work):
    grid_(std::move(grid)),metadata_(std::move(metadata)),options_(o),
    values_(std::size_t(grid_.width()*grid_.height())*o.channels,0.),valid_(values_.size(),0),
    reasons_(values_.size(),std::uint8_t(ScientificRawReason::OutsideSource)),work_bytes_(work) {}
namespace {
struct RawCapture {
    ScientificRawDigest digest;
    std::optional<RawPixelBuffer> source;
};
RawCapture capture_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled,bool materialize) {
    checkpoint(cancelled);o.validate();
    if(server.closed()) throw std::runtime_error("scientific raw reader is closed");
    const auto observed=server.raw_metadata(cancelled);checkpoint(cancelled);
    if(!observed) throw std::invalid_argument("scientific raw sampling requires a quantitative reader");
    const auto work=work_bytes(*observed,grid,o,materialize);
    // Snapshot before decoder callbacks; subsequent metadata changes cannot mix epochs.
    RawCapture capture;capture.digest.metadata=*observed;
    const auto* metadata=&capture.digest.metadata;
    auto reader=server.reader_id();
    if(reader.size()>65536 || reader.capacity()>131072) throw std::length_error("scientific raw reader identity exceeds budget");
    const auto b=grid.clipped_read_bounds();const auto source_pixels=std::uint64_t(b.width)*b.height;
    auto& source=capture.source;
    checkpoint(cancelled);
    if(b.width && b.height) {
        source.emplace(server.read_raw(request_for(grid,o),cancelled));checkpoint(cancelled);
        if(source->width()!=b.width || source->height()!=b.height || source->channels()!=o.channels ||
           source->sample_type()!=metadata->sample_type || source->storage().index()!=std::size_t(metadata->sample_type) ||
           source->valid().size()!=source_pixels ||
           std::visit([](const auto& values){return values.size();},source->storage())!=source_pixels*o.channels)
            throw std::runtime_error("scientific raw reader returned unexpected shape/type/storage");
        if(source->payload_bytes()>source_pixels*(std::uint64_t(o.channels)*sample_bytes(metadata->sample_type)+1))
            throw std::length_error("scientific raw reader returned excess buffer capacity");
        for(std::size_t i=0;i<source->valid().size();++i) {
            if((i&4095)==0) checkpoint(cancelled);
            if(source->valid()[i]>1) throw std::runtime_error("scientific raw reader returned invalid support bits");
        }
    }
    auto& out=capture.digest;out.reader=std::move(reader);out.estimated_work_bytes=work;
    Sha256 decoded;constexpr char format[]="qpx.raw-level-zero-window/1";decoded.update(format,sizeof(format)-1);
    for(auto v:{metadata->width,metadata->height,std::int64_t(metadata->channels),std::int64_t(metadata->z_planes),
                std::int64_t(metadata->timepoints),std::int64_t(metadata->series),std::int64_t(metadata->series_count),std::int64_t(metadata->sample_type),
                b.x,b.y,b.width,b.height,std::int64_t(o.first_channel),std::int64_t(o.channels),std::int64_t(o.z),std::int64_t(o.t)}) decoded.integer(v);
    if(source) {
        std::visit([&](const auto& values) {
            std::array<std::uint8_t,65536> bytes{};std::size_t used=0;
            for(std::size_t i=0;i<values.size();++i) {
                if((i&4095)==0) checkpoint(cancelled);
                const auto bits=raw_bits(values[i]);
                for(unsigned j=0;j<sizeof(values[i]);++j) {
                    bytes[used++]=std::uint8_t(bits>>(8*j));
                    if(used==bytes.size()) {decoded.update(bytes.data(),used);used=0;}
                }
            }
            if(used) decoded.update(bytes.data(),used);
        },source->storage());
        for(std::size_t at=0;at<source->valid().size();) {
            checkpoint(cancelled);const auto count=std::min<std::size_t>(65536,source->valid().size()-at);
            decoded.update(source->valid().data()+at,count);at+=count;
        }
    }
    out.decoded_source_sha256=decoded.finish();
    Sha256 input;constexpr char recipe[]="qpx.grid-raw-area-exact/1";input.update(recipe,sizeof(recipe)-1);
    text(input,out.decoded_source_sha256);text(input,out.reader);
    for(auto v:{grid.image_width(),grid.image_height(),grid.x().origin(),grid.y().origin(),grid.x().extent(),grid.y().extent(),
                grid.x().samples(),grid.y().samples(),grid.x().context(),grid.y().context()}) input.integer(v);
    text(input,metadata->dimension_order);input.integer(metadata->levels.size());
    for(const auto& level:metadata->levels) {checkpoint(cancelled);input.integer(level.width);input.integer(level.height);real(input,level.downsample);}
    input.integer(metadata->channel_names.size());for(const auto& name:metadata->channel_names) {checkpoint(cancelled);text(input,name);}
    optional(input,metadata->pixel_width_um);optional(input,metadata->pixel_height_um);
    optional(input,metadata->z_spacing_um);optional(input,metadata->time_interval_seconds);
    out.input_sha256=input.finish();checkpoint(cancelled);
    return capture;
}
}
ScientificRawDigest digest_scientific_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled) {
    return capture_raw(server,grid,o,cancelled,false).digest;
}
ScientificRawGrid read_scientific_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled) {
    auto capture=capture_raw(server,grid,o,cancelled,true);
    const auto& metadata=capture.digest.metadata;
    ScientificRawGrid out(grid,metadata,o,capture.digest.estimated_work_bytes);
    out.reader_=std::move(capture.digest.reader);
    out.decoded_source_sha256_=std::move(capture.digest.decoded_source_sha256);
    out.input_sha256_=std::move(capture.digest.input_sha256);
    const auto& source=capture.source;
    if(!source) return out;
    const auto b=grid.clipped_read_bounds();
    const auto& ax=grid.x();const auto& ay=grid.y();std::uint64_t visited=0;
    std::visit([&](const auto& values) {
        for(auto y=ay.first_index();y<ay.stop_index();++y) {
            checkpoint(cancelled);const auto first_y=ay.edge(y).floor(),stop_y=ay.edge(y+1).ceil();
            for(auto x=ax.first_index();x<ax.stop_index();++x) {
                if((++visited&4095)==0) checkpoint(cancelled);
                const auto first_x=ax.edge(x).floor(),stop_x=ax.edge(x+1).ceil();
                if(first_x<0 || first_y<0 || stop_x>metadata.width || stop_y>metadata.height) continue;
                const auto output=std::size_t((y-ay.first_index())*grid.width()+x-ax.first_index())*o.channels;
                bool supported=true;
                for(auto sy=first_y;sy<stop_y && supported;++sy) for(auto sx=first_x;sx<stop_x;++sx) {
                    if((++visited&4095)==0) checkpoint(cancelled);
                    const auto pixel=std::size_t((sy-b.y)*b.width+sx-b.x);
                    if(!source->valid()[pixel]) {supported=false;break;}
                }
                if(!supported) {
                    for(std::uint32_t c=0;c<o.channels;++c) out.reasons_[output+c]=std::uint8_t(ScientificRawReason::InvalidSource);
                    continue;
                }
                for(std::uint32_t c=0;c<o.channels;++c) {
                    checkpoint(cancelled);detail::ExactWeightedMean mean;bool finite_channel=true;
                    for(auto sy=first_y;sy<stop_y && finite_channel;++sy) for(auto sx=first_x;sx<stop_x;++sx) {
                        if((++visited&4095)==0) checkpoint(cancelled);
                        const auto index=std::size_t((sy-b.y)*b.width+sx-b.x)*o.channels+c;
                        if(!finite(values[index])) {finite_channel=false;break;}
                        const auto wx=std::min(ax.origin()*ax.samples()+(x+1)*ax.extent(),(sx+1)*ax.samples())-
                                      std::max(ax.origin()*ax.samples()+x*ax.extent(),sx*ax.samples());
                        const auto wy=std::min(ay.origin()*ay.samples()+(y+1)*ay.extent(),(sy+1)*ay.samples())-
                                      std::max(ay.origin()*ay.samples()+y*ay.extent(),sy*ay.samples());
                        mean.add(values[index],std::uint32_t(wx*wy));
                    }
                    if(!finite_channel) {out.reasons_[output+c]=std::uint8_t(ScientificRawReason::NonfiniteChannel);continue;}
                    const auto expected=std::uint64_t(ax.extent())*ay.extent();
                    if(mean.weight()!=expected) throw std::logic_error("scientific raw footprint weight differs");
                    out.values_[output+c]=mean.mean();out.valid_[output+c]=1;out.reasons_[output+c]=std::uint8_t(ScientificRawReason::Valid);
                }
            }
        }
    },source->storage());
    checkpoint(cancelled);return out;
}
}
