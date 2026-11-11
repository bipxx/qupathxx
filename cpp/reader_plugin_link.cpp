#include "reader_plugin_link.hpp"
#include "reader_plugin.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace qpx::detail {
namespace {
using Json=nlohmann::json;
constexpr std::size_t text_limit=4096,raw_limit=1024*1024;
constexpr std::string_view binding_marker=":plugin/1:";
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(std::string("Invalid reader plugin link: ")+reason);}
bool valid_id(std::string_view id) noexcept {
    if(id.empty() || id.size()>128 || id.find('.')==std::string_view::npos)return false;
    bool start=true;
    for(char c:id) {
        if(c=='.'){if(start)return false;start=true;}
        else {if(start && !(c>='a' && c<='z'))return false;
            if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-'))return false;start=false;}
    }
    return !start;
}
bool digest(std::string_view value) noexcept {
    return value.size()==64 && value.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
void text(const std::string& value,bool required=false,std::size_t maximum=text_limit) {
    bounded_utf8(value,maximum);if(required && value.empty())invalid("empty required text");
}
Json parse(const std::string& value,std::size_t maximum) {
    bounded_utf8(value,maximum);
    try {
        return Json::parse(value,[](int depth,Json::parse_event_t,Json&) {
            if(depth>64)invalid("JSON nesting exceeds limit");return true;
        });
    }catch(const std::invalid_argument&){throw;}
    catch(const std::exception&){invalid("malformed JSON");}
}
std::string string(const Json& value,const char* key,std::size_t maximum=text_limit) {
    const auto& field=value.at(key);if(!field.is_string())invalid("expected text");
    auto result=field.get<std::string>();text(result,false,maximum);return result;
}
std::uint64_t integer(const Json& value,const char* key,std::uint64_t maximum=std::numeric_limits<std::uint32_t>::max()) {
    const auto& field=value.at(key);
    if(!field.is_number_integer() || (field.is_number_integer() && !field.is_number_unsigned() && field.get<std::int64_t>()<0))
        invalid("expected nonnegative integer");
    const auto result=field.get<std::uint64_t>();if(result>maximum)invalid("integer exceeds limit");return result;
}
std::int64_t positive_dimension(const Json& value,const char* key) {
    const auto result=integer(value,key,std::uint64_t(std::numeric_limits<std::int64_t>::max()));
    if(!result)invalid("nonpositive image dimension");return static_cast<std::int64_t>(result);
}
double real(const Json& value,const char* key) {
    const auto& field=value.at(key);if(!field.is_number())invalid("expected number");
    const auto result=field.get<double>();if(!std::isfinite(result))invalid("nonfinite metadata number");return result;
}
std::optional<double> optional(const Json& value,const char* key) {
    if(value.at(key).is_null())return {};const auto result=real(value,key);if(result<=0)invalid("nonpositive calibration");return result;
}
Json optional(std::optional<double> value) {return value?Json(*value):Json(nullptr);}
void calibration(std::optional<double> value) {if(value && (!std::isfinite(*value) || *value<=0))invalid("invalid calibration");}
void levels(const std::vector<ImageLevel>& value,std::int64_t width,std::int64_t height) {
    if(value.empty() || value.size()>65536 || value[0].width!=width || value[0].height!=height || value[0].downsample!=1)
        invalid("invalid level-zero lattice");
    for(const auto& level:value)if(level.width<=0 || level.height<=0 || !std::isfinite(level.downsample) || level.downsample<1)
        invalid("invalid pyramid level");
}
Json levels(const std::vector<ImageLevel>& value) {
    auto result=Json::array();for(const auto& level:value)result.push_back({{"width",level.width},{"height",level.height},{"downsample",level.downsample}});return result;
}
std::vector<ImageLevel> read_levels(const Json& value) {
    if(!value.is_array() || value.empty() || value.size()>65536)invalid("invalid level count");
    std::vector<ImageLevel> result;result.reserve(value.size());
    for(const auto& level:value)result.push_back({positive_dimension(level,"width"),positive_dimension(level,"height"),real(level,"downsample")});return result;
}
bool same_levels(const std::vector<ImageLevel>& a,const std::vector<ImageLevel>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(a[i].width!=b[i].width || a[i].height!=b[i].height || a[i].downsample!=b[i].downsample)return false;
    return true;
}
void image_metadata(const ImageMetadata& value) {
    if(value.width<=0 || value.height<=0 || value.pixel_type!=PixelType::UInt8 || value.layout!=PixelLayout::RGBA)
        invalid("invalid display metadata");
    levels(value.levels,value.width,value.height);text(value.vendor);calibration(value.pixel_width_um);calibration(value.pixel_height_um);
}
void raw_metadata(const RawImageMetadata& value) {
    if(value.width<=0 || value.height<=0 || !value.channels || value.channels>65536 || !value.z_planes || !value.timepoints ||
        !value.series_count || value.series>=value.series_count || value.channel_names.size()!=value.channels)
        invalid("invalid raw axes");
    sample_bytes(value.sample_type);levels(value.levels,value.width,value.height);text(value.dimension_order);
    for(const auto& name:value.channel_names)text(name);
    for(auto number:{value.pixel_width_um,value.pixel_height_um,value.z_spacing_um,value.time_interval_seconds})calibration(number);
    if(value.payload_bytes()>raw_limit)throw std::length_error("Reader plugin raw metadata exceeds 1 MiB");
}
Json image_json(const ImageMetadata& value) {
    return {{"width",value.width},{"height",value.height},{"levels",levels(value.levels)},
        {"pixel_type","uint8"},{"layout","rgba"},{"vendor",value.vendor},
        {"pixel_width_um",optional(value.pixel_width_um)},{"pixel_height_um",optional(value.pixel_height_um)}};
}
ImageMetadata read_image(const Json& value) {
    ImageMetadata result;result.width=positive_dimension(value,"width");result.height=positive_dimension(value,"height");
    if(string(value,"pixel_type")!="uint8" || string(value,"layout")!="rgba")invalid("unsupported display pixel type/layout");
    result.levels=read_levels(value.at("levels"));result.vendor=string(value,"vendor");
    result.pixel_width_um=optional(value,"pixel_width_um");result.pixel_height_um=optional(value,"pixel_height_um");return result;
}
Json raw_json(const RawImageMetadata& value) {
    return {{"width",value.width},{"height",value.height},{"levels",levels(value.levels)},
        {"channels",value.channels},{"sample_type",sample_name(value.sample_type)},{"z_planes",value.z_planes},
        {"timepoints",value.timepoints},{"series",value.series},{"series_count",value.series_count},
        {"channel_names",value.channel_names},{"dimension_order",value.dimension_order},
        {"pixel_width_um",optional(value.pixel_width_um)},{"pixel_height_um",optional(value.pixel_height_um)},
        {"z_spacing_um",optional(value.z_spacing_um)},{"time_interval_seconds",optional(value.time_interval_seconds)}};
}
SampleType read_sample_type(const std::string& name) {
    const SampleType types[]={SampleType::UInt8,SampleType::UInt16,SampleType::Int8,SampleType::Int16,SampleType::UInt32,
        SampleType::Int32,SampleType::UInt64,SampleType::Int64,SampleType::Float32,SampleType::Float64};
    for(auto type:types)if(sample_name(type)==name)return type;invalid("unknown raw sample type");
}
std::shared_ptr<const RawImageMetadata> read_raw(const Json& value) {
    auto result=std::make_shared<RawImageMetadata>();result->width=positive_dimension(value,"width");result->height=positive_dimension(value,"height");
    result->channels=static_cast<std::uint32_t>(integer(value,"channels",65536));result->sample_type=read_sample_type(string(value,"sample_type"));
    result->z_planes=static_cast<std::uint32_t>(integer(value,"z_planes"));result->timepoints=static_cast<std::uint32_t>(integer(value,"timepoints"));
    result->series=static_cast<std::uint32_t>(integer(value,"series"));result->series_count=static_cast<std::uint32_t>(integer(value,"series_count"));
    const auto& names=value.at("channel_names");const auto& table=value.at("levels");
    if(!names.is_array() || names.size()!=result->channels || !table.is_array() || table.size()>65536 ||
        table.size()*sizeof(ImageLevel)+names.size()*sizeof(std::string)>raw_limit)
        invalid("raw metadata count exceeds bounds");
    result->levels=read_levels(table);result->channel_names.reserve(names.size());
    std::size_t bytes=sizeof(*result)+result->levels.capacity()*sizeof(ImageLevel)+result->channel_names.capacity()*sizeof(std::string);
    if(bytes>raw_limit)throw std::length_error("Reader plugin raw metadata exceeds 1 MiB");
    for(const auto& name:names) {
        if(!name.is_string())invalid("invalid channel name");auto item=name.get<std::string>();text(item);
        if(item.size()>raw_limit-bytes)throw std::length_error("Reader plugin raw metadata exceeds 1 MiB");bytes+=item.size();result->channel_names.push_back(std::move(item));
    }
    result->dimension_order=string(value,"dimension_order");result->pixel_width_um=optional(value,"pixel_width_um");result->pixel_height_um=optional(value,"pixel_height_um");
    result->z_spacing_um=optional(value,"z_spacing_um");result->time_interval_seconds=optional(value,"time_interval_seconds");return result;
}
Json implementation_json(const ReaderPluginImplementation& value) {
    auto dependencies=Json::array();for(const auto& dependency:value.dependencies)dependencies.push_back({{"name",dependency.name},{"sha256",dependency.sha256}});
    return {{"schema","qpx.reader-plugin-implementation/1"},{"provider_id",value.provider_id},{"version",value.version},
        {"contract",value.contract},{"profile",value.profile},{"abi",value.abi},{"capabilities",value.capabilities},
        {"module_sha256",value.module_sha256},{"dependencies",std::move(dependencies)},
        {"options",value.options.empty()?Json::object():parse(value.options,text_limit)}};
}
ReaderPluginImplementation read_implementation(const Json& value) {
    if(string(value,"schema")!="qpx.reader-plugin-implementation/1")invalid("unsupported implementation schema");
    ReaderPluginImplementation result;result.provider_id=string(value,"provider_id",128);result.version=string(value,"version");
    result.contract=string(value,"contract");result.profile=string(value,"profile");result.module_sha256=string(value,"module_sha256",64);
    result.abi=static_cast<std::uint32_t>(integer(value,"abi"));result.capabilities=integer(value,"capabilities",std::numeric_limits<std::uint64_t>::max());
    const auto& dependencies=value.at("dependencies");if(!dependencies.is_array() || dependencies.size()>63)invalid("dependency count exceeds limit");
    result.dependencies.reserve(dependencies.size());for(const auto& item:dependencies)result.dependencies.push_back({string(item,"name"),string(item,"sha256",64)});
    if(!value.at("options").is_object())invalid("options must be a JSON object");result.options=normalize_reader_plugin_options(value.at("options").dump());return result;
}
FileVersion read_file(const Json& value) {
    FileVersion result;result.path=string(value,"path",32768);result.identity_scheme=string(value,"identity_scheme");result.identity=string(value,"identity");
    result.stamp_scheme=string(value,"stamp_scheme");result.modified_token=string(value,"modified_token");result.change_token=string(value,"change_token");
    const auto maximum=std::uint64_t(std::numeric_limits<std::int64_t>::max());result.bytes=integer(value,"bytes",maximum);result.observed_unix_ms=integer(value,"observed_unix_ms",maximum);return result;
}
ReaderSource read_observed(const Json& value) {
    ReaderSource result;result.generation=string(value,"generation",128);result.reader=string(value,"reader",256);
    result.locator=string(value,"locator",32768);result.scope=string(value,"scope",32);
    if(value.at("primary").is_null())invalid("missing primary file observation");result.primary=std::make_shared<const FileVersion>(read_file(value.at("primary")));
    if(value.contains("companions")) {
        const auto& companions=value.at("companions");if(!companions.is_array() || companions.size()>63)invalid("companion count exceeds limit");
        result.companions.reserve(companions.size());for(const auto& file:companions)result.companions.push_back(read_file(file));
    }
    return result;
}
std::string sha(const std::string& value) {Sha256 hash;hash.update(value.data(),value.size());return hash.finish();}
bool same_image(const ImageMetadata& a,const ImageMetadata& b) {
    return a.width==b.width && a.height==b.height && a.pixel_type==b.pixel_type && a.layout==b.layout && a.vendor==b.vendor &&
        a.pixel_width_um==b.pixel_width_um && a.pixel_height_um==b.pixel_height_um && same_levels(a.levels,b.levels);
}
bool same_raw(const RawImageMetadata& a,const RawImageMetadata& b) {
    return a.width==b.width && a.height==b.height && a.channels==b.channels && a.sample_type==b.sample_type &&
        a.z_planes==b.z_planes && a.timepoints==b.timepoints && a.series==b.series && a.series_count==b.series_count &&
        a.channel_names==b.channel_names && a.dimension_order==b.dimension_order && same_levels(a.levels,b.levels) &&
        a.pixel_width_um==b.pixel_width_um && a.pixel_height_um==b.pixel_height_um && a.z_spacing_um==b.z_spacing_um && a.time_interval_seconds==b.time_interval_seconds;
}
}
bool valid_reader_plugin_id(const std::string& value) noexcept {return valid_id(value);}
std::string normalize_reader_plugin_options(const std::string& value) {
    if(value.empty())return {};auto object=parse(value,text_limit);if(!object.is_object())invalid("options must be a JSON object");
    if(object.empty())return {};auto result=object.dump();text(result);return result;
}
std::size_t ReaderPluginImplementation::payload_bytes() const {
    std::size_t bytes=sizeof(*this);for(const auto* value:{&provider_id,&version,&contract,&profile,&module_sha256,&options})bytes+=value->capacity();
    bytes+=dependencies.capacity()*sizeof(ReaderPluginDependency);for(const auto& value:dependencies)bytes+=value.name.capacity()+value.sha256.capacity();return bytes;
}
void ReaderPluginImplementation::validate() const {
    if(!valid_reader_plugin_id(provider_id))invalid("invalid provider ID");for(const auto* value:{&version,&contract,&profile})text(*value,true);
    if(abi!=QPX_READER_ABI_V1 || !(capabilities&QPX_READER_CAP_RGBA) || (capabilities&~UINT64_C(7)) || !digest(module_sha256))
        invalid("invalid ABI, capabilities or module hash");
    text(options);if(normalize_reader_plugin_options(options)!=options)invalid("noncanonical options");
    if(dependencies.size()>63)invalid("dependency count exceeds limit");std::string previous;
    for(const auto& dependency:dependencies) {
        text(dependency.name,true);
        if(dependency.name=="." || dependency.name==".." || dependency.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos ||
            (!previous.empty() && dependency.name<=previous) || !digest(dependency.sha256))invalid("invalid, duplicate or unordered dependency");
        previous=dependency.name;
    }
    if(payload_bytes()>reader_plugin_link_limit)throw std::length_error("Reader plugin implementation exceeds 2 MiB");
}
std::string ReaderPluginImplementation::sha256() const {validate();return sha(implementation_json(*this).dump());}
ReaderPluginImplementation reader_plugin_implementation(const ReaderPluginInfo& info,const std::string& options) {
    ReaderPluginImplementation result;result.provider_id=info.provider_id;result.version=info.version;result.contract=info.contract;
    result.profile=info.profile;result.module_sha256=info.module_sha256;result.capabilities=info.capabilities;result.options=normalize_reader_plugin_options(options);
    result.dependencies=info.dependencies;std::sort(result.dependencies.begin(),result.dependencies.end(),[](const auto& a,const auto& b){return a.name<b.name;});result.validate();return result;
}
std::string reader_plugin_binding(const ReaderPluginImplementation& value) {return value.provider_id+std::string(binding_marker)+value.sha256();}
bool is_reader_plugin_binding(const std::string& value) noexcept {
    const auto marker=value.find(binding_marker);if(marker==std::string::npos)return false;
    const std::string_view view(value);return valid_id(view.substr(0,marker)) && digest(view.substr(marker+binding_marker.size()));
}
void require_same_reader_plugin_binding(const ReaderSource* archived,const ReaderSource* current) {
    const bool old_plugin=archived && is_reader_plugin_binding(archived->reader);
    const bool new_plugin=current && is_reader_plugin_binding(current->reader);
    if((old_plugin || new_plugin) && (!archived || !current || archived->reader!=current->reader))
        throw std::invalid_argument("reader implementation changed; recompute new result");
}
std::size_t ReaderPluginLink::payload_bytes() const {
    return sizeof(*this)+implementation.payload_bytes()-sizeof(implementation)+metadata.vendor.capacity()+metadata.levels.capacity()*sizeof(ImageLevel)+
        (raw?raw->payload_bytes():0)+observed.payload_bytes()-sizeof(observed);
}
void ReaderPluginLink::validate() const {
    implementation.validate();image_metadata(metadata);observed.validate();
    if((observed.scope!="single_file" && observed.scope!="compound_files") || observed.reader!=reader_plugin_binding(implementation) ||
        (observed.scope=="compound_files" && !(implementation.capabilities&UINT64_C(4))))invalid("source observation differs from implementation binding");
    if(raw) {
        raw_metadata(*raw);if(!(implementation.capabilities&QPX_READER_CAP_RAW) || raw->width!=metadata.width || raw->height!=metadata.height ||
            !same_levels(raw->levels,metadata.levels) || raw->pixel_width_um!=metadata.pixel_width_um || raw->pixel_height_um!=metadata.pixel_height_um)
            invalid("raw/display metadata lattice differs");
    }
    if(payload_bytes()>reader_plugin_link_limit)throw std::length_error("Reader plugin link exceeds 2 MiB");
}
std::string encode_reader_plugin_link(const ReaderPluginLink& value) {
    value.validate();Json result={{"schema","qpx.reader-plugin-link/1"},{"implementation",implementation_json(value.implementation)},
        {"metadata",image_json(value.metadata)},{"raw",value.raw?raw_json(*value.raw):Json(nullptr)},
        {"observed",parse(reader_source_json(value.observed),reader_plugin_link_limit)}};
    result["sha256"]=sha(result.dump());auto encoded=result.dump();
    if(encoded.size()>reader_plugin_link_limit)throw std::length_error("Encoded reader plugin link exceeds 2 MiB");return encoded;
}
std::shared_ptr<const ReaderPluginLink> decode_reader_plugin_link(const std::string& encoded) {
    if(encoded.empty() || encoded.size()>reader_plugin_link_limit)invalid("encoded link exceeds bounds");
    try {
        auto document=parse(encoded,reader_plugin_link_limit);
        if(string(document,"schema")!="qpx.reader-plugin-link/1")invalid("unsupported link schema");
        const auto checksum=string(document,"sha256",64);if(!digest(checksum))invalid("invalid checksum");
        document.erase("sha256");if(sha(document.dump())!=checksum)invalid("checksum mismatch");
        auto result=std::make_shared<ReaderPluginLink>();result->implementation=read_implementation(document.at("implementation"));
        result->metadata=read_image(document.at("metadata"));if(!document.at("raw").is_null())result->raw=read_raw(document.at("raw"));
        result->observed=read_observed(document.at("observed"));result->validate();
        if(encode_reader_plugin_link(*result)!=encoded)invalid("noncanonical encoding");return result;
    }catch(const nlohmann::json::exception&){invalid("malformed link fields");}
}
bool same_reader_plugin_metadata(const ReaderPluginLink& a,const ReaderPluginLink& b) {
    return same_image(a.metadata,b.metadata) && bool(a.raw)==bool(b.raw) && (!a.raw || same_raw(*a.raw,*b.raw));
}
void hash_reader_plugin_binding(Sha256& hash,const ImageServer& server) {
    const auto source=server.reader_source();if(!source || !is_reader_plugin_binding(source->reader))return;
    constexpr std::string_view domain="qpx.reader-implementation-binding/1";
    hash.integer(domain.size());hash.update(domain.data(),domain.size());hash.integer(source->reader.size());hash.update(source->reader.data(),source->reader.size());
}
}
