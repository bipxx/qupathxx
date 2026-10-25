#pragma once
#include "qupathxx/core.hpp"

namespace qpx {
struct InterchangeIssue {
    std::string code,message;
    std::size_t count=0;
};
struct InterchangeReport {
    std::string text;
    std::vector<InterchangeIssue> issues;
    std::vector<std::uint64_t> imported_ids;
    std::vector<std::string> source_ids;
    std::size_t features=0;
};
bool has_interchange();
InterchangeReport export_geojson(const Document&,const std::vector<std::uint64_t>& ids={},bool allow_losses=false,bool nested_hierarchy=false);
InterchangeReport import_geojson(Document&,const std::string& json,bool assume_pixels=false,bool allow_losses=false,bool qupath_ellipse_precision=false);
InterchangeReport save_geojson(const Document&,const std::string& path,const std::vector<std::uint64_t>& ids={},bool allow_losses=false,bool nested_hierarchy=false);
InterchangeReport load_geojson(Document&,const std::string& path,bool assume_pixels=false,bool allow_losses=false,bool qupath_ellipse_precision=false);
std::string measurements_csv(const Document&,const std::vector<std::uint64_t>& ids={},bool include_intensity=false);
void save_measurements_csv(const Document&,const std::string& path,const std::vector<std::uint64_t>& ids={},bool include_intensity=false);
}
