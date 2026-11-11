#include "annotation_format_registry.hpp"
#include <array>
#include <stdexcept>

namespace qpx::detail {
#ifdef QUPATHXX_HAS_INTERCHANGE
// Existing private factory; keep JSON/parser headers out of the registry API.
InterchangeReport import_geojson_class_context(Document&,const std::string&,const std::map<std::string,ObjectClass>&);
#endif
namespace {
enum class Provider {GeoJson,MeasurementsCsv};
struct Builtin {
    Provider provider;
    const char* id;
    const char* name;
    bool requires_geojson,import_text,import_file,export_text,export_file,clipboard,class_context;
};
constexpr std::array<Builtin,2> builtins{{
    {Provider::GeoJson,"geojson","GeoJSON",true,true,true,true,true,true,true},
    {Provider::MeasurementsCsv,"measurements-csv","Object measurements CSV",false,false,false,true,true,false,false}
}};
const Builtin& builtin(const std::string& id) {
    for(const auto& entry:builtins)if(id==entry.id)return entry;
    throw std::invalid_argument("unknown annotation format: "+id);
}
void import_provider(const std::string& id,bool file=false) {
    const auto& entry=builtin(id);
    if(!(file?entry.import_file:entry.import_text))throw std::invalid_argument("annotation format does not support import: "+id);
}
void clipboard_provider(const std::string& id) {
    if(!builtin(id).clipboard)throw std::invalid_argument("annotation format does not support clipboard: "+id);
}
template<class Options>const Options& export_options(const AnnotationExportOptions& options) {
    if(const auto* selected=std::get_if<Options>(&options))return *selected;
    throw std::invalid_argument("annotation export options do not match provider");
}
}
AnnotationFormatFeatures annotation_format_features() {return {has_interchange()};}
bool known_annotation_format(const std::string& id) noexcept {for(const auto& entry:builtins)if(id==entry.id)return true;return false;}
const std::vector<std::uint64_t>& annotation_empty_selection() noexcept {static const std::vector<std::uint64_t> ids;return ids;}
const std::vector<std::string>& annotation_empty_protected_paths() noexcept {static const std::vector<std::string> paths;return paths;}
std::vector<AnnotationFormatDescriptor> annotation_format_catalog(AnnotationFormatFeatures features) {
    std::vector<AnnotationFormatDescriptor> result;result.reserve(builtins.size());
    for(const auto& entry:builtins) {
        const auto available=!entry.requires_geojson || features.geojson;
        result.push_back({entry.id,entry.name,available,available?"":"GeoJSON requires QUPATHXX_BUILD_INTERCHANGE=ON",
            entry.import_text,entry.import_file,entry.export_text,entry.export_file,entry.clipboard,entry.class_context});
    }
    return result;
}
InterchangeReport import_annotations(Document& document,const AnnotationTextImportRequest& request) {
    import_provider(request.provider_id);
    if(const auto* options=std::get_if<AnnotationGeoJsonImportOptions>(&request.policy))
        return import_geojson(document,request.text.get(),options->assume_pixels,options->allow_losses,options->qupath_ellipse_precision);
    const auto& context=std::get<AnnotationQuPathClassContext>(request.policy);
    if(!builtin(request.provider_id).class_context)throw std::invalid_argument("annotation format does not support class context: "+request.provider_id);
#ifdef QUPATHXX_HAS_INTERCHANGE
    return import_geojson_class_context(document,request.text.get(),context.classes.get());
#else
    (void)context;
    // Preserve the disabled native factory's exception type/message.
    return import_geojson(document,request.text.get(),true,true,false);
#endif
}
InterchangeReport import_annotations(Document& document,const AnnotationFileImportRequest& request) {
    import_provider(request.provider_id,true);const auto& options=request.options;
    return load_geojson(document,request.path,options.assume_pixels,options.allow_losses,options.qupath_ellipse_precision);
}
InterchangeReport import_annotations(Document& document,const AnnotationClipboardImportRequest& request) {
    clipboard_provider(request.provider_id);
    return paste_objects(document,request.text,request.plane,request.allow_losses,request.max_bytes);
}
InterchangeReport export_annotations(const Document& document,const AnnotationTextExportRequest& request) {
    const auto& selected=builtin(request.provider_id);
    if(!selected.export_text)throw std::invalid_argument("annotation format does not support text export: "+request.provider_id);
    if(selected.provider==Provider::GeoJson) {
        const auto& options=export_options<AnnotationGeoJsonExportOptions>(request.options);
        return export_geojson(document,options.ids.get(),options.allow_losses,options.nested_hierarchy);
    }
    const auto& options=export_options<AnnotationMeasurementsCsvOptions>(request.options);
    InterchangeReport report;report.text=measurements_csv(document,options.ids.get(),options.include_intensity);return report;
}
InterchangeReport export_annotations(const Document& document,const AnnotationFileExportRequest& request) {
    const auto& selected=builtin(request.provider_id);
    if(!selected.export_file)throw std::invalid_argument("annotation format does not support file export: "+request.provider_id);
    if(selected.provider==Provider::GeoJson) {
        const auto& options=export_options<AnnotationGeoJsonExportOptions>(request.options);
        return save_geojson_with_policy(document,request.path,request.write_policy,options.ids.get(),options.allow_losses,options.nested_hierarchy,request.protected_paths.get());
    }
    const auto& options=export_options<AnnotationMeasurementsCsvOptions>(request.options);
    save_object_measurements_csv_with_policy(document,request.path,request.write_policy,options.ids.get(),options.include_intensity,request.protected_paths.get());
    return {};
}
InterchangeReport export_annotations(const Document& document,const AnnotationClipboardExportRequest& request) {
    clipboard_provider(request.provider_id);
    return copy_objects(document,request.ids.get(),request.include_descendants,request.allow_losses,request.max_bytes);
}
}
