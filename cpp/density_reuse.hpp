#pragma once
#include "qupathxx/spatial_density.hpp"
#include <string>
namespace qpx::detail {
inline bool density_reuse_compatible(const SpatialDensityRequest& request,const SpatialDensityResult& result) {
    if(std::string(result.algorithm())!="qpx.roi-tissue-density/1" || std::string(result.geometry_policy())!="qpx.polygon512-closed-exclusion/1")return false;
    if(request.x_scale()!=result.x_scale() || request.y_scale()!=result.y_scale() ||
        request.provenance()->source.width!=result.provenance()->source.width || request.provenance()->source.height!=result.provenance()->source.height)return false;
    const auto a=request.options(),b=result.options();
    if(a.prefer_nucleus!=b.prefer_nucleus || a.polygonize_curves!=b.polygonize_curves || a.snapshot_bytes<b.snapshot_bytes || a.work_bytes<b.work_bytes ||
        a.max_membership_tests<b.max_membership_tests || a.max_geometry_operations<b.max_geometry_operations || a.max_geometry_vertices<b.max_geometry_vertices)return false;
    const auto same=[](const auto& x,const auto& y){return x.id==y.id && x.revision==y.revision && x.geometry_sha256==y.geometry_sha256;};
    if(request.window_count()!=result.window_count() || request.tissue_count()!=result.tissue_count() || request.exclusion_count()!=result.exclusion_count() || request.cell_count()!=result.cell_count())return false;
    for(std::size_t i=0;i<request.window_count();++i)if(!same(request.window_object(i),result.window_object(i)))return false;
    for(std::size_t i=0;i<request.tissue_count();++i)if(!same(request.tissue_object(i),result.tissue_object(i)))return false;
    for(std::size_t i=0;i<request.exclusion_count();++i)if(!same(request.exclusion_object(i),result.exclusion_object(i)))return false;
    for(std::size_t i=0;i<request.cell_count();++i){const auto& x=request.cell_object(i);const auto& y=result.cell_object(i);
        if(!same(x,y) || x.kind!=y.kind || x.nucleus!=y.nucleus || x.centroid.x!=y.centroid.x || x.centroid.y!=y.centroid.y)return false;}
    return true;
}
}
