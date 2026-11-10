#include "expression_aggregation_internal.hpp"
#include "atomic_export.hpp"
#include <charconv>

namespace qpx {
namespace {
std::string csv(const std::string& s){std::string out="\"";for(auto c:s){out+=c;if(c=='"')out+='"';}return out+'"';}
std::string number(double v){char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),v,std::chars_format::general,17);if(result.ec!=std::errc{})throw std::runtime_error("Aggregation CSV numeric conversion failed");return {buffer,result.ptr};}
void limit(std::size_t n){if(!n || n>1024ull*1024*1024)throw std::invalid_argument("Aggregation CSV limit must be1 byte..1GiB");}
void write(detail::AtomicExportFile& out,const std::string& s){out.write(s.data(),s.size());}
}
std::size_t save_expression_aggregation_csv(const ExpressionAggregationResult& r,const std::string& path,std::size_t bytes,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);limit(bytes);r.parent()->require_current(cancelled);detail::AtomicExportFile out(path,bytes,cancelled);
    write(out,"result_id,result_sha256,kind,source_sha256,value_source_sha256,value_domain,cohort_id,sample_id,replicate_id,group_id,membership_origin,feature_index,feature_id,feature_name,observations,sum,mean\n");
    for(std::size_t f=0;f<r.features().size();++f)for(std::size_t g=0;g<r.cohorts().size();++g){workflow_checkpoint(cancelled);const auto& cohort=r.cohorts()[g];const auto& feature=r.features()[f];const auto slot=f*r.cohorts().size()+g;const auto mean=r.mean(f,g);
        write(out,r.id()+","+r.sha256()+","+(r.kind()==ExpressionAggregationKind::Pseudobulk?"pseudobulk":"regional-summary")+","+r.source_sha256()+","+r.value_source_sha256()+","+csv(r.value_domain())+","+csv(cohort.id)+","+csv(cohort.sample)+","+csv(cohort.replicate)+","+csv(cohort.group)+","+csv(cohort.origin)+","+std::to_string(feature.index)+","+csv(feature.id)+","+csv(feature.name)+","+std::to_string(cohort.observations.size())+","+(r.domain()==ExpressionValueDomain::Counts?std::to_string(r.counts()[slot]):number(r.continuous()[slot]))+","+(mean?number(*mean):std::string{})+"\n");
    }
    r.parent()->require_current(cancelled);workflow_checkpoint(cancelled);out.commit(false);return out.bytes();
}
std::size_t save_expression_membership_csv(const ExpressionAggregationResult& r,const std::string& path,std::size_t bytes,std::size_t work,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);limit(bytes);detail::aggregation::work_limit(work);std::size_t retained=r.payload_bytes()+4096;std::size_t n=0;for(const auto& c:r.cohorts())n+=c.observations.size();detail::aggregation::charge(retained,n,sizeof(std::pair<std::uint64_t,std::size_t>),work);if(work-retained<4u*1024*1024)throw WorkflowCaptureLimit("Membership export leaves no ID query workspace");
    r.parent()->require_current(cancelled);std::vector<std::pair<std::uint64_t,std::size_t>> members;members.reserve(n);for(std::size_t g=0;g<r.cohorts().size();++g)for(auto o:r.cohorts()[g].observations)members.emplace_back(o,g);std::sort(members.begin(),members.end());
    detail::AtomicExportFile out(path,bytes,cancelled);write(out,"result_id,source_sha256,cohort_id,observation_index,observation_id\n");std::vector<std::string> page;std::uint64_t first=0;
    for(const auto& member:members){workflow_checkpoint(cancelled);const auto o=member.first;if(page.empty() || o<first || o-first>=page.size()){first=o;page=r.parent()->observation_page(first,std::size_t(std::min<std::uint64_t>(128,r.parent()->metadata().observations-first)),work-retained,cancelled);if(page.empty())throw std::invalid_argument("Membership observation catalogue truncated");}
        write(out,r.id()+","+r.source_sha256()+","+csv(r.cohorts()[member.second].id)+","+std::to_string(o)+","+csv(page.at(std::size_t(o-first)))+"\n");
    }
    r.parent()->require_current(cancelled);workflow_checkpoint(cancelled);out.commit(false);return out.bytes();
}
}
