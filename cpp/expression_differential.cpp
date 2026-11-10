#include "expression_differential_internal.hpp"
#include <map>
#include <numeric>
#ifdef QUPATHXX_HAS_STATISTICS
#include <boost/math/distributions/students_t.hpp>
#endif

namespace qpx {
bool has_expression_statistics(){
#ifdef QUPATHXX_HAS_STATISTICS
    return true;
#else
    return false;
#endif
}
const char* expression_differential_status(ExpressionDifferentialStatus s){
    switch(s){case ExpressionDifferentialStatus::Tested:return "tested";case ExpressionDifferentialStatus::AllZero:return "all-zero";case ExpressionDifferentialStatus::InsufficientReplicates:return "insufficient-replicates";case ExpressionDifferentialStatus::ZeroVariance:return "zero-variance";case ExpressionDifferentialStatus::TailUnderflow:return "tail-underflow";}
    throw std::invalid_argument("Invalid differential status");
}
struct ExpressionDifferentialResult::Data {
    std::shared_ptr<const ExpressionAggregationResult> parent;
    ExpressionDifferentialOptions options;
    std::vector<ExpressionDifferentialReplicate> replicates;
    std::vector<ExpressionDifferentialGene> genes;
    std::size_t n_a=0,n_b=0,correction=0,peak=0;
    std::string sha256,recipe,id;
};
ExpressionDifferentialResult::ExpressionDifferentialResult(std::shared_ptr<const Data> d):data_(std::move(d)){}
const std::shared_ptr<const ExpressionAggregationResult>& ExpressionDifferentialResult::parent()const{return data_->parent;}
const ExpressionDifferentialOptions& ExpressionDifferentialResult::options()const{return data_->options;}
const std::vector<ExpressionDifferentialReplicate>& ExpressionDifferentialResult::replicates()const{return data_->replicates;}
const std::vector<ExpressionDifferentialGene>& ExpressionDifferentialResult::genes()const{return data_->genes;}
std::size_t ExpressionDifferentialResult::n_a()const{return data_->n_a;}
std::size_t ExpressionDifferentialResult::n_b()const{return data_->n_b;}
std::size_t ExpressionDifferentialResult::correction_features()const{return data_->correction;}
const std::string& ExpressionDifferentialResult::sha256()const{return data_->sha256;}
const std::string& ExpressionDifferentialResult::recipe()const{return data_->recipe;}
const std::string& ExpressionDifferentialResult::id()const{return data_->id;}
std::size_t ExpressionDifferentialResult::peak_work_bytes()const{return data_->peak;}
namespace {
void add(std::uint64_t& n,std::uint64_t value){if(value>UINT64_MAX-n)throw std::overflow_error("Replicate raw count/library overflow");n+=value;}
struct Moments {
    std::size_t n=0;double mean=0,m2=0;
    void add(double x){++n;const auto delta=x-mean;mean+=delta/double(n);m2+=delta*(x-mean);}
    double variance()const{return n>1?std::max(0.0,m2/double(n-1)):0;}
};
}
std::shared_ptr<const ExpressionDifferentialResult> differential_expression(std::shared_ptr<const ExpressionAggregationResult> bulk,const ExpressionDifferentialOptions& o,const std::atomic<bool>* cancelled){
    using namespace detail::differential;
    options(o);workflow_checkpoint(cancelled);
    if(!has_expression_statistics())throw std::runtime_error("Native DE statistics disabled; enable QUPATHXX_BUILD_STATISTICS (pinned standalone Boost.Math)");
    if(!bulk || bulk->kind()!=ExpressionAggregationKind::Pseudobulk || bulk->domain()!=ExpressionValueDomain::Counts || bulk->overlap()!=ExpressionOverlapPolicy::Reject || bulk->features().size()!=bulk->parent()->metadata().features)throw std::invalid_argument("DE requires full-universe disjoint raw-count pseudobulk");
    bulk->parent()->require_current(cancelled);
    auto d=std::make_shared<ExpressionDifferentialResult::Data>();d->parent=std::move(bulk);d->options=o;
    const auto& cohorts=d->parent->cohorts();const auto nf=d->parent->features().size(),nc=cohorts.size();
    std::size_t charged=4u*1024*1024;
    workflow_capture_charge(charged,d->parent->payload_bytes(),o.work_bytes);
    detail::aggregation::charge(charged,nf,sizeof(ExpressionDifferentialGene)+sizeof(std::size_t),o.work_bytes);
    detail::aggregation::charge(charged,nc,8192,o.work_bytes);
    std::map<std::string,std::size_t> indices;std::map<std::string,std::string> samples;
    std::vector<std::size_t> destinations;destinations.reserve(nc);
    for(std::size_t c=0;c<nc;++c){
        workflow_checkpoint(cancelled);const auto& g=cohorts[c];
        if(g.observations.empty() || (g.group!=o.group_a && g.group!=o.group_b))throw std::invalid_argument("DE refuses empty cohorts and groups outside the two explicit groups");
        const auto sample=samples.emplace(g.sample,g.replicate);if(!sample.second && sample.first->second!=g.replicate)throw std::invalid_argument("One biological sample cannot become multiple independent replicates");
        const auto slot=indices.emplace(g.replicate,d->replicates.size());
        if(slot.second){d->replicates.push_back({g.replicate,g.sample,g.group,{},{},0});if(g.group==o.group_a)++d->n_a;else ++d->n_b;}
        auto& replicate=d->replicates[slot.first->second];
        if(replicate.sample!=g.sample || replicate.group!=g.group)throw std::invalid_argument("Replicate has conflicting sample/group; paired/repeated subjects are unsupported");
        replicate.cohorts.push_back(c);add(replicate.observations,g.observations.size());destinations.push_back(slot.first->second);
    }
    if(!d->n_a || !d->n_b)throw std::invalid_argument("DE requires observations in both explicit groups");
    const auto nr=d->replicates.size();detail::aggregation::charge(charged,nf,nr*sizeof(std::uint64_t),o.work_bytes);
    std::vector<std::uint64_t> counts(nf*nr,0);
    for(std::size_t f=0;f<nf;++f){workflow_checkpoint(cancelled);for(std::size_t c=0;c<nc;++c){const auto n=d->parent->counts().at(f*nc+c);add(counts[f*nr+destinations[c]],n);add(d->replicates[destinations[c]].library,n);}}
    for(const auto& r:d->replicates)if(!r.library)throw std::invalid_argument("DE refuses zero-library biological replicates; no implicit sample exclusion");
    d->genes.resize(nf);std::vector<std::size_t> tested;tested.reserve(nf);
    for(std::size_t f=0;f<nf;++f){
        workflow_checkpoint(cancelled);Moments a,b;bool nonzero=false;
        for(std::size_t r=0;r<nr;++r){const auto raw=counts[f*nr+r];nonzero|=raw!=0;const auto value=std::log1p((double(raw)/double(d->replicates[r].library))*o.library_target);if(d->replicates[r].group==o.group_a)a.add(value);else b.add(value);}
        auto& g=d->genes[f];g.mean_a=a.mean;g.mean_b=b.mean;if(a.n>1)g.variance_a=a.variance();if(b.n>1)g.variance_b=b.variance();g.effect=b.mean-a.mean;
        if(a.n<2 || b.n<2){g.status=ExpressionDifferentialStatus::InsufficientReplicates;continue;}
        if(!nonzero){g.status=ExpressionDifferentialStatus::AllZero;continue;}
        const auto va=*g.variance_a/double(a.n),vb=*g.variance_b/double(b.n),se2=va+vb;
        if(!(se2>0)){g.status=ExpressionDifferentialStatus::ZeroVariance;continue;}
        const auto df=se2*se2/(va*va/double(a.n-1)+vb*vb/double(b.n-1));const auto t=g.effect/std::sqrt(se2);
        if(!std::isfinite(df) || !(df>0) || !std::isfinite(t))throw std::runtime_error("Nonfinite Welch statistic");
        double p=0;
#ifdef QUPATHXX_HAS_STATISTICS
        p=2*boost::math::cdf(boost::math::complement(boost::math::students_t_distribution<double>(df),std::abs(t)));
#endif
        if(!std::isfinite(p) || p<0 || p>1)throw std::runtime_error("Invalid native Student-t probability");
        g.t=t;g.degrees_of_freedom=df;g.p=p;g.status=p==0?ExpressionDifferentialStatus::TailUnderflow:ExpressionDifferentialStatus::Tested;tested.push_back(f);
    }
    // Stable total order makes ties deterministic. Reverse cumulative minima
    // implement BH over EVERY testable feature, including any rounded zero tail.
    std::sort(tested.begin(),tested.end(),[&](auto a,auto b){const auto pa=*d->genes[a].p,pb=*d->genes[b].p;return pa<pb || (pa==pb && a<b);});d->correction=tested.size();
    double previous=1;for(std::size_t rank=tested.size();rank>0;--rank){workflow_checkpoint(cancelled);auto& g=d->genes[tested[rank-1]];previous=std::min(previous,*g.p*(double(tested.size())/double(rank)));g.q=previous;}
    Sha256 hash;detail::aggregation::text(hash,method);detail::aggregation::text(hash,backend);detail::aggregation::text(hash,d->parent->id());detail::aggregation::text(hash,d->parent->sha256());detail::aggregation::text(hash,o.group_a);detail::aggregation::text(hash,o.group_b);hash.integer(detail::aggregation::bits(o.library_target));hash.integer(nf);hash.integer(nr);hash.integer(d->correction);
    for(const auto& r:d->replicates){detail::aggregation::text(hash,r.id);detail::aggregation::text(hash,r.sample);detail::aggregation::text(hash,r.group);hash.integer(r.library);hash.integer(r.observations);hash.integer(r.cohorts.size());for(auto c:r.cohorts)hash.integer(c);}
    for(const auto& g:d->genes){workflow_checkpoint(cancelled);hash.integer(static_cast<unsigned>(g.status));for(auto v:{g.mean_a,g.mean_b,g.effect})hash.integer(detail::aggregation::bits(v));for(const auto& v:{g.variance_a,g.variance_b,g.t,g.degrees_of_freedom,g.p,g.q}){hash.integer(v.has_value()?1:0);if(v)hash.integer(detail::aggregation::bits(*v));}}
    d->sha256=hash.finish();d->recipe=encode({o,d->parent->id(),d->parent->sha256(),d->parent->source_sha256(),d->sha256});d->id=d->recipe.substr(d->recipe.size()-64);d->peak=charged;
    d->parent->parent()->require_current(cancelled);workflow_checkpoint(cancelled);
    return std::shared_ptr<const ExpressionDifferentialResult>(new ExpressionDifferentialResult(std::move(d)));
}
}
