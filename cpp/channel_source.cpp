#include "qupathxx/channel_source.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cstring>
#include <type_traits>
namespace qpx {
namespace {
constexpr std::size_t maximum_work=1024ull*1024*1024;
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v){std::uint64_t bits;std::memcpy(&bits,&v,8);h.integer(bits);}
void optional(Sha256& h,std::optional<double> v){h.integer(bool(v));if(v)real(h,*v);}
std::size_t sum(std::initializer_list<std::size_t> values){std::size_t total=0;for(auto v:values)workflow_capture_charge(total,v,maximum_work);return total;}
void budget(std::size_t required,std::size_t available){if(!available || available>maximum_work)throw std::invalid_argument("Channel work budget must be1..1024MiB");if(required>available)throw WorkflowCaptureLimit("Channel read exceeds accounted work budget");}
std::size_t bytes(const RawRegionRequest& r,SampleType type){
    const auto& p=r.region;const auto per=std::uint64_t(r.channels)*sample_bytes(type)+1;
    if(p.width<=0 || p.height<=0 || !r.channels || r.channels>65536 || std::uint64_t(p.width)>maximum_work/per || std::uint64_t(p.height)>maximum_work/(std::uint64_t(p.width)*per))throw WorkflowCaptureLimit("Channel block exceeds work range");
    return std::size_t(p.width)*std::size_t(p.height)*std::size_t(per);
}
std::string metadata_sha(const RawImageMetadata& m){
    if(m.width<=0 || m.height<=0 || !m.channels || m.channels>65536 || !m.z_planes || !m.timepoints || !m.series_count || m.series>=m.series_count || m.levels.empty() || m.levels.size()>65536 || m.channel_names.size()>m.channels || m.payload_bytes()>1024*1024)throw std::invalid_argument("Invalid channel source axes/metadata bounds");
    sample_bytes(m.sample_type);Sha256 h;text(h,"qpx.channel-source-metadata/1");
    for(auto v:{std::uint64_t(m.width),std::uint64_t(m.height),std::uint64_t(m.channels),std::uint64_t(m.z_planes),std::uint64_t(m.timepoints),std::uint64_t(m.series),std::uint64_t(m.series_count),std::uint64_t(m.sample_type)})h.integer(v);
    h.integer(m.levels.size());for(const auto& l:m.levels){if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1)throw std::invalid_argument("Invalid channel source level");h.integer(l.width);h.integer(l.height);real(h,l.downsample);}
    if(m.levels[0].width!=m.width || m.levels[0].height!=m.height || m.levels[0].downsample!=1)throw std::invalid_argument("Channel source needs a level-zero lattice");
    for(auto v:{m.pixel_width_um,m.pixel_height_um,m.z_spacing_um,m.time_interval_seconds}){if(v && (!std::isfinite(*v) || *v<=0))throw std::invalid_argument("Invalid channel source calibration");optional(h,v);}
    detail::bounded_utf8(m.dimension_order,4096);text(h,m.dimension_order);h.integer(m.channel_names.size());for(const auto& name:m.channel_names){detail::bounded_utf8(name,4096);text(h,name);}return h.finish();
}
void version(Sha256& h,const FileVersion& f){for(const auto* s:{&f.path,&f.identity_scheme,&f.identity,&f.stamp_scheme,&f.modified_token,&f.change_token})text(h,*s);h.integer(f.bytes);}
std::string observed_sha(const ReaderSource& s){Sha256 h;text(h,"qpx.channel-source-observation/1");text(h,s.reader);text(h,s.locator);text(h,s.scope);h.integer(bool(s.primary));if(s.primary){version(h,*s.primary);h.integer(s.companions.size());for(const auto& f:s.companions)version(h,f);}else text(h,s.generation);return h.finish();}
bool same_observation(const ReaderSource& a,const ReaderSource& b){return a.primary || b.primary?detail::same_reader_files(a,b):a.generation==b.generation && a.reader==b.reader && a.locator==b.locator && a.scope==b.scope;}
void shape(const RawPixelBuffer& p,const RawRegionRequest& r,SampleType type){if(p.width()!=r.region.width || p.height()!=r.region.height || p.channels()!=r.channels || p.sample_type()!=type || p.storage().index()!=std::size_t(type) || p.valid().size()!=std::size_t(p.width())*std::size_t(p.height()) || std::visit([](const auto& v){return v.size();},p.storage())!=p.valid().size()*p.channels())throw WorkflowInputChanged("Channel decoder returned a different block lattice");}
class Original final:public ChannelSource {
    std::shared_ptr<ImageServer> server_;RawImageMetadata metadata_;ReaderSource observed_;std::string metadata_id_,observation_id_,id_,reader_,locator_;
public:
    Original(std::shared_ptr<ImageServer> server,const std::atomic<bool>* cancelled):server_(std::move(server)){
        workflow_checkpoint(cancelled);if(!server_ || server_->closed())throw std::invalid_argument("Channel source requires an open native reader");
        auto m=server_->raw_metadata(cancelled);auto s=server_->reader_source();if(!m || !s)throw std::invalid_argument("Channel source requires quantitative metadata and source identity");
        metadata_id_=metadata_sha(*m);s->validate();metadata_=*m;observed_=*s;if(s->primary)observed_.primary=std::make_shared<const FileVersion>(*s->primary);
        observation_id_=observed_sha(observed_);reader_=server_->reader_id();locator_=server_->source_locator();detail::bounded_utf8(reader_,65536);detail::bounded_utf8(locator_,65536);
        Sha256 h;text(h,"qpx.original-channel-source/1");text(h,reader_);text(h,locator_);text(h,metadata_id_);text(h,observation_id_);id_=h.finish();require_current(cancelled);
    }
    const RawImageMetadata& metadata()const override{return metadata_;}
    const std::string& domain()const override{static const std::string value="original-channel-samples";return value;}
    const std::string& sha256()const override{return id_;}
    std::size_t payload_bytes()const override{return sizeof(*this)+metadata_.payload_bytes()+observed_.payload_bytes()+reader_.capacity()+locator_.capacity()+512;}
    std::size_t read_work_bytes(const RawRegionRequest& r)const override{r.validate(metadata_);return sum({payload_bytes(),bytes(r,metadata_.sample_type)});}
    void require_current(const std::atomic<bool>* cancelled)const override{
        workflow_checkpoint(cancelled);if(server_->closed() || server_->reader_id()!=reader_ || server_->source_locator()!=locator_)throw WorkflowInputChanged("Channel reader changed/closed");
        auto now=server_->reader_source();auto m=server_->raw_metadata(cancelled);if(!now || !m || !same_observation(observed_,*now) || metadata_sha(*m)!=metadata_id_)throw WorkflowInputChanged("Channel source identity/metadata changed");
        if(observed_.primary){auto current=observed_;std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
            guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(observed_.primary->path));current.primary=guards.back()->version();
            for(auto& file:current.companions){workflow_checkpoint(cancelled);guards.emplace_back(std::make_unique<detail::ReaderFileGuard>(file.path));file=*guards.back()->version();}
            for(const auto& guard:guards)guard->verify();if(!detail::same_reader_files(observed_,current))throw WorkflowInputChanged("Channel source files changed");}
        workflow_checkpoint(cancelled);
    }
    RawPixelBuffer read(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const override{
        budget(read_work_bytes(r),work);require_current(cancelled);auto block=server_->read_raw(r,cancelled);shape(block,r,metadata_.sample_type);budget(sum({payload_bytes(),block.payload_bytes()}),work);require_current(cancelled);return block;
    }
};
struct Samples {
    const void* data;double (*at)(const void*,std::size_t);
    explicit Samples(const RawPixelBuffer& p){std::visit([&](const auto& v){using T=typename std::decay_t<decltype(v)>::value_type;data=v.data();at=[](const void* ptr,std::size_t i){const auto value=static_cast<const T*>(ptr)[i];
        if constexpr(std::is_integral_v<T> && sizeof(T)==8){constexpr std::uint64_t limit=std::uint64_t(1)<<53;bool loss=false;if constexpr(std::is_signed_v<T>)loss=value>std::int64_t(limit) || value< -std::int64_t(limit);else loss=value>limit;if(loss)throw std::overflow_error("Channel correction refuses integer precision loss beyond2^53");}
        return double(value);};},p.storage());}
};
void same_axes(const RawImageMetadata& a,const RawImageMetadata& b){if(a.width!=b.width || a.height!=b.height || a.channels!=b.channels || a.channel_names!=b.channel_names || a.z_planes!=b.z_planes || a.timepoints!=b.timepoints || a.pixel_width_um!=b.pixel_width_um || a.pixel_height_um!=b.pixel_height_um || a.z_spacing_um!=b.z_spacing_um || a.time_interval_seconds!=b.time_interval_seconds)throw std::invalid_argument("Dark/flat fields require identical spatial/channel/Z/time axes and calibration");}
class Corrected final:public ChannelSource {
    std::shared_ptr<const ChannelSource> signal_,dark_,flat_;DarkFlatOptions options_;RawImageMetadata metadata_;std::string id_;
public:
    Corrected(std::shared_ptr<const ChannelSource> s,std::shared_ptr<const ChannelSource> d,std::shared_ptr<const ChannelSource> f,DarkFlatOptions options):signal_(std::move(s)),dark_(std::move(d)),flat_(std::move(f)),options_(std::move(options)){
        if(!signal_ || !dark_ || !flat_)throw std::invalid_argument("Dark/flat requires three explicit sources");options_.validate();
        for(const auto& source:{signal_,dark_,flat_}){if(source->domain()!="original-channel-samples")throw std::invalid_argument("Dark/flat requires original sample sources");metadata_sha(source->metadata());}
        same_axes(signal_->metadata(),dark_->metadata());same_axes(signal_->metadata(),flat_->metadata());metadata_=signal_->metadata();if(options_.scales.size()!=metadata_.channels)throw std::invalid_argument("Dark/flat needs one explicit scale per source channel");
        metadata_.sample_type=SampleType::Float64;metadata_.levels.resize(1);
        Sha256 h;text(h,"qpx.dark-flat-source/1");text(h,signal_->sha256());text(h,dark_->sha256());text(h,flat_->sha256());text(h,options_.sha256());id_=h.finish();require_current(nullptr);
    }
    const RawImageMetadata& metadata()const override{return metadata_;}
    const std::string& domain()const override{static const std::string value="qpx.dark-flat-corrected-samples/1";return value;}
    const std::string& sha256()const override{return id_;}
    std::size_t payload_bytes()const override{return sizeof(*this)+metadata_.payload_bytes()+options_.scales.capacity()*sizeof(double)+signal_->payload_bytes()+dark_->payload_bytes()+flat_->payload_bytes()+128;}
    std::size_t read_work_bytes(const RawRegionRequest& r)const override{
        if(r.region.level!=0)throw std::invalid_argument("Dark/flat currently requires level-zero blocks");r.validate(metadata_);
        const auto s=bytes(r,signal_->metadata().sample_type),d=bytes(r,dark_->metadata().sample_type),f=bytes(r,flat_->metadata().sample_type),out=bytes(r,SampleType::Float64);
        return sum({payload_bytes(),std::max({signal_->read_work_bytes(r),sum({s,dark_->read_work_bytes(r)}),sum({s,d,flat_->read_work_bytes(r)}),sum({s,d,f,out})})});
    }
    void require_current(const std::atomic<bool>* cancelled)const override{signal_->require_current(cancelled);dark_->require_current(cancelled);flat_->require_current(cancelled);}
    RawPixelBuffer read(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const override{
        budget(read_work_bytes(r),work);require_current(cancelled);const auto available=work-payload_bytes();
        auto s=signal_->read(r,available,cancelled);auto d=dark_->read(r,available-s.payload_bytes(),cancelled);auto f=flat_->read(r,available-s.payload_bytes()-d.payload_bytes(),cancelled);
        auto out=correct_dark_flat(s,d,f,options_,r.first_channel,available+options_.scales.capacity()*sizeof(double),cancelled);require_current(cancelled);return out;
    }
};
}
std::shared_ptr<const ChannelSource> original_channel_source(std::shared_ptr<ImageServer> s,const std::atomic<bool>* cancelled){return std::make_shared<const Original>(std::move(s),cancelled);}
void DarkFlatOptions::validate()const{if(scales.empty() || scales.size()>65536 || !std::isfinite(minimum_denominator) || minimum_denominator<0)throw std::invalid_argument("Invalid dark/flat scales/denominator threshold");for(auto scale:scales)if(!std::isfinite(scale) || scale<=0)throw std::invalid_argument("Dark/flat scales must be positive finite source-sample values");}
std::string DarkFlatOptions::sha256()const{validate();Sha256 h;text(h,"qpx.dark-flat-recipe/1");h.integer(scales.size());for(auto v:scales)real(h,v);real(h,minimum_denominator);return h.finish();}
RawPixelBuffer correct_dark_flat(const RawPixelBuffer& signal,const RawPixelBuffer& dark,const RawPixelBuffer& flat,const DarkFlatOptions& options,std::uint32_t first,std::size_t work,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);options.validate();const auto channels=signal.channels();if(first>=options.scales.size() || channels>options.scales.size()-first)throw std::invalid_argument("Dark/flat scale channel block differs");
    const RawRegionRequest r{{0,0,0,signal.width(),signal.height()},0,channels};
    shape(signal,r,signal.sample_type());shape(dark,r,dark.sample_type());shape(flat,r,flat.sample_type());
    budget(sum({signal.payload_bytes(),dark.payload_bytes(),flat.payload_bytes(),bytes(r,SampleType::Float64),options.scales.capacity()*sizeof(double)}),work);
    RawPixelBuffer out(signal.width(),signal.height(),channels,SampleType::Float64);auto& values=std::get<std::vector<double>>(out.writable_storage());auto& valid=out.writable_valid();const Samples s(signal),d(dark),f(flat);
    for(std::size_t p=0;p<valid.size();++p){if((p&1023)==0)workflow_checkpoint(cancelled);valid[p]=signal.valid()[p] && dark.valid()[p] && flat.valid()[p];if(!valid[p])continue;
        for(std::size_t c=0;c<channels;++c){if(c && (c&4095)==0)workflow_checkpoint(cancelled);const auto i=p*channels+c;const double sv=s.at(s.data,i),dv=d.at(d.data,i),fv=f.at(f.data,i),denominator=fv-dv;
            auto value=std::numeric_limits<double>::quiet_NaN();if(std::isfinite(sv) && std::isfinite(dv) && std::isfinite(fv) && std::isfinite(denominator) && denominator>options.minimum_denominator){value=((sv-dv)/denominator)*options.scales[first+c];if(!std::isfinite(value))value=std::numeric_limits<double>::quiet_NaN();}values[i]=value;
        }
    }workflow_checkpoint(cancelled);return out;
}
std::shared_ptr<const ChannelSource> dark_flat_channel_source(std::shared_ptr<const ChannelSource> s,std::shared_ptr<const ChannelSource> d,std::shared_ptr<const ChannelSource> f,DarkFlatOptions options){return std::make_shared<const Corrected>(std::move(s),std::move(d),std::move(f),std::move(options));}
}
