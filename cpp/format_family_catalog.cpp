#include "format_family_catalog.hpp"
#include "matrix_format_registry.hpp"
#include "annotation_format_registry.hpp"
#include "qupathxx/format_plugins.hpp"
#include <nlohmann/json.hpp>

namespace qpx::detail {
std::string format_family_catalog_json() {
    using Json=nlohmann::json;
    Json matrices=Json::array(),annotations=Json::array();
    for(const auto& value:matrix_format_catalog()) {
        Json row{{"provider_id",value.provider_id},{"display_name",value.display_name},
            {"available",value.available},{"unavailable_reason",value.unavailable_reason},
            {"can_open",value.can_open},{"can_import",value.can_import}};
        if(value.format==MatrixFormat::MatrixMarket)row["can_import_gzip"]=value.can_import_gzip;
        matrices.push_back(std::move(row));
    }
    for(const auto& value:annotation_format_catalog())
        annotations.push_back({{"provider_id",value.provider_id},{"display_name",value.display_name},
            {"available",value.available},{"unavailable_reason",value.unavailable_reason},
            {"import_text",value.import_text},{"import_file",value.import_file},
            {"export_text",value.export_text},{"export_file",value.export_file},
            {"clipboard",value.clipboard},{"class_context",value.class_context}});
    for(const auto& value:format_plugin_infos()) {
        Json row{{"provider_id",value.provider_id},{"display_name",value.display_name},
            {"available",true},{"unavailable_reason",""},{"format_id",value.format_id},
            {"version",value.version},{"contract",value.contract},{"implementation_sha256",value.implementation_sha256}};
        if(value.domain==FormatPluginDomain::Matrix) {
            row["available"]=has_expression_matrix_storage();
            row["unavailable_reason"]=has_expression_matrix_storage()?"":"Native project/SQLite storage is disabled";
            row["can_open"]=false;row["can_import"]=true;matrices.push_back(std::move(row));
        } else {
            row["import_text"]=false;row["import_file"]=true;row["export_text"]=false;row["export_file"]=false;
            row["clipboard"]=false;row["class_context"]=true;annotations.push_back(std::move(row));
        }
    }
    Json result{{"schema","qpx.format-catalog/1"},{"matrix",std::move(matrices)},
        {"annotations",std::move(annotations)}};
    const auto diagnostics=format_plugin_diagnostics();if(!diagnostics.empty())result["plugin_diagnostics"]=diagnostics;
    return result.dump()+"\n";
}
}
