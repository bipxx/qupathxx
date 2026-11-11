#include "expression_aggregation_internal.hpp"
#include "geometry_archive.hpp"
#include "qupathxx/spatial_index.hpp"
#include <map>
#include <set>
#include <numeric>

namespace qpx {
using namespace detail::aggregation;
namespace {
void policy(ExpressionOverlapPolicy p){if(p!=ExpressionOverlapPolicy::Independent && p!=ExpressionOverlapPolicy::Reject)throw std::invalid_argument("Unknown expression overlap policy");}
void add(std::uint64_t& sum,std::uint64_t value){if(value>std::numeric_limits<std::uint64_t>::max()-sum)throw std::overflow_error("Expression aggregation exact uint64 sum overflow");sum+=value;}
}
namespace detail::aggregation {
void validate(const ExpressionMatrixMetadata& m,const std::string& file,const std::vector<ExpressionCohort>& cohorts,const ExpressionAggregationOptions& o){
    work_limit(o.work_bytes);policy(o.overlap);digest(m.source_sha256);digest(file);
    label(m.value_domain,true);
    if(!m.features || m.features>1000000 || !m.observations || m.observations>1000000 || cohorts.empty() || cohorts.size()>64)throw std::invalid_argument("Aggregation requires1..64 cohorts and source axes within1..1000000");
    if(m.domain!=ExpressionValueDomain::Counts && m.domain!=ExpressionValueDomain::Continuous)throw std::invalid_argument("Unknown aggregation value domain");
    if(o.kind!=ExpressionAggregationKind::RegionalSummary && o.kind!=ExpressionAggregationKind::Pseudobulk)throw std::invalid_argument("Unknown expression aggregation kind");
    if(o.normalization_recipe.size()>recipe_limit)throw WorkflowCaptureLimit("Aggregation normalization recipe exceeds bound");
    if(o.features.size()>m.features || (!o.features.empty() && (o.features.back()>=m.features || !std::is_sorted(o.features.begin(),o.features.end()) || std::adjacent_find(o.features.begin(),o.features.end())!=o.features.end())))throw std::invalid_argument("Aggregation features must be ascending unique source indices");
    const bool bulk=o.kind==ExpressionAggregationKind::Pseudobulk;
    if(bulk && (m.domain!=ExpressionValueDomain::Counts || m.value_domain!="raw-counts" || !o.features.empty() || !o.normalization_recipe.empty() || o.overlap!=ExpressionOverlapPolicy::Reject))throw std::invalid_argument("Pseudobulk requires the full raw-count universe and disjoint cohorts");
    std::set<std::string> names;std::map<std::string,std::pair<std::string,std::string>> replicates;
    std::size_t members=0;for(const auto& c:cohorts){
        label(c.id,true);label(c.sample,bulk);label(c.replicate,bulk);label(c.group,bulk);bounded_utf8(c.origin,2048);
        if(c.origin.empty() || !names.insert(c.id).second || c.source_sha256!=m.source_sha256)throw std::invalid_argument("Cohort identity/source differs or is duplicated");
        if(c.observations.size()>65536-members)throw WorkflowCaptureLimit("Aggregation memberships exceed65536");members+=c.observations.size();
        if(!c.observations.empty() && (c.observations.back()>=m.observations || !std::is_sorted(c.observations.begin(),c.observations.end()) || std::adjacent_find(c.observations.begin(),c.observations.end())!=c.observations.end()))throw std::invalid_argument("Cohort observations must be ascending unique source indices");
        if(bulk){auto association=std::make_pair(c.sample,c.group);const auto r=replicates.emplace(c.replicate,association);if(!r.second && r.first->second!=association)throw std::invalid_argument("One replicate has inconsistent sample/group metadata");}
    }
    if(o.overlap==ExpressionOverlapPolicy::Reject){std::set<std::uint64_t> seen;for(const auto& c:cohorts)for(auto i:c.observations)if(!seen.insert(i).second)throw std::invalid_argument("Overlapping expression cohorts refused by explicit policy");}
}
}

ExpressionCohort expression_observation_cohort(std::shared_ptr<const ExpressionMatrixSource> source,std::string id,const std::vector<std::string>& ids,std::string sample,std::string replicate,std::string group,std::size_t work,const std::atomic<bool>* cancelled){
    work_limit(work);workflow_checkpoint(cancelled);if(!source)throw std::invalid_argument("Cohort requires a native expression source");
    label(id,true);label(sample);label(replicate);label(group);if(ids.size()>65536)throw WorkflowCaptureLimit("Cohort exceeds65536 observations");
    std::size_t live=source->payload_bytes();charge(live,ids.capacity(),sizeof(std::string)+16,work);for(const auto& s:ids){label(s,true);workflow_capture_charge(live,s.capacity(),work);}
    if(work-live<4u*1024*1024)throw WorkflowCaptureLimit("Cohort join leaves no read workspace");
    auto indices=source->observation_indices(ids,work-live,cancelled);if(indices.size()!=ids.size())throw std::invalid_argument("Observation ID join shape differs");
    std::sort(indices.begin(),indices.end());if(std::adjacent_find(indices.begin(),indices.end())!=indices.end() || (!indices.empty() && indices.back()>=source->metadata().observations))throw std::invalid_argument("Duplicate/out-of-range observation IDs");
    source->require_current(cancelled);return {std::move(id),std::move(sample),std::move(replicate),std::move(group),std::move(indices),source->metadata().source_sha256,"explicit-observation-ids/1"};
}

std::vector<ExpressionCohort> expression_region_cohorts(const ExpressionMatrixSpatialSnapshot& spatial,const std::vector<ExpressionRegion>& regions,const CoordinateFrame& target,ExpressionOverlapPolicy overlap,const std::optional<RegistrationDefinition>& registration,std::size_t work,std::uint64_t max_tests,const std::atomic<bool>* cancelled){
    work_limit(work);policy(overlap);workflow_checkpoint(cancelled);target.validate();const auto& source=spatial.source();
    if(regions.empty() || regions.size()>64 || !max_tests || max_tests>1000000000)throw std::invalid_argument("Region selection requires1..64 regions and1..1000000000 membership tests");
    if(registration){registration->validate();if(registration->source.sha256()!=spatial.dataset_frame().sha256() || registration->target.sha256()!=target.sha256())throw WorkflowInputChanged("Region registration source/target frame differs");}
    else if(target.sha256()!=spatial.frame().sha256())throw std::invalid_argument("Source-coordinate regions require identical explicit coordinate frames");
    const auto& rows=spatial.observations();std::size_t live=spatial.payload_bytes();charge(live,rows.size(),sizeof(std::string)+32+SpatialIndex::estimated_bytes(1),work);
    for(const auto& r:rows)workflow_capture_charge(live,r.id.capacity(),work);
    std::set<std::string> names;for(const auto& region:regions){label(region.id,true);label(region.sample);label(region.replicate);label(region.group);if(!names.insert(region.id).second || !region.geometry.is_area())throw std::invalid_argument("Regions require unique IDs and area geometry");workflow_capture_charge(live,region.geometry.payload_bytes()*3+4096,work);std::size_t vertices=0;for(const auto& path:region.geometry.paths()){if(path.size()>10000-vertices)throw WorkflowCaptureLimit("Region exceeds10000 geometry vertices");vertices+=path.size();}}
    if(work-live<4u*1024*1024)throw WorkflowCaptureLimit("Region join leaves no read workspace");
    std::vector<std::string> ids;ids.reserve(rows.size());std::vector<SpatialEntry> entries;entries.reserve(rows.size());std::vector<Point> positions;positions.reserve(rows.size());
    for(std::size_t i=0;i<rows.size();++i){workflow_checkpoint(cancelled);ids.push_back(rows[i].id);const auto p=registration?registration->transform.map(rows[i].position):rows[i].position;positions.push_back(p);entries.push_back({std::uint64_t(i),{p.x,p.y,0,0}});}
    const auto indices=source->observation_indices(ids,work-live,cancelled);if(indices.size()!=rows.size())throw std::invalid_argument("Region observation join shape differs");
    SpatialIndex index(std::move(entries));std::vector<std::uint8_t> seen(rows.size());std::vector<ExpressionCohort> result;result.reserve(regions.size());
    std::uint64_t tests=0;std::size_t members=0;
    for(const auto& r:regions){
        workflow_checkpoint(cancelled);const auto geometry=GeometryArchiveCodec::encode(r.geometry);Sha256 h;h.update(geometry.data(),geometry.size());
        const auto origin=std::string("spot-centre-roi-contains/1;spatial=")+spatial.sha256()+";frame="+target.sha256()+";coordinates="+(registration?"registered;registration="+registration->sha256():"source")+";geometry="+h.finish()+";overlap="+(overlap==ExpressionOverlapPolicy::Reject?"reject":"independent");
        ExpressionCohort cohort{r.id,r.sample,r.replicate,r.group,{},source->metadata().source_sha256,origin};
        const auto candidates=index.query(r.geometry.bounds());
        for(auto row:candidates){workflow_checkpoint(cancelled);if(tests++==max_tests)throw WorkflowCaptureLimit("Expression region membership test budget exceeded");if(!r.geometry.contains(positions.at(row)))continue;if(overlap==ExpressionOverlapPolicy::Reject && seen.at(row))throw std::invalid_argument("Observation belongs to overlapping expression regions");seen.at(row)=1;if(members==65536)throw WorkflowCaptureLimit("Expression region memberships exceed65536");++members;workflow_capture_charge(live,16,work);cohort.observations.push_back(indices.at(row));}
        std::sort(cohort.observations.begin(),cohort.observations.end());result.push_back(std::move(cohort));
    }
    source->require_current(cancelled);return result;
}

struct ExpressionAggregationResult::Data {
    std::shared_ptr<const ExpressionMatrixSource> parent;
    std::vector<ExpressionCohort> cohorts;
    std::vector<ExpressionFeature> features;
    std::vector<std::uint64_t> counts;
    std::vector<double> continuous;
    ExpressionAggregationOptions options;
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::string value_domain,value_source,sha,recipe,id;
    std::size_t bytes=0,peak=0,batches=0;
};
ExpressionAggregationResult::ExpressionAggregationResult(std::shared_ptr<const Data> d):data_(std::move(d)){}
ExpressionAggregationKind ExpressionAggregationResult::kind()const{return data_->options.kind;}
ExpressionOverlapPolicy ExpressionAggregationResult::overlap()const{return data_->options.overlap;}
const std::vector<ExpressionCohort>& ExpressionAggregationResult::cohorts()const{return data_->cohorts;}
const std::vector<ExpressionFeature>& ExpressionAggregationResult::features()const{return data_->features;}
ExpressionValueDomain ExpressionAggregationResult::domain()const{return data_->domain;}
const std::string& ExpressionAggregationResult::value_domain()const{return data_->value_domain;}
const std::string& ExpressionAggregationResult::source_sha256()const{return data_->parent->metadata().source_sha256;}
const std::string& ExpressionAggregationResult::file_sha256()const{return data_->parent->file_sha256();}
const std::string& ExpressionAggregationResult::value_source_sha256()const{return data_->value_source;}
const std::vector<std::uint64_t>& ExpressionAggregationResult::counts()const{return data_->counts;}
const std::vector<double>& ExpressionAggregationResult::continuous()const{return data_->continuous;}
std::optional<double> ExpressionAggregationResult::mean(std::size_t f,std::size_t c)const{if(f>=features().size() || c>=cohorts().size())throw std::out_of_range("Aggregation cell exceeds shape");const auto n=cohorts()[c].observations.size();if(!n)return {};const auto i=f*cohorts().size()+c;return (domain()==ExpressionValueDomain::Counts?double(counts()[i]):continuous()[i])/double(n);}
const std::string& ExpressionAggregationResult::sha256()const{return data_->sha;}
const std::string& ExpressionAggregationResult::recipe()const{return data_->recipe;}
const std::string& ExpressionAggregationResult::id()const{return data_->id;}
std::size_t ExpressionAggregationResult::payload_bytes()const{return data_->bytes;}
std::size_t ExpressionAggregationResult::peak_work_bytes()const{return data_->peak;}
std::size_t ExpressionAggregationResult::batches()const{return data_->batches;}
const std::shared_ptr<const ExpressionMatrixSource>& ExpressionAggregationResult::parent()const{return data_->parent;}

std::shared_ptr<const ExpressionAggregationResult> aggregate_expression(std::shared_ptr<const ExpressionMatrixSource> parent,const std::vector<ExpressionCohort>& cohorts,const ExpressionAggregationOptions& options,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);if(!parent)throw std::invalid_argument("Aggregation requires a native expression source");
    // Reserve validation trees, request copies, recipe encoding and output before
    // copying external vectors or allocating the feature-by-cohort result.
    const auto work=options.work_bytes;work_limit(work);std::size_t fixed=sizeof(ExpressionAggregationResult)+sizeof(ExpressionAggregationResult::Data)+parent->payload_bytes()+8u*recipe_limit;
    charge(fixed,cohort_bytes(cohorts),4,work);charge(fixed,options.features.capacity(),32,work);charge(fixed,options.normalization_recipe.capacity(),4,work);
    validate(parent->metadata(),parent->file_sha256(),cohorts,options);parent->require_current(cancelled);
    const auto& m=parent->metadata();const auto n=options.features.empty()?std::size_t(m.features):options.features.size();charge(fixed,n,1024+8*cohorts.size(),work);
    if(work-fixed<8u*1024*1024)throw WorkflowCaptureLimit("Aggregation output leaves no bounded sparse read workspace");
    std::shared_ptr<const ExpressionMatrixSource> values=parent;
    if(!options.normalization_recipe.empty()){auto normalized=replay_expression_normalization(parent,options.normalization_recipe,work-fixed,cancelled);values=normalized.source();workflow_capture_charge(fixed,values->payload_bytes(),work);}
    if(work-fixed<8u*1024*1024)throw WorkflowCaptureLimit("Aggregation derived source leaves no read workspace");
    auto d=std::make_shared<ExpressionAggregationResult::Data>();d->parent=parent;d->cohorts=cohorts;d->options=options;d->domain=values->metadata().domain;d->value_domain=values->metadata().value_domain;d->value_source=values->metadata().source_sha256;d->features.reserve(n);
    if(d->domain==ExpressionValueDomain::Counts)d->counts.resize(n*cohorts.size());else d->continuous.resize(n*cohorts.size());
    // Sparse observation-to-cohort associations: at most65536 memberships. No
    // dense observation-by-gene matrix or observation-by-cohort allocation.
    std::vector<std::pair<std::uint64_t,std::size_t>> members;std::size_t membership_count=0;for(const auto& c:cohorts)membership_count+=c.observations.size();members.reserve(membership_count);for(std::size_t g=0;g<cohorts.size();++g)for(auto o:cohorts[g].observations)members.emplace_back(o,g);std::sort(members.begin(),members.end());
    const auto available=work-fixed;const auto batch=std::max<std::size_t>(1,std::min<std::size_t>(128,(available/8)/(std::size_t(m.observations)*32)));
    const auto read_work=available-batch*2048-8192;d->peak=fixed+read_work+batch*2048+8192;
    for(std::size_t first=0;first<n;first+=batch){
        workflow_checkpoint(cancelled);ExpressionMatrixSelection selection;const auto count=std::min(batch,n-first);selection.features.reserve(count);for(std::size_t k=0;k<count;++k)selection.features.push_back(options.features.empty()?first+k:options.features[first+k]);
        // Catalog metadata is retained and bounded by the preflight reservation.
        if(options.features.empty()){auto page=values->feature_page(first,count,read_work,cancelled);if(page.size()!=count)throw std::invalid_argument("Aggregation feature catalogue truncated");for(std::size_t k=0;k<count;++k){if(page[k].index!=selection.features[k])throw std::invalid_argument("Aggregation feature axis differs");d->features.push_back(std::move(page[k]));}}
        else for(auto f:selection.features){auto page=values->feature_page(f,1,read_work,cancelled);if(page.size()!=1 || page[0].index!=f)throw std::invalid_argument("Aggregation selected feature axis differs");d->features.push_back(std::move(page[0]));}
        const auto block=values->read(selection,read_work,cancelled);++d->batches;const auto entries=block.observation_indices.size();
        if(block.source_sha256!=d->value_source || block.domain!=d->domain || block.features!=selection.features || block.offsets.size()!=count+1 || block.offsets.front()!=0 || block.offsets.back()!=entries || !std::is_sorted(block.offsets.begin(),block.offsets.end()) || (d->domain==ExpressionValueDomain::Counts?(block.counts.size()!=entries || !block.continuous.empty()):(block.continuous.size()!=entries || !block.counts.empty())))throw std::invalid_argument("Aggregation sparse block shape/source/domain differs");
        for(std::size_t k=0;k<count;++k){
            if(block.offsets[k]>block.offsets[k+1])throw std::invalid_argument("Aggregation sparse offsets decrease");std::optional<std::uint64_t> previous;
            for(auto i=block.offsets[k];i<block.offsets[k+1];++i){
                if((i&4095)==0)workflow_checkpoint(cancelled);const auto o=block.observation_indices[i];if(o>=m.observations || (previous && o<=*previous))throw std::invalid_argument("Aggregation sparse observation axis differs");previous=o;
                if(d->domain==ExpressionValueDomain::Continuous && !std::isfinite(block.continuous[i]))throw std::invalid_argument("Aggregation source value is nonfinite");
                auto member=std::lower_bound(members.begin(),members.end(),std::make_pair(o,std::size_t(0)));for(;member!=members.end() && member->first==o;++member){const auto slot=(first+k)*cohorts.size()+member->second;if(d->domain==ExpressionValueDomain::Counts)add(d->counts[slot],block.counts[i]);else {auto& sum=d->continuous[slot];sum+=block.continuous[i];if(!std::isfinite(sum))throw std::overflow_error("Continuous expression aggregation sum overflow");}}
            }
        }
    }
    values->require_current(cancelled);Recipe recipe;recipe.source=m.source_sha256;recipe.file=parent->file_sha256();recipe.features=m.features;recipe.observations=m.observations;recipe.cohorts=cohorts;recipe.options=options;recipe.result=std::string(64,'0');
    const auto request=encode(recipe);Sha256 h;text(h,"qpx.expression-aggregation-result/1");text(h,request);text(h,d->value_source);text(h,d->value_domain);h.integer(std::uint64_t(d->domain));for(const auto& f:d->features){workflow_checkpoint(cancelled);h.integer(f.index);text(h,f.id);text(h,f.name);text(h,f.type);text(h,f.sha256);}for(std::size_t i=0;i<d->counts.size();++i){if((i&4095)==0)workflow_checkpoint(cancelled);h.integer(d->counts[i]);}for(std::size_t i=0;i<d->continuous.size();++i){if((i&4095)==0)workflow_checkpoint(cancelled);h.integer(bits(d->continuous[i]));}d->sha=h.finish();recipe.result=d->sha;d->recipe=encode(recipe);d->id=d->recipe.substr(d->recipe.size()-64);
    d->bytes=sizeof(ExpressionAggregationResult)+sizeof(*d)+parent->payload_bytes()+cohort_bytes(d->cohorts)+d->features.capacity()*sizeof(ExpressionFeature)+(d->counts.capacity()+d->continuous.capacity()+d->options.features.capacity())*8+d->options.normalization_recipe.capacity()+d->recipe.capacity()+d->id.capacity()+d->sha.capacity()+d->value_source.capacity()+d->value_domain.capacity();for(const auto& f:d->features)d->bytes+=f.id.capacity()+f.name.capacity()+f.type.capacity()+f.sha256.capacity();if(d->bytes>fixed)throw WorkflowCaptureLimit("Aggregation actual result capacity exceeds reservation");
    parent->require_current(cancelled);return std::shared_ptr<const ExpressionAggregationResult>(new ExpressionAggregationResult(std::move(d)));
}
}
