#include "qupathxx/landmark_registration.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace qpx {
namespace {
constexpr const char* fit_key="qupathxx.landmark-registrations/1";
constexpr const char* active_key="qupathxx.active-registration/1";
void name(const std::string& value) {detail::bounded_utf8(value,256);if(value.empty() || value.find_first_of("\r\n\t")!=std::string::npos)throw std::invalid_argument("Invalid landmark identifier");}
void text(Sha256& sha,const std::string& value) {sha.integer(value.size());sha.update(value.data(),value.size());}
void real(Sha256& sha,double value) {if(value==0)value=0;std::uint64_t bits;std::memcpy(&bits,&value,8);sha.integer(bits);}
void point(Sha256& sha,Point value) {real(sha,value.x);real(sha,value.y);}
void finite(Point p) {if(!std::isfinite(p.x) || !std::isfinite(p.y))throw std::invalid_argument("Landmark coordinates must be finite");}
struct Normalization {Point center;double scale;};
Normalization normalization(const std::vector<LandmarkPair>& pairs,bool target) {
    Point lo{},hi{};bool first=true;
    for(const auto& pair:pairs)if(!pair.held_out) {const auto p=target?pair.target:pair.source;
        if(first) {lo=hi=p;first=false;}else {lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);}}
    const Point center{lo.x*.5+hi.x*.5,lo.y*.5+hi.y*.5};double scale=0;
    for(const auto& pair:pairs)if(!pair.held_out) {const auto p=target?pair.target:pair.source;scale=std::max({scale,std::abs(p.x-center.x),std::abs(p.y-center.y)});}
    return {center,scale?scale:1};
}
Point normalized(Point p,const Normalization& n) {return {(p.x-n.center.x)/n.scale,(p.y-n.center.y)/n.scale};}
double ratio(double coefficient,double numerator,double denominator) {
    if(!coefficient)return 0;int a,b,c;const auto x=std::frexp(coefficient,&a),y=std::frexp(numerator,&b),z=std::frexp(denominator,&c);
    const auto result=std::ldexp(x*y/z,a+b-c);if(!std::isfinite(result))throw std::invalid_argument("Landmark coefficients overflow");return result;
}
Affine2D fit_matrix(const std::vector<LandmarkPair>& pairs,const LandmarkFitOptions& options,const std::atomic<bool>* cancelled) {
    const auto s=normalization(pairs,false),t=normalization(pairs,true);std::size_t count=0;
    Point sm{},tm{};for(const auto& pair:pairs)if(!pair.held_out) {const auto a=normalized(pair.source,s),b=normalized(pair.target,t);sm.x+=a.x;sm.y+=a.y;tm.x+=b.x;tm.y+=b.y;++count;}
    sm.x/=count;sm.y/=count;tm.x/=count;tm.y/=count;
    Affine2D result;
    if(options.model==LandmarkModel::Translation) {
        result.matrix[2]=std::fma(t.scale,tm.x,t.center.x)-std::fma(s.scale,sm.x,s.center.x);
        result.matrix[5]=std::fma(t.scale,tm.y,t.center.y)-std::fma(s.scale,sm.y,s.center.y);
    } else {
        std::array<double,6> m{};
        if(options.model==LandmarkModel::Similarity) {
            double aa=0,bb=0,denominator=0;const double reflection=options.reflect_source_y?-1.:1.;
            for(const auto& pair:pairs)if(!pair.held_out) {workflow_checkpoint(cancelled);auto a=normalized(pair.source,s),b=normalized(pair.target,t);
                a.x-=sm.x;a.y=(a.y-sm.y)*reflection;b.x-=tm.x;b.y-=tm.y;
                aa+=a.x*b.x+a.y*b.y;bb+=a.x*b.y-a.y*b.x;denominator+=a.x*a.x+a.y*a.y;}
            if(denominator<=count*1e-24)throw std::invalid_argument("Similarity landmarks require distinct source positions");
            aa/=denominator;bb/=denominator;m={aa,-bb*reflection,0,bb,aa*reflection,0};
            m[2]=tm.x-m[0]*sm.x-m[1]*sm.y;m[5]=tm.y-m[3]*sm.x-m[4]*sm.y;
        } else {
            std::vector<std::array<double,3>> a;std::vector<std::array<double,2>> b;a.reserve(count);b.reserve(count);
            for(const auto& pair:pairs)if(!pair.held_out) {const auto x=normalized(pair.source,s),y=normalized(pair.target,t);a.push_back({x.x,x.y,1});b.push_back({y.x,y.y});}
            for(std::size_t k=0;k<3;++k) {
                workflow_checkpoint(cancelled);double norm=0;for(std::size_t i=k;i<count;++i)norm=std::hypot(norm,a[i][k]);
                if(norm<=std::sqrt(double(count))*1e-12)throw std::invalid_argument("Affine landmarks are degenerate or poorly distributed");
                const double alpha=-std::copysign(norm,a[k][k]);std::vector<double> v(count-k);v[0]=a[k][k]-alpha;
                for(std::size_t i=k+1;i<count;++i)v[i-k]=a[i][k];double length=0;for(auto x:v)length=std::hypot(length,x);for(auto& x:v)x/=length;
                for(std::size_t column=k;column<3;++column) {double dot=0;for(std::size_t i=k;i<count;++i)dot+=v[i-k]*a[i][column];for(std::size_t i=k;i<count;++i)a[i][column]-=2*v[i-k]*dot;}
                for(unsigned column=0;column<2;++column) {double dot=0;for(std::size_t i=k;i<count;++i)dot+=v[i-k]*b[i][column];for(std::size_t i=k;i<count;++i)b[i][column]-=2*v[i-k]*dot;}
            }
            for(unsigned column=0;column<2;++column)for(int row=2;row>=0;--row) {double value=b[std::size_t(row)][column];for(unsigned j=unsigned(row)+1;j<3;++j)value-=a[std::size_t(row)][j]*m[column*3+j];m[column*3+unsigned(row)]=value/a[std::size_t(row)][std::size_t(row)];}
        }
        result.matrix={ratio(m[0],t.scale,s.scale),ratio(m[1],t.scale,s.scale),0,ratio(m[3],t.scale,s.scale),ratio(m[4],t.scale,s.scale),0};
        result.matrix[2]=std::fma(-result.matrix[0],s.center.x,std::fma(-result.matrix[1],s.center.y,std::fma(t.scale,m[2],t.center.x)));
        result.matrix[5]=std::fma(-result.matrix[3],s.center.x,std::fma(-result.matrix[4],s.center.y,std::fma(t.scale,m[5],t.center.y)));
    }
    result.validate();return result;
}
double rmse(const std::vector<LandmarkPair>& pairs,const std::vector<LandmarkResidual>& residuals,bool held) {
    double scale=0,sum=0;std::size_t count=0;
    for(std::size_t i=0;i<pairs.size();++i)if(pairs[i].held_out==held) {scale=std::max(scale,residuals[i].distance);++count;}
    if(!scale)return 0;for(std::size_t i=0;i<pairs.size();++i)if(pairs[i].held_out==held) {const auto v=residuals[i].distance/scale;sum+=v*v;}
    return scale*std::sqrt(sum/count);
}
std::string digest(const LandmarkFit& fit) {
    Sha256 sha;text(sha,"qpx.landmark-fit/1");sha.integer(static_cast<std::uint64_t>(fit.options().model));sha.integer(fit.options().reflect_source_y);sha.integer(static_cast<std::uint64_t>(fit.options().target_unit));
    sha.integer(fit.landmarks().size());for(const auto& pair:fit.landmarks()) {text(sha,pair.id);point(sha,pair.source);point(sha,pair.target);sha.integer(pair.held_out);}
    for(auto x:fit.transform().matrix)real(sha,x);for(const auto& residual:fit.residuals()) {point(sha,residual.delta);real(sha,residual.distance);}
    real(sha,fit.training_rmse());sha.integer(bool(fit.held_out_rmse()));if(fit.held_out_rmse())real(sha,*fit.held_out_rmse());return sha.finish();
}
using V=ImageDataValue;
template<class T>const T& typed(const V& value) {const auto* result=std::get_if<T>(&value.value);if(!result)throw std::invalid_argument("Invalid stored landmark type");return *result;}
const V& member(const V::Map& values,const char* key) {const auto at=values.find(key);if(at==values.end())throw std::invalid_argument("Missing stored landmark field");return at->second;}
}
void LandmarkFitOptions::validate() const {
    if(model!=LandmarkModel::Translation && model!=LandmarkModel::Similarity && model!=LandmarkModel::Affine)throw std::invalid_argument("Unknown landmark model");
    if(reflect_source_y && model!=LandmarkModel::Similarity)throw std::invalid_argument("Explicit reflection applies to similarity fitting");
    if(target_unit!=CoordinateUnit::Pixel && target_unit!=CoordinateUnit::Micrometre)throw std::invalid_argument("Unknown landmark residual unit");
}
struct LandmarkFit::Data {
    LandmarkFitOptions options;std::vector<LandmarkPair> pairs;std::vector<LandmarkResidual> residuals;Affine2D transform;
    double training=0;std::optional<double> held;std::string sha;
};
LandmarkFit::LandmarkFit(std::shared_ptr<const Data> data):data_(std::move(data)) {}
const LandmarkFitOptions& LandmarkFit::options() const {return data_->options;}
const std::vector<LandmarkPair>& LandmarkFit::landmarks() const {return data_->pairs;}
const std::vector<LandmarkResidual>& LandmarkFit::residuals() const {return data_->residuals;}
const Affine2D& LandmarkFit::transform() const {return data_->transform;}
double LandmarkFit::training_rmse() const {return data_->training;}
std::optional<double> LandmarkFit::held_out_rmse() const {return data_->held;}
const std::string& LandmarkFit::sha256() const {return data_->sha;}
LandmarkFit fit_landmarks(std::vector<LandmarkPair> pairs,LandmarkFitOptions options,std::size_t work,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);options.validate();if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid landmark work budget");
    if(pairs.empty() || pairs.size()>1024)throw std::invalid_argument("Select1..1024 landmark pairs");
    std::size_t bytes=512;if(pairs.capacity()>work/sizeof(LandmarkPair))throw WorkflowCaptureLimit("Landmark capacity exceeds work budget");
    workflow_capture_charge(bytes,pairs.capacity()*sizeof(LandmarkPair),work);workflow_capture_charge(bytes,pairs.size()*256,work);
    std::set<std::string> ids;std::size_t training=0,held=0;
    for(const auto& pair:pairs) {workflow_checkpoint(cancelled);name(pair.id);finite(pair.source);finite(pair.target);workflow_capture_charge(bytes,pair.id.capacity()+256,work);
        if(!ids.insert(pair.id).second)throw std::invalid_argument("Duplicate landmark ID");if(pair.held_out)++held;else ++training;}
    const auto minimum=options.model==LandmarkModel::Translation?1u:options.model==LandmarkModel::Similarity?2u:3u;
    if(training<minimum)throw std::invalid_argument("Insufficient training landmarks for selected model");
    auto data=std::make_shared<LandmarkFit::Data>();data->options=options;data->transform=fit_matrix(pairs,options,cancelled);data->pairs=std::move(pairs);data->residuals.reserve(data->pairs.size());
    for(const auto& pair:data->pairs) {workflow_checkpoint(cancelled);const auto mapped=data->transform.map(pair.source);const Point delta{mapped.x-pair.target.x,mapped.y-pair.target.y};
        const auto distance=std::hypot(delta.x,delta.y);if(!std::isfinite(distance))throw std::invalid_argument("Landmark residual overflow");data->residuals.push_back({delta,distance});}
    data->training=rmse(data->pairs,data->residuals,false);if(held)data->held=rmse(data->pairs,data->residuals,true);
    LandmarkFit result(data);data->sha=digest(result);workflow_checkpoint(cancelled);return result;
}
void accept_landmark_registration(Document& document,const std::string& id,const RegistrationRequest& request,const LandmarkFit& fit,const std::atomic<bool>* cancelled) {
    name(id);if(fit.transform().matrix!=request.definition().transform.matrix || fit.options().target_unit!=request.definition().target.unit)throw std::invalid_argument("Landmark fit differs from registration request");
    require_current_registration(document,request,cancelled);
    const auto base=with_registration(document.image_data_properties(),id,request.definition());auto values=base->values();V::Map fits;
    if(const auto at=values.find(fit_key);at!=values.end())fits=typed<V::Map>(at->second);
    V::List pairs,residuals;
    for(const auto& pair:fit.landmarks())pairs.push_back(V{V::List{V{pair.id},V{pair.source.x},V{pair.source.y},V{pair.target.x},V{pair.target.y},V{pair.held_out}}});
    for(const auto& residual:fit.residuals())residuals.push_back(V{V::List{V{residual.delta.x},V{residual.delta.y},V{residual.distance}}});
    fits[id]=V{V::Map{{"schema",V{std::string("qpx.landmark-fit/1")}},{"definition_sha256",V{request.definition().sha256()}},{"fit_sha256",V{fit.sha256()}},
        {"model",V{std::int64_t(fit.options().model)}},{"reflection",V{fit.options().reflect_source_y}},{"unit",V{std::int64_t(fit.options().target_unit)}},
        {"pairs",V{std::move(pairs)}},{"residuals",V{std::move(residuals)}},{"training_rmse",V{fit.training_rmse()}},{"held_out_rmse",fit.held_out_rmse()?V{*fit.held_out_rmse()}:V{}}}};
    values[fit_key]=V{std::move(fits)};values[active_key]=V{id};workflow_checkpoint(cancelled);
    document.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
}
std::shared_ptr<const LandmarkFit> stored_landmark_fit(std::shared_ptr<const ImageDataProperties> properties,const std::string& id) {
    name(id);if(!properties)return {};const auto at=properties->values().find(fit_key);if(at==properties->values().end())return {};
    const auto& fits=typed<V::Map>(at->second);const auto found=fits.find(id);if(found==fits.end())return {};
    const auto definition=stored_registration(properties,id);if(!definition)throw std::invalid_argument("Stored landmark fit lacks its registration");
    const auto& m=typed<V::Map>(found->second);
    if(m.size()!=10 || typed<std::string>(member(m,"schema"))!="qpx.landmark-fit/1" || typed<std::string>(member(m,"definition_sha256"))!=definition->sha256())throw std::invalid_argument("Stored landmark registration mismatch");
    const auto model=typed<std::int64_t>(member(m,"model")),unit=typed<std::int64_t>(member(m,"unit"));
    if(model<0 || model>2 || unit<0 || unit>1)throw std::invalid_argument("Unknown stored landmark model/unit");
    auto data=std::make_shared<LandmarkFit::Data>();data->options.model=static_cast<LandmarkModel>(model);
    data->options.reflect_source_y=typed<bool>(member(m,"reflection"));data->options.target_unit=static_cast<CoordinateUnit>(unit);data->options.validate();
    if(data->options.target_unit!=definition->target.unit)throw std::invalid_argument("Stored landmark units differ from target");data->transform=definition->transform;
    const auto& pairs=typed<V::List>(member(m,"pairs"));const auto& residuals=typed<V::List>(member(m,"residuals"));
    if(pairs.empty() || pairs.size()>1024 || residuals.size()!=pairs.size())throw std::invalid_argument("Invalid stored landmark shape");
    std::set<std::string> ids;std::size_t training=0,held=0;
    for(std::size_t i=0;i<pairs.size();++i) {
        const auto& p=typed<V::List>(pairs[i]);const auto& r=typed<V::List>(residuals[i]);if(p.size()!=6 || r.size()!=3)throw std::invalid_argument("Invalid stored landmark row");
        LandmarkPair pair{typed<std::string>(p[0]),{typed<double>(p[1]),typed<double>(p[2])},{typed<double>(p[3]),typed<double>(p[4])},typed<bool>(p[5])};name(pair.id);finite(pair.source);finite(pair.target);
        if(!ids.insert(pair.id).second)throw std::invalid_argument("Duplicate stored landmark ID");if(pair.held_out)++held;else ++training;
        LandmarkResidual residual{{typed<double>(r[0]),typed<double>(r[1])},typed<double>(r[2])};finite(residual.delta);if(!std::isfinite(residual.distance) || residual.distance<0)throw std::invalid_argument("Invalid stored landmark residual");
        const auto predicted=data->transform.map(pair.source);const Point delta{predicted.x-pair.target.x,predicted.y-pair.target.y};
        const auto close=[](double a,double b){return std::abs(a-b)<=1e-10*std::max({1.,std::abs(a),std::abs(b)});};
        if(!close(delta.x,residual.delta.x) || !close(delta.y,residual.delta.y) || !close(std::hypot(delta.x,delta.y),residual.distance))throw std::invalid_argument("Stored landmark residual disagrees with matrix");
        data->pairs.push_back(std::move(pair));data->residuals.push_back(residual);
    }
    if(training<(data->options.model==LandmarkModel::Translation?1u:data->options.model==LandmarkModel::Similarity?2u:3u))throw std::invalid_argument("Insufficient stored training landmarks");
    data->training=typed<double>(member(m,"training_rmse"));if(!std::holds_alternative<std::monostate>(member(m,"held_out_rmse").value))data->held=typed<double>(member(m,"held_out_rmse"));
    if(!std::isfinite(data->training) || data->training<0 || bool(data->held)!=bool(held) || (data->held && (!std::isfinite(*data->held) || *data->held<0)))throw std::invalid_argument("Invalid stored landmark summary");
    const auto close=[](double a,double b){return std::abs(a-b)<=1e-10*std::max({1.,std::abs(a),std::abs(b)});};
    if(!close(data->training,rmse(data->pairs,data->residuals,false)) ||
        (data->held && !close(*data->held,rmse(data->pairs,data->residuals,true))))throw std::invalid_argument("Stored landmark summary disagrees with residuals");
    LandmarkFit result(data);data->sha=digest(result);if(data->sha!=typed<std::string>(member(m,"fit_sha256")))throw std::invalid_argument("Stored landmark fit identity mismatch");
    return std::shared_ptr<const LandmarkFit>(new LandmarkFit(std::move(data)));
}
std::optional<std::string> active_registration_id(std::shared_ptr<const ImageDataProperties> properties) {
    if(!properties)return {};const auto at=properties->values().find(active_key);if(at==properties->values().end())return {};
    const auto& id=typed<std::string>(at->second);name(id);return id;
}
std::optional<RegistrationField> active_registration_field(const Document& document,ExpressionDataset dataset,std::size_t work,const std::atomic<bool>* cancelled) {
    const auto id=active_registration_id(document.image_data_properties());if(!id)return {};
    const auto definition=stored_registration(document.image_data_properties(),*id);if(!definition)throw std::invalid_argument("Active registration is missing");
    if(definition->source.source_id!=dataset.source_id())return {};
    const auto target=image_coordinate_frame(document,definition->target.id,definition->target.plane,cancelled);require_current_registration(*definition,dataset,target);
    return registration_field(RegistrationRequest(std::move(dataset),definition->source,definition->target,definition->transform,work),cancelled);
}
}
