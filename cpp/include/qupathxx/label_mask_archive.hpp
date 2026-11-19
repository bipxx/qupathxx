// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/ngff_label_mask.hpp"

namespace qpx {

struct LabelMaskArchiveContext {
    // Exact opaque record/target-input identities; only the decoded content
    // identity can be independently reconstructed from this archive's payload.
    std::string record_sha256, input_sha256, content_sha256;
    std::string observed_store_sha256, reader_sha256, parameters_json;
    LabelMaskTargetFrame target;
    LabelMaskPlaneSelection plane;
};

struct LabelMaskArchive {
    LabelRaster raster;
    LabelCodeMap source_codes;
    std::shared_ptr<const LabelMaskSourceDescription> description;
    LabelMaskArchiveContext context;
};

// Separate QPLM1 wire format; the existing raster archive is embedded unchanged.
// Bytes0..7: 'Q','P','L','M','1',0,0,0. Bytes8..39: four little-endian UInt64
// lengths for context JSON, exact descriptor JSON, decimal/newline code text,
// and embedded raster bytes. Those payloads follow in that order, then64 ASCII
// lowercase SHA256 bytes over every preceding byte, including all four lengths.
// Context JSON is at most1MiB; descriptor text at most65536 bytes. Foreground
// code i is the source value of dense label i+1; zero remains background. The
// source integral type selects the same compact map storage as normalization.
// Work accounting includes borrowed input, text/DOM/encoded overlap and owned
// decoded values. No source pixels, frame inference or resampling is performed.
std::vector<std::uint8_t> encode_label_mask_archive(const LabelRaster&, const LabelCodeMap&,
    const LabelMaskSourceDescription&, const LabelMaskArchiveContext&,
    LabelRasterLimits = {}, const std::atomic<bool>* cancelled = nullptr);
LabelMaskArchive decode_label_mask_archive(const void*, std::size_t,
    LabelRasterLimits = {}, const std::atomic<bool>* cancelled = nullptr);
LabelMaskArchive open_label_mask_archive(const std::string& path,
    LabelRasterLimits = {}, const std::atomic<bool>* cancelled = nullptr);

}
