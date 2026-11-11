#include "expression_differential_internal.hpp"
#include "atomic_export.hpp"
#include <charconv>
namespace qpx {
namespace {
std::string csv(const std::string& s){std::string out="\"";for(auto c:s){out+=c;if(c=='"')out+='"';}return out+'"';}
std::string number(double v){char buffer[64];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),v,std::chars_format::general,17);if(r.ec!=std::errc{} || !std::isfinite(v))throw std::runtime_error("DE CSV numeric conversion failed");return {buffer,r.ptr};}
std::string number(const std::optional<double>& v){return v?number(*v):std::string{};}
void limit(std::size_t n){if(!n || n>1024ull*1024*1024)throw std::invalid_argument("DE CSV limit must be1 byte..1GiB");}
void write(detail::AtomicExportFile& out,const std::string& s){out.write(s.data(),s.size());}
std::string identity(const ExpressionDifferentialResult& r){return r.id()+","+r.sha256()+","+r.parent()->id()+","+r.parent()->source_sha256()+","+detail::differential::method+","+detail::differential::backend+","+csv(r.options().group_a)+","+csv(r.options().group_b)+","+number(r.options().library_target)+","+std::to_string(r.parent()->features().size())+","+std::to_string(r.correction_features())+","+std::to_string(r.n_a())+","+std::to_string(r.n_b())+",";}
constexpr const char* header="result_id,result_sha256,pseudobulk_id,source_sha256,method,backend,group_a,group_b,library_target,source_features,BH_testable_features,n_a,n_b,";
}
std::size_t save_expression_differential_csv(const ExpressionDifferentialResult& r,const std::string& path,std::size_t bytes,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);limit(bytes);r.parent()->parent()->require_current(cancelled);detail::AtomicExportFile out(path,bytes,cancelled);write(out,std::string(header)+"feature_index,feature_id,feature_name,mean_log1p_a,mean_log1p_b,sample_variance_a,sample_variance_b,effect_mean_log1p_b_minus_a,welch_t,welch_df,p_two_sided,q_BH,status\n");const auto prefix=identity(r);
    for(std::size_t f=0;f<r.genes().size();++f){workflow_checkpoint(cancelled);const auto& feature=r.parent()->features()[f];const auto& g=r.genes()[f];write(out,prefix+std::to_string(feature.index)+","+csv(feature.id)+","+csv(feature.name)+","+number(g.mean_a)+","+number(g.mean_b)+","+number(g.variance_a)+","+number(g.variance_b)+","+number(g.effect)+","+number(g.t)+","+number(g.degrees_of_freedom)+","+number(g.p)+","+number(g.q)+","+expression_differential_status(g.status)+"\n");}
    r.parent()->parent()->require_current(cancelled);workflow_checkpoint(cancelled);out.commit(false);return out.bytes();
}
std::size_t save_expression_differential_replicates_csv(const ExpressionDifferentialResult& r,const std::string& path,std::size_t bytes,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);limit(bytes);r.parent()->parent()->require_current(cancelled);detail::AtomicExportFile out(path,bytes,cancelled);write(out,std::string(header)+"replicate_id,sample_id,group_id,raw_library_total,observations,cohort_id\n");const auto prefix=identity(r);
    for(const auto& replicate:r.replicates())for(auto c:replicate.cohorts){workflow_checkpoint(cancelled);write(out,prefix+csv(replicate.id)+","+csv(replicate.sample)+","+csv(replicate.group)+","+std::to_string(replicate.library)+","+std::to_string(replicate.observations)+","+csv(r.parent()->cohorts().at(c).id)+"\n");}
    r.parent()->parent()->require_current(cancelled);workflow_checkpoint(cancelled);out.commit(false);return out.bytes();
}
}
