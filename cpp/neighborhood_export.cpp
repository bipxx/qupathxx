#include "qupathxx/spatial_neighborhood.hpp"
#include "atomic_export.hpp"
#include <charconv>
#include <functional>
#include <stdexcept>
namespace qpx {
namespace {
std::string number(double value) {
    char text[64];const auto out=std::to_chars(text,text+sizeof(text),value,std::chars_format::general,17);
    if(out.ec!=std::errc{}) throw std::runtime_error("spatial CSV numeric conversion failed");return {text,out.ptr};
}
void write_csv(const SpatialNeighborhoodResult& result,const std::function<void(const std::string&)>& write,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);
    write("source_id,source_revision,source_kind,source_nucleus,source_x,source_y,count,radius,units,target_count,algorithm,input_sha256,result_sha256,x_scale,y_scale,prefer_nucleus,exclude_self\n");
    const auto options=result.options();
    for(const auto& row:result.rows()) {
        detail::export_checkpoint(cancelled);
        std::string line=std::to_string(row.source_id)+","+std::to_string(row.source_revision)+","+std::to_string(unsigned(row.source_kind))+","+(row.source_nucleus?"true":"false")+","+number(row.source_centroid.x)+","+number(row.source_centroid.y)+","+std::to_string(row.count)+","+number(options.radius)+",";
        line+=(options.units==SpatialDistanceUnits::Pixels?"px":"um");
        line+=","+std::to_string(result.target_count())+","+result.algorithm()+","+result.input_sha256()+","+result.sha256()+","+number(result.x_scale())+","+number(result.y_scale())+","+(options.prefer_nucleus?"true":"false")+","+(options.exclude_self?"true":"false")+"\n";write(line);
    }
    detail::export_checkpoint(cancelled);
}
void limit_check(std::size_t bytes) {if(!bytes || bytes>1024u*1024*1024) throw std::invalid_argument("spatial CSV limit must be 1 byte..1 GiB");}
}
std::string spatial_neighborhood_csv(const SpatialNeighborhoodResult& result,std::size_t limit,const std::atomic<bool>* c) {
    limit_check(limit);std::string out;write_csv(result,[&](const std::string& line) {if(line.size()>limit-out.size()) throw std::length_error("spatial CSV exceeds byte limit");out+=line;},c);return out;
}
std::size_t save_spatial_neighborhood_csv(const SpatialNeighborhoodResult& result,const std::string& path,std::size_t limit,bool replace,const std::atomic<bool>* c,const std::function<void()>& guard) {
    limit_check(limit);detail::AtomicExportFile out(path,limit,c);write_csv(result,[&](const std::string& line) {out.write(line.data(),line.size());},c);if(guard) guard();out.commit(replace);return out.bytes();
}
}
