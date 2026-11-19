#include "reader_plugin.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/directory_store.hpp"
#include "utf8_text.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace qpx::detail {
namespace {
constexpr std::size_t text_limit=4096,metadata_limit=1024*1024;
constexpr std::size_t label_description_limit=65536;
constexpr std::uint64_t work_limit=256u*1024*1024;
[[noreturn]] void malformed(const char* reason) {
    throw std::invalid_argument(std::string("Reader plugin returned invalid ")+reason);
}
void charge(std::size_t& total,std::size_t count) {
    if(count>metadata_limit-total)throw std::length_error("Reader plugin metadata exceeds 1 MiB");
    total+=count;
}
struct Text {
    std::array<char,text_limit> bytes{};
    qpx_reader_text_v1 value{bytes.data(),bytes.size(),0};
    std::string copy(const qpx_reader_text_v1& returned) const {
        if(returned.data!=bytes.data() || returned.capacity!=bytes.size() || returned.size>bytes.size())
            malformed("text buffer descriptor");
        std::string result(bytes.data(),static_cast<std::size_t>(returned.size));
        bounded_utf8(result,text_limit);return result;
    }
};
void finish_invoke(const char* operation,const Text& error,qpx_reader_status_v1 status) {
    if(error.value.data!=error.bytes.data() || error.value.capacity!=error.bytes.size())
        malformed("text buffer descriptor");
    if(status==QPX_READER_BUFFER_TOO_SMALL && error.value.size>error.value.capacity)
        throw std::length_error(std::string("Reader plugin ")+operation+": error text exceeds the 4096-byte limit");
    const auto message=error.copy(error.value);
    if(status==QPX_READER_OK)return;
    const auto explanation=std::string("Reader plugin ")+operation+": "+(message.empty()?"operation failed":message);
    switch(status) {
    case QPX_READER_CANCELLED:throw WorkflowCancelled(explanation);
    case QPX_READER_INVALID_REQUEST:throw std::invalid_argument(explanation);
    case QPX_READER_BUFFER_TOO_SMALL:throw std::length_error(explanation);
    case QPX_READER_UNSUPPORTED:case QPX_READER_IO_ERROR:case QPX_READER_INTERNAL_ERROR:
        throw std::runtime_error(explanation);
    default:throw std::runtime_error("Reader plugin returned an unknown status");
    }
}
template<class Call> void invoke(const char* operation,Call&& call) {
    Text error;qpx_reader_status_v1 status;
    try {status=call(&error.value);}
    catch(...) {throw std::runtime_error(std::string("Reader plugin threw across the C ABI during ")+operation);}
    finish_invoke(operation,error,status);
}
int32_t QPX_READER_CALL cancelled_callback(void* context) {
    return context && static_cast<const std::atomic<bool>*>(context)->load(std::memory_order_relaxed)?1:0;
}
qpx_reader_cancel_v1 cancellation(const std::atomic<bool>* cancelled) {
    return {cancelled_callback,const_cast<std::atomic<bool>*>(cancelled)};
}
SampleType sample_type(std::uint32_t type) {
    switch(type) {
    case QPX_READER_UINT8:return SampleType::UInt8;case QPX_READER_UINT16:return SampleType::UInt16;
    case QPX_READER_INT8:return SampleType::Int8;case QPX_READER_INT16:return SampleType::Int16;
    case QPX_READER_UINT32:return SampleType::UInt32;case QPX_READER_INT32:return SampleType::Int32;
    case QPX_READER_UINT64:return SampleType::UInt64;case QPX_READER_INT64:return SampleType::Int64;
    case QPX_READER_FLOAT32:return SampleType::Float32;case QPX_READER_FLOAT64:return SampleType::Float64;
    default:malformed("raw sample type");
    }
}
std::uint32_t abi_sample_type(SampleType type) {
    switch(type) {
    case SampleType::UInt8:return QPX_READER_UINT8;case SampleType::UInt16:return QPX_READER_UINT16;
    case SampleType::Int8:return QPX_READER_INT8;case SampleType::Int16:return QPX_READER_INT16;
    case SampleType::UInt32:return QPX_READER_UINT32;case SampleType::Int32:return QPX_READER_INT32;
    case SampleType::UInt64:return QPX_READER_UINT64;case SampleType::Int64:return QPX_READER_INT64;
    case SampleType::Float32:return QPX_READER_FLOAT32;case SampleType::Float64:return QPX_READER_FLOAT64;
    default:malformed("raw sample type");
    }
}
std::optional<double> calibration(std::uint32_t flags,std::uint32_t flag,double value) {
    if(!(flags&flag))return {};
    if(!std::isfinite(value) || value<=0)malformed("calibration");
    return value;
}
// Divide an int64 coordinate by the exact binary value of a double scale.
// MSVC's long double is double, so floating division alone loses large origins.
std::int64_t lattice_start(std::int64_t coordinate,double scale) {
    int exponent=0;const auto fraction=std::frexp(scale,&exponent);
    const auto mantissa=static_cast<std::uint64_t>(std::ldexp(fraction,53));
    const auto magnitude=coordinate<0?std::uint64_t(-(coordinate+1))+1:std::uint64_t(coordinate);
    std::uint64_t quotient=0,remainder=magnitude;
    const auto shift=exponent-53;
    if(shift>=0) {
        if(shift<64 && mantissa<=(std::numeric_limits<std::uint64_t>::max()>>shift)) {
            const auto divisor=mantissa<<shift;quotient=magnitude/divisor;remainder=magnitude%divisor;
        }
    } else {
        quotient=magnitude/mantissa;remainder=magnitude%mantissa;
        for(int bit=0;bit< -shift;++bit) {
            quotient*=2;remainder*=2;
            if(remainder>=mantissa){++quotient;remainder-=mantissa;}
        }
    }
    if(coordinate>=0)return static_cast<std::int64_t>(quotient);
    quotient+=remainder!=0;
    if(quotient==std::uint64_t(std::numeric_limits<std::int64_t>::max())+1)
        return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(quotient);
}
bool outside(std::int64_t first,std::int64_t offset,std::int64_t extent) {
    if(first>=0)return first>=extent || offset>=extent-first;
    const auto distance=std::uint64_t(-(first+1))+1;
    return std::uint64_t(offset)<distance || std::uint64_t(offset)-distance>=std::uint64_t(extent);
}
qpx_reader_region_v1 region(const RegionRequest& request) {
    qpx_reader_region_v1 value{};value.struct_size=sizeof(value);value.level=request.level;
    value.x=request.x;value.y=request.y;value.width=request.width;value.height=request.height;return value;
}
void check_pixels(const qpx_reader_pixels_v1& actual,const qpx_reader_pixels_v1& expected) {
    if(actual.struct_size!=expected.struct_size || actual.sample_type!=expected.sample_type ||
        actual.width!=expected.width || actual.height!=expected.height || actual.channels!=expected.channels ||
        actual.reserved!=expected.reserved || actual.row_stride!=expected.row_stride ||
        actual.pixel_stride!=expected.pixel_stride || actual.channel_stride!=expected.channel_stride ||
        actual.data!=expected.data || actual.capacity!=expected.capacity || actual.validity!=expected.validity ||
        actual.validity_capacity!=expected.validity_capacity || actual.written!=expected.capacity ||
        actual.validity_written!=expected.validity_capacity)malformed("pixel shape, stride or fill length");
}
qpx_reader_pixels_v1 pixels(const RegionRequest& r,std::uint32_t type,std::uint32_t channels,
    std::size_t bytes_per_sample,std::uint8_t* data,std::size_t bytes,std::uint8_t* valid=nullptr,std::size_t validity=0) {
    qpx_reader_pixels_v1 value{};value.struct_size=sizeof(value);value.sample_type=type;
    value.width=r.width;value.height=r.height;value.channels=channels;value.channel_stride=bytes_per_sample;
    value.pixel_stride=bytes_per_sample*channels;value.row_stride=value.pixel_stride*std::uint64_t(r.width);
    value.data=data;value.capacity=bytes;value.validity=valid;value.validity_capacity=validity;return value;
}
void padding(const RegionRequest& r,const ImageMetadata& image,const qpx_reader_pixels_v1& output,bool raw) {
    const auto& level=image.levels[std::size_t(r.level)];
    const auto first_x=lattice_start(r.x,level.downsample),first_y=lattice_start(r.y,level.downsample);
    for(std::int64_t row=0;row<r.height;++row)for(std::int64_t col=0;col<r.width;++col) {
        const auto index=std::size_t(row)*std::size_t(r.width)+std::size_t(col);
        const auto oob=outside(first_x,col,level.width) || outside(first_y,row,level.height);
        bool zero=oob;
        if(raw) {
            if(output.validity[index]>1 || (oob && output.validity[index]))malformed("raw validity");
            zero=zero || !output.validity[index];
        }
        if(zero)for(std::uint64_t byte=0;byte<output.pixel_stride;++byte)
            if(output.data[index*std::size_t(output.pixel_stride)+std::size_t(byte)])malformed("out-of-bounds or invalid sample padding");
    }
}
class PluginImageServer final:public ImageServer {
    std::shared_ptr<const AdmittedReaderPlugin> plugin_;
    std::size_t work_bytes_=work_limit;
    std::string path_;
    std::shared_ptr<const DirectoryStoreSnapshot> directory_;
    DirectoryStoreLimits directory_limits_;
    ReaderPluginImplementation implementation_;
    std::string binding_;
    std::shared_ptr<const ReaderPluginLink> link_;
    ImageMetadata image_;
    std::shared_ptr<const RawImageMetadata> raw_;
    std::shared_ptr<const ReaderSource> source_;
    std::shared_ptr<const std::string> label_description_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_{false};
    void* handle_=nullptr;
    void available(const std::atomic<bool>* cancelled=nullptr) const {
        workflow_checkpoint(cancelled);if(closed_)throw std::runtime_error("reader plugin image server is closed");
    }
    void destroy_locked() noexcept {
        closed_=true;auto* old=handle_;handle_=nullptr;
        if(old)try {plugin_->api.destroy(old);}catch(...) {/* ABI violation; never retry destruction. */}
        directory_.reset();
    }
    struct Guards {
        std::vector<std::unique_ptr<ReaderFileGuard>> files;
        std::shared_ptr<const DirectoryStoreSnapshot> store;
        std::shared_ptr<const ReaderSource> source;
        std::string primary;
    };
    qpx_reader_open_v1 open_request(const std::atomic<bool>* cancelled) const {
        qpx_reader_open_v1 request{};request.struct_size=sizeof(request);request.path={path_.data(),path_.size()};
        request.options={implementation_.options.data(),implementation_.options.size()};request.work_bytes=work_bytes_;request.cancel=cancellation(cancelled);return request;
    }
    std::vector<std::string> companion_paths(const std::atomic<bool>* cancelled) const {
        std::vector<std::string> result;
        if(!(plugin_->info.capabilities&QPX_READER_CAP_SOURCE_FILES))return result;
        auto request=open_request(cancelled);std::uint32_t count=0;
        invoke("source file count",[&](auto* error){return plugin_->files.count(&request,&count,error);});
        if(count>63)throw std::length_error("Reader plugin source exceeds 63 companions");
        std::size_t bytes=0;
        for(std::uint32_t i=0;i<count;++i) {
            workflow_checkpoint(cancelled);std::array<char,32768> buffer{};qpx_reader_text_v1 output{buffer.data(),buffer.size(),0};
            invoke("source file path",[&](auto* error){return plugin_->files.path(&request,i,&output,error);});
            if(output.data!=buffer.data() || output.capacity!=buffer.size() || output.size>buffer.size())malformed("source path buffer");
            std::string path(buffer.data(),std::size_t(output.size));bounded_utf8(path,32768);
            if(path.empty() || !std::filesystem::u8path(path).is_absolute())malformed("absolute companion path");
            path=std::filesystem::u8path(path).lexically_normal().u8string();bytes+=path.size()+sizeof(FileVersion);
            if(bytes>128u*1024)throw std::length_error("Reader plugin source paths exceed 128 KiB");
            result.push_back(std::move(path));
        }
        std::sort(result.begin(),result.end());
        if(std::adjacent_find(result.begin(),result.end())!=result.end())malformed("duplicate companion paths");
        return result;
    }
    std::string primary_metadata(const std::atomic<bool>* cancelled) const {
        auto request=open_request(cancelled);const auto expected=request;
        Text output;
        invoke("store primary metadata",[&](auto* error){return plugin_->store.primary_metadata(&request,&output.value,error);});
        if(request.struct_size!=expected.struct_size || request.reserved!=expected.reserved ||
            request.path.data!=expected.path.data || request.path.size!=expected.path.size ||
            request.options.data!=expected.options.data || request.options.size!=expected.options.size ||
            request.work_bytes!=expected.work_bytes || request.cancel.is_cancelled!=expected.cancel.is_cancelled || request.cancel.context!=expected.cancel.context)
            malformed("store input descriptor mutation");
        auto key=output.copy(output.value);if(key.empty())malformed("empty store primary metadata key");
        return key;
    }
    Guards file_guards(const std::atomic<bool>* cancelled) const {
        workflow_checkpoint(cancelled);Guards guards;
        if(directory_) {
            guards.store=directory_;guards.primary=primary_metadata(cancelled);
            guards.source=guards.store->reader_source(binding_,guards.primary,cancelled);
            return guards;
        }
        guards.files.push_back(std::make_unique<ReaderFileGuard>(path_));
        for(const auto& path:companion_paths(cancelled))guards.files.push_back(std::make_unique<ReaderFileGuard>(path));
        (void)observed(guards); // Validate identities/quotas before decoder open.
        return guards;
    }
    std::shared_ptr<const ReaderSource> observed(const Guards& guards) const {
        if(guards.store)return guards.source;
        std::vector<FileVersion> companions;for(std::size_t i=1;i<guards.files.size();++i)companions.push_back(*guards.files[i]->version());
        if(companions.empty())return make_reader_source(binding_,path_,"single_file",guards.files.front()->version());
        return make_compound_reader_source(binding_,path_,guards.files.front()->version(),std::move(companions));
    }
    Guards current_guards(const std::atomic<bool>* cancelled=nullptr) const {
        available(cancelled);
        try {
            auto guards=file_guards(cancelled);auto current=observed(guards);
            if(!same_reader_files(*source_,*current))throw WorkflowInputChanged("Reader plugin source files changed");
            return guards;
        }catch(const WorkflowCancelled&){throw;}
        catch(const WorkflowInputChanged&){throw;}
        catch(const std::exception& e){throw WorkflowInputChanged(std::string("Reader plugin source changed or unavailable: ")+e.what());}
    }
    static void verify_guard(const ReaderFileGuard& guard) {
        try {guard.verify();}catch(const std::exception& e){throw WorkflowInputChanged(std::string("Reader plugin source changed during access: ")+e.what());}
    }
    void verify_guards(const Guards& guards,const std::atomic<bool>* cancelled) const {
        if(guards.store) {
            if(path_!=guards.store->directory())malformed("store locator mutation");
            if(primary_metadata(cancelled)!=guards.primary)throw WorkflowInputChanged("Reader plugin store primary selection changed during access");
            guards.store->require_current(cancelled);return;
        }
        for(const auto& guard:guards.files)verify_guard(*guard);
        const auto paths=companion_paths(cancelled);std::vector<std::string> expected;
        for(std::size_t i=1;i<guards.files.size();++i)expected.push_back(guards.files[i]->version()->path);
        std::sort(expected.begin(),expected.end());
        if(paths!=expected)throw WorkflowInputChanged("Reader plugin source file set changed during access");
        for(const auto& guard:guards.files)verify_guard(*guard);
    }
    template<class Call> void observed_call(const std::atomic<bool>* cancelled,Call&& call) const {
        auto guards=current_guards(cancelled);
        try {call();}catch(...){verify_guards(guards,nullptr);throw;}
        verify_guards(guards,cancelled);workflow_checkpoint(cancelled);
    }
    void copy_metadata(const std::atomic<bool>* cancelled) {
        Text vendor,order;qpx_reader_metadata_v1 metadata{};metadata.struct_size=sizeof(metadata);
        metadata.vendor=vendor.value;metadata.dimension_order=order.value;
        invoke("metadata",[&](auto* error){return plugin_->api.metadata(handle_,&metadata,cancellation(cancelled),error);});
        if(metadata.struct_size!=sizeof(metadata) || !metadata.level_count || metadata.level_count>65536 ||
            metadata.width<=0 || metadata.height<=0 || metadata.has_raw>1 || (metadata.calibration_flags&~15u))
            malformed("image metadata");
        if(metadata.has_raw && (!(plugin_->info.capabilities&QPX_READER_CAP_RAW) || !metadata.channels ||
            metadata.channels>65536 || !metadata.z_planes || !metadata.timepoints || !metadata.series_count ||
            metadata.series>=metadata.series_count))malformed("raw axes");
        auto raw=metadata.has_raw?std::make_shared<RawImageMetadata>():nullptr;
        if(raw)raw->sample_type=sample_type(metadata.sample_type);
        std::size_t charged=sizeof(image_)+(raw?sizeof(*raw):0);
        charge(charged,std::size_t(metadata.level_count)*sizeof(ImageLevel)*(raw?2:1));
        if(raw)charge(charged,std::size_t(metadata.channels)*sizeof(std::string));
        image_.width=metadata.width;image_.height=metadata.height;image_.vendor=vendor.copy(metadata.vendor);
        const auto dimension_order=order.copy(metadata.dimension_order);charge(charged,image_.vendor.size());charge(charged,dimension_order.size());
        image_.pixel_width_um=calibration(metadata.calibration_flags,QPX_READER_CALIBRATION_X,metadata.pixel_width_um);
        image_.pixel_height_um=calibration(metadata.calibration_flags,QPX_READER_CALIBRATION_Y,metadata.pixel_height_um);
        const auto z_spacing=calibration(metadata.calibration_flags,QPX_READER_CALIBRATION_Z,metadata.z_spacing_um);
        const auto time_spacing=calibration(metadata.calibration_flags,QPX_READER_CALIBRATION_TIME,metadata.time_interval_seconds);
        image_.levels.reserve(metadata.level_count);
        for(std::uint32_t i=0;i<metadata.level_count;++i) {
            workflow_checkpoint(cancelled);qpx_reader_level_v1 level{};level.struct_size=sizeof(level);
            invoke("level",[&](auto* error){return plugin_->api.level(handle_,i,&level,error);});
            if(level.struct_size!=sizeof(level) || level.reserved || level.width<=0 || level.height<=0 ||
                !std::isfinite(level.downsample) || level.downsample<1)malformed("pyramid level");
            if(!i && (level.width!=image_.width || level.height!=image_.height || level.downsample!=1))
                malformed("level-zero lattice");
            image_.levels.push_back({level.width,level.height,level.downsample});
        }
        if(raw) {
            raw->width=image_.width;raw->height=image_.height;raw->levels=image_.levels;raw->channels=metadata.channels;
            raw->z_planes=metadata.z_planes;raw->timepoints=metadata.timepoints;raw->series=metadata.series;raw->series_count=metadata.series_count;
            raw->pixel_width_um=image_.pixel_width_um;raw->pixel_height_um=image_.pixel_height_um;
            raw->z_spacing_um=z_spacing;raw->time_interval_seconds=time_spacing;raw->dimension_order=dimension_order;
            raw->channel_names.reserve(raw->channels);
            for(std::uint32_t i=0;i<raw->channels;++i) {
                workflow_checkpoint(cancelled);Text name;
                invoke("channel name",[&](auto* error){return plugin_->api.channel_name(handle_,i,&name.value,error);});
                auto text=name.copy(name.value);charge(charged,text.size());raw->channel_names.push_back(std::move(text));
            }
            if(raw->payload_bytes()>metadata_limit)throw std::length_error("Reader plugin metadata exceeds 1 MiB");
            raw_=std::move(raw);
        }
    }
    void copy_label_description(const std::atomic<bool>* cancelled) {
        if(!(plugin_->info.capabilities&QPX_READER_CAP_LABEL_DESCRIPTION))return;
        if(!raw_)malformed("label description without raw metadata");
        std::array<char,label_description_limit> bytes{};
        qpx_reader_text_v1 output{bytes.data(),bytes.size(),0};
        observed_call(cancelled,[&] {
            Text error;qpx_reader_status_v1 status;
            try {status=plugin_->label.describe(handle_,&output,cancellation(cancelled),&error.value);}
            catch(...) {throw std::runtime_error("Reader plugin threw across the C ABI during label description");}
            if(output.data!=bytes.data() || output.capacity!=bytes.size())malformed("label description buffer descriptor");
            if(status==QPX_READER_OK) {
                if(!output.size || output.size>bytes.size())malformed("label description size");
                if(error.value.size)malformed("label description success error text");
            } else if(status==QPX_READER_BUFFER_TOO_SMALL) {
                if(output.size<=output.capacity)malformed("label description required size");
            } else if(output.size)malformed("failed label description output size");
            finish_invoke("label description",error,status);
        });
        std::string description(bytes.data(),std::size_t(output.size));
        bounded_utf8(description,label_description_limit);
        label_description_=std::make_shared<const std::string>(std::move(description));
        workflow_checkpoint(cancelled);
    }
public:
    PluginImageServer(const std::string& path,std::shared_ptr<const AdmittedReaderPlugin> plugin,const std::atomic<bool>* cancelled,const std::string& options,std::size_t work_bytes):plugin_(std::move(plugin)),work_bytes_(work_bytes) {
        if(!work_bytes_ || work_bytes_>work_limit)throw std::invalid_argument("Reader plugin work allowance must be1 byte..256MiB");
        workflow_checkpoint(cancelled);bounded_utf8(path,32768);if(path.empty())throw std::invalid_argument("Source path must not be empty");
        if(!plugin_ || plugin_->api.struct_size<sizeof(qpx_reader_api_v1) || plugin_->api.abi_version!=QPX_READER_ABI_V1 ||
            !plugin_->api.open || !plugin_->api.destroy || !plugin_->api.metadata || !plugin_->api.level ||
            !plugin_->api.read_rgba || !(plugin_->info.capabilities&QPX_READER_CAP_RGBA) ||
            (plugin_->info.capabilities&~(QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_FILES|QPX_READER_CAP_SOURCE_STORE|QPX_READER_CAP_LABEL_DESCRIPTION)) ||
            ((plugin_->info.capabilities&QPX_READER_CAP_RAW) && (!plugin_->api.read_raw || !plugin_->api.channel_name)) ||
            ((plugin_->info.capabilities&QPX_READER_CAP_SOURCE_FILES) && (plugin_->files.struct_size!=sizeof(plugin_->files) || plugin_->files.abi_version!=1 || !plugin_->files.count || !plugin_->files.path)) ||
            ((plugin_->info.capabilities&QPX_READER_CAP_SOURCE_STORE) && ((plugin_->info.capabilities&QPX_READER_CAP_SOURCE_FILES) || plugin_->store.struct_size!=sizeof(plugin_->store) || plugin_->store.abi_version!=1 || !plugin_->store.primary_metadata)) ||
            ((plugin_->info.capabilities&QPX_READER_CAP_LABEL_DESCRIPTION) &&
                ((plugin_->info.capabilities&(QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_STORE))!=(QPX_READER_CAP_RAW|QPX_READER_CAP_SOURCE_STORE) ||
                    plugin_->label.struct_size!=sizeof(plugin_->label) || plugin_->label.abi_version!=1 || !plugin_->label.describe)))
            throw std::invalid_argument("Invalid admitted reader plugin API");
        bounded_utf8(plugin_->info.provider_id,256);if(plugin_->info.provider_id.empty())throw std::invalid_argument("Reader plugin provider ID must not be empty");
        implementation_=reader_plugin_implementation(plugin_->info,options);
        binding_=reader_plugin_binding(implementation_);
        std::lock_guard<std::mutex> lock(mutex_);
        const auto absolute=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();
        std::unique_ptr<ReaderFileGuard> guard;
        if(plugin_->info.capabilities&QPX_READER_CAP_SOURCE_STORE) {
            const bool labels=(plugin_->info.capabilities&QPX_READER_CAP_LABEL_DESCRIPTION)!=0;
            if(work_bytes_<(labels?8u:4u)*1024*1024)
                throw std::invalid_argument(labels?"Label reader requires at least8MiB total store/description/decoder work":"Directory reader requires at least4MiB total store/decoder work");
            const auto host_overhead=(labels?2u:1u)*1024*1024;
            directory_limits_.inventory_bytes=std::min<std::size_t>(64u*1024*1024,(work_bytes_-host_overhead)/4);
            // Reserve a retained and a simultaneous re-observation inventory,
            // plus wrapper/primary/guard overhead, before private decoder open.
            work_bytes_-=2*directory_limits_.inventory_bytes+host_overhead;
            directory_=std::make_shared<DirectoryStoreSnapshot>(absolute,directory_limits_,cancelled);
            path_=directory_->directory();
        } else {
            guard=std::make_unique<ReaderFileGuard>(absolute);path_=guard->version()->path;
        }
        auto guards=file_guards(cancelled);auto request=open_request(cancelled);
        const auto expected_request=request;const auto expected_options=directory_?implementation_.options:std::string{};
        try {
            invoke("open",[&](auto* error){return plugin_->api.open(&request,&handle_,error);});
            if(directory_ && (request.struct_size!=expected_request.struct_size || request.reserved!=expected_request.reserved ||
                request.path.data!=expected_request.path.data || request.path.size!=expected_request.path.size ||
                request.options.data!=expected_request.options.data || request.options.size!=expected_request.options.size ||
                request.work_bytes!=expected_request.work_bytes || request.cancel.is_cancelled!=expected_request.cancel.is_cancelled ||
                request.cancel.context!=expected_request.cancel.context || implementation_.options!=expected_options || path_!=directory_->directory()))
                malformed("store open input mutation");
            if(!handle_)malformed("open handle");if(guard)verify_guard(*guard);
            source_=observed(guards);
            copy_metadata(cancelled);verify_guards(guards,cancelled);if(guard)verify_guard(*guard);workflow_checkpoint(cancelled);
            copy_label_description(cancelled);
            auto link=std::make_shared<ReaderPluginLink>();link->implementation=implementation_;link->metadata=image_;link->raw=raw_;link->observed=*source_;link->validate();link_=std::move(link);
        }catch(...){destroy_locked();throw;}
    }
    ~PluginImageServer() override {std::lock_guard<std::mutex> lock(mutex_);destroy_locked();}
    const ImageMetadata& metadata() const override {return image_;}
    std::string reader_id() const override {return plugin_->info.provider_id;}
    std::string source_locator() const override {return path_;}
    std::shared_ptr<const ReaderSource> reader_source() const override {return source_;}
    std::shared_ptr<const ReaderPluginLink> link() const {return link_;}
    std::shared_ptr<const DirectoryStoreSnapshot> store() const {std::lock_guard<std::mutex> lock(mutex_);return directory_;}
    std::shared_ptr<const std::string> label_description() const {std::lock_guard<std::mutex> lock(mutex_);return label_description_;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled=nullptr) const override {
        std::lock_guard<std::mutex> lock(mutex_);available(cancelled);return raw_;
    }
    void require_raw_current(const std::atomic<bool>* cancelled=nullptr) const override {
        std::lock_guard<std::mutex> lock(mutex_);auto guards=current_guards(cancelled);verify_guards(guards,cancelled);workflow_checkpoint(cancelled);
    }
    PixelBuffer read_region(const RegionRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);available();validate_region(image_,request);PixelBuffer output(request.width,request.height);
        auto descriptor=pixels(request,QPX_READER_UINT8,4,1,output.writable_bytes().data(),output.bytes().size());const auto expected=descriptor;
        auto input=region(request);
        observed_call(nullptr,[&]{invoke("display read",[&](auto* error){return plugin_->api.read_rgba(handle_,&input,&descriptor,error);});});
        check_pixels(descriptor,expected);padding(request,image_,descriptor,false);return output;
    }
    RawPixelBuffer read_raw(const RawRegionRequest& request,const std::atomic<bool>* cancelled=nullptr) const override {
        std::lock_guard<std::mutex> lock(mutex_);available(cancelled);
        if(!raw_)throw std::runtime_error("image reader does not expose raw quantitative samples");
        request.validate(*raw_);RawPixelBuffer output(request.region.width,request.region.height,request.channels,raw_->sample_type);
        auto* data=std::visit([](auto& values){return reinterpret_cast<std::uint8_t*>(values.data());},output.writable_storage());
        const auto bytes=std::visit([](const auto& values){return values.size()*sizeof(typename std::decay_t<decltype(values)>::value_type);},output.storage());
        auto descriptor=pixels(request.region,abi_sample_type(raw_->sample_type),request.channels,sample_bytes(raw_->sample_type),data,bytes,
            output.writable_valid().data(),output.valid().size());const auto expected=descriptor;
        auto input=region(request.region);input.first_channel=request.first_channel;input.channels=request.channels;input.z=request.z;input.t=request.t;
        observed_call(cancelled,[&]{invoke("raw read",[&](auto* error){return plugin_->api.read_raw(handle_,&input,&descriptor,cancellation(cancelled),error);});});
        check_pixels(descriptor,expected);padding(request.region,image_,descriptor,true);workflow_checkpoint(cancelled);return output;
    }
    void close() override {std::lock_guard<std::mutex> lock(mutex_);destroy_locked();}
    bool closed() const override {return closed_.load();}
};
}
std::shared_ptr<ImageServer> open_plugin_image_server(const std::string& path,std::shared_ptr<const AdmittedReaderPlugin> plugin,const std::atomic<bool>* cancelled,const std::string& options) {
    return open_bounded_plugin_image_server(path,std::move(plugin),work_limit,cancelled,options);
}
std::shared_ptr<ImageServer> open_bounded_plugin_image_server(const std::string& path,std::shared_ptr<const AdmittedReaderPlugin> plugin,std::size_t work_bytes,const std::atomic<bool>* cancelled,const std::string& options) {
    return std::make_shared<PluginImageServer>(path,std::move(plugin),cancelled,options,work_bytes);
}
bool is_reader_plugin_server(const ImageServer& server) noexcept {return dynamic_cast<const PluginImageServer*>(&server)!=nullptr;}
std::shared_ptr<const ReaderPluginLink> capture_native_reader_plugin_link(const ImageServer& server) {
    if(const auto* native=dynamic_cast<const PluginImageServer*>(&server))return native->link();return {};
}
}

namespace qpx::detail {
std::shared_ptr<const DirectoryStoreSnapshot> capture_native_reader_plugin_store(const ImageServer& server) {
    const auto* plugin=dynamic_cast<const PluginImageServer*>(&server);
    return plugin?plugin->store():nullptr;
}
std::shared_ptr<const std::string> capture_native_reader_plugin_label_description(const ImageServer& server) {
    const auto* plugin=dynamic_cast<const PluginImageServer*>(&server);
    return plugin?plugin->label_description():nullptr;
}
}
