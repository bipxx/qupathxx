#include "qupathxx/registration.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_file_version.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace qpx {
namespace {
constexpr const char* properties_key="qupathxx.registrations/1";
void identifier(const std::string& value) {
    detail::bounded_utf8(value,4096);
    if(value.empty() || value.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Invalid coordinate/registration identifier");
}
void hash_text(Sha256& digest,const std::string& value) {digest.integer(value.size());digest.update(value.data(),value.size());}
void hash_real(Sha256& digest,double value) {
    if(value==0)value=0;std::uint64_t bits;std::memcpy(&bits,&value,8);digest.integer(bits);
}
void hash_plane(Sha256& digest,ObjectPlane plane) {digest.integer(static_cast<std::uint64_t>(plane.c+1));digest.integer(plane.z);digest.integer(plane.t);}
Point mapped(const Affine2D& affine,Point point) {
    if(!std::isfinite(point.x) || !std::isfinite(point.y))throw std::invalid_argument("Nonfinite coordinate");
    const auto& m=affine.matrix;
    Point result{std::fma(m[0],point.x,std::fma(m[1],point.y,m[2])),std::fma(m[3],point.x,std::fma(m[4],point.y,m[5]))};
    if(!std::isfinite(result.x) || !std::isfinite(result.y))throw std::invalid_argument("Affine coordinate overflow");
    return result;
}
std::array<double,4> inverse_linear(const Affine2D& affine) {
    const auto& m=affine.matrix;for(auto v:m)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite affine coefficient");
    const auto scale=std::max({std::abs(m[0]),std::abs(m[1]),std::abs(m[3]),std::abs(m[4])});
    if(!scale)throw std::invalid_argument("Singular affine transform");
    const auto a=m[0]/scale,b=m[1]/scale,c=m[3]/scale,d=m[4]/scale;
    const auto determinant=a*d-b*c;
    if(!determinant)throw std::invalid_argument("Singular affine transform");
    const auto norm=std::max(std::abs(a)+std::abs(b),std::abs(c)+std::abs(d));
    const auto inv_norm=std::max(std::abs(d)+std::abs(b),std::abs(c)+std::abs(a))/std::abs(determinant);
    if(!std::isfinite(inv_norm) || norm*inv_norm>1e12)throw std::invalid_argument("Ill-conditioned affine transform");
    std::array<double,4> inverse{d/determinant/scale,-b/determinant/scale,-c/determinant/scale,a/determinant/scale};
    for(auto v:inverse)if(!std::isfinite(v))throw std::invalid_argument("Affine inverse overflows");
    return inverse;
}
void file_identity(Sha256& digest,const FileVersion& captured,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);captured.validate();
    std::shared_ptr<const FileVersion> now;
    try {now=detail::workflow_file_version(captured.path);}catch(const std::exception&) {throw WorkflowInputChanged("Registration image source is unavailable");}
    if(captured.identity_scheme!=now->identity_scheme || captured.identity!=now->identity || captured.bytes!=now->bytes ||
       captured.stamp_scheme!=now->stamp_scheme || captured.modified_token!=now->modified_token || captured.change_token!=now->change_token)
        throw WorkflowInputChanged("Registration image source changed");
    hash_text(digest,captured.path);hash_text(digest,captured.identity_scheme);hash_text(digest,captured.identity);
    digest.integer(captured.bytes);hash_text(digest,captured.stamp_scheme);hash_text(digest,captured.modified_token);hash_text(digest,captured.change_token);
}
using V=ImageDataValue;
V value(std::string v) {return V{std::move(v)};}
V value(std::int64_t v) {return V{v};}
V value(double v) {return V{v};}
V frame_value(const CoordinateFrame& frame) {
    V::Map values{{"id",value(frame.id)},{"source_id",value(frame.source_id)},
        {"unit",value(frame.unit==CoordinateUnit::Pixel?"pixel":"micrometre")},
        {"c",value(std::int64_t(frame.plane.c))},{"z",value(std::int64_t(frame.plane.z))},{"t",value(std::int64_t(frame.plane.t))},
        {"width",value(frame.width)},{"height",value(frame.height)},{"image_level_zero",V{frame.image_level_zero}}};
    if(frame.micrometres_per_pixel)values["micrometres_per_pixel"]=V{V::List{value((*frame.micrometres_per_pixel)[0]),value((*frame.micrometres_per_pixel)[1])}};
    return V{std::move(values)};
}
template<class T> const T& typed(const V& v) {
    const auto* result=std::get_if<T>(&v.value);if(!result)throw std::invalid_argument("Invalid stored registration value type");return *result;
}
const V& member(const V::Map& m,const char* key) {
    const auto at=m.find(key);if(at==m.end())throw std::invalid_argument("Missing stored registration field");return at->second;
}
CoordinateFrame decode_frame(const V& v) {
    const auto& m=typed<V::Map>(v);CoordinateFrame frame;
    if(m.size()!=9 && m.size()!=10)throw std::invalid_argument("Invalid stored coordinate-frame members");
    frame.id=typed<std::string>(member(m,"id"));frame.source_id=typed<std::string>(member(m,"source_id"));
    const auto& unit=typed<std::string>(member(m,"unit"));
    if(unit!="pixel" && unit!="micrometre")throw std::invalid_argument("Unknown stored coordinate unit");
    frame.unit=unit=="pixel"?CoordinateUnit::Pixel:CoordinateUnit::Micrometre;
    const auto c=typed<std::int64_t>(member(m,"c")),z=typed<std::int64_t>(member(m,"z")),t=typed<std::int64_t>(member(m,"t"));
    if(c< -1 || c>=65536 || z<0 || z>=65536 || t<0 || t>=65536)throw std::invalid_argument("Invalid stored registration plane");
    frame.plane=ObjectPlane(static_cast<std::int32_t>(c),static_cast<std::uint32_t>(z),static_cast<std::uint32_t>(t));
    frame.width=typed<std::int64_t>(member(m,"width"));frame.height=typed<std::int64_t>(member(m,"height"));
    frame.image_level_zero=typed<bool>(member(m,"image_level_zero"));
    const auto at=m.find("micrometres_per_pixel");
    if(m.size()!=9+std::size_t(at!=m.end()))throw std::invalid_argument("Unknown stored coordinate-frame member");
    if(at!=m.end()) {const auto& calibration=typed<V::List>(at->second);if(calibration.size()!=2)throw std::invalid_argument("Invalid stored coordinate calibration");
        frame.micrometres_per_pixel=std::array<double,2>{typed<double>(calibration[0]),typed<double>(calibration[1])};}
    frame.validate();return frame;
}
}
void CoordinateFrame::validate() const {
    identifier(id);identifier(source_id);plane.validate();
    if(unit!=CoordinateUnit::Pixel && unit!=CoordinateUnit::Micrometre)throw std::invalid_argument("Unknown coordinate unit");
    if(width<0 || height<0 || width>9007199254740992LL || height>9007199254740992LL || bool(width)!=bool(height))
        throw std::invalid_argument("Invalid coordinate-frame extent");
    if(image_level_zero && (unit!=CoordinateUnit::Pixel || !width || !height))throw std::invalid_argument("Image level-zero frame requires pixel dimensions");
    if(micrometres_per_pixel) {
        if(unit!=CoordinateUnit::Pixel)throw std::invalid_argument("Pixel calibration on a non-pixel frame");
        for(auto v:*micrometres_per_pixel)if(!std::isfinite(v) || v<=0)throw std::invalid_argument("Invalid coordinate-frame calibration");
    }
}
std::string CoordinateFrame::sha256() const {
    validate();Sha256 digest;hash_text(digest,"qpx.coordinate-frame/1");hash_text(digest,id);hash_text(digest,source_id);
    digest.integer(static_cast<std::uint64_t>(unit));hash_plane(digest,plane);digest.integer(image_level_zero);digest.integer(width);digest.integer(height);
    digest.integer(bool(micrometres_per_pixel));if(micrometres_per_pixel)for(auto v:*micrometres_per_pixel)hash_real(digest,v);
    return digest.finish();
}
void Affine2D::validate() const {inverse_linear(*this);}
Point Affine2D::map(Point p) const {validate();return mapped(*this,p);}
Affine2D Affine2D::inverse() const {
    const auto inv=inverse_linear(*this);Affine2D result{{inv[0],inv[1],0,inv[2],inv[3],0}};
    const auto offset=mapped(result,{-matrix[2],-matrix[5]});result.matrix[2]=offset.x;result.matrix[5]=offset.y;result.validate();return result;
}
Affine2D Affine2D::then(const Affine2D& next) const {
    validate();next.validate();const auto& a=matrix;const auto& b=next.matrix;
    Affine2D result{{std::fma(b[0],a[0],b[1]*a[3]),std::fma(b[0],a[1],b[1]*a[4]),std::fma(b[0],a[2],std::fma(b[1],a[5],b[2])),
        std::fma(b[3],a[0],b[4]*a[3]),std::fma(b[3],a[1],b[4]*a[4]),std::fma(b[3],a[2],std::fma(b[4],a[5],b[5]))}};
    result.validate();return result;
}
Affine2D coordinate_unit_conversion(const CoordinateFrame& from,const CoordinateFrame& to) {
    from.validate();to.validate();if(from.unit==to.unit)return {};
    const auto& pixel=from.unit==CoordinateUnit::Pixel?from:to;
    if(!pixel.micrometres_per_pixel)throw std::invalid_argument("Physical pixel calibration is unknown");
    auto scale=*pixel.micrometres_per_pixel;
    if(from.unit==CoordinateUnit::Micrometre)for(auto& v:scale)v=1/v;
    Affine2D result{{scale[0],0,0,0,scale[1],0}};result.validate();return result;
}
CoordinateFrame image_coordinate_frame(const Document& document,std::string id,ObjectPlane plane,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);identifier(id);document.validate_object_plane(plane);
    const auto server=document.server();if(server->closed())throw std::invalid_argument("Registration image is closed");
    auto source=server->reader_source();
    if(source && source->scope=="unopened") {server->read_region({0,0,0,1,1});workflow_checkpoint(cancelled);source=server->reader_source();}
    if(!source)throw std::invalid_argument("Image reader has no registration source identity");source->validate();
    Sha256 digest;hash_text(digest,"qpx.registration-image-source/1");hash_text(digest,server->reader_id());
    digest.integer(document.metadata().width);digest.integer(document.metadata().height);
    if(source->primary) {
        file_identity(digest,*source->primary,cancelled);digest.integer(source->companions.size());
        for(const auto& file:source->companions)file_identity(digest,file,cancelled);
        if(source->scope=="directory_store"){server->require_raw_current(cancelled);hash_text(digest,source->directory_sha256);}
        const auto view=server->raw_view();digest.integer(bool(view));if(view)digest.integer(view->series);
    } else {
        // External mutable readers without stable file versions are deliberately
        // refused; memory-backed native RGB is immutable and can be hashed once.
        const auto& pixels=dense_image(*server).pixels();
        for(std::size_t at=0;at<pixels.size();) {workflow_checkpoint(cancelled);const auto count=std::min<std::size_t>(65536,pixels.size()-at);digest.update(pixels.data()+at,count);at+=count;}
    }
    CoordinateFrame frame;frame.id=std::move(id);frame.source_id=digest.finish();frame.plane=plane;
    frame.image_level_zero=true;frame.width=document.metadata().width;frame.height=document.metadata().height;
    if(document.pixel_width_um() && document.pixel_height_um())frame.micrometres_per_pixel=std::array<double,2>{*document.pixel_width_um(),*document.pixel_height_um()};
    frame.validate();return frame;
}
void RegistrationDefinition::validate() const {
    source.validate();target.validate();transform.validate();
    if(!target.image_level_zero)throw std::invalid_argument("Registration target must be image level zero");
    if(dataset_sha256.size()!=64 || dataset_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid registration dataset identity");
    // The supplied matrix already maps the declared numeric units. This checks
    // their admissibility; it never silently prepends another conversion.
    coordinate_unit_conversion(source,target);
}
std::string RegistrationDefinition::sha256() const {
    validate();Sha256 digest;hash_text(digest,"qpx.registration/1");hash_text(digest,source.sha256());hash_text(digest,target.sha256());
    hash_text(digest,dataset_sha256);for(auto v:transform.matrix)hash_real(digest,v);return digest.finish();
}
struct RegistrationRequest::Data {ExpressionDataset dataset;RegistrationDefinition definition;std::size_t work;};
RegistrationRequest::RegistrationRequest(ExpressionDataset dataset,CoordinateFrame source,CoordinateFrame target,Affine2D transform,std::size_t work) {
    if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid registration work budget");
    RegistrationDefinition definition{std::move(source),std::move(target),dataset.sha256(),transform};definition.validate();
    if(definition.source.id!=dataset.coordinate_frame() || definition.source.source_id!=dataset.source_id() || definition.source.plane!=dataset.plane())
        throw std::invalid_argument("Registration source frame differs from expression dataset");
    std::size_t bytes=sizeof(Data)+sizeof(RegistrationField);
    for(const auto* frame:{&definition.source,&definition.target}) {
        workflow_capture_charge(bytes,frame->id.capacity(),work);workflow_capture_charge(bytes,frame->source_id.capacity(),work);
        workflow_capture_charge(bytes,frame->id.capacity(),work);workflow_capture_charge(bytes,frame->source_id.capacity(),work);
    }
    workflow_capture_charge(bytes,2*definition.dataset_sha256.capacity()+256,work);
    if(dataset.observations().size()>work/(sizeof(Point)+1))throw WorkflowCaptureLimit("Registration positions exceed work budget");
    workflow_capture_charge(bytes,dataset.observations().size()*(sizeof(Point)+1),work);
    data_=std::make_shared<Data>(Data{std::move(dataset),std::move(definition),work});
}
const ExpressionDataset& RegistrationRequest::dataset() const {return data_->dataset;}
const RegistrationDefinition& RegistrationRequest::definition() const {return data_->definition;}
std::size_t RegistrationRequest::work_bytes() const {return data_->work;}
RegistrationField registration_field(const RegistrationRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);RegistrationField field{request.dataset(),request.definition(),{}, {}};
    field.positions.reserve(field.dataset.observations().size());field.inside_image.reserve(field.dataset.observations().size());
    for(const auto& observation:field.dataset.observations()) {
        workflow_checkpoint(cancelled);const auto position=mapped(field.definition.transform,observation.position);field.positions.push_back(position);
        field.inside_image.push_back(position.x>=0 && position.y>=0 && position.x<field.definition.target.width && position.y<field.definition.target.height);
    }
    return field;
}
void require_current_registration(const RegistrationDefinition& definition,const ExpressionDataset& current,const CoordinateFrame& target) {
    definition.validate();target.validate();
    if(definition.dataset_sha256!=current.sha256() || definition.source.id!=current.coordinate_frame() || definition.source.source_id!=current.source_id() ||
       definition.source.plane!=current.plane() || definition.target.sha256()!=target.sha256())throw WorkflowInputChanged("Registration source/frame changed");
}
void require_current_registration(const Document& document,const RegistrationRequest& request,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);const auto& definition=request.definition();
    ExpressionCaptureOptions options;options.coordinate_frame=definition.source.id;options.source_id=definition.source.source_id;
    options.plane=definition.source.plane;options.features=request.dataset().features();options.work_bytes=request.work_bytes();
    for(const auto& observation:request.dataset().observations()) {
        if(!observation.object_id)throw std::invalid_argument("Registration publication requires materialized observations");
        options.object_ids.push_back(observation.object_id);
    }
    if(options.object_ids.empty())throw std::invalid_argument("Registration publication requires an explicit nonempty cohort");
    auto current=ExpressionDataset::capture(document,options,cancelled);
    const auto target=image_coordinate_frame(document,definition.target.id,definition.target.plane,cancelled);
    require_current_registration(definition,current,target);workflow_checkpoint(cancelled);
}
void accept_registration(Document& document,const std::string& id,const RegistrationRequest& request,const std::atomic<bool>* cancelled) {
    identifier(id);require_current_registration(document,request,cancelled);
    document.set_image_data_properties(with_registration(document.image_data_properties(),id,request.definition()));
}
std::shared_ptr<const ImageDataProperties> with_registration(std::shared_ptr<const ImageDataProperties> properties,const std::string& id,const RegistrationDefinition& definition) {
    identifier(id);definition.validate();V::Map values=properties?properties->values():V::Map{};V::Map registrations;
    const auto at=values.find(properties_key);if(at!=values.end())registrations=typed<V::Map>(at->second);
    V::List matrix;for(auto v:definition.transform.matrix)matrix.push_back(value(v));
    registrations[id]=V{V::Map{{"schema",value("qpx.registration/1")},{"source",frame_value(definition.source)},{"target",frame_value(definition.target)},
        {"dataset_sha256",value(definition.dataset_sha256)},{"transform",V{std::move(matrix)}},{"sha256",value(definition.sha256())}}};
    values[properties_key]=V{std::move(registrations)};return std::make_shared<const ImageDataProperties>(std::move(values));
}
std::optional<RegistrationDefinition> stored_registration(std::shared_ptr<const ImageDataProperties> properties,const std::string& id) {
    identifier(id);if(!properties)return {};
    const auto at=properties->values().find(properties_key);if(at==properties->values().end())return {};
    const auto& registrations=typed<V::Map>(at->second);const auto found=registrations.find(id);if(found==registrations.end())return {};
    const auto& m=typed<V::Map>(found->second);
    if(m.size()!=6 || typed<std::string>(member(m,"schema"))!="qpx.registration/1")throw std::invalid_argument("Unknown registration schema/members");
    RegistrationDefinition definition;definition.source=decode_frame(member(m,"source"));definition.target=decode_frame(member(m,"target"));
    definition.dataset_sha256=typed<std::string>(member(m,"dataset_sha256"));const auto& matrix=typed<V::List>(member(m,"transform"));
    if(matrix.size()!=6)throw std::invalid_argument("Invalid stored affine shape");for(std::size_t i=0;i<6;++i)definition.transform.matrix[i]=typed<double>(matrix[i]);
    definition.validate();if(definition.sha256()!=typed<std::string>(member(m,"sha256")))throw std::invalid_argument("Stored registration identity mismatch");
    return definition;
}
}
