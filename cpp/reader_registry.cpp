#include "reader_registry.hpp"
#include "reader_plugin.hpp"
#include "reader_provenance.hpp"
#include "qupathxx/core.hpp"
#include "ascii_case.hpp"
#include "json_string.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace qpx::detail {
namespace {
using Factory=std::shared_ptr<ImageServer>(*)(const std::string&,const ReaderSelection&,
    const ReaderOpenOptions&,const std::atomic<bool>*);
struct Builtin {
    const char* id;
    const char* name;
    const char* unavailable;
    bool raw;
    bool (*available)(ReaderFeatures);
    Factory open;
};
std::shared_ptr<ImageServer> ppm(const std::string& path,const ReaderSelection& selected,
    const ReaderOpenOptions&,const std::atomic<bool>*) {
    return selected.adapter==ReaderAdapter::MemoryRgb?
        dense_image_server(Image::read_ppm(path)):ppm_image_server(path);
}
std::shared_ptr<ImageServer> slide(const std::string& path,const ReaderSelection&,
    const ReaderOpenOptions&,const std::atomic<bool>*) {return open_slide(path);}
std::shared_ptr<ImageServer> tiff(const std::string& path,const ReaderSelection&,
    const ReaderOpenOptions& options,const std::atomic<bool>* cancelled) {return open_tiff(path,options.tiff,cancelled);}
std::shared_ptr<ImageServer> zarr(const std::string& path,const ReaderSelection&,
    const ReaderOpenOptions& options,const std::atomic<bool>* cancelled) {return open_ome_zarr(path,options.zarr,cancelled);}
// Referenced factory table: static archives cannot discard a provider registration.
const std::array<Builtin,4>& builtins() {
    static const std::array<Builtin,4> table{{
        {"ppm","PPM (P6, uint8 RGB)","",false,[](ReaderFeatures){return true;},ppm},
        {"openslide","OpenSlide","rebuild with QUPATHXX_BUILD_OPENSLIDE=ON and OpenSlide_ROOT pointing to an SDK",false,
            [](ReaderFeatures f){return f.openslide;},slide},
        {"tiff","Quantitative TIFF/OME-TIFF","TIFF reader requires QUPATHXX_BUILD_TIFF=ON",true,
            [](ReaderFeatures f){return f.tiff;},tiff},
        {"ome-zarr","OME-Zarr NGFF0.4/Zarr2","OME-Zarr support was not enabled",true,
            [](ReaderFeatures f){return f.ome_zarr;},zarr}
    }};
    return table;
}
std::string extension(const std::string& path) {
    auto result=std::filesystem::u8path(path).extension().u8string();
    ascii_lower_in_place(result);return result;
}
[[noreturn]] void unknown(ReaderPolicy policy) {
    if(policy==ReaderPolicy::RgbNominal)
        throw std::invalid_argument("Source reader must be auto, ppm, tiff, ome-zarr or openslide");
    if(policy==ReaderPolicy::SpatialImport)
        throw std::invalid_argument("Image reader must be auto, ppm or tiff");
    throw std::invalid_argument("image reader must be auto, ppm, openslide, tiff or ome-zarr");
}
}

ReaderFeatures reader_features() {return {has_openslide(),has_tiff(),has_ome_zarr()};}
bool known_reader_id(const std::string& id) noexcept {
    for(const auto& entry:builtins())if(id==entry.id)return true;
    return false;
}
bool valid_reader_request(const std::string& id) noexcept {
    if(id=="auto" || known_reader_id(id))return true;
    if(!valid_reader_plugin_id(id))return false;
    try {for(const auto& plugin:reader_plugin_catalog().readers)if(plugin->info.provider_id==id)return true;}catch(...){}
    return false;
}
std::vector<ReaderDescriptor> reader_catalog(ReaderFeatures features) {
    std::vector<ReaderDescriptor> result;result.reserve(builtins().size());
    for(const auto& entry:builtins()) {
        const bool available=entry.available(features);
        result.push_back({entry.id,entry.name,available,available?"":entry.unavailable,true,entry.raw});
    }
    for(const auto& plugin:reader_plugin_catalog().readers) {
        const auto& info=plugin->info;
        result.push_back({info.provider_id,info.display_name,true,"",true,bool(info.capabilities&QPX_READER_CAP_RAW),info.version,info.contract,info.module_sha256});
    }
    return result;
}
std::string reader_catalog_json(ReaderFeatures features) {
    std::string result="{\"schema\":\"qpx.reader-catalog/1\",\"readers\":[";bool first=true;
    for(const auto& item:reader_catalog(features)) {
        if(!first)result+=',';first=false;
        result+="{\"provider_id\":"+json_string(item.provider_id)+",\"display_name\":"+json_string(item.display_name)+
            ",\"available\":"+(item.available?"true":"false")+",\"unavailable_reason\":"+json_string(item.unavailable_reason)+
            ",\"display_rgba\":"+(item.display_rgba?"true":"false")+",\"raw_samples\":"+(item.raw_samples?"true":"false")+
            ",\"version\":"+json_string(item.version)+",\"contract\":"+json_string(item.contract)+",\"module_sha256\":"+json_string(item.module_sha256)+"}";
    }
    result+="],\"plugin_diagnostics\":[";first=true;
    for(const auto& message:reader_plugin_catalog().diagnostics){if(!first)result+=',';first=false;result+=json_string(message);}
    return result+"]}\n";
}
ReaderSelection select_reader(const std::string& path,const std::string& requested,
    ReaderPolicy policy,ReaderFeatures features) {
    const bool builtin=requested=="auto" || known_reader_id(requested);
    bool plugin=false;
    if(!builtin && (policy==ReaderPolicy::RegionCli || policy==ReaderPolicy::Project))
        for(const auto& entry:reader_plugin_catalog().readers)if(entry->info.provider_id==requested){plugin=true;break;}
    if((!builtin && !plugin) || (policy==ReaderPolicy::SpatialImport &&
        requested!="auto" && requested!="ppm" && requested!="tiff"))unknown(policy);
    auto id=requested;
    if(id=="auto") {
        const auto ext=extension(path);
        if(policy==ReaderPolicy::SpatialImport) {
            if(ext!=".ppm" && ext!=".tif" && ext!=".tiff")
                throw std::invalid_argument("Spatial import image must use the admitted PPM/TIFF profile");
            id=ext==".ppm"?"ppm":"tiff";
        } else if(policy==ReaderPolicy::RgbNominal) {
            id=ext==".zarr"?"ome-zarr":ext==".ppm"?"ppm":ext==".tif" || ext==".tiff"?"tiff":"openslide";
        } else if(policy==ReaderPolicy::QuPathImport) {
            id=ext==".tif" || ext==".tiff" || is_ome_tiff_path(path)?"tiff":ext==".ppm"?"ppm":"openslide";
        } else {
            id=policy==ReaderPolicy::Project && ext==".zarr"?"ome-zarr":ext==".ppm"?"ppm":
                features.tiff && is_ome_tiff_path(path)?"tiff":"openslide";
        }
    }
    return {id,policy,policy==ReaderPolicy::RgbNominal && id=="ppm"?ReaderAdapter::MemoryRgb:ReaderAdapter::FileObserved};
}
ReaderRoute desktop_add_selection(const std::string& path,const std::string& requested,ReaderFeatures features) {
    auto selected=select_reader(path,requested,ReaderPolicy::Project,features);
    // Legacy GuiRaw applies this override after the desktop chooses its raw route.
    if(requested=="tiff" && extension(path)==".zarr")selected.provider_id="ome-zarr";
    const bool raw=selected.provider_id=="tiff" || selected.provider_id=="ome-zarr";
    return {std::move(selected),raw};
}
ReaderRoute desktop_startup_selection(const std::string& path,const std::string& requested,ReaderFeatures features) {
    auto selected=select_reader(path,requested,ReaderPolicy::Project,features);
    const bool raw=selected.provider_id=="tiff";
    return {std::move(selected),raw};
}
std::shared_ptr<ImageServer> open_reader(const std::string& path,const ReaderSelection& selected,
    const ReaderOpenOptions& options,const std::atomic<bool>* cancelled) {
    if(selected.adapter==ReaderAdapter::MemoryRgb && selected.provider_id!="ppm")
        throw std::invalid_argument("Memory RGB adapter requires ppm");
    const auto& plugins=reader_plugin_catalog();
    for(const auto& entry:builtins())if(selected.provider_id==entry.id)return entry.open(path,selected,options,cancelled);
    if(selected.policy==ReaderPolicy::RegionCli || selected.policy==ReaderPolicy::Project)
        for(const auto& entry:plugins.readers)if(selected.provider_id==entry->info.provider_id)return open_plugin_image_server(path,entry,cancelled,entry->info.default_options);
    unknown(selected.policy);
}
}
