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
// Clipboard transport uses image-pixel GeoJSON, with explicit selection and
// optional descendants. It never rescales geometry or target calibration.
// Limits: 16 MiB by default (at most 256 MiB), 100,000 objects.
InterchangeReport copy_objects(const Document&,const std::vector<std::uint64_t>& ids,bool include_descendants=false,bool allow_losses=false,std::size_t max_bytes=16*1024*1024);
InterchangeReport paste_objects(Document&,const std::string& text,std::optional<ObjectPlane> plane=std::nullopt,bool allow_losses=false,std::size_t max_bytes=16*1024*1024);
InterchangeReport save_geojson(const Document&,const std::string& path,const std::vector<std::uint64_t>& ids={},bool allow_losses=false,bool nested_hierarchy=false);
InterchangeReport load_geojson(Document&,const std::string& path,bool assume_pixels=false,bool allow_losses=false,bool qupath_ellipse_precision=false);
std::string measurements_csv(const Document&,const std::vector<std::uint64_t>& ids={},bool include_intensity=false);
void save_measurements_csv(const Document&,const std::string& path,const std::vector<std::uint64_t>& ids={},bool include_intensity=false);
}
