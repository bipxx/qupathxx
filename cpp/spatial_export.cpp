#include "qupathxx/spatial_analysis.hpp"
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
void write_csv(const SpatialDistanceResult& result,const std::function<void(const std::string&)>& write,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);
    write("source_id,source_revision,source_kind,source_nucleus,source_x,source_y,target_id,target_revision,target_kind,target_nucleus,target_x,target_y,distance,units,status,algorithm,input_sha256,result_sha256,x_scale,y_scale,prefer_nucleus,exclude_self\n");
    for(const auto& row:result.rows()) {
        detail::export_checkpoint(cancelled);
        std::string line=std::to_string(row.source_id)+","+std::to_string(row.source_revision)+","+std::to_string(unsigned(row.source_kind))+","+(row.source_nucleus?"true":"false")+","+number(row.source_centroid.x)+","+number(row.source_centroid.y)+",";
        if(row.target_id) line+=std::to_string(*row.target_id)+","+std::to_string(*row.target_revision)+","+std::to_string(unsigned(*row.target_kind))+","+(row.target_nucleus?"true":"false")+","+number(row.target_centroid->x)+","+number(row.target_centroid->y)+","+number(*row.distance);
        else line+=",,,,,,";
        const auto options=result.options();
        line+=std::string(",")+(options.units==SpatialDistanceUnits::Pixels?"px":"um")+","+(row.target_id?"matched":"no_target")+","+result.algorithm()+","+result.input_sha256()+","+result.sha256()+","+number(result.x_scale())+","+number(result.y_scale())+","+(options.prefer_nucleus?"true":"false")+","+(options.exclude_self?"true":"false")+"\n";write(line);
    }
    detail::export_checkpoint(cancelled);
}
void limit_check(std::size_t bytes) {if(!bytes || bytes>1024u*1024*1024) throw std::invalid_argument("spatial CSV limit must be 1 byte..1 GiB");}
}
std::string spatial_distance_csv(const SpatialDistanceResult& result,std::size_t limit,const std::atomic<bool>* c) {
    limit_check(limit);std::string out;write_csv(result,[&](const std::string& line) {if(line.size()>limit-out.size()) throw std::length_error("spatial CSV exceeds byte limit");out+=line;},c);return out;
}
std::size_t save_spatial_distance_csv(const SpatialDistanceResult& result,const std::string& path,std::size_t limit,bool replace,const std::atomic<bool>* c,const std::function<void()>& guard) {
    limit_check(limit);detail::AtomicExportFile out(path,limit,c);write_csv(result,[&](const std::string& line) {out.write(line.data(),line.size());},c);if(guard) guard();out.commit(replace);return out.bytes();
}
}
