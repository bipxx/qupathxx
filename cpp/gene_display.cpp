#include "qupathxx/gene_display.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <numeric>
#include <set>

namespace qpx {
namespace {
void real(Sha256& digest,double value) {
    if(value==0)value=0;std::uint64_t bits;std::memcpy(&bits,&value,8);digest.integer(bits);
}
void text(Sha256& digest,const std::string& value) {digest.integer(value.size());digest.update(value.data(),value.size());}
std::uint8_t alpha(std::uint8_t value,double opacity) {return static_cast<std::uint8_t>(std::lround(value*opacity));}
double percentile(const std::vector<double>& sorted,double p) {
    if(sorted.empty())return 0;
    const auto index=p/100*static_cast<double>(sorted.size()-1);const auto lower=static_cast<std::size_t>(index);
    const auto upper=std::min(sorted.size()-1,lower+1);const auto fraction=index-lower;
    // Convex form avoids overflowing upper-lower for signed continuous values.
    return (1-fraction)*sorted[lower]+fraction*sorted[upper];
}
std::array<std::uint8_t,4> color(double value,const GeneDisplayOptions& options) {
    // Repository-authored monotone sequential ramps; no external palette asset.
    const std::array<double,3> low=options.palette==GeneDisplayPalette::Gray?std::array<double,3>{32,32,32}:std::array<double,3>{24,42,96};
    const std::array<double,3> high=options.palette==GeneDisplayPalette::Gray?std::array<double,3>{245,245,245}:std::array<double,3>{255,230,65};
    std::array<std::uint8_t,4> result{};
    for(std::size_t i=0;i<3;++i)result[i]=static_cast<std::uint8_t>(std::lround((1-value)*low[i]+value*high[i]));
    result[3]=alpha(255,options.opacity);return result;
}
double normalized(double value,double lower,double upper) {
    if(lower==upper)return value<lower?0:value>upper?1:.5;
    if(value<=lower)return 0;if(value>=upper)return 1;
    // Scaling first keeps a finite range spanning -DBL_MAX..DBL_MAX usable.
    const auto scale=std::max({std::abs(lower),std::abs(upper),1.});
    return std::clamp((value/scale-lower/scale)/(upper/scale-lower/scale),0.,1.);
}
}
void GeneDisplayOptions::validate() const {
    if(transform!=GeneDisplayTransform::Linear && transform!=GeneDisplayTransform::Log1p)throw std::invalid_argument("Invalid gene display transform");
    if(range!=GeneDisplayRange::MinMax && range!=GeneDisplayRange::Percentile && range!=GeneDisplayRange::Manual)throw std::invalid_argument("Invalid gene display range");
    if(palette!=GeneDisplayPalette::BlueYellow && palette!=GeneDisplayPalette::Gray)throw std::invalid_argument("Invalid gene palette");
    if(!std::isfinite(opacity) || opacity<0 || opacity>1 || !std::isfinite(lower) || !std::isfinite(upper) ||
       !std::isfinite(lower_percentile) || !std::isfinite(upper_percentile) || lower_percentile<0 || upper_percentile>100 || lower_percentile>=upper_percentile ||
       (range==GeneDisplayRange::Manual && lower>upper))throw std::invalid_argument("Invalid gene display limits/opacity");
}
std::array<std::uint8_t,4> gene_display_palette_color(double position,const GeneDisplayOptions& options) {
    options.validate();if(!std::isfinite(position) || position<0 || position>1)throw std::invalid_argument("Invalid gene palette position");return color(position,options);
}
struct GeneDisplayRequest::Data {
    ExpressionDataset dataset;std::string feature,sha;GeneDisplayOptions options;std::vector<std::size_t> rows;
    std::size_t column=0,work=0;
    explicit Data(ExpressionDataset value):dataset(std::move(value)) {}
};
GeneDisplayRequest::GeneDisplayRequest(ExpressionDataset dataset,std::string feature,GeneDisplayOptions options,
    std::optional<std::vector<std::size_t>> selected,std::size_t work) {
    options.validate();if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid gene display work budget");
    const auto n=selected?selected->size():dataset.observations().size();
    if(n>work/128)throw WorkflowCaptureLimit("Gene display cohort exceeds work budget");
    if(selected && selected->capacity()>work/sizeof(std::size_t))
        throw WorkflowCaptureLimit("Gene display retained cohort exceeds work budget");
    std::size_t used=sizeof(Data);
    workflow_capture_charge(used,feature.capacity(),work);
    workflow_capture_charge(used,(selected?selected->capacity():n)*sizeof(std::size_t),work);
    workflow_capture_charge(used,n*120,work);
    auto data=std::make_shared<Data>(std::move(dataset));data->feature=std::move(feature);data->options=options;
    data->column=data->dataset.feature_index(data->feature);data->work=work;
    if(selected)data->rows=std::move(*selected);
    else {data->rows.resize(n);std::iota(data->rows.begin(),data->rows.end(),0);}
    std::set<std::size_t> unique;
    for(auto row:data->rows)if(row>=data->dataset.observations().size() || !unique.insert(row).second)
        throw std::invalid_argument("Gene display cohort has duplicate/out-of-range observation rows");
    Sha256 digest;text(digest,"qpx.gene-display/1");text(digest,data->dataset.sha256());text(digest,data->feature);
    digest.integer(static_cast<std::uint64_t>(options.transform));digest.integer(static_cast<std::uint64_t>(options.range));digest.integer(static_cast<std::uint64_t>(options.palette));
    if(options.range==GeneDisplayRange::Manual) {real(digest,options.lower);real(digest,options.upper);}
    if(options.range==GeneDisplayRange::Percentile) {real(digest,options.lower_percentile);real(digest,options.upper_percentile);}
    real(digest,options.opacity);
    for(auto component:options.missing_color)digest.integer(component);
    for(auto component:options.zero_color)digest.integer(component);
    digest.integer(data->rows.size());for(auto row:data->rows)digest.integer(row);
    data->sha=digest.finish();data_=std::move(data);
}
const ExpressionDataset& GeneDisplayRequest::dataset() const {return data_->dataset;}
const std::string& GeneDisplayRequest::feature() const {return data_->feature;}
const GeneDisplayOptions& GeneDisplayRequest::options() const {return data_->options;}
const std::vector<std::size_t>& GeneDisplayRequest::rows() const {return data_->rows;}
std::size_t GeneDisplayRequest::feature_index() const {return data_->column;}
std::size_t GeneDisplayRequest::work_bytes() const {return data_->work;}
const std::string& GeneDisplayRequest::sha256() const {return data_->sha;}
std::shared_ptr<const GeneDisplayField> gene_display_field(const GeneDisplayRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);const auto& dataset=request.dataset();const auto& options=request.options();
    const auto size=request.rows().size();
    if(size>request.work_bytes()/64)throw WorkflowCaptureLimit("Gene display buffers exceed work budget");
    auto result=std::make_shared<GeneDisplayField>(GeneDisplayField{dataset});result->feature=request.feature();result->request_sha256=request.sha256();
    result->rows=request.rows();result->transformed_values.assign(size,0);result->valid.assign(size,0);result->colors.resize(size);
    auto& legend=result->legend;legend.domain=dataset.domain();legend.transform=options.transform;legend.range=options.range;
    std::vector<double> samples;samples.reserve(size);
    for(std::size_t i=0;i<size;++i) {
        if(!(i&4095))workflow_checkpoint(cancelled);
        const auto at=result->rows[i]*dataset.features().size()+request.feature_index();
        if(!dataset.valid()[at]) {++legend.missing;continue;}
        auto value=dataset.domain()==ExpressionValueDomain::Counts?static_cast<double>(dataset.counts()[at]):dataset.continuous()[at];
        ++legend.measured;if(value==0)++legend.zeros;
        if(options.transform==GeneDisplayTransform::Log1p) {
            if(value<0)throw std::invalid_argument("Log1p gene display requires nonnegative measured values");
            value=std::log1p(value);
        }
        result->transformed_values[i]=value;result->valid[i]=1;samples.push_back(value);
    }
    if(options.range==GeneDisplayRange::Manual) {legend.lower=options.lower;legend.upper=options.upper;}
    else if(!samples.empty()) {
        if(options.range==GeneDisplayRange::Percentile) {
            std::sort(samples.begin(),samples.end());workflow_checkpoint(cancelled);
            legend.lower=percentile(samples,options.lower_percentile);legend.upper=percentile(samples,options.upper_percentile);
        } else {const auto limits=std::minmax_element(samples.begin(),samples.end());legend.lower=*limits.first;legend.upper=*limits.second;}
    }
    for(std::size_t i=0;i<size;++i) {
        if(!(i&4095))workflow_checkpoint(cancelled);
        if(!result->valid[i])result->colors[i]=options.missing_color;
        else {
            const auto value=result->transformed_values[i];legend.clipped_lower+=value<legend.lower;legend.clipped_upper+=value>legend.upper;
            result->colors[i]=value==0?options.zero_color:color(normalized(value,legend.lower,legend.upper),options);
        }
        if(!result->valid[i] || result->transformed_values[i]==0)
            result->colors[i][3]=alpha(result->colors[i][3],options.opacity);
    }
    result->payload_bytes=sizeof(GeneDisplayField)+dataset.payload_bytes()+result->feature.capacity()+result->request_sha256.capacity()+
        result->rows.capacity()*sizeof(std::size_t)+result->transformed_values.capacity()*sizeof(double)+result->valid.capacity()+result->colors.capacity()*4;
    workflow_checkpoint(cancelled);return result;
}
GeneDisplayPair gene_display_pair(ExpressionDataset dataset,const std::array<std::string,2>& features,
    GeneDisplayOptions options,bool compare,bool shared,std::size_t work,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);options.validate();
    if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid gene comparison work budget");
    if(dataset.observations().size()>work/256)throw WorkflowCaptureLimit("Gene comparison buffers exceed work budget");
    GeneDisplayPair pair;pair.range_policy=options.range;pair.shared_range=compare && shared;
    const auto count=compare?2u:1u;
    for(unsigned i=0;i<count;++i)pair.fields[i]=gene_display_field(GeneDisplayRequest(dataset,features[i],options,std::nullopt,work),cancelled);
    if(pair.shared_range && options.range!=GeneDisplayRange::Manual) {
        std::vector<double> samples;bool measured=false;double lower=0,upper=0;
        if(options.range==GeneDisplayRange::Percentile)samples.reserve(2*dataset.observations().size());
        for(const auto& field:pair.fields)for(std::size_t i=0;i<field->valid.size();++i) {
            if(!(i&4095))workflow_checkpoint(cancelled);if(!field->valid[i])continue;
            const auto v=field->transformed_values[i];
            if(!measured) {lower=upper=v;measured=true;}else {lower=std::min(lower,v);upper=std::max(upper,v);}
            if(options.range==GeneDisplayRange::Percentile)samples.push_back(v);
        }
        if(!samples.empty()) {
            std::sort(samples.begin(),samples.end());workflow_checkpoint(cancelled);
            lower=percentile(samples,options.lower_percentile);upper=percentile(samples,options.upper_percentile);
        }
        options.range=GeneDisplayRange::Manual;options.lower=lower;options.upper=upper;
        for(unsigned i=0;i<2;++i)pair.fields[i]=gene_display_field(GeneDisplayRequest(dataset,features[i],options,std::nullopt,work),cancelled);
    }
    return pair;
}
struct GeneDisplayCache::Data {
    std::thread::id owner=std::this_thread::get_id();
    std::size_t limit,max_entries,bytes=0;
    std::list<std::shared_ptr<const GeneDisplayField>> fields;
};
GeneDisplayCache::GeneDisplayCache(std::size_t bytes,std::size_t count) {
    if(!bytes || bytes>1024ULL*1024*1024 || !count || count>1024)throw std::invalid_argument("Invalid gene display cache limits");
    data_=std::make_unique<Data>();data_->limit=bytes;data_->max_entries=count;
}
GeneDisplayCache::~GeneDisplayCache()=default;
std::shared_ptr<const GeneDisplayField> GeneDisplayCache::get(const GeneDisplayRequest& request,const std::atomic<bool>* cancelled) {
    workflow_require_owner(data_->owner);workflow_checkpoint(cancelled);
    for(auto at=data_->fields.begin();at!=data_->fields.end();++at)if((*at)->request_sha256==request.sha256()) {
        auto result=*at;data_->fields.splice(data_->fields.begin(),data_->fields,at);return result;
    }
    auto result=gene_display_field(request,cancelled);
    if(result->payload_bytes>data_->limit)return result;
    while(data_->fields.size()>=data_->max_entries || result->payload_bytes>data_->limit-data_->bytes) {
        data_->bytes-=data_->fields.back()->payload_bytes;data_->fields.pop_back();
    }
    data_->bytes+=result->payload_bytes;data_->fields.push_front(result);return result;
}
void GeneDisplayCache::clear() {workflow_require_owner(data_->owner);data_->fields.clear();data_->bytes=0;}
std::size_t GeneDisplayCache::retained_bytes() const {workflow_require_owner(data_->owner);return data_->bytes;}
std::size_t GeneDisplayCache::entries() const {workflow_require_owner(data_->owner);return data_->fields.size();}
}
