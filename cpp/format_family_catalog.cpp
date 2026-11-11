#include "format_family_catalog.hpp"
#include "matrix_format_registry.hpp"
#include "annotation_format_registry.hpp"
#include <nlohmann/json.hpp>

namespace qpx::detail {
std::string format_family_catalog_json() {
    using Json=nlohmann::json;
    Json matrices=Json::array(),annotations=Json::array();
    for(const auto& value:matrix_format_catalog())
        matrices.push_back({{"provider_id",value.provider_id},{"display_name",value.display_name},
            {"available",value.available},{"unavailable_reason",value.unavailable_reason},
            {"can_open",value.can_open},{"can_import",value.can_import}});
    for(const auto& value:annotation_format_catalog())
        annotations.push_back({{"provider_id",value.provider_id},{"display_name",value.display_name},
            {"available",value.available},{"unavailable_reason",value.unavailable_reason},
            {"import_text",value.import_text},{"import_file",value.import_file},
            {"export_text",value.export_text},{"export_file",value.export_file},
            {"clipboard",value.clipboard},{"class_context",value.class_context}});
    return Json{{"schema","qpx.format-catalog/1"},{"matrix",std::move(matrices)},
        {"annotations",std::move(annotations)}}.dump()+"\n";
}
}
