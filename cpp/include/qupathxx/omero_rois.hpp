#pragma once
#include "qupathxx/omero.hpp"
#include "qupathxx/geometry.hpp"
namespace qpx {
struct OmeroShape {
    std::uint64_t roi_id = 0, shape_id = 0;
    Roi geometry;
    // Missing plane coordinates mean all planes in OMERO, never native Z=0.
    std::optional<std::uint32_t> c, z, t;
    std::string source_json;
    // Packed MSB-first row-major binary raster, retained independently of its
    // exact pixel-boundary geometry and the server's color/alpha rendering.
    std::vector<std::uint8_t> mask_bits;
    std::string mask_png_sha256, mask_decoder;
};
class OmeroRois {
  public:
    const OmeroImageInfo& image() const;
    const std::string& server() const;
    const std::vector<OmeroShape>& shapes() const;
    const std::string& sha256() const;
    const OmeroRoiOptions& options() const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit OmeroRois(std::shared_ptr<const Data>);
    friend class OmeroClient;
};
// Native image-pixel GeoJSON with explicit original source association and
// lossless OMERO shape/plane/style extensions. No pixel source is manufactured.
std::string omero_rois_geojson(const OmeroRois&);
// Validates the original frame against fresh native image metadata. Returns a
// versioned offline ROI/result exchange envelope, not a claim of server upload.
// Geometry is interpreted by the existing native GeoJSON/Roi authorities.
std::string omero_rois_exchange(const std::string& geojson, const OmeroRois& current);
void save_omero_roi_text(const std::string&, const std::string& new_file,
                         const std::atomic<bool>* cancelled = nullptr);
} // namespace qpx
