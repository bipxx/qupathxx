#include "qupathxx/h5ad.hpp"
#include "expression_matrix_writer.hpp"
#include <chrono>
namespace qpx {
ExpressionMatrixImportResult import_h5ad_matrix(const std::string& input,const std::string& store,H5adOptions options,const ExpressionMatrixImportOptions& limits,const std::atomic<bool>* cancelled){
#ifdef QUPATHXX_HAS_PROJECT
    limits.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto destination=detail::export_path(store);if(std::filesystem::exists(std::filesystem::u8path(destination)))throw WorkflowOutputExists("Expression sidecar output already exists");
    options.work_bytes=std::min(options.work_bytes,limits.work_bytes);options.chunk_bytes=std::min(options.chunk_bytes,options.work_bytes/4);options.file_bytes=std::min(options.file_bytes,limits.file_bytes);options.max_features=std::min(options.max_features,limits.max_features);options.max_observations=std::min(options.max_observations,limits.max_observations);options.max_entries=std::min(options.max_entries,limits.max_entries);
    const auto matrix_key=options.matrix,spatial_key=options.spatial;H5adReader reader(input,std::move(options),cancelled);const auto& h=reader.metadata();ExpressionMatrixMetadata m;m.domain=h.domain;m.value_domain=h.value_domain;m.input_profile="h5ad-"+h.encoding+"/1";m.features=h.features.size();m.observations=h.observations.size();m.entries=h.entries;m.spatial_frame=h.frame;m.input_matrix=matrix_key;m.input_spatial=spatial_key;
    detail::matrix::Writer writer(destination,m,limits,reader.stream_work_bytes(),cancelled);
    for(std::size_t i=0;i<h.features.size();++i)writer.feature_row(i,h.features[i],h.features[i],"Gene Expression");
    Sha256 axis;detail::matrix::text(axis,"qpx.h5ad-observation-axis/1");axis.integer(h.observations.size());
    for(std::size_t i=0;i<h.observations.size();++i){writer.observation_row(i,h.observations[i],h.positions[i]);detail::matrix::text(axis,h.observations[i]);}
    reader.stream([&](const H5adEntry& e){if(h.domain==ExpressionValueDomain::Counts)writer.count(e.feature,e.observation,e.count);else writer.continuous(e.feature,e.observation,e.continuous);},cancelled);
    auto result=writer.finish({h.input_sha256,h.source_sha256,axis.finish()},std::filesystem::file_size(std::filesystem::u8path(input)),[&]{reader.require_current(cancelled);});result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
#else
    throw std::runtime_error("Indexed H5AD import requires native project/SQLite support");
#endif
}
}
