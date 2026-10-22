#include "geometry_archive.hpp"
#include <cstring>
#include <stdexcept>
namespace qpx {
namespace {
struct Encoder {
    std::vector<std::uint8_t> bytes={'Q','R','O',1};
    void u32(std::size_t value) { if(value>1000000) throw std::length_error("geometry count exceeds limit"); for(int i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i))); }
    void real(double v) { std::uint64_t bits; static_assert(sizeof(v)==sizeof(bits),"64-bit doubles required"); std::memcpy(&bits,&v,8); for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(bits>>(8*i))); }
    void ring(const Ring& ring) { u32(ring.size()); for(auto p:ring) { real(p.x); real(p.y); } }
};
struct Decoder {
    const std::uint8_t* p; std::size_t remaining,total=0;
    std::uint8_t byte() { if(!remaining) throw std::invalid_argument("truncated ROI blob"); --remaining; return *p++; }
    std::uint32_t u32() { std::uint32_t v=0; for(int i=0;i<4;++i) v|=std::uint32_t(byte())<<(8*i); return v; }
    double real() { std::uint64_t bits=0; for(int i=0;i<8;++i) bits|=std::uint64_t(byte())<<(8*i); double v; std::memcpy(&v,&bits,8); return v; }
    Ring ring() { auto n=u32(); if(n>1000000 || n>remaining/16 || total>1000000-n) throw std::invalid_argument("invalid persisted vertex count"); total+=n; Ring r; r.reserve(n); for(std::uint32_t i=0;i<n;++i) { auto x=real(),y=real(); r.emplace_back(x,y); } return r; }
};
}
std::vector<std::uint8_t> GeometryArchiveCodec::encode(const Roi& roi) {
    Encoder e; e.bytes.push_back(static_cast<std::uint8_t>(roi.kind()));
    if(roi.kind()==RoiKind::Rectangle || roi.kind()==RoiKind::Ellipse) { auto r=roi.bounds(); for(auto v:{r.x,r.y,r.width,r.height}) e.real(v); }
    else if(roi.is_area()) { auto parts=roi.polygon_parts(); e.u32(parts.size()); for(const auto& part:parts) { e.ring(part.shell); e.u32(part.holes.size()); for(const auto& hole:part.holes) e.ring(hole); } }
    else e.ring(roi.paths().at(0));
    return std::move(e.bytes);
}
Roi GeometryArchiveCodec::decode(const std::uint8_t* data,std::size_t size) {
    if(!data || size<5 || size>32u*1024u*1024u) throw std::invalid_argument("invalid ROI blob");
    Decoder d{data,size};
    if(d.byte()!='Q' || d.byte()!='R' || d.byte()!='O' || d.byte()!=1) throw std::invalid_argument("unsupported ROI encoding");
    auto kind=static_cast<RoiKind>(d.byte());
    auto roi=[&]()->Roi {
        if(kind==RoiKind::Rectangle || kind==RoiKind::Ellipse) { auto x=d.real(),y=d.real(),w=d.real(),h=d.real(); return kind==RoiKind::Rectangle?Roi::rectangle({x,y,w,h}):Roi::ellipse({x,y,w,h}); }
        if(kind==RoiKind::Polyline) return Roi::polyline(d.ring());
        if(kind==RoiKind::Points) return Roi::points(d.ring());
        if(kind!=RoiKind::Polygon && kind!=RoiKind::MultiPolygon) throw std::invalid_argument("unknown persisted ROI kind");
        auto count=d.u32(); if(!count || count>1000000 || count>d.remaining/8 || (kind==RoiKind::Polygon && count!=1) || (kind==RoiKind::MultiPolygon && count<2)) throw std::invalid_argument("invalid persisted polygon count");
        std::vector<PolygonPart> parts;
        for(std::uint32_t i=0;i<count;++i) { auto shell=d.ring(); auto n=d.u32(); if(n>d.remaining/4) throw std::invalid_argument("invalid hole count"); std::vector<Ring> holes; for(std::uint32_t j=0;j<n;++j) holes.push_back(d.ring()); parts.emplace_back(std::move(shell),std::move(holes)); }
        return Roi::multipolygon(std::move(parts));
    }();
    if(d.remaining) throw std::invalid_argument("trailing ROI blob bytes"); return roi;
}
}
