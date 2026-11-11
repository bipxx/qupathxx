#include "../format_family_catalog.hpp"
#include "../reader_plugin.hpp"
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/h5ad.hpp"
#include "qupathxx/interchange.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}
int main() {
    using namespace qpx;
    using namespace qpx::detail;
    require(cached_reader_plugin_catalog()==nullptr,"Format discovery accessed image plugins");
    const auto bytes=format_family_catalog_json();
    const auto report=nlohmann::json::parse(bytes);
    require(report.at("schema")=="qpx.format-catalog/1","Wrong format catalog schema");
    const auto& matrix=report.at("matrix");const auto& annotations=report.at("annotations");
    require(matrix.size()==3 && annotations.size()==2,"Wrong family provider membership");
    require(matrix[0].at("provider_id")=="qmx" && matrix[0].at("can_open")==true && matrix[0].at("can_import")==false,
        "Indexed matrix open contract changed");
    require(matrix[1].at("provider_id")=="matrix-market" && matrix[1].at("can_open")==false && matrix[1].at("can_import")==true,
        "Matrix Market import contract changed");
    require(matrix[2].at("provider_id")=="h5ad" && matrix[2].at("can_open")==false && matrix[2].at("can_import")==true,
        "H5AD indexed-import contract changed");
    require(matrix[0].at("available")==has_expression_matrix_storage() && matrix[1].at("available")==has_expression_matrix_storage() &&
        matrix[2].at("available")==bool(has_expression_matrix_storage() && has_h5ad()),"Matrix feature availability is misleading");
    require(annotations[0].at("provider_id")=="geojson" && annotations[0].at("available")==has_interchange() &&
        annotations[0].at("import_text")==true && annotations[0].at("clipboard")==true && annotations[0].at("class_context")==true,
        "GeoJSON capabilities changed");
    require(annotations[1].at("provider_id")=="measurements-csv" && annotations[1].at("available")==true &&
        annotations[1].at("import_text")==false && annotations[1].at("import_file")==false &&
        annotations[1].at("export_text")==true && annotations[1].at("export_file")==true && annotations[1].at("clipboard")==false,
        "Object CSV must remain export-only and independent of GeoJSON");
    for(const auto* family:{&matrix,&annotations})for(const auto& provider:*family)
        require(provider.at("unavailable_reason").get<std::string>().empty()==provider.at("available").get<bool>(),
            "Availability must have an honest reason");
    require(cached_reader_plugin_catalog()==nullptr,"Family discovery froze image plugin state");
    qpx::detail::configure_reader_plugins(std::filesystem::absolute("not-opened-format-catalog-manifest.json").u8string());
    require(format_family_catalog_json()==bytes && cached_reader_plugin_catalog()==nullptr,
        "Family discovery loaded or froze an unrelated configured image plugin");
    std::cout<<"Typed family capabilities and image-plugin-independent discovery passed\n";
}
