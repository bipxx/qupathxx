#pragma once
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include <optional>
namespace qpx {
struct RawRegionExportOptions {
    // The legacy default is TIFF; other readers are always explicit.
    std::string reader = "tiff";
    TiffOpenOptions tiff;
    RawRegionRequest request;
    bool full_width = true, full_height = true;
    // Replace manifest defaults for an explicit native plugin. An object of {}
    // selects empty options. Absence uses the manifest defaults.
    std::optional<std::string> reader_plugin_options;
    // Write an H×W structured NPY with a C-sample values field and a valid flag.
    bool with_validity = false;
    // Interpret request XY as selected-array indices. Only built-in OME-Zarr supports it.
    bool level_coordinates = false;
    // Publish raw.npy and receipt.json in one atomic stored ZIP.
    bool bundle = false;
    void validate() const;
};
struct RawRegionExportGrid {
    double downsample = 1;
    // Physical spacing of the exported XY samples, when the source declares units.
    std::optional<double> pixel_width_um, pixel_height_um;
    // Source array cell for output row 0, column 0, including padding.
    std::int64_t level_x = 0, level_y = 0;
    std::string dataset;
    // The selected source array's axis order, declared units and effective transform.
    std::vector<std::string> axes, units;
    std::vector<double> scale, translation;
    bool level_coordinates = false;
};
struct RawRegionProjectAxes {
    std::optional<double> z_spacing_um, z_position_um, timepoint_seconds;
};
struct RawRegionProjectSource {
    std::string project_id, project_sha256;
    std::uint64_t generation = 0, image_id = 0;
    // Effective positions for the requested plane when the document has an override.
    std::optional<RawRegionProjectAxes> axes = std::nullopt;
};
struct RawRegionExportReport {
    std::string reader;
    RawRegionRequest request;
    std::shared_ptr<const RawImageMetadata> metadata;
    std::shared_ptr<const ReaderSource> source;
    // Existing native saved-source serializer owns the exact plugin contract,
    // options/module/dependency identity and raw/display metadata interpretation.
    std::string reader_snapshot_json;
    std::uint64_t valid_pixels = 0;
    std::size_t raw_bytes = 0;
    bool with_validity = false;
    std::optional<RawRegionExportGrid> output_grid;
    std::optional<RawRegionProjectSource> project;
};
// New-only atomic NumPy array or ZIP bundle export through the native readers.
// Observed primary/companion files or an identified native directory store
// stay guarded through the operation. The output must be outside the store.
// Plain plugin and OME-Zarr arrays require complete validity. Structured exports record
// validity per pixel. Legacy plain TIFF padding stays unchanged.
RawRegionExportReport export_raw_region(const std::string& input, const std::string& output,
                                        const RawRegionExportOptions& = {},
                                        const std::atomic<bool>* cancelled = nullptr);
struct ProjectRawRegionExportOptions {
    // Source selection fields in region must retain their defaults.
    RawRegionExportOptions region;
    bool follow_saved_z = true, follow_saved_time = true;
};
// Use the saved image reader, series and interpretation. Z and time use the
// saved view where available unless explicitly overridden in the request.
RawRegionExportReport export_project_raw_region(const std::string& project, std::uint64_t image_id,
                                                const std::string& output,
                                                const ProjectRawRegionExportOptions& = {},
                                                const std::atomic<bool>* cancelled = nullptr);
// Plain TIFF keeps schema1. Plain exports via other readers use schema2 and
// structured exports use schema3. Plugins provide a reader snapshot. Built-in
// OME-Zarr provides its observed directory identity and available calibration.
std::string raw_region_export_report_json(const RawRegionExportReport&);
// Return the validated ZIP receipt without opening its referenced source.
std::string verify_raw_region_bundle(const std::string& zip,
                                     const std::atomic<bool>* cancelled = nullptr);
// Verify and atomically publish the exact NPY member, then return its receipt.
std::string extract_raw_region_bundle(const std::string& zip,const std::string& output,
                                      const std::atomic<bool>* cancelled = nullptr);
// Existing display-region PAM representation, now shared/native and new-only.
// PixelBuffer is an owned display snapshot; source supplies protected primary,
// companion and directory paths, not a claim of raw calibration or transaction.
void save_region_pam(const PixelBuffer&, const std::string& output,
                     const std::shared_ptr<const ReaderSource>& source = {},
                     const std::atomic<bool>* cancelled = nullptr);
// Render a level-zero origin and a level-sized rectangle using the saved
// reader view, then atomically publish a new RGBA8 PAM file.
void export_project_display_region(const std::string& project, std::uint64_t image_id,
                                   const RegionRequest&, const std::string& output,
                                   const std::atomic<bool>* cancelled = nullptr);
// Publish the same PAM, then return its JSON receipt with the exact project,
// reader, view and artifact identity.
std::string export_project_display_region_with_receipt(const std::string& project,
    std::uint64_t image_id,const RegionRequest&,const std::string& output,
    const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
