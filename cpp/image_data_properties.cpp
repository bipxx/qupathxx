#include "image_data_properties_codec.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
using V=ImageDataValue;using P=ImageDataProperties;
std::uint64_t bits(double v) {std::uint64_t b;std::memcpy(&b,&v,8);return b;}
double real(std::uint64_t b) {double v;std::memcpy(&v,&b,8);return v;}
struct Budget {
    std::size_t nodes=0,bytes=sizeof(P);
    void node(std::size_t depth) {if(depth>P::max_depth || ++nodes>P::max_nodes)throw std::invalid_argument("ImageData properties exceed depth/node limit");add(sizeof(V));}
    void count(std::size_t n) const {if(n>P::max_entries)throw std::invalid_argument("ImageData collection exceeds 4096 entries");}
    void add(std::size_t n) {if(n>P::max_payload-bytes)throw std::invalid_argument("ImageData properties exceed 1 MiB payload");bytes+=n;}
    void text(const std::string& s) {detail::bounded_utf8(s,4096);add(s.size());}
};
void normalize(V&,std::size_t,Budget&);
void normalize_map(V::Map& map,std::size_t depth,Budget& b) {
    b.count(map.size());for(auto& [k,v]:map) {b.text(k);b.add(96);normalize(v,depth+1,b);}
}
void normalize(V& v,std::size_t depth,Budget& b) {
    b.node(depth);
    if(auto* d=std::get_if<double>(&v.value)) {if(std::isnan(*d))*d=std::numeric_limits<double>::quiet_NaN();}
    else if(auto* s=std::get_if<std::string>(&v.value))b.text(*s);
    else if(auto* list=std::get_if<V::List>(&v.value)) {b.count(list->size());for(auto& item:*list)normalize(item,depth+1,b);}
    else if(auto* map=std::get_if<V::Map>(&v.value))normalize_map(*map,depth,b);
}
bool equal(const V& a,const V& b) {
    if(a.value.index()!=b.value.index())return false;
    if(const auto* x=std::get_if<double>(&a.value))return bits(*x)==bits(std::get<double>(b.value));
    if(const auto* x=std::get_if<V::List>(&a.value)) {const auto& y=std::get<V::List>(b.value);if(x->size()!=y.size())return false;for(std::size_t i=0;i<x->size();++i)if(!equal((*x)[i],y[i]))return false;return true;}
    if(const auto* x=std::get_if<V::Map>(&a.value)) {const auto& y=std::get<V::Map>(b.value);if(x->size()!=y.size())return false;auto j=y.begin();for(const auto& [k,v]:*x) {if(k!=j->first || !equal(v,j->second))return false;++j;}return true;}
    switch(a.value.index()) {
        case 0:return true;case 1:return std::get<bool>(a.value)==std::get<bool>(b.value);
        case 2:return std::get<std::int64_t>(a.value)==std::get<std::int64_t>(b.value);
        case 4:return std::get<std::string>(a.value)==std::get<std::string>(b.value);default:return false;
    }
}
struct Writer {
    std::vector<std::uint8_t> out{'Q','D','P',1};
    void integer(std::uint64_t n) {for(int i=0;i<8;++i)out.push_back(std::uint8_t(n>>(i*8)));}
    void text(const std::string& s) {integer(s.size());out.insert(out.end(),s.begin(),s.end());}
    void map(const V::Map& m) {integer(m.size());for(const auto& [k,v]:m) {text(k);value(v);}}
    void value(const V& v) {
        out.push_back(std::uint8_t(v.value.index()));
        switch(v.value.index()) {
            case 0:break;case 1:out.push_back(std::uint8_t(std::get<bool>(v.value)));break;
            case 2:{const auto n=std::get<std::int64_t>(v.value);std::uint64_t b;std::memcpy(&b,&n,8);integer(b);break;}
            case 3:integer(bits(std::get<double>(v.value)));break;
            case 4:text(std::get<std::string>(v.value));break;
            case 5:{const auto& a=std::get<V::List>(v.value);integer(a.size());for(const auto& item:a)value(item);break;}
            case 6:map(std::get<V::Map>(v.value));break;
        }
    }
};
struct Reader {
    const std::uint8_t* p;std::size_t left;Budget budget;
    std::uint8_t byte() {if(!left)throw std::invalid_argument("truncated ImageData property");--left;return *p++;}
    std::uint64_t integer() {std::uint64_t n=0;for(int i=0;i<8;++i)n|=std::uint64_t(byte())<<(i*8);return n;}
    std::size_t count() {const auto n=integer();if(n>P::max_entries)throw std::invalid_argument("ImageData collection exceeds 4096 entries");return std::size_t(n);}
    std::string text() {const auto n=integer();if(n>4096 || n>left)throw std::invalid_argument("invalid ImageData property text length");std::string s(reinterpret_cast<const char*>(p),std::size_t(n));p+=n;left-=std::size_t(n);budget.text(s);return s;}
    V::Map map(std::size_t depth) {const auto n=count();V::Map out;for(std::size_t i=0;i<n;++i) {auto k=text();budget.add(96);auto v=value(depth+1);if(!out.emplace(std::move(k),std::move(v)).second)throw std::invalid_argument("duplicate ImageData property key");}return out;}
    V value(std::size_t depth) {
        budget.node(depth);const auto tag=byte();
        switch(tag) {
            case 0:return {};
            case 1:{const auto b=byte();if(b>1)throw std::invalid_argument("invalid ImageData boolean");return V{bool(b)};}
            case 2:{const auto b=integer();std::int64_t n;std::memcpy(&n,&b,8);return V{n};}
            case 3:return V{real(integer())};case 4:return V{text()};
            case 5:{const auto n=count();V::List out;for(std::size_t i=0;i<n;++i)out.push_back(value(depth+1));return V{std::move(out)};}
            case 6:return V{map(depth)};
            default:throw std::invalid_argument("unknown ImageData property type");
        }
    }
};
}
ImageDataProperties::ImageDataProperties(V::Map values):values_(std::move(values)) {Budget b;b.node(0);normalize_map(values_,0,b);bytes_=b.bytes;}
bool ImageDataProperties::equals(const P& b) const {
    if(values_.size()!=b.values_.size())return false;auto j=b.values_.begin();for(const auto& [k,v]:values_) {if(k!=j->first || !equal(v,j->second))return false;++j;}return true;
}
namespace detail {
std::vector<std::uint8_t> encode_image_data_properties(const P& p) {Writer w;w.map(p.values());Sha256 h;h.update(w.out.data(),w.out.size());const auto sha=h.finish();w.out.insert(w.out.end(),sha.begin(),sha.end());return std::move(w.out);}
std::shared_ptr<const P> decode_image_data_properties(const void* data,std::size_t n) {
    if(!data || n<76 || n>2*P::max_payload)throw std::invalid_argument("invalid ImageData properties payload");
    const auto* p=static_cast<const std::uint8_t*>(data);if(std::memcmp(p,"QDP\1",4))throw std::invalid_argument("unsupported ImageData properties encoding");
    Sha256 h;h.update(p,n-64);if(h.finish()!=std::string(reinterpret_cast<const char*>(p+n-64),64))throw std::invalid_argument("ImageData properties checksum mismatch");
    Reader r{p+4,n-68,{}};r.budget.node(0);auto map=r.map(0);if(r.left)throw std::invalid_argument("trailing ImageData properties");
    auto result=std::make_shared<const P>(std::move(map));const auto canonical=encode_image_data_properties(*result);
    if(canonical.size()!=n || std::memcmp(canonical.data(),data,n))throw std::invalid_argument("noncanonical ImageData properties");return result;
}
}
}
