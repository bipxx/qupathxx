#include "qupathxx/spatial_density.hpp"
#include "atomic_export.hpp"
#include <charconv>
#include <stdexcept>
namespace qpx {
namespace {
std::string number(double value) {
    char text[64];const auto out=std::to_chars(text,text+sizeof(text),value,std::chars_format::general,17);
    if(out.ec!=std::errc{}) throw std::runtime_error("density CSV numeric conversion failed");return {text,out.ptr};
}
void write_csv(const SpatialDensityResult& result,const std::function<void(const std::string&)>& write,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);
    write("window_id,window_revision,count,area_pixels,area_mm2,density_per_mm2,status,units,cell_count,tissue_count,exclusion_count,algorithm,geometry_policy,footprint_sha256,input_sha256,result_sha256,pixel_width_um,pixel_height_um,prefer_nucleus,polygonize_curves\n");
    const auto options=result.options();
    for(const auto& row:result.rows()) {
        detail::export_checkpoint(cancelled);
        std::string line=std::to_string(row.window_id)+","+std::to_string(row.window_revision)+","+std::to_string(row.count)+","+number(row.area_pixels)+","+number(row.area_mm2)+",";
        if(row.density_per_mm2) line+=number(*row.density_per_mm2);
        line+=std::string(",")+(row.density_per_mm2?"valid":"empty_area")+",cells/mm2,"+std::to_string(result.cell_count())+","+std::to_string(result.tissue_count())+","+std::to_string(result.exclusion_count())+","+result.algorithm()+","+result.geometry_policy()+","+row.geometry_sha256+","+result.input_sha256()+","+result.sha256()+","+number(result.x_scale())+","+number(result.y_scale())+","+(options.prefer_nucleus?"true":"false")+","+(options.polygonize_curves?"true":"false")+"\n";
        write(line);
    }
    detail::export_checkpoint(cancelled);
}
void limit_check(std::size_t bytes) {if(!bytes || bytes>1024u*1024*1024) throw std::invalid_argument("density CSV limit must be 1 byte..1 GiB");}
}
std::string spatial_density_csv(const SpatialDensityResult& result,std::size_t limit,const std::atomic<bool>* c) {
    limit_check(limit);std::string out;write_csv(result,[&](const std::string& line) {if(line.size()>limit-out.size()) throw std::length_error("density CSV exceeds byte limit");out+=line;},c);return out;
}
std::size_t save_spatial_density_csv(const SpatialDensityResult& result,const std::string& path,std::size_t limit,bool replace,const std::atomic<bool>* c,const std::function<void()>& guard) {
    limit_check(limit);detail::AtomicExportFile out(path,limit,c);write_csv(result,[&](const std::string& line) {out.write(line.data(),line.size());},c);if(guard) guard();out.commit(replace);return out.bytes();
}
}
