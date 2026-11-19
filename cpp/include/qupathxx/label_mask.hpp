#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/source_identity.hpp"
namespace qpx {
enum class ExternalLabelMaskFormat { Tiff, Png, PngIndexed };
struct ExternalLabelMaskOptions {
    std::string path, producer, producer_version, model_sha256, input_sha256;
    std::size_t max_regions = 100000;
    ExternalLabelMaskFormat format = ExternalLabelMaskFormat::Tiff;
    // Captured native PNG implementation identity, including module/dependency
    // hashes. Empty for legacy TIFF; callers may leave empty for a new PNG capture.
    std::string reader_sha256;
    void validate() const;
};
struct ImportedLabelMask {
    LabelRaster raster;
    // Dense native label i+1 maps to the exact unsigned value in the file.
    std::vector<std::uint32_t> source_labels;
    std::string file_sha256;
    std::shared_ptr<const ReaderSource> source;
};
// Offline adapters share raw-code interpretation, not display conversion. TIFF
// admits UInt8/16/32; PNG requires one unsigned grayscale or palette-index
// channel according to the selected profile. All require exact crop dimensions
// and one series/Z/time plane.
ImportedLabelMask import_label_mask(const ExternalLabelMaskOptions&, std::int64_t width,
                                    std::int64_t height,
                                    std::size_t work_bytes = 256u * 1024 * 1024,
                                    const std::atomic<bool>* cancelled = nullptr);
// Retained TIFF-only entry point. No implicit format detection or conversion.
ImportedLabelMask import_label_mask_tiff(const ExternalLabelMaskOptions&, std::int64_t width,
                                         std::int64_t height,
                                         std::size_t work_bytes = 256u * 1024 * 1024,
                                         const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
