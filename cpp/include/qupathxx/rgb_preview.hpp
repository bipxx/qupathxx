#pragma once
#include "qupathxx/resampling.hpp"
#include <memory>
#include <optional>
#include <string>
namespace qpx {
class Document;
// Native factories only; preview auto preserves this consumer's all-TIFF policy.
std::shared_ptr<ImageServer> open_rgb_nominal_source(const std::string& path,const std::string& reader="auto");
// Own the sampled RGB pixels as a new source, with nominally scaled calibration.
// Partial source calibration stays unknown; this does not transfer source replay authority.
std::shared_ptr<Document> make_rgb_nominal_preview(const std::shared_ptr<ImageServer>& source,
    const RgbNominalRequest&,std::optional<double> pixel_width_um={},std::optional<double> pixel_height_um={},
    const std::atomic<bool>* cancelled=nullptr);
std::shared_ptr<Document> make_rgb_nominal_preview(const Document&,const RgbNominalRequest&,
    const std::atomic<bool>* cancelled=nullptr);
}
