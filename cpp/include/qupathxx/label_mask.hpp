#pragma once
#include "qupathxx/label_raster.hpp"
#include "qupathxx/source_identity.hpp"
namespace qpx {
struct ExternalLabelMaskOptions {
    std::string path, producer, producer_version, model_sha256, input_sha256;
    std::size_t max_regions = 100000;
    void validate() const;
};
struct ImportedLabelMask {
    LabelRaster raster;
    // Dense native label i+1 maps to the exact unsigned value in the file.
    std::vector<std::uint32_t> source_labels;
    std::string file_sha256;
    std::shared_ptr<const ReaderSource> source;
};
// Offline file adapter over the existing native TIFF reader. One unsigned
// UInt8/16/32 channel, series, Z and time plane; exact crop dimensions. No
// resizing, inferred calibration, model execution or bundled model assets.
ImportedLabelMask import_label_mask_tiff(const ExternalLabelMaskOptions&, std::int64_t width,
                                         std::int64_t height,
                                         std::size_t work_bytes = 256u * 1024 * 1024,
                                         const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
