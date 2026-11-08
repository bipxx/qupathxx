#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
namespace qpx {
class StainTransform;
// Owned value: no image, Python wrapper, worker callback or cached transform.
class ImageAnalysisSettings {
public:
    using Vector=std::array<double,3>;
    using Basis=std::array<Vector,3>;
    explicit ImageAnalysisSettings(std::string image_type="UNSET",const std::vector<Vector>& stains={},Vector white={255,255,255},std::string profile_name="",std::vector<std::string> stain_names={});
    static ImageAnalysisSettings from_normalized(std::string image_type,const Basis&,Vector white,std::string profile_name,std::vector<std::string> stain_names,bool residual);
    const std::string& image_type() const {return image_type_;}
    bool has_stains() const {return has_stains_;}
    bool residual_stain() const {return residual_;}
    const Basis& basis() const {return basis_;}
    const Vector& white() const {return white_;}
    const std::string& profile_name() const {return profile_name_;}
    const std::vector<std::string>& stain_names() const {return names_;}
    bool rgb8_supported() const;
    StainTransform transform() const;
    std::string model_sha256() const;
    std::size_t payload_bytes() const;
    bool empty() const {return image_type_=="UNSET" && !has_stains_;}
    bool equals(const ImageAnalysisSettings&) const;
private:
    void validate() const;
    std::string image_type_,profile_name_;std::vector<std::string> names_;
    bool has_stains_=false,residual_=false;
    Basis basis_{};Vector white_{255,255,255};
};
}
