#pragma once
#include "qupathxx/directory_store.hpp"
#include "qupathxx/label_codes.hpp"
#include "qupathxx/label_mask_frame.hpp"
#include <array>

namespace qpx {
// Authored masks have an explicit author and no fabricated model identity.
// Both origins declare the exact target raw-input identity used for linkage.
enum class LabelMaskOrigin { Authored, Producer };
struct LabelMaskReplaySource {
    std::string description_json, codes_text, observed_store_sha256, content_sha256;
    std::size_t payload_bytes() const;
};
struct NgffLabelMaskOptions {
    std::string path, label_group, reference_group;
    std::uint32_t level = 0, z = 0, t = 0, reference_z = 0, reference_t = 0;
    LabelMaskOrigin origin = LabelMaskOrigin::Authored;
    std::string author, producer, producer_version, model_sha256, input_sha256;
    std::string reader_sha256;
    std::size_t max_regions = 100000;
    std::shared_ptr<const LabelMaskReplaySource> replay_source;
    void validate() const;
    std::size_t payload_bytes() const;
};
struct LabelMaskSourceProperty {
    LabelCode code;
    // Opaque original property object, interpreted only by the format provider.
    std::string properties;
    std::optional<std::array<std::uint8_t, 4>> rgba;
};
// Neutral, owned provider report. Application services do not parse NGFF files.
struct LabelMaskSourceDescription {
    std::string format, label_group, reference_group, source_reference;
    bool default_reference = false;
    std::uint32_t level = 0, z = 0, t = 0;
    SampleType sample_type = SampleType::UInt8;
    LabelMaskFrame label_frame, reference_frame;
    std::string label_metadata, reference_metadata;
    std::vector<LabelMaskSourceProperty> label_properties;
    std::string json, sha256;
    std::size_t payload_bytes() const;
};
inline constexpr std::size_t label_mask_description_limit = 65536;
LabelMaskSourceDescription decode_label_mask_description(
    std::string, const std::atomic<bool>* cancelled = nullptr);
// Exact dense-ID map, one canonical nonzero decimal per newline; empty for no
// foreground. Suitable for bounded native metadata and archive interchange.
std::string label_code_map_text(const LabelCodeMap&, std::size_t byte_limit = 1024u * 1024,
                                 const std::atomic<bool>* cancelled = nullptr);
// Capture or compare the exact module/dependency/options identity, before a job.
NgffLabelMaskOptions capture_ngff_label_mask_options(NgffLabelMaskOptions);
std::string ngff_label_selection_json(const NgffLabelMaskOptions&);
struct ImportedNgffLabelMask {
    LabelRaster raster;
    LabelCodeMap source_codes;
    std::shared_ptr<const ReaderSource> source;
    std::shared_ptr<const DirectoryStoreSnapshot> store;
    std::shared_ptr<const LabelMaskSourceDescription> description;
    LabelMaskCropLink crop;
    // A declared decoded-content identity; never a whole-file fingerprint.
    std::string content_sha256, reader_sha256;
};
ImportedNgffLabelMask import_ngff_label_mask(
    const NgffLabelMaskOptions&, const LabelMaskTargetFrame&,
    std::size_t work_bytes = 256u * 1024 * 1024,
    const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
