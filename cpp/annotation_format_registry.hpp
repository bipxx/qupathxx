#pragma once
// Internal synchronous dispatch. Native interchange factories remain authorities.
#include "qupathxx/interchange.hpp"
#include <functional>
#include <variant>

namespace qpx::detail {
struct AnnotationFormatFeatures {bool geojson=false;};
struct AnnotationFormatDescriptor {
    std::string provider_id,display_name;
    bool available=false;
    std::string unavailable_reason;
    bool import_text=false,import_file=false,export_text=false,export_file=false;
    bool clipboard=false,class_context=false;
};
AnnotationFormatFeatures annotation_format_features();
bool known_annotation_format(const std::string&) noexcept;
std::vector<AnnotationFormatDescriptor> annotation_format_catalog(AnnotationFormatFeatures features=annotation_format_features());
const std::vector<std::uint64_t>& annotation_empty_selection() noexcept;
const std::vector<std::string>& annotation_empty_protected_paths() noexcept;

struct AnnotationGeoJsonImportOptions {
    bool assume_pixels=false,allow_losses=false,qupath_ellipse_precision=false;
};
struct AnnotationQuPathClassContext {
    // Borrowed only for the synchronous call; never retained or copied.
    std::reference_wrapper<const std::map<std::string,ObjectClass>> classes;
};
using AnnotationTextImportPolicy=std::variant<AnnotationGeoJsonImportOptions,AnnotationQuPathClassContext>;
struct AnnotationTextImportRequest {
    std::string provider_id="geojson";
    // Borrowed for this synchronous call; avoids copying bounded large payloads.
    std::reference_wrapper<const std::string> text;
    AnnotationTextImportPolicy policy=AnnotationGeoJsonImportOptions{};
};
struct AnnotationFileImportRequest {
    std::string provider_id="geojson",path;
    AnnotationGeoJsonImportOptions options;
};
struct AnnotationClipboardImportRequest {
    std::string provider_id="geojson",text;
    std::optional<ObjectPlane> plane;
    bool allow_losses=false;
    std::size_t max_bytes=16u*1024*1024;
};

struct AnnotationGeoJsonExportOptions {
    std::reference_wrapper<const std::vector<std::uint64_t>> ids=std::cref(annotation_empty_selection());
    bool allow_losses=false,nested_hierarchy=false;
};
struct AnnotationMeasurementsCsvOptions {
    std::reference_wrapper<const std::vector<std::uint64_t>> ids=std::cref(annotation_empty_selection());
    bool include_intensity=false;
};
using AnnotationExportOptions=std::variant<AnnotationGeoJsonExportOptions,AnnotationMeasurementsCsvOptions>;
struct AnnotationTextExportRequest {
    std::string provider_id="geojson";
    AnnotationExportOptions options=AnnotationGeoJsonExportOptions{};
};
struct AnnotationFileExportRequest {
    std::string provider_id="geojson",path;
    AnnotationExportOptions options=AnnotationGeoJsonExportOptions{};
    ExportWritePolicy write_policy=ExportWritePolicy::ReplaceExisting;
    std::reference_wrapper<const std::vector<std::string>> protected_paths=std::cref(annotation_empty_protected_paths());
};
struct AnnotationClipboardExportRequest {
    std::string provider_id="geojson";
    std::reference_wrapper<const std::vector<std::uint64_t>> ids=std::cref(annotation_empty_selection());
    bool include_descendants=false,allow_losses=false;
    std::size_t max_bytes=16u*1024*1024;
};
InterchangeReport import_annotations(Document&,const AnnotationTextImportRequest&);
InterchangeReport import_annotations(Document&,const AnnotationFileImportRequest&);
InterchangeReport import_annotations(Document&,const AnnotationClipboardImportRequest&);
InterchangeReport export_annotations(const Document&,const AnnotationTextExportRequest&);
// CSV text returns native text only; CSV file export returns an empty report.
// The void native writer supplies no feature count or loss report to infer.
InterchangeReport export_annotations(const Document&,const AnnotationFileExportRequest&);
InterchangeReport export_annotations(const Document&,const AnnotationClipboardExportRequest&);
}
