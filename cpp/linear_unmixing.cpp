#include "qupathxx/linear_unmixing.hpp"
#include "qupathxx/workflow.hpp"
#include "raw_sample_view.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <set>
#ifdef QUPATHXX_HAS_LINEAR_ALGEBRA
#include <opencv2/core.hpp>
#endif
namespace qpx {
namespace {
constexpr std::size_t max_work=1024ull*1024*1024;
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double v){std::uint64_t bits;std::memcpy(&bits,&v,8);h.integer(bits);}
void name(const std::string& s){detail::bounded_utf8(s,96);if(s.empty())throw std::invalid_argument("Unmixing labels must not be empty");}
std::size_t total(std::initializer_list<std::size_t> values){std::size_t n=0;for(auto v:values)workflow_capture_charge(n,v,max_work);return n;}
void budget(std::size_t n,std::size_t work){if(!work || work>max_work)throw std::invalid_argument("Unmixing work must be1..1024MiB");if(n>work)throw WorkflowCaptureLimit("Unmixing block exceeds accounted work budget");}
std::size_t block_bytes(std::int64_t w,std::int64_t h,std::uint32_t c,SampleType t){const auto unit=std::uint64_t(c)*sample_bytes(t)+1;if(w<=0 || h<=0 || !c || c>65536 || std::uint64_t(w)>256ull*1024*1024/unit || std::uint64_t(h)>256ull*1024*1024/(std::uint64_t(w)*unit))throw WorkflowCaptureLimit("Unmixing block exceeds native buffer limit");return std::size_t(w)*std::size_t(h)*std::size_t(unit);}
void shape(const RawPixelBuffer& b,std::int64_t w,std::int64_t h,std::uint32_t channels,SampleType type){if(b.width()!=w || b.height()!=h || b.channels()!=channels || b.sample_type()!=type || b.storage().index()!=std::size_t(type) || b.valid().size()!=std::size_t(w)*std::size_t(h) || std::visit([](const auto& v){return v.size();},b.storage())!=b.valid().size()*channels)throw WorkflowInputChanged("Unmixing input sample layout differs");}
}
void LinearMixingMatrix::validate() const {
    const auto rows=inputs.size(),cols=components.size();if(!cols || rows<cols || rows>64 || cols>64 || coefficients.size()!=rows*cols || !std::isfinite(maximum_condition) || maximum_condition<1 || maximum_condition>1e12)throw std::invalid_argument("Unmixing requires1..64 full-column-rank components/channels and condition limit1..1e12");
    if(input_domain!="original-channel-samples" && input_domain!="qpx.dark-flat-corrected-samples/1")throw std::invalid_argument("Unsupported unmixing input domain");
    std::set<std::uint32_t> channels;std::set<std::string> labels;
    for(const auto& input:inputs){name(input.name);if(input.channel>65535 || !channels.insert(input.channel).second || !labels.insert(input.name).second)throw std::invalid_argument("Unmixing input indices/labels must be unique and bounded");}
    labels.clear();for(const auto& label:components){name(label);if(label.rfind("residual:",0)==0 || !labels.insert(label).second)throw std::invalid_argument("Unmixing component labels must be unique, without reserved residual: prefix");}
    for(auto value:coefficients)if(!std::isfinite(value))throw std::invalid_argument("Unmixing matrix must be finite");
}
std::string LinearMixingMatrix::sha256() const {validate();Sha256 h;text(h,"qpx.linear-mixing-matrix/1");text(h,input_domain);h.integer(inputs.size());h.integer(components.size());for(const auto& v:inputs){h.integer(v.channel);text(h,v.name);}for(const auto& v:components)text(h,v);for(auto v:coefficients)real(h,v);real(h,maximum_condition);return h.finish();}
std::size_t LinearMixingMatrix::payload_bytes() const {auto n=sizeof(*this)+input_domain.capacity()+inputs.capacity()*sizeof(LinearMixingChannel)+components.capacity()*sizeof(std::string)+coefficients.capacity()*sizeof(double);for(const auto& v:inputs)n+=v.name.capacity();for(const auto& v:components)n+=v.capacity();return n;}
std::vector<std::string> LinearMixingMatrix::output_names() const {validate();auto names=components;for(const auto& input:inputs)names.push_back("residual:"+input.name);return names;}
ImageDataValue LinearMixingMatrix::to_value() const {
    validate();using V=ImageDataValue;V::List rows,labels,values;
    rows.reserve(inputs.size());labels.reserve(components.size());values.reserve(coefficients.size());
    for(const auto& input:inputs)rows.push_back(V{V::Map{{"channel",V{std::int64_t(input.channel)}},{"name",V{input.name}}}});
    for(const auto& label:components)labels.push_back(V{label});
    for(auto coefficient:coefficients)values.push_back(V{coefficient});
    return V{V::Map{{"schema",V{std::string("qpx.linear-mixing-matrix/1")}},{"orientation",V{std::string("measured=matrix*components")}},{"input_domain",V{input_domain}},{"inputs",V{std::move(rows)}},{"components",V{std::move(labels)}},{"coefficients",V{std::move(values)}},{"maximum_condition",V{maximum_condition}}}};
}
LinearMixingMatrix LinearMixingMatrix::from_value(const ImageDataValue& value) {
    using V=ImageDataValue;
    const auto object=[](const V& v)->const V::Map&{const auto* p=std::get_if<V::Map>(&v.value);if(!p)throw std::invalid_argument("Unmixing matrix record requires object");return *p;};
    const auto field=[](const V::Map& m,const char* key)->const V&{const auto it=m.find(key);if(it==m.end())throw std::invalid_argument("Unmixing matrix record missing field");return it->second;};
    const auto string=[](const V& v)->const std::string&{const auto* p=std::get_if<std::string>(&v.value);if(!p)throw std::invalid_argument("Unmixing matrix record requires text");return *p;};
    const auto list=[](const V& v)->const V::List&{const auto* p=std::get_if<V::List>(&v.value);if(!p)throw std::invalid_argument("Unmixing matrix record requires list");return *p;};
    const auto real=[](const V& v){const auto* p=std::get_if<double>(&v.value);if(!p)throw std::invalid_argument("Unmixing matrix record requires binary64 coefficient/condition");return *p;};
    const auto& m=object(value);
    if(m.size()!=7 || string(field(m,"schema"))!="qpx.linear-mixing-matrix/1" || string(field(m,"orientation"))!="measured=matrix*components")throw std::invalid_argument("Unsupported unmixing matrix record schema/orientation");
    const auto& rows=list(field(m,"inputs"));const auto& labels=list(field(m,"components"));const auto& values=list(field(m,"coefficients"));
    if(labels.empty() || rows.size()<labels.size() || rows.size()>64 || labels.size()>64 || values.size()!=rows.size()*labels.size())throw std::invalid_argument("Unmixing matrix record dimensions exceed contract");
    LinearMixingMatrix p;const auto& domain=string(field(m,"input_domain"));
    // Admit bounded strings before copying caller-owned potentially large values.
    if(domain!="original-channel-samples" && domain!="qpx.dark-flat-corrected-samples/1")throw std::invalid_argument("Unsupported unmixing input domain");
    p.input_domain=domain;p.maximum_condition=real(field(m,"maximum_condition"));
    p.inputs.reserve(rows.size());p.components.reserve(labels.size());p.coefficients.reserve(values.size());
    for(const auto& row:rows){const auto& input=object(row);if(input.size()!=2)throw std::invalid_argument("Unmixing matrix record input fields differ");const auto* index=std::get_if<std::int64_t>(&field(input,"channel").value);if(!index || *index<0 || *index>65535)throw std::invalid_argument("Unmixing matrix record channel index invalid");const auto& label=string(field(input,"name"));name(label);p.inputs.push_back({std::uint32_t(*index),label});}
    for(const auto& label:labels){const auto& text=string(label);name(text);p.components.push_back(text);}
    for(const auto& coefficient:values)p.coefficients.push_back(real(coefficient));
    p.validate();return p;
}
bool has_linear_unmixing(){
#ifdef QUPATHXX_HAS_LINEAR_ALGEBRA
    return true;
#else
    return false;
#endif
}
struct LinearUnmixModel::Data {LinearMixingMatrix matrix;std::vector<double> inverse;double condition=0;std::string sha;};
LinearUnmixModel::LinearUnmixModel(LinearMixingMatrix matrix,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);matrix.validate();
#ifdef QUPATHXX_HAS_LINEAR_ALGEBRA
    auto data=std::make_shared<Data>();data->matrix=std::move(matrix);const auto& p=data->matrix;const int rows=int(p.inputs.size()),cols=int(p.components.size());
    double scale=0;for(auto v:p.coefficients)scale=std::max(scale,std::abs(v));if(scale==0)throw std::invalid_argument("Unmixing matrix is rank deficient");cv::Mat normalized(rows,cols,CV_64F);
    for(int i=0;i<rows;++i)for(int j=0;j<cols;++j){const auto original=p.coefficients[std::size_t(i)*cols+j];const auto v=original/scale;if(original!=0 && v==0)throw std::invalid_argument("Unmixing matrix normalization is not representable");normalized.at<double>(i,j)=v;}
    cv::SVD svd(normalized,0);workflow_checkpoint(cancelled);const auto largest=svd.w.at<double>(0),smallest=svd.w.at<double>(cols-1);
    if(!std::isfinite(largest) || !std::isfinite(smallest) || smallest<=std::numeric_limits<double>::epsilon()*rows*largest)throw std::invalid_argument("Unmixing matrix is numerically rank deficient");
    data->condition=largest/smallest;if(!std::isfinite(data->condition) || data->condition>p.maximum_condition)throw std::invalid_argument("Unmixing matrix exceeds its condition limit");
    data->inverse.resize(std::size_t(rows)*cols);
    for(int j=0;j<cols;++j)for(int i=0;i<rows;++i){double value=0;for(int k=0;k<cols;++k)value+=svd.vt.at<double>(k,j)*(svd.u.at<double>(i,k)/svd.w.at<double>(k));const auto scaled=value/scale;if(!std::isfinite(scaled) || (value!=0 && scaled==0))throw std::invalid_argument("Unmixing inverse is not representable");data->inverse[std::size_t(j)*rows+i]=scaled;}
    Sha256 identity;text(identity,"qpx.linear-unmixing-svd/1");text(identity,"opencv-core-4.11.0-thin-svd");text(identity,p.sha256());for(auto value:data->inverse)real(identity,value);real(identity,data->condition);data->sha=identity.finish();
    workflow_checkpoint(cancelled);data_=std::move(data);
#else
    throw std::runtime_error("Linear unmixing requires the optional native OpenCV core capability");
#endif
}
const LinearMixingMatrix& LinearUnmixModel::matrix() const{return data_->matrix;}
double LinearUnmixModel::condition() const{return data_->condition;}
const std::string& LinearUnmixModel::sha256() const{return data_->sha;}
std::size_t LinearUnmixModel::payload_bytes() const{return sizeof(*this)+sizeof(Data)+data_->matrix.payload_bytes()+data_->inverse.capacity()*sizeof(double)+data_->sha.capacity();}
struct LinearUnmixKernel {
    static RawPixelBuffer evaluate(const LinearUnmixModel& model,const RawPixelBuffer& input,const std::vector<std::uint32_t>& offsets,std::uint32_t first,std::uint32_t count,std::size_t work,const std::atomic<bool>* cancelled){
        workflow_checkpoint(cancelled);const auto& d=*model.data_;const auto& p=d.matrix;const auto rows=p.inputs.size(),cols=p.components.size(),outputs=rows+cols;
        if(first>=outputs || !count || count>outputs-first || offsets.size()!=rows)throw std::invalid_argument("Invalid unmixing output/input selection");for(auto offset:offsets)if(offset>=input.channels())throw std::invalid_argument("Unmixing input offset outside block");shape(input,input.width(),input.height(),input.channels(),input.sample_type());
        budget(total({model.payload_bytes(),input.payload_bytes(),block_bytes(input.width(),input.height(),count,SampleType::Float64),4096}),work);
        RawPixelBuffer out(input.width(),input.height(),count,SampleType::Float64);auto& values=std::get<std::vector<double>>(out.writable_storage());auto& valid=out.writable_valid();const detail::RawSampleView samples(input);
        std::vector<double> measured(rows),components(cols);const bool residual=std::size_t(first)+count>cols;const auto nan=std::numeric_limits<double>::quiet_NaN();
        for(std::size_t pixel=0;pixel<valid.size();++pixel){if((pixel&1023)==0)workflow_checkpoint(cancelled);valid[pixel]=bool(input.valid()[pixel]);if(!valid[pixel])continue;bool finite=true;
            for(std::size_t i=0;i<rows;++i){measured[i]=samples.at(samples.data,pixel*input.channels()+offsets[i]);finite=finite && std::isfinite(measured[i]);}
            if(!finite){std::fill_n(values.begin()+pixel*count,count,nan);continue;}
            const auto solve=[&](std::size_t j){double value=0;for(std::size_t i=0;i<rows;++i)value+=d.inverse[j*rows+i]*measured[i];return std::isfinite(value)?value:nan;};
            bool all_components=true;if(residual)for(std::size_t j=0;j<cols;++j){components[j]=solve(j);all_components=all_components && std::isfinite(components[j]);}
            for(std::size_t c=0;c<count;++c){const auto output=std::size_t(first)+c;if(output<cols)values[pixel*count+c]=residual?components[output]:solve(output);
                else{const auto i=output-cols;double prediction=0;if(all_components)for(std::size_t j=0;j<cols;++j)prediction+=p.coefficients[i*cols+j]*components[j];const auto value=measured[i]-prediction;values[pixel*count+c]=all_components && std::isfinite(value)?value:nan;}
            }
        }workflow_checkpoint(cancelled);return out;
    }
};
RawPixelBuffer linear_unmix_block(const RawPixelBuffer& input,const LinearUnmixModel& model,std::uint32_t first,std::uint32_t count,std::size_t work,const std::atomic<bool>* cancelled){if(input.channels()!=model.matrix().inputs.size())throw std::invalid_argument("Unmixing input must follow matrix row order exactly");std::vector<std::uint32_t> offsets(input.channels());std::iota(offsets.begin(),offsets.end(),0);const auto all=model.matrix().inputs.size()+model.matrix().components.size();if(!count){if(first>=all)throw std::invalid_argument("Unmixing first output outside result");count=std::uint32_t(all-first);}return LinearUnmixKernel::evaluate(model,input,offsets,first,count,work,cancelled);}
namespace {
class Unmixed final:public ChannelSource {
    struct Group {std::uint32_t first=0;std::vector<std::uint32_t> rows;};
    std::shared_ptr<const ChannelSource> input_;LinearUnmixModel model_;RawImageMetadata metadata_;std::vector<Group> groups_;std::vector<std::uint32_t> offsets_;std::string id_;
    RawRegionRequest selection(const RawRegionRequest& r,const Group& g)const {auto input=r;input.first_channel=g.first;input.channels=std::uint32_t(g.rows.size());return input;}
public:
    Unmixed(std::shared_ptr<const ChannelSource> input,LinearUnmixModel model,const std::atomic<bool>* cancelled):input_(std::move(input)),model_(std::move(model)){
        if(!input_ || input_->domain()!=model_.matrix().input_domain)throw std::invalid_argument("Unmixing source value domain differs");const auto& p=model_.matrix();const auto& m=input_->metadata();
        std::vector<std::pair<std::uint32_t,std::uint32_t>> sorted;for(std::size_t i=0;i<p.inputs.size();++i){const auto& binding=p.inputs[i];if(binding.channel>=m.channels || binding.channel>=m.channel_names.size() || m.channel_names[binding.channel]!=binding.name)throw std::invalid_argument("Unmixing source channel index/name binding differs");sorted.emplace_back(binding.channel,std::uint32_t(i));}std::sort(sorted.begin(),sorted.end());
        for(const auto& item:sorted){if(groups_.empty() || groups_.back().first+groups_.back().rows.size()!=item.first)groups_.push_back({item.first,{}});groups_.back().rows.push_back(item.second);}
        offsets_.resize(p.inputs.size());if(groups_.size()==1){for(std::size_t i=0;i<p.inputs.size();++i)offsets_[i]=p.inputs[i].channel-groups_[0].first;}else std::iota(offsets_.begin(),offsets_.end(),0);
        metadata_=m;metadata_.sample_type=SampleType::Float64;metadata_.channels=std::uint32_t(p.inputs.size()+p.components.size());metadata_.channel_names=p.output_names();if(metadata_.levels.empty())throw std::invalid_argument("Unmixing source has no levels");metadata_.levels.resize(1);
        Sha256 h;text(h,"qpx.linear-unmixed-source/2");text(h,input_->sha256());text(h,model_.sha256());id_=h.finish();input_->require_current(cancelled);
    }
    const RawImageMetadata& metadata()const override{return metadata_;}
    const std::string& domain()const override{static const std::string value="qpx.linear-unmixed-samples/1";return value;}
    const std::string& sha256()const override{return id_;}
    std::size_t payload_bytes()const override{auto n=sizeof(*this)+input_->payload_bytes()+model_.payload_bytes()+metadata_.payload_bytes()+offsets_.capacity()*sizeof(std::uint32_t)+groups_.capacity()*sizeof(Group)+128;for(const auto& g:groups_)n+=g.rows.capacity()*sizeof(std::uint32_t);return n;}
    std::size_t read_work_bytes(const RawRegionRequest& r)const override{
        if(r.region.level!=0)throw std::invalid_argument("Unmixing currently requires level-zero blocks");r.validate(metadata_);const auto rows=std::uint32_t(model_.matrix().inputs.size());const auto gathered=block_bytes(r.region.width,r.region.height,rows,input_->metadata().sample_type);const auto output=block_bytes(r.region.width,r.region.height,r.channels,SampleType::Float64);auto peak=total({gathered,output,4096});
        for(const auto& g:groups_)peak=std::max(peak,total({groups_.size()>1?gathered:0,input_->read_work_bytes(selection(r,g))}));return total({payload_bytes(),peak});
    }
    void require_current(const std::atomic<bool>* cancelled)const override{input_->require_current(cancelled);}
    RawPixelBuffer read(const RawRegionRequest& r,std::size_t work,const std::atomic<bool>* cancelled)const override{
        budget(read_work_bytes(r),work);workflow_checkpoint(cancelled);const auto available=work-payload_bytes();
        if(groups_.size()==1){auto samples=input_->read(selection(r,groups_[0]),available,cancelled);shape(samples,r.region.width,r.region.height,std::uint32_t(groups_[0].rows.size()),input_->metadata().sample_type);auto out=LinearUnmixKernel::evaluate(model_,samples,offsets_,r.first_channel,r.channels,available+model_.payload_bytes(),cancelled);require_current(cancelled);return out;}
        RawPixelBuffer gathered(r.region.width,r.region.height,std::uint32_t(model_.matrix().inputs.size()),input_->metadata().sample_type);std::fill(gathered.writable_valid().begin(),gathered.writable_valid().end(),1);
        for(const auto& g:groups_){workflow_checkpoint(cancelled);auto samples=input_->read(selection(r,g),available-gathered.payload_bytes(),cancelled);shape(samples,r.region.width,r.region.height,std::uint32_t(g.rows.size()),input_->metadata().sample_type);
            std::visit([&](const auto& values){using T=typename std::decay_t<decltype(values)>::value_type;auto& output=std::get<std::vector<T>>(gathered.writable_storage());for(std::size_t pixel=0;pixel<gathered.valid().size();++pixel){if((pixel&1023)==0)workflow_checkpoint(cancelled);gathered.writable_valid()[pixel]=gathered.valid()[pixel] && samples.valid()[pixel];for(std::size_t c=0;c<g.rows.size();++c)output[pixel*gathered.channels()+g.rows[c]]=values[pixel*g.rows.size()+c];}},samples.storage());
        }
        auto out=LinearUnmixKernel::evaluate(model_,gathered,offsets_,r.first_channel,r.channels,available+model_.payload_bytes(),cancelled);require_current(cancelled);return out;
    }
};
}
std::shared_ptr<const ChannelSource> linear_unmix_channel_source(std::shared_ptr<const ChannelSource> input,LinearMixingMatrix matrix,const std::atomic<bool>* cancelled){return linear_unmix_channel_source(std::move(input),LinearUnmixModel(std::move(matrix),cancelled),cancelled);}
std::shared_ptr<const ChannelSource> linear_unmix_channel_source(std::shared_ptr<const ChannelSource> input,LinearUnmixModel model,const std::atomic<bool>* cancelled){return std::make_shared<const Unmixed>(std::move(input),std::move(model),cancelled);}
}
