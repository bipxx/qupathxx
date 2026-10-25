#include "qupathxx/scientific_raw_sampling.hpp"
#include "exact_weighted_mean.hpp"
#include "scientific_raw_sampling_internal.hpp"
#include "sha256.hpp"
#include "qupathxx/source_identity.hpp"
#include <array>
#include <cstring>
#include <optional>
#include <type_traits>

namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* c) {
    if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("scientific raw sampling cancelled");
}
// Keep canonical HWC order while bounding a hash-only decoded chunk. The
// validity suffix needs one byte per captured pixel to preserve the existing
// values-then-validity digest grammar without a second decoder pass.
constexpr std::uint64_t digest_chunk_bytes=65536;
struct RawWindowPlan {
    std::int64_t width=0,height=0;
    std::uint64_t bytes=0;
    bool streamed=false;
};
RawWindowPlan window_plan(const RawImageMetadata& metadata,const ScientificGrid& grid,const ScientificRawOptions& o,bool materialize) {
    const auto b=grid.clipped_read_bounds();
    const auto pixel=std::uint64_t(o.channels)*sample_bytes(metadata.sample_type)+1;
    const auto source=std::uint64_t(b.width)*b.height*pixel;
    if(materialize || source<=digest_chunk_bytes) return {b.width,b.height,source,false};
    const auto capacity=std::max<std::uint64_t>(1,digest_chunk_bytes/pixel);
    const auto width=std::min<std::uint64_t>(b.width,capacity);
    const auto height=std::min<std::uint64_t>(b.height,std::max<std::uint64_t>(1,capacity/width));
    return {std::int64_t(width),std::int64_t(height),width*height*pixel+std::uint64_t(b.width)*b.height,true};
}
RawRegionRequest request_for(const ScientificGrid& grid,const ScientificRawOptions& o,const RawWindowPlan& plan) {
    const auto b=grid.clipped_read_bounds();
    return {{b.x,b.y,0,std::max<std::int64_t>(1,plan.width),std::max<std::int64_t>(1,plan.height)},o.first_channel,o.channels,o.z,o.t};
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
std::string detail::scientific_raw_input_sha256(const RawImageMetadata& metadata,const ScientificGrid& grid,const std::string& reader,const std::string& decoded,const std::atomic<bool>* cancelled) {
    Sha256 input;constexpr char recipe[]="qpx.grid-raw-area-exact/1";input.update(recipe,sizeof(recipe)-1);
    text(input,decoded);text(input,reader);
    for(auto v:{grid.image_width(),grid.image_height(),grid.x().origin(),grid.y().origin(),grid.x().extent(),grid.y().extent(),
                grid.x().samples(),grid.y().samples(),grid.x().context(),grid.y().context()}) input.integer(v);
    text(input,metadata.dimension_order);input.integer(metadata.levels.size());
    for(const auto& level:metadata.levels) {checkpoint(cancelled);input.integer(level.width);input.integer(level.height);real(input,level.downsample);}
    input.integer(metadata.channel_names.size());for(const auto& name:metadata.channel_names) {checkpoint(cancelled);text(input,name);}
    optional(input,metadata.pixel_width_um);optional(input,metadata.pixel_height_um);
    optional(input,metadata.z_spacing_um);optional(input,metadata.time_interval_seconds);
    return input.finish();
}
void ScientificRawOptions::validate() const {
    if(!channels || channels>65536 || first_channel>=65536 || channels>65536-first_channel || z>=65536 || t>=65536)
        throw std::out_of_range("scientific raw channel/Z/time selection exceeds limits");
    if(!work_bytes || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid scientific raw work budget");
}
namespace {
std::size_t work_bytes(const RawImageMetadata& m,const ScientificGrid& grid,ScientificRawOptions o,bool materialize,const detail::ScientificRawReserve& reserve={}) {
    if(reserve.expected_series && m.series!=*reserve.expected_series) throw std::invalid_argument("raw grid source series changed");
    o.validate();sample_bytes(m.sample_type);
    if(m.width!=grid.image_width() || m.height!=grid.image_height() || m.levels.empty() ||
       m.levels[0].width!=m.width || m.levels[0].height!=m.height || m.levels[0].downsample!=1)
        throw std::invalid_argument("scientific raw grid requires matching unit-scale raw level zero");
    if(!m.series_count || m.series>=m.series_count) throw std::invalid_argument("scientific raw source has invalid series metadata");
    const auto plan=window_plan(m,grid,o,materialize);
    auto request=request_for(grid,o,plan);request.validate(m);
    const auto elements=std::uint64_t(grid.width())*grid.height()*o.channels;
    constexpr std::uint64_t visits=1ULL<<28;
    // Account for a per-channel exact-division pass as well as source visits.
    if(elements>visits/68 || grid.overlap_visits()>(visits-68*elements)/(std::uint64_t(o.channels)+1))
        throw std::length_error("scientific raw grid exceeds numeric traversal limit");
    if(std::uint64_t(grid.x().extent())>detail::ExactWeightedMean::maximum_weight/std::uint64_t(grid.y().extent()))
        throw std::length_error("scientific raw footprint exceeds exact weight bound");
    const auto metadata=m.payload_bytes();
    if(metadata>o.work_bytes/4) throw std::length_error("scientific raw metadata exceeds work budget");
    const auto source=plan.bytes;
    if(reserve.fixed_bytes>o.work_bytes || (metadata && reserve.metadata_copies>(o.work_bytes-reserve.fixed_bytes)/metadata))
        throw std::length_error("raw analysis captured workspace exceeds work budget");
    const auto captured=reserve.fixed_bytes+reserve.metadata_copies*metadata;
    const auto bytes=source+(materialize?10*elements:0)+4*metadata+4*65536+8192+sizeof(ScientificRawGrid)+captured;
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
void validate_buffer(const RawPixelBuffer& source,const RawRegionRequest& request,SampleType type,const std::atomic<bool>* cancelled) {
    const auto pixels=std::uint64_t(request.region.width)*request.region.height;
    if(source.width()!=request.region.width || source.height()!=request.region.height || source.channels()!=request.channels ||
       source.sample_type()!=type || source.storage().index()!=std::size_t(type) || source.valid().size()!=pixels ||
       std::visit([](const auto& values){return values.size();},source.storage())!=pixels*request.channels)
        throw std::runtime_error("scientific raw reader returned unexpected shape/type/storage");
    if(source.payload_bytes()>pixels*(std::uint64_t(request.channels)*sample_bytes(type)+1))
        throw std::length_error("scientific raw reader returned excess buffer capacity");
    for(std::size_t i=0;i<source.valid().size();++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(source.valid()[i]>1) throw std::runtime_error("scientific raw reader returned invalid support bits");
    }
}
void hash_values(Sha256& hash,const RawPixelBuffer& source,const std::atomic<bool>* cancelled) {
    std::visit([&](const auto& values) {
        std::array<std::uint8_t,65536> bytes{};std::size_t used=0;
        for(std::size_t i=0;i<values.size();++i) {
            if((i&4095)==0) checkpoint(cancelled);
            const auto bits=raw_bits(values[i]);
            for(unsigned j=0;j<sizeof(values[i]);++j) {
                bytes[used++]=std::uint8_t(bits>>(8*j));
                if(used==bytes.size()) {hash.update(bytes.data(),used);used=0;}
            }
        }
        if(used) hash.update(bytes.data(),used);
    },source.storage());
}
void hash_validity(Sha256& hash,const std::vector<std::uint8_t>& validity,const std::atomic<bool>* cancelled) {
    for(std::size_t at=0;at<validity.size();) {
        checkpoint(cancelled);const auto count=std::min<std::size_t>(65536,validity.size()-at);
        hash.update(validity.data()+at,count);at+=count;
    }
}
RawCapture capture_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled,bool materialize,const detail::ScientificRawReserve& reserve={}) {
    checkpoint(cancelled);o.validate();
    if(server.closed()) throw std::runtime_error("scientific raw reader is closed");
    const auto observed=server.raw_metadata(cancelled);checkpoint(cancelled);
    if(!observed) throw std::invalid_argument("scientific raw sampling requires a quantitative reader");
    auto admitted_reserve=reserve;
    if(reserve.observed_source) {
        auto source=server.reader_source();
        if(!source || !source->primary) throw std::invalid_argument("raw analysis source has no observed file identity");
        const auto bytes=source->payload_bytes();
        if(admitted_reserve.fixed_bytes>o.work_bytes || (bytes && reserve.source_copies>(o.work_bytes-admitted_reserve.fixed_bytes)/bytes))
            throw std::length_error("raw analysis source workspace exceeds work budget");
        admitted_reserve.fixed_bytes+=reserve.source_copies*bytes;
        *reserve.observed_source=std::move(source);
    }
    const auto admitted=work_bytes(*observed,grid,o,materialize,admitted_reserve);
    // Snapshot before decoder callbacks; subsequent metadata changes cannot mix epochs.
    RawCapture capture;capture.digest.metadata=*observed;
    const auto* metadata=&capture.digest.metadata;
    const auto work=std::max(admitted,work_bytes(*metadata,grid,o,materialize,admitted_reserve));
    auto reader=server.reader_id();
    if(reader.size()>65536 || reader.capacity()>131072) throw std::length_error("scientific raw reader identity exceeds budget");
    const auto b=grid.clipped_read_bounds();
    const auto plan=window_plan(*metadata,grid,o,materialize);
    auto& out=capture.digest;out.reader=std::move(reader);out.estimated_work_bytes=work;
    Sha256 decoded;constexpr char format[]="qpx.raw-level-zero-window/1";decoded.update(format,sizeof(format)-1);
    for(auto v:{metadata->width,metadata->height,std::int64_t(metadata->channels),std::int64_t(metadata->z_planes),
                std::int64_t(metadata->timepoints),std::int64_t(metadata->series),std::int64_t(metadata->series_count),std::int64_t(metadata->sample_type),
                b.x,b.y,b.width,b.height,std::int64_t(o.first_channel),std::int64_t(o.channels),std::int64_t(o.z),std::int64_t(o.t)}) decoded.integer(v);
    if(plan.streamed) {
        checkpoint(cancelled);
        std::vector<std::uint8_t> validity(std::size_t(b.width)*b.height);
        if(validity.capacity()>validity.size()) throw std::length_error("scientific raw validity exceeds work budget");
        // Full-width row groups, or one row split horizontally: each chunk is
        // a contiguous range in the original logical HWC window.
        for(auto y=b.y;y<b.y+b.height;y+=plan.height) for(auto x=b.x;x<b.x+b.width;x+=plan.width) {
            checkpoint(cancelled);
            auto request=request_for(grid,o,plan);request.region.x=x;request.region.y=y;
            request.region.width=std::min(plan.width,b.x+b.width-x);
            request.region.height=std::min(plan.height,b.y+b.height-y);
            auto chunk=server.read_raw(request,cancelled);checkpoint(cancelled);
            validate_buffer(chunk,request,metadata->sample_type,cancelled);
            hash_values(decoded,chunk,cancelled);
            const auto offset=std::size_t((y-b.y)*b.width+x-b.x);
            std::copy(chunk.valid().begin(),chunk.valid().end(),validity.begin()+offset);
        }
        hash_validity(decoded,validity,cancelled);
    } else if(b.width && b.height) {
        checkpoint(cancelled);const auto request=request_for(grid,o,plan);
        capture.source.emplace(server.read_raw(request,cancelled));checkpoint(cancelled);
        validate_buffer(*capture.source,request,metadata->sample_type,cancelled);
        hash_values(decoded,*capture.source,cancelled);
        hash_validity(decoded,capture.source->valid(),cancelled);
    }
    out.decoded_source_sha256=decoded.finish();
    out.input_sha256=detail::scientific_raw_input_sha256(*metadata,grid,out.reader,out.decoded_source_sha256,cancelled);checkpoint(cancelled);
    return capture;
}
}
ScientificRawDigest digest_scientific_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled) {
    return capture_raw(server,grid,o,cancelled,false).digest;
}
struct ScientificRawKernel {
static ScientificRawGrid read(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled,const detail::ScientificRawReserve& reserve) {
    auto capture=capture_raw(server,grid,o,cancelled,true,reserve);
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
};
ScientificRawGrid read_scientific_raw(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const std::atomic<bool>* cancelled) {
    return ScientificRawKernel::read(server,grid,o,cancelled,{});
}
ScientificRawGrid detail::read_scientific_raw_reserved(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const ScientificRawReserve& reserve,const std::atomic<bool>* cancelled) {
    return ScientificRawKernel::read(server,grid,o,cancelled,reserve);
}
ScientificRawDigest detail::digest_scientific_raw_reserved(const ImageServer& server,const ScientificGrid& grid,ScientificRawOptions o,const ScientificRawReserve& reserve,const std::atomic<bool>* cancelled) {
    return capture_raw(server,grid,o,cancelled,false,reserve).digest;
}
std::size_t detail::scientific_raw_reserved_work_bytes(const RawImageMetadata& metadata,const ScientificGrid& grid,ScientificRawOptions o,bool materialize,const ScientificRawReserve& reserve) {
    return work_bytes(metadata,grid,o,materialize,reserve);
}
}
