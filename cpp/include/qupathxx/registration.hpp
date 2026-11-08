#pragma once
#include "qupathxx/expression_dataset.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <array>
#include <optional>

namespace qpx {
enum class CoordinateUnit { Pixel, Micrometre };
struct CoordinateFrame {
    std::string id,source_id;
    CoordinateUnit unit=CoordinateUnit::Pixel;
    ObjectPlane plane;
    std::optional<std::array<double,2>> micrometres_per_pixel;
    bool image_level_zero=false;
    std::int64_t width=0,height=0;
    void validate() const;
    std::string sha256() const;
};
struct Affine2D {
    // Row-major [a,b,tx,c,d,ty]: (x,y) -> (a*x+b*y+tx,c*x+d*y+ty).
    std::array<double,6> matrix{1,0,0,0,1,0};
    // Finite, invertible, with linear infinity-norm condition <=1e12.
    void validate() const;
    Point map(Point) const;
    Affine2D inverse() const;
    // Return next(this(point)); the order is explicit.
    Affine2D then(const Affine2D& next) const;
};
// Unit conversion only; no guessed alignment, origin, axes or physical pixel
// pitch conversion between two different pixel frames. Calibration is required
// for the pixel side of a Pixel <-> Micrometre conversion.
Affine2D coordinate_unit_conversion(const CoordinateFrame& from,const CoordinateFrame& to);
// Captures level-zero geometry/calibration and stable source identity. Embedded
// RGB uses exact bytes; linked readers use verified primary/companion file
// versions and selected series, excluding display windows/process generations.
// frame_id is the caller's persistent project/image identity, not a path guess.
CoordinateFrame image_coordinate_frame(const Document&,std::string frame_id,ObjectPlane={},
    const std::atomic<bool>* cancelled=nullptr);

struct RegistrationDefinition {
    CoordinateFrame source,target;
    std::string dataset_sha256;
    Affine2D transform;
    void validate() const;
    std::string sha256() const;
};
class RegistrationRequest {
public:
    RegistrationRequest(ExpressionDataset,CoordinateFrame source,CoordinateFrame target,
        Affine2D,std::size_t work_bytes=64u*1024*1024);
    const ExpressionDataset& dataset() const;
    const RegistrationDefinition& definition() const;
    std::size_t work_bytes() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
};
struct RegistrationField {
    ExpressionDataset dataset;
    RegistrationDefinition definition;
    std::vector<Point> positions;
    // Outside positions are retained, never clamped or silently dropped.
    std::vector<std::uint8_t> inside_image;
};
RegistrationField registration_field(const RegistrationRequest&,const std::atomic<bool>* cancelled=nullptr);
void require_current_registration(const RegistrationDefinition&,const ExpressionDataset& current,
    const CoordinateFrame& current_target);
// Owner-thread publication for the existing materialized point projection.
// Recaptures the requested cohort and image frame, checks stale inputs, then
// publishes through the existing native image-properties undo command.
void accept_registration(Document&,const std::string& id,const RegistrationRequest&,
    const std::atomic<bool>* cancelled=nullptr);

// Merge one versioned transform into existing native image-data properties.
// Document::set_image_data_properties supplies existing undo/save/reopen behavior.
// Original observation coordinates/counts are never copied over or rewritten.
std::shared_ptr<const ImageDataProperties> with_registration(
    std::shared_ptr<const ImageDataProperties>,const std::string& id,const RegistrationDefinition&);
std::optional<RegistrationDefinition> stored_registration(
    std::shared_ptr<const ImageDataProperties>,const std::string& id);
}
