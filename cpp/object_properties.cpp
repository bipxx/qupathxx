#include "object_properties_codec.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
std::uint64_t bits(double v) {std::uint64_t n;std::memcpy(&n,&v,8);return n;}
double real(std::uint64_t n) {double v;std::memcpy(&v,&n,8);return v;}
constexpr std::size_t max_entries=4096,max_payload=1024*1024;
}
struct ObjectProperties::Facts {
    std::map<std::string,std::string> metadata;
    std::map<std::string,double> measurements;
};
const std::map<std::string,std::string>& ObjectProperties::metadata() const {return facts_->metadata;}
const std::map<std::string,double>& ObjectProperties::measurements() const {return facts_->measurements;}
ObjectProperties::ObjectProperties(std::map<std::string,std::string> metadata,std::map<std::string,double> measurements,bool locked,std::optional<std::uint32_t> color,std::optional<double> probability)
    :locked_(locked),color_(color),probability_(probability),bytes_(sizeof(ObjectProperties)+sizeof(Facts)) {
    if(color && *color>0xffffff)throw std::invalid_argument("object color must be 24-bit RGB");
    if(probability_ && std::isnan(*probability_))probability_.reset();
    if(metadata.size()>max_entries || measurements.size()>max_entries-metadata.size())throw std::length_error("object properties exceed 4096 entries");
    auto name=[&](const std::string& s) {detail::bounded_utf8(s,4096);if(s.empty())throw std::invalid_argument("object property name must not be empty");};
    for(const auto& [k,v]:metadata) {name(k);detail::bounded_utf8(v,4096);bytes_+=128+k.size()+v.size();}
    for(auto& [k,v]:measurements) {name(k);if(std::isnan(v))v=std::numeric_limits<double>::quiet_NaN();bytes_+=96+k.size();}
    if(bytes_>max_payload)throw std::length_error("object properties exceed 1 MiB payload");
    facts_=std::make_shared<const Facts>(Facts{std::move(metadata),std::move(measurements)});
}
std::shared_ptr<const ObjectProperties> ObjectProperties::with_edit_state(bool locked,std::optional<std::uint32_t> color,std::optional<double> probability) const {
    if(color && *color>0xffffff)throw std::invalid_argument("object color must be 24-bit RGB");
    auto result=std::make_shared<ObjectProperties>(*this);result->locked_=locked;result->color_=color;result->probability_=probability;
    if(result->probability_ && std::isnan(*result->probability_))result->probability_.reset();return result;
}
bool ObjectProperties::equals(const ObjectProperties& other) const {
    if(locked_!=other.locked_ || color_!=other.color_ || probability_.has_value()!=other.probability_.has_value() || (probability_ && bits(*probability_)!=bits(*other.probability_)))return false;
    if(facts_==other.facts_)return true;
    if(metadata()!=other.metadata() || measurements().size()!=other.measurements().size())return false;
    auto b=other.measurements().begin();
    for(const auto& [k,v]:measurements()) {if(k!=b->first || bits(v)!=bits(b->second))return false;++b;}return true;
}
namespace detail {
std::vector<std::uint8_t> encode_object_properties(const ObjectProperties& value) {
    const bool state=value.locked() || value.color_rgb() || value.class_probability();
    std::vector<std::uint8_t> out{'Q','O','P',std::uint8_t(state?2:1)};
    auto integer=[&](std::uint64_t v) {for(unsigned i=0;i<8;++i)out.push_back(std::uint8_t(v>>(8*i)));};
    auto text=[&](const std::string& s) {integer(s.size());out.insert(out.end(),s.begin(),s.end());};
    integer(value.metadata().size());for(const auto& [k,v]:value.metadata()){text(k);text(v);}
    integer(value.measurements().size());for(const auto& [k,v]:value.measurements()){text(k);integer(bits(v));}
    if(state) {integer((value.locked()?1:0)|(value.color_rgb()?2:0)|(value.class_probability()?4:0));if(value.color_rgb())integer(*value.color_rgb());if(value.class_probability())integer(bits(*value.class_probability()));}
    Sha256 hash;hash.update(out.data(),out.size());const auto digest=hash.finish();out.insert(out.end(),digest.begin(),digest.end());return out;
}
std::shared_ptr<const ObjectProperties> decode_object_properties(const void* data,std::size_t size,bool allow_edit_state) {
    if(!data || size<84 || size>2*max_payload)throw std::invalid_argument("invalid object properties payload");
    const auto* p=static_cast<const std::uint8_t*>(data);std::size_t at=4;
    if(p[0]!='Q' || p[1]!='O' || p[2]!='P' || (p[3]!=1 && (p[3]!=2 || !allow_edit_state)))throw std::invalid_argument("unsupported object properties encoding");
    Sha256 hash;hash.update(p,size-64);if(hash.finish()!=std::string(reinterpret_cast<const char*>(p+size-64),64))throw std::invalid_argument("object properties checksum mismatch");
    auto integer=[&]() {if(at>size-64 || size-64-at<8)throw std::invalid_argument("truncated object properties");std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(p[at++])<<(8*i);return n;};
    auto text=[&]() {const auto n=integer();if(n>4096 || n>size-64-at)throw std::invalid_argument("invalid object property text");std::string s(reinterpret_cast<const char*>(p+at),std::size_t(n));at+=std::size_t(n);return s;};
    std::map<std::string,std::string> metadata;std::map<std::string,double> measurements;
    auto n=integer();if(n>max_entries)throw std::invalid_argument("invalid object metadata count");
    for(std::size_t i=0;i<n;++i) {auto k=text(),v=text();if(!metadata.emplace(std::move(k),std::move(v)).second)throw std::invalid_argument("duplicate object metadata");}
    n=integer();if(n>max_entries-metadata.size())throw std::invalid_argument("invalid object measurement count");
    for(std::size_t i=0;i<n;++i) {auto k=text();const auto v=real(integer());if(!measurements.emplace(std::move(k),v).second)throw std::invalid_argument("duplicate object measurement");}
    bool locked=false;std::optional<std::uint32_t> color;std::optional<double> probability;
    if(p[3]==2) {const auto flags=integer();if(flags>7)throw std::invalid_argument("invalid object editing flags");locked=bool(flags&1);
        if(flags&2) {const auto v=integer();if(v>0xffffff)throw std::invalid_argument("invalid object RGB color");color=std::uint32_t(v);}if(flags&4)probability=real(integer());}
    if(at!=size-64)throw std::invalid_argument("trailing object properties");
    auto result=std::make_shared<const ObjectProperties>(std::move(metadata),std::move(measurements),locked,color,probability);
    const auto canonical=encode_object_properties(*result);if(canonical.size()!=size || std::memcmp(canonical.data(),data,size))throw std::invalid_argument("noncanonical object properties");return result;
}
}
}
