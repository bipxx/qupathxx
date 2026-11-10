#include "qupathxx/gene_view.hpp"
#include "qupathxx/workflow.hpp"
#include "utf8_text.hpp"
#include "expression_normalization_internal.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <future>
#include <limits>
#include <string_view>

namespace qpx {
namespace {
void feature_name(const std::string& id) {
    detail::bounded_utf8(id,4096);
    if(id.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Invalid gene-view feature ID");
}
template<class T> std::string text(T value) {
    char buffer[96];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);
    if(result.ec!=std::errc{})throw std::invalid_argument("Gene-view preference number overflow");return {buffer,result.ptr};
}
template<class T> T number(std::string_view value) {
    T result{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(value.empty() || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid gene-view preference number");return result;
}
std::string folded(std::string value) {for(auto& c:value)if(c>='A' && c<='Z')c=char(c-'A'+'a');return value;}
bool same_groups(const std::vector<ExpressionFeatureGroup>& a,const std::vector<ExpressionFeatureGroup>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].name!=b[i].name || a[i].features!=b[i].features)return false;return true;}
}
void GeneViewOptions::validate() const {
    for(const auto& gene:genes)feature_name(gene);display.validate();
    if(!std::isfinite(radius) || radius<1 || radius>20)throw std::invalid_argument("Gene glyph radius must be1..20 screen pixels");
}
std::string gene_view_preference(const GeneViewOptions& options) {
    options.validate();std::vector<std::string> fields{"qpx.gene-view/1",text(int(options.enabled)),text(int(options.compare)),text(int(options.shared_range)),options.genes[0],options.genes[1],
        text(int(options.display.transform)),text(int(options.display.range)),text(int(options.display.palette)),text(options.display.lower),text(options.display.upper),
        text(options.display.lower_percentile),text(options.display.upper_percentile),text(options.display.opacity),text(options.radius)};
    for(auto v:options.display.missing_color)fields.push_back(text(int(v)));
    for(auto v:options.display.zero_color)fields.push_back(text(int(v)));
    std::string result;for(const auto& field:fields) {if(!result.empty())result+='\t';result+=field;}return result;
}
GeneViewOptions parse_gene_view_preference(const std::string& input) {
    if(input.size()>16384 || input.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("Invalid gene-view preference line");
    std::vector<std::string_view> fields;std::string_view remaining=input;
    for(;;) {const auto at=remaining.find('\t');fields.push_back(remaining.substr(0,at));if(at==std::string_view::npos)break;remaining.remove_prefix(at+1);}
    if(fields.size()!=23 || fields[0]!="qpx.gene-view/1")throw std::invalid_argument("Unknown gene-view preference schema/fields");
    const auto flag=[&](std::size_t at) {const auto n=number<int>(fields[at]);if(n!=0 && n!=1)throw std::invalid_argument("Invalid gene-view flag");return bool(n);};
    GeneViewOptions options;options.enabled=flag(1);options.compare=flag(2);options.shared_range=flag(3);
    options.genes={std::string(fields[4]),std::string(fields[5])};options.display.transform=static_cast<GeneDisplayTransform>(number<int>(fields[6]));
    options.display.range=static_cast<GeneDisplayRange>(number<int>(fields[7]));options.display.palette=static_cast<GeneDisplayPalette>(number<int>(fields[8]));
    options.display.lower=number<double>(fields[9]);options.display.upper=number<double>(fields[10]);options.display.lower_percentile=number<double>(fields[11]);
    options.display.upper_percentile=number<double>(fields[12]);options.display.opacity=number<double>(fields[13]);options.radius=number<double>(fields[14]);
    for(std::size_t i=0;i<8;++i) {const auto n=number<int>(fields[15+i]);if(n<0 || n>255)throw std::invalid_argument("Invalid gene-view color");
        (i<4?options.display.missing_color[i]:options.display.zero_color[i-4])=static_cast<std::uint8_t>(n);}
    options.validate();return options;
}
GeneFeaturePage gene_feature_page(const ExpressionDataset& dataset,const std::string& query,std::size_t page,std::size_t size) {
    return gene_feature_page(dataset.features(),query,page,size);
}
GeneFeaturePage gene_feature_page(const std::vector<std::string>& features,const std::string& query,std::size_t page,std::size_t size) {
    feature_name(query);if(!size || size>256 || page>std::numeric_limits<std::size_t>::max()/size)throw std::invalid_argument("Invalid gene catalogue page");
    const auto needle=folded(query);GeneFeaturePage result;result.page=page;const auto start=page*size;
    for(std::size_t i=0;i<features.size();++i)if(folded(features[i]).find(needle)!=std::string::npos) {
        if(result.matches>=start && result.indices.size()<size)result.indices.push_back(i);++result.matches;
    }
    return result;
}
std::string gene_workspace_preference(const GeneViewOptions& view,const std::string& source) {
    if(!source.empty() && (source.size()!=64 || source.find_first_not_of("0123456789abcdef")!=std::string::npos))
        throw std::invalid_argument("Invalid workspace expression-source identity");
    return "qpx.gene-workspace/1\t"+source+"\t"+gene_view_preference(view);
}
GeneWorkspacePreference parse_gene_workspace_preference(const std::string& input) {
    const std::string prefix="qpx.gene-workspace/1\t";
    if(input.compare(0,prefix.size(),prefix)!=0)return {parse_gene_view_preference(input),{}};
    if(input.size()>16384)throw std::invalid_argument("Gene workspace preference exceeds limit");
    const auto end=input.find('\t',prefix.size());if(end==std::string::npos)throw std::invalid_argument("Truncated gene workspace preference");
    GeneWorkspacePreference result{parse_gene_view_preference(input.substr(end+1)),input.substr(prefix.size(),end-prefix.size())};
    gene_workspace_preference(result.view,result.source_id);return result;
}
struct GeneViewSession::Data {
    struct Ready {GeneDisplayPair pair;std::optional<ExpressionMatrixSpatialSnapshot> spatial,raw;std::shared_ptr<const ExpressionQcResult> qc;std::shared_ptr<const ExpressionNormalization> normalization;};
    std::thread::id owner=std::this_thread::get_id();std::size_t work;
    GeneViewOptions options;std::optional<ExpressionDataset> dataset;std::uint64_t generation=0,running=0;
    std::optional<ExpressionMatrixSpatialSnapshot> spatial;
    std::optional<ExpressionMatrixSpatialSnapshot> raw_spatial;
    std::optional<ExpressionMatrixSpatialRequest> spatial_request;
    std::shared_ptr<const ExpressionQcResult> qc;
    std::shared_ptr<const ExpressionNormalization> normalization;
    std::shared_ptr<const ExpressionMatrixSource> normalization_parent;
    ExpressionQcOptions qc_options;ExpressionNormalizationOptions normalization_options;
    bool want_qc=false,want_normalized=false;
    std::string science_source,saved_recipe;
    bool pending=false;std::atomic<bool> cancelled{false};std::future<Ready> future;
    std::shared_ptr<const GeneDisplayPair> result;std::string error;
    std::shared_ptr<const ExpressionMatrixSource> raw_source()const{return raw_spatial?raw_spatial->source():spatial_request?spatial_request->source():nullptr;}
    void source_changed(std::shared_ptr<const ExpressionMatrixSource> source){
        const auto next=source?source->metadata().source_sha256:std::string{};
        if(next!=science_source){qc.reset();normalization.reset();normalization_parent.reset();qc_options={};normalization_options={};want_qc=want_normalized=false;saved_recipe.clear();}
        science_source=next;
    }
    void changed() {
        if(generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Gene-view generation exhausted");
        ++generation;cancelled.store(true);result.reset();error.clear();pending=(options.enabled || want_qc) && (bool(dataset) || bool(raw_spatial) || bool(spatial_request));
    }
};
GeneViewSession::GeneViewSession(std::size_t work):data_(std::make_unique<Data>()) {
    if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid gene-view work budget");data_->work=work;
}
GeneViewSession::~GeneViewSession(){data_->cancelled.store(true);if(data_->future.valid())data_->future.wait();}
void GeneViewSession::bind(ExpressionDataset dataset) {
    workflow_require_owner(data_->owner);if(!data_->spatial && data_->dataset && data_->dataset->sha256()==dataset.sha256())return;
    for(std::size_t i=0;i<2;++i)if(std::find(dataset.features().begin(),dataset.features().end(),data_->options.genes[i])==dataset.features().end())
        data_->options.genes[i]=dataset.features()[std::min(i,dataset.features().size()-1)];
    data_->source_changed({});data_->spatial_request.reset();data_->spatial.reset();data_->raw_spatial.reset();data_->dataset=std::move(dataset);data_->changed();
}
void GeneViewSession::bind(ExpressionMatrixSpatialSnapshot spatial) {
    workflow_require_owner(data_->owner);
    if(data_->raw_spatial && data_->raw_spatial->sha256()==spatial.sha256() && data_->raw_spatial->source()==spatial.source())return;
    data_->source_changed(spatial.source());data_->spatial_request.reset();data_->raw_spatial=std::move(spatial);data_->spatial.reset();data_->dataset.reset();data_->changed();
}
void GeneViewSession::bind(ExpressionMatrixSpatialRequest request) {
    workflow_require_owner(data_->owner);
    if(data_->spatial_request && data_->spatial_request->sha256()==request.sha256() && data_->spatial_request->source()==request.source())return;
    data_->source_changed(request.source());data_->spatial_request=std::move(request);data_->spatial.reset();data_->raw_spatial.reset();data_->dataset.reset();data_->changed();
}
void GeneViewSession::clear() {workflow_require_owner(data_->owner);data_->source_changed({});data_->spatial_request.reset();data_->raw_spatial.reset();data_->spatial.reset();data_->dataset.reset();data_->changed();}
void GeneViewSession::configure(GeneViewOptions options) {
    workflow_require_owner(data_->owner);options.validate();
    if(data_->want_normalized && data_->normalization_options.log1p && options.display.transform!=GeneDisplayTransform::Linear)throw std::invalid_argument("Log-normalized values require a linear display transform; do not log twice");
    if(data_->dataset && !data_->spatial && !data_->spatial_request)for(auto& feature:options.genes)if(!feature.empty())data_->dataset->feature_index(feature);
    if(data_->dataset && !data_->spatial && !data_->spatial_request)for(std::size_t i=0;i<2;++i)if(options.genes[i].empty())options.genes[i]=data_->dataset->features()[std::min(i,data_->dataset->features().size()-1)];
    if(gene_view_preference(options)==gene_view_preference(data_->options))return;
    auto previous=data_->options;previous.radius=options.radius;
    if(gene_view_preference(options)==gene_view_preference(previous)){data_->options=std::move(options);return;}
    data_->options=std::move(options);data_->changed();
}
void GeneViewSession::request_qc(ExpressionQcOptions options){
    workflow_require_owner(data_->owner);const auto source=data_->raw_source();if(!source)throw std::invalid_argument("Full-source QC requires indexed raw expression, not a gene projection");detail::normalization::raw(*source);options.work_bytes=std::min(options.work_bytes,data_->work);options.validate(source->metadata().features);
    const bool groups_same=same_groups(options.groups,data_->qc_options.groups);if(data_->want_qc && groups_same && data_->qc)return;
    if(!groups_same){data_->qc.reset();data_->normalization.reset();data_->spatial.reset();data_->dataset.reset();data_->saved_recipe.clear();}data_->qc_options=std::move(options);data_->want_qc=true;data_->changed();
}
void GeneViewSession::normalize(ExpressionNormalizationOptions options){
    workflow_require_owner(data_->owner);options.validate();const auto source=data_->raw_source();if(!source)throw std::invalid_argument("Normalization requires indexed raw expression, not a gene projection");detail::normalization::raw(*source);
    if(data_->want_normalized && data_->normalization_options.target_sum==options.target_sum && data_->normalization_options.log1p==options.log1p && data_->saved_recipe.empty())return;
    data_->normalization_options=options;data_->want_normalized=data_->want_qc=true;data_->saved_recipe.clear();data_->normalization.reset();data_->spatial.reset();data_->dataset.reset();data_->options.display.transform=GeneDisplayTransform::Linear;data_->changed();
}
void GeneViewSession::raw_values(){workflow_require_owner(data_->owner);if(!data_->want_normalized)return;data_->want_normalized=false;data_->saved_recipe.clear();data_->spatial.reset();data_->dataset.reset();data_->changed();}
void GeneViewSession::restore_normalization(std::string recipe){
    workflow_require_owner(data_->owner);const auto saved=detail::normalization::parse_recipe(recipe);const auto source=data_->raw_source();if(!source || source->metadata().source_sha256!=saved.source || source->file_sha256()!=saved.file)throw std::invalid_argument("Saved normalization belongs to another raw source");
    if(data_->want_normalized && data_->saved_recipe==recipe)return;if(data_->qc && (!same_groups(data_->qc->groups(),saved.qc_options.groups) || data_->qc->sha256()!=saved.qc))data_->qc.reset();
    data_->qc_options=saved.qc_options;data_->qc_options.work_bytes=std::min(data_->qc_options.work_bytes,data_->work);data_->normalization_options=saved.options;data_->saved_recipe=std::move(recipe);data_->want_normalized=data_->want_qc=true;data_->normalization.reset();data_->spatial.reset();data_->dataset.reset();data_->options.display.transform=GeneDisplayTransform::Linear;data_->changed();
}
bool GeneViewSession::uses_normalized_values()const{workflow_require_owner(data_->owner);return data_->want_normalized;}
const ExpressionNormalizationOptions& GeneViewSession::normalization_options()const{workflow_require_owner(data_->owner);return data_->normalization_options;}
std::shared_ptr<const ExpressionQcResult> GeneViewSession::qc_result()const{workflow_require_owner(data_->owner);return data_->qc;}
std::shared_ptr<const ExpressionNormalization> GeneViewSession::normalization_result()const{workflow_require_owner(data_->owner);return data_->want_normalized?data_->normalization:nullptr;}
void GeneViewSession::refresh(){workflow_require_owner(data_->owner);data_->changed();}
void GeneViewSession::cancel(){workflow_require_owner(data_->owner);data_->changed();data_->pending=false;}
void GeneViewSession::poll() {
    workflow_require_owner(data_->owner);
    if(data_->future.valid() && data_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto ready=data_->future.get();if(data_->running==data_->generation) {
            auto& result=ready.pair;
            if(ready.spatial) {
                ready.spatial->source()->require_current();data_->spatial=std::move(ready.spatial);data_->raw_spatial=std::move(ready.raw);data_->dataset=result.fields[0]->dataset;
                data_->qc=std::move(ready.qc);data_->normalization=std::move(ready.normalization);if(data_->want_normalized)data_->normalization_parent=data_->raw_spatial->source();
                if(data_->options.genes[0].empty())data_->options.genes[0]=result.fields[0]->feature;
                if(data_->options.genes[1].empty())data_->options.genes[1]=result.fields[1]?result.fields[1]->feature:result.fields[0]->feature;
            }
            data_->result=std::make_shared<const GeneDisplayPair>(std::move(result));
        }}
        catch(...) {if(data_->running==data_->generation){data_->error=workflow_failure_message(std::current_exception());data_->qc.reset();data_->normalization.reset();data_->normalization_parent.reset();}}
    }
    if(!data_->future.valid() && data_->pending) {
        data_->pending=false;data_->running=data_->generation;data_->cancelled.store(false);
        try {
            data_->future=std::async(std::launch::async,[this,dataset=data_->dataset,spatial=data_->spatial,raw=data_->raw_spatial,request=data_->spatial_request,options=data_->options,qc=data_->qc,normalization=data_->normalization,normalization_parent=data_->normalization_parent,qc_options=data_->qc_options,normalization_options=data_->normalization_options,want_qc=data_->want_qc,want_normalized=data_->want_normalized,saved_recipe=data_->saved_recipe]() mutable {
                auto work=data_->work;
                if(request) {
                    if(request->payload_bytes()>=work)throw WorkflowCaptureLimit("Spatial request exceeds gene-view budget");
                    work-=request->payload_bytes();
                }
                if(request && !raw)raw=request->resolve(data_->work,&data_->cancelled);
                if(raw){
                    const auto& parent=raw->source();parent->require_current(&data_->cancelled);
                    auto analysis_work=work;if(want_qc || want_normalized){const auto retained=raw->payload_bytes()+(dataset?dataset->payload_bytes():0)+saved_recipe.capacity();if(retained>=analysis_work || analysis_work-retained<8u*1024*1024)throw WorkflowCaptureLimit("QC/derived view leaves no worker workspace");analysis_work-=retained;}
                    if(want_qc && (!qc || qc->source_sha256()!=parent->metadata().source_sha256 || !same_groups(qc->groups(),qc_options.groups))){qc_options.work_bytes=std::min(qc_options.work_bytes,analysis_work);qc=expression_qc(parent,qc_options,&data_->cancelled);}
                    if(want_normalized){
                        if(!normalization || normalization_parent!=parent || normalization->qc()!=qc || normalization->options().target_sum!=normalization_options.target_sum || normalization->options().log1p!=normalization_options.log1p)normalization=std::make_shared<const ExpressionNormalization>(normalize_expression(parent,qc,normalization_options,analysis_work,&data_->cancelled));
                        if(!saved_recipe.empty() && normalization->recipe()!=saved_recipe)throw std::invalid_argument("Saved normalization QC/recipe identity differs");
                        if(!spatial || spatial->source()!=normalization->source())spatial=ExpressionMatrixSpatialSnapshot::create(normalization->source(),raw->observations(),raw->frame(),analysis_work,&data_->cancelled);
                        work=analysis_work;
                    }else spatial=raw;
                }
                if(spatial) {
                    const auto& source=spatial->source();source->require_current(&data_->cancelled);
                    std::vector<std::string> selected;
                    for(unsigned i=0;i<(options.compare?2u:1u);++i) {
                        workflow_checkpoint(&data_->cancelled);
                        if(options.genes[i].empty())options.genes[i]=source->feature_page(std::min<std::uint64_t>(i,source->metadata().features-1),1,work,&data_->cancelled).at(0).id;
                        if(std::find(selected.begin(),selected.end(),options.genes[i])==selected.end())selected.push_back(options.genes[i]);
                    }
                    // bind() discards the projection when either the spatial scope or
                    // runtime source changes. Retain only this exact selected cohort.
                    if(!dataset || dataset->source_id()!=spatial->sha256() || dataset->features()!=selected) {
                        std::vector<std::uint64_t> features;
                        for(const auto& id:selected) {
                            const auto index=source->feature_index(id,&data_->cancelled);
                            if(!index)throw std::invalid_argument("Selected gene ID is absent from backed expression matrix");
                            features.push_back(*index);
                        }
                        dataset=spatial->dataset(features,work,&data_->cancelled);
                    }
                }
                const auto result=gene_display_pair(*dataset,options.genes,options.display,options.compare,options.shared_range,work,&data_->cancelled);
                if(spatial)spatial->source()->require_current(&data_->cancelled);
                return Data::Ready{result,std::move(spatial),std::move(raw),std::move(qc),std::move(normalization)};
            });
        }catch(...) {data_->error=workflow_failure_message(std::current_exception());}
    }
}
void GeneViewSession::wait(){workflow_require_owner(data_->owner);poll();while(data_->future.valid()) {data_->future.wait();poll();}}
bool GeneViewSession::busy() const {workflow_require_owner(data_->owner);return data_->pending || data_->future.valid();}
const GeneViewOptions& GeneViewSession::options() const {workflow_require_owner(data_->owner);return data_->options;}
const std::optional<ExpressionDataset>& GeneViewSession::dataset() const {workflow_require_owner(data_->owner);return data_->dataset;}
const std::optional<ExpressionMatrixSpatialSnapshot>& GeneViewSession::spatial_snapshot() const {workflow_require_owner(data_->owner);return data_->spatial;}
std::shared_ptr<const GeneDisplayPair> GeneViewSession::result() const {workflow_require_owner(data_->owner);return data_->result;}
const std::string& GeneViewSession::error() const {workflow_require_owner(data_->owner);return data_->error;}
std::uint64_t GeneViewSession::generation() const {workflow_require_owner(data_->owner);return data_->generation;}
}
