#include "image_analysis_settings_codec.hpp"
#include "object_properties_codec.hpp"
#include "utf8_text.hpp"
#include "qupathxx/stain_estimation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace qpx {
namespace {
bool brightfield(const std::string& type) {return type=="BRIGHTFIELD_H_E" || type=="BRIGHTFIELD_H_DAB" || type=="BRIGHTFIELD_OTHER";}
void label(const std::string& s) {detail::bounded_utf8(s,4096);}
}
ImageAnalysisSettings::ImageAnalysisSettings(std::string type,const std::vector<Vector>& stains,Vector white,std::string name,std::vector<std::string> names)
    :image_type_(std::move(type)),profile_name_(std::move(name)),names_(std::move(names)),has_stains_(!stains.empty()),residual_(stains.size()==2),white_(white) {
    if(has_stains_) {
        // Reuse normalization/residual/singularity rules, not a second kernel.
        basis_=StainTransform(stains,{255,255,255}).stains();
        if(names_.empty())names_={"Stain 1","Stain 2",residual_?"Residual":"Stain 3"};
        else if(residual_ && names_.size()==2)names_.push_back("Residual");
    }
    validate();
}
ImageAnalysisSettings ImageAnalysisSettings::from_normalized(std::string type,const Basis& basis,Vector white,std::string name,std::vector<std::string> names,bool residual) {
    ImageAnalysisSettings value(std::move(type));value.has_stains_=true;value.basis_=basis;value.white_=white;value.profile_name_=std::move(name);value.names_=std::move(names);value.residual_=residual;value.validate();return value;
}
void ImageAnalysisSettings::validate() const {
    if(image_type_!="UNSET" && image_type_!="OTHER" && image_type_!="FLUORESCENCE" && !brightfield(image_type_))throw std::invalid_argument("unsupported image analysis type");
    label(profile_name_);
    if(!has_stains_) {
        if(!profile_name_.empty() || !names_.empty() || residual_ || white_!=Vector{255,255,255})throw std::invalid_argument("stain metadata requires stain vectors");
        return;
    }
    if(!brightfield(image_type_))throw std::invalid_argument("stain defaults require a brightfield image type");
    if(names_.size()!=3)throw std::invalid_argument("stain defaults require three channel names");
    for(const auto& name:names_) {label(name);if(name.empty())throw std::invalid_argument("stain channel names must not be empty");}
    for(auto v:white_)if(!std::isfinite(v) || v<=0)throw std::invalid_argument("stain white must be finite and positive");
    StainTransform::validate_normalized(basis_,{255,255,255});
    if(residual_) {
        const auto derived=StainTransform({basis_[0],basis_[1]},{255,255,255}).stains()[2];
        for(std::size_t c=0;c<3;++c)if(std::abs(derived[c]-basis_[2][c])>1e-12)throw std::invalid_argument("residual stain does not match its first two vectors");
    }
}
bool ImageAnalysisSettings::rgb8_supported() const {return has_stains_ && std::all_of(white_.begin(),white_.end(),[](double v){return v>=1 && v<=255;});}
StainTransform ImageAnalysisSettings::transform() const {
    if(!has_stains_)throw std::invalid_argument("image settings contain no stain defaults");
    if(!rgb8_supported())throw std::invalid_argument("retained stain white is outside native RGB8 range; quantitative stain application is not supported");
    return StainTransform::from_normalized(basis_,white_);
}
std::string ImageAnalysisSettings::model_sha256() const {return rgb8_supported()?stain_model_sha256(transform()):std::string{};}
std::size_t ImageAnalysisSettings::payload_bytes() const {std::size_t n=sizeof(*this)+image_type_.size()+profile_name_.size();for(const auto& s:names_)n+=sizeof(std::string)+s.size();return n;}
bool ImageAnalysisSettings::equals(const ImageAnalysisSettings& b) const {return image_type_==b.image_type_ && profile_name_==b.profile_name_ && names_==b.names_ && has_stains_==b.has_stains_ && residual_==b.residual_ && basis_==b.basis_ && white_==b.white_;}
namespace detail {
std::vector<std::uint8_t> encode_image_analysis_settings(const ImageAnalysisSettings& s) {
    std::map<std::string,std::string> metadata{{"schema","qpx.image-analysis-settings/1"},{"image_type",s.image_type()}};std::map<std::string,double> values;
    if(s.has_stains()) {
        metadata["name"]=s.profile_name();metadata["residual"]=s.residual_stain()?"1":"0";
        for(std::size_t i=0;i<3;++i) {metadata["stain_"+std::to_string(i)]=s.stain_names()[i];values["white_"+std::to_string(i)]=s.white()[i];for(std::size_t c=0;c<3;++c)values["basis_"+std::to_string(i)+"_"+std::to_string(c)]=s.basis()[i][c];}
    }
    return encode_object_properties(ObjectProperties(std::move(metadata),std::move(values)));
}
ImageAnalysisSettings decode_image_analysis_settings(const void* data,std::size_t n) try {
    const auto p=decode_object_properties(data,n);const auto& m=p->metadata();const auto& v=p->measurements();
    if(p->locked() || p->color_rgb() || p->class_probability() || m.at("schema")!="qpx.image-analysis-settings/1")throw std::invalid_argument("invalid image analysis settings payload");
    if(m.size()==2 && v.empty())return ImageAnalysisSettings(m.at("image_type"));
    if(m.size()!=7 || v.size()!=12 || (m.at("residual")!="0" && m.at("residual")!="1"))throw std::invalid_argument("invalid image stain settings fields");
    ImageAnalysisSettings::Basis basis{};ImageAnalysisSettings::Vector white{};std::vector<std::string> names;
    for(std::size_t i=0;i<3;++i) {names.push_back(m.at("stain_"+std::to_string(i)));white[i]=v.at("white_"+std::to_string(i));for(std::size_t c=0;c<3;++c)basis[i][c]=v.at("basis_"+std::to_string(i)+"_"+std::to_string(c));}
    return ImageAnalysisSettings::from_normalized(m.at("image_type"),basis,white,m.at("name"),std::move(names),m.at("residual")=="1");
} catch(const std::out_of_range&) {throw std::invalid_argument("missing image analysis settings field");}
}
}
