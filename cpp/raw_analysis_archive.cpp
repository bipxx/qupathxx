#include "raw_analysis_internal.hpp"
#include "qupathxx/source_identity.hpp"
#include "geometry_archive.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
constexpr std::size_t limit=16u*1024*1024;
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','R','A',1};
    void integer(std::uint64_t value) {if(bytes.size()>limit-8) throw std::length_error("raw record exceeds byte limit");for(int i=0;i<8;++i) bytes.push_back(std::uint8_t(value>>(8*i)));}
    void real(double value) {std::uint64_t bits;std::memcpy(&bits,&value,8);integer(bits);}
    void optional(std::optional<double> value) {integer(value.has_value());if(value) real(*value);}
    void blob(const void* data,std::size_t count) {if(count>limit-8 || bytes.size()>limit-8-count) throw std::length_error("raw record exceeds byte limit");integer(count);const auto* first=static_cast<const std::uint8_t*>(data);bytes.insert(bytes.end(),first,first+count);}
    void text(const std::string& value) {blob(value.data(),value.size());}
};
struct Reader {
    const std::uint8_t* data;std::size_t size,at=4;
    Reader(const std::uint8_t* input,std::size_t count):data(input),size(count) {
        if(!input || count<68 || count>limit || std::memcmp(input,"QRA",3) || input[3]<1 || input[3]>4) throw std::invalid_argument("invalid raw analysis record encoding");
        Sha256 hash;hash.update(input,count-64);if(hash.finish()!=std::string(reinterpret_cast<const char*>(input+count-64),64)) throw std::invalid_argument("raw analysis record checksum mismatch");size-=64;
    }
    std::uint64_t integer() {if(at>size || size-at<8) throw std::invalid_argument("truncated raw analysis record");std::uint64_t value=0;for(int i=0;i<8;++i) value|=std::uint64_t(data[at++])<<(8*i);return value;}
    std::uint64_t bounded(std::uint64_t maximum) {const auto n=integer();if(n>maximum) throw std::invalid_argument("raw analysis integer exceeds limit");return n;}
    std::int64_t signed_integer() {const auto n=integer();return n<=std::uint64_t(INT64_MAX)?std::int64_t(n):-1-std::int64_t(UINT64_MAX-n);}
    double real() {const auto bits=integer();double value;std::memcpy(&value,&bits,8);return value;}
    std::optional<double> optional() {return bounded(1)?std::optional<double>(real()):std::nullopt;}
    std::pair<const std::uint8_t*,std::size_t> blob(std::size_t maximum) {const auto n=integer();if(n>maximum || at>size || n>size-at) throw std::invalid_argument("invalid raw analysis byte length");const auto* first=data+at;at+=std::size_t(n);return {first,std::size_t(n)};}
    std::string text(std::size_t maximum) {const auto value=blob(maximum);std::string result(reinterpret_cast<const char*>(value.first),value.second);detail::bounded_utf8(result,maximum);return result;}
};
}
std::vector<std::uint8_t> RawAnalysisCodec::encode(const RawAnalysisRecord& record,const ImageMetadata& image,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);record.validate(image,cancelled);const auto& result=*record.result;const auto& region=result.region_;const auto& metadata=result.metadata_;const auto& stats=result.statistics_;Writer out;
    if(result.grid_) {
        out.bytes[3]=2;const auto& captured=*result.grid_;const auto& g=captured.grid;const auto& raw=captured.source_metadata;
        for(auto n:{g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context()}) out.integer(n);
        out.integer(captured.options.work_bytes);out.text(captured.reader);out.text(captured.decoded_source_sha256);out.text(captured.sampling_input_sha256);
        for(auto n:{raw.width,raw.height,std::int64_t(raw.channels),std::int64_t(raw.z_planes),std::int64_t(raw.timepoints),std::int64_t(raw.series),std::int64_t(raw.series_count),std::int64_t(raw.sample_type)}) out.integer(n);
        out.text(raw.dimension_order);out.integer(raw.levels.size());
        for(const auto& level:raw.levels) {detail::export_checkpoint(cancelled);out.integer(level.width);out.integer(level.height);out.real(level.downsample);}
        out.integer(raw.channel_names.size());for(const auto& name:raw.channel_names) {detail::export_checkpoint(cancelled);out.text(name);}
        out.optional(raw.pixel_width_um);out.optional(raw.pixel_height_um);out.optional(raw.z_spacing_um);out.optional(raw.time_interval_seconds);
    }
    for(auto n:{record.id,std::uint64_t(record.requested_tile_size),record.max_pixels,result.object_id_,result.object_revision_,result.document_revision_,std::uint64_t(result.nucleus_),std::uint64_t(result.tile_size_),result.tiles_}) out.integer(n);
    const auto& context=result.context_;out.text(context.name);out.text(context.class_name);out.integer(context.parent_id);out.integer(std::uint64_t(context.kind));out.integer(context.tma.has_value());
    if(context.tma) {const auto& t=*context.tma;for(auto n:{t.core_id,t.core_revision,std::uint64_t(t.row),std::uint64_t(t.column),std::uint64_t(t.missing)}) out.integer(n);for(const auto* text:{&t.grid_name,&t.row_label,&t.column_label,&t.core_name,&t.case_id}) out.text(*text);}
    out.real(result.elapsed_ms_);out.text(result.algorithm_);out.text(result.input_sha256_);out.optional(result.pixel_width_um_);out.optional(result.pixel_height_um_);
    for(auto n:{std::uint64_t(region.region.x),std::uint64_t(region.region.y),std::uint64_t(region.region.width),std::uint64_t(region.region.height),std::uint64_t(region.region.level),std::uint64_t(region.first_channel),std::uint64_t(region.channels),std::uint64_t(region.z),std::uint64_t(region.t),
        std::uint64_t(metadata.width),std::uint64_t(metadata.height),std::uint64_t(metadata.level_width),std::uint64_t(metadata.level_height),std::uint64_t(metadata.channels),std::uint64_t(metadata.z_planes),std::uint64_t(metadata.timepoints),std::uint64_t(metadata.series),std::uint64_t(metadata.sample_type)}) out.integer(n);
    if(result.grid_) out.integer(0x7ff8000000000000ULL);else out.real(metadata.downsample);out.optional(metadata.z_spacing_um);out.optional(metadata.time_interval_seconds);
    out.integer(stats.pixels);out.integer(stats.valid_pixels);
    for(std::size_t i=0;i<region.channels;++i) {if(i%64==0) detail::export_checkpoint(cancelled);out.text(metadata.channel_names[i]);out.integer(stats.finite_samples[i]);out.integer(stats.nonfinite_samples[i]);out.optional(stats.means[i]);out.optional(stats.minima[i]);out.optional(stats.maxima[i]);}
    out.integer(record.mask.has_value());if(record.mask) {const auto bytes=GeometryArchiveCodec::encode(*record.mask);out.blob(bytes.data(),bytes.size());}
    // Original absolute observations remain historical facts across OS moves;
    // the project image link and explicit re-verification locate current data.
    const auto& source=*result.source_;const auto& file=*source.primary;
    for(const auto* text:{&source.generation,&source.reader,&source.locator,&source.scope,&file.path,&file.identity_scheme,&file.identity,&file.stamp_scheme,&file.modified_token,&file.change_token}) out.text(*text);
    out.integer(file.bytes);out.integer(file.observed_unix_ms);
    if(!source.companions.empty()) {
        out.bytes[3]=result.grid_?4:3;out.integer(source.companions.size());
        for(const auto& companion:source.companions) {
            detail::export_checkpoint(cancelled);
            for(const auto* text:{&companion.path,&companion.identity_scheme,&companion.identity,&companion.stamp_scheme,&companion.modified_token,&companion.change_token}) out.text(*text);
            out.integer(companion.bytes);out.integer(companion.observed_unix_ms);
        }
    }
    if(out.bytes.size()>limit-64) throw std::length_error("raw analysis record exceeds byte limit");Sha256 hash;hash.update(out.bytes.data(),out.bytes.size());const auto sum=hash.finish();out.bytes.insert(out.bytes.end(),sum.begin(),sum.end());detail::export_checkpoint(cancelled);return std::move(out.bytes);
}
std::shared_ptr<const RawAnalysisRecord> RawAnalysisCodec::decode(const std::uint8_t* data,std::size_t size,const ImageMetadata& image,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);Reader in(data,size);auto record=std::make_shared<RawAnalysisRecord>();auto result=std::shared_ptr<RawAnalysisResult>(new RawAnalysisResult());record->result=result;
    if(data[3]==2 || data[3]==4) {
        // Read in explicit sequence: function argument evaluation order is not
        // a portable stream order in C++17.
        const auto iw=in.signed_integer(),ih=in.signed_integer(),x=in.signed_integer(),y=in.signed_integer();
        const auto w=in.signed_integer(),h=in.signed_integer(),nx=in.signed_integer(),ny=in.signed_integer(),cx=in.signed_integer(),cy=in.signed_integer();
        ScientificRawOptions options;options.work_bytes=std::size_t(in.bounded(1024ULL*1024*1024));
        const auto reader=in.text(65536),decoded=in.text(64),input=in.text(64);
        RawImageMetadata raw;raw.width=in.signed_integer();raw.height=in.signed_integer();raw.channels=std::uint32_t(in.bounded(65536));
        raw.z_planes=std::uint32_t(in.bounded(65536));raw.timepoints=std::uint32_t(in.bounded(65536));raw.series=std::uint32_t(in.bounded(65535));raw.series_count=std::uint32_t(in.bounded(65536));raw.sample_type=SampleType(in.bounded(9));
        raw.dimension_order=in.text(65536);const auto levels=in.bounded(65536);raw.levels.reserve(std::size_t(levels));
        for(std::uint64_t i=0;i<levels;++i) {detail::export_checkpoint(cancelled);const auto lw=in.signed_integer(),lh=in.signed_integer();const auto downsample=in.real();raw.levels.push_back({lw,lh,downsample});}
        const auto channels=in.bounded(65536);raw.channel_names.reserve(std::size_t(channels));std::size_t label_bytes=0;
        for(std::uint64_t i=0;i<channels;++i) {detail::export_checkpoint(cancelled);auto name=in.text(4096);label_bytes+=name.size();if(label_bytes>4u*1024*1024) throw std::length_error("raw grid source labels exceed limit");raw.channel_names.push_back(std::move(name));}
        raw.pixel_width_um=in.optional();raw.pixel_height_um=in.optional();raw.z_spacing_um=in.optional();raw.time_interval_seconds=in.optional();
        result->grid_.emplace(RawAnalysisGrid{ScientificGrid(iw,ih,{x,w,nx,cx},{y,h,ny,cy},1024ULL*1024*1024),options,std::move(raw),decoded,input,reader});
    }
    record->id=in.integer();record->requested_tile_size=std::size_t(in.bounded(1024));record->max_pixels=in.integer();result->object_id_=in.integer();result->object_revision_=in.integer();result->document_revision_=in.integer();result->nucleus_=in.bounded(1)!=0;result->tile_size_=std::size_t(in.bounded(1024));result->tiles_=in.integer();
    auto& context=result->context_;context.name=in.text(4096);context.class_name=in.text(4096);context.parent_id=in.integer();context.kind=static_cast<ObjectKind>(in.bounded(2));
    if(in.bounded(1)) {RawTmaContext t;t.core_id=in.integer();t.core_revision=in.integer();t.row=std::uint32_t(in.bounded(9999));t.column=std::uint32_t(in.bounded(9999));t.missing=in.bounded(1)!=0;t.grid_name=in.text(256);t.row_label=in.text(128);t.column_label=in.text(128);t.core_name=in.text(4096);t.case_id=in.text(4096);context.tma=std::move(t);}
    result->elapsed_ms_=in.real();result->algorithm_=in.text(64);result->input_sha256_=in.text(64);result->pixel_width_um_=in.optional();result->pixel_height_um_=in.optional();
    auto& region=result->region_;region.region.x=in.signed_integer();region.region.y=in.signed_integer();region.region.width=std::int64_t(in.bounded(INT64_MAX));region.region.height=std::int64_t(in.bounded(INT64_MAX));region.region.level=std::int32_t(in.bounded(65535));
    region.first_channel=std::uint32_t(in.bounded(65535));region.channels=std::uint32_t(in.bounded(65536));region.z=std::uint32_t(in.bounded(65535));region.t=std::uint32_t(in.bounded(65535));
    auto& metadata=result->metadata_;metadata.width=std::int64_t(in.bounded(INT64_MAX));metadata.height=std::int64_t(in.bounded(INT64_MAX));metadata.level_width=std::int64_t(in.bounded(INT64_MAX));metadata.level_height=std::int64_t(in.bounded(INT64_MAX));
    metadata.channels=std::uint32_t(in.bounded(65536));metadata.z_planes=std::uint32_t(in.bounded(65536));metadata.timepoints=std::uint32_t(in.bounded(65536));metadata.series=std::uint32_t(in.bounded(65535));metadata.sample_type=static_cast<SampleType>(in.bounded(9));metadata.downsample=in.real();metadata.z_spacing_um=in.optional();metadata.time_interval_seconds=in.optional();
    auto& stats=result->statistics_;stats.pixels=in.integer();stats.valid_pixels=in.integer();
    metadata.channel_names.reserve(region.channels);stats.finite_samples.reserve(region.channels);stats.nonfinite_samples.reserve(region.channels);stats.means.reserve(region.channels);stats.minima.reserve(region.channels);stats.maxima.reserve(region.channels);
    std::size_t label_bytes=0;
    for(std::uint32_t i=0;i<region.channels;++i) {if(i%64==0) detail::export_checkpoint(cancelled);auto name=in.text(4096);label_bytes+=name.size();if(label_bytes>4u*1024*1024) throw std::length_error("raw analysis channel labels exceed limit");metadata.channel_names.push_back(std::move(name));stats.finite_samples.push_back(in.integer());stats.nonfinite_samples.push_back(in.integer());stats.means.push_back(in.optional());stats.minima.push_back(in.optional());stats.maxima.push_back(in.optional());}
    if(in.bounded(1)) {const auto bytes=in.blob(limit);record->mask=GeometryArchiveCodec::decode(bytes.first,bytes.second);}
    auto source=std::make_shared<ReaderSource>();auto file=std::make_shared<FileVersion>();source->generation=in.text(128);source->reader=in.text(256);source->locator=in.text(32768);source->scope=in.text(32);
    file->path=in.text(32768);file->identity_scheme=in.text(128);file->identity=in.text(256);file->stamp_scheme=in.text(128);file->modified_token=in.text(256);file->change_token=in.text(256);file->bytes=in.integer();file->observed_unix_ms=in.integer();source->primary=file;result->source_=source;
    if(data[3]>=3) {
        const auto count=in.bounded(63);if(!count) throw std::invalid_argument("empty compound raw source");source->companions.reserve(std::size_t(count));
        for(std::uint64_t i=0;i<count;++i) {
            detail::export_checkpoint(cancelled);FileVersion companion;
            companion.path=in.text(32768);companion.identity_scheme=in.text(128);companion.identity=in.text(256);companion.stamp_scheme=in.text(128);companion.modified_token=in.text(256);companion.change_token=in.text(256);companion.bytes=in.integer();companion.observed_unix_ms=in.integer();source->companions.push_back(std::move(companion));
        }
    }
    if(result->grid_) {
        auto& options=result->grid_->options;options.first_channel=region.first_channel;options.channels=region.channels;options.z=region.z;options.t=region.t;
        if(region.region.x!=0 || region.region.y!=0 || region.region.width!=1 || region.region.height!=1 || region.region.level!=0)
            throw std::invalid_argument("noncanonical raw grid internal selection");
        // QRA2 uses one canonical sentinel; reject alternate NaN payloads.
        std::uint64_t actual;std::memcpy(&actual,&metadata.downsample,8);
        if(actual!=0x7ff8000000000000ULL) throw std::invalid_argument("noncanonical raw grid scale sentinel");
    }
    if(in.at!=in.size) throw std::invalid_argument("raw analysis record has trailing bytes");record->validate(image,cancelled);detail::export_checkpoint(cancelled);
    // Only a complete semantic decode may establish this private authority.
    if(result->grid_) result->validated_grid_eligible_=result->statistics_.pixels;
    return record;
}
}
