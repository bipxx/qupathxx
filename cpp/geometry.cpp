#include "qupathxx/geometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
#ifdef QUPATHXX_HAS_GEOS
#include <geos_c.h>
#endif

namespace qpx {
namespace {
constexpr double pi=3.14159265358979323846;
void finite(Point p) { if (!std::isfinite(p.x) || !std::isfinite(p.y)) throw std::invalid_argument("geometry coordinates must be finite"); }
bool same(Point a,Point b) { return a.x==b.x && a.y==b.y; }
bool equal_ring(const Ring& a,const Ring& b) {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) if(!same(a[i],b[i])) return false;
    return true;
}
void check_vertices(const Ring& r) {
    if(r.size()>1000000) throw std::invalid_argument("ROI exceeds vertex limit");
    for(auto p:r) finite(p);
}
Ring closed(Ring r) {
    check_vertices(r);
    if(r.size()>1 && same(r.front(),r.back())) r.pop_back();
    if(r.size()<3) throw std::invalid_argument("polygon ring requires three distinct vertices");
    for(std::size_t i=0;i<r.size();++i) if(same(r[i],r[(i+1)%r.size()])) throw std::invalid_argument("polygon ring has repeated adjacent vertices");
    return r;
}
Rect envelope(const std::vector<Ring>& rings) {
    double x=std::numeric_limits<double>::infinity(),y=x,right=-x,bottom=-x;
    std::size_t count=0;
    for(const auto& ring:rings) for(auto p:ring) { finite(p); ++count; x=std::min(x,p.x); y=std::min(y,p.y); right=std::max(right,p.x); bottom=std::max(bottom,p.y); }
    if(!count || count>1000000 || !std::isfinite(right-x) || !std::isfinite(bottom-y)) throw std::invalid_argument("invalid geometry extent or vertex count");
    return {x,y,right-x,bottom-y};
}
void append_ring(std::ostream& out,const Ring& ring,bool close) {
    out<<'(';
    for(std::size_t i=0;i<ring.size()+(close?1:0);++i) { if(i) out<<','; auto p=ring[i%ring.size()]; out<<p.x<<' '<<p.y; }
    out<<')';
}
Ring box(Rect r) { return {{r.x,r.y},{r.x+r.width,r.y},{r.x+r.width,r.y+r.height},{r.x,r.y+r.height}}; }
Ring ellipse_ring(Rect r) {
    Ring result; result.reserve(512);
    for(int i=0;i<512;++i) { double t=2*pi*i/512; result.emplace_back(r.x+r.width/2+r.width/2*std::cos(t),r.y+r.height/2+r.height/2*std::sin(t)); }
    return result;
}
double ellipse_length(double a,double b) {
    auto f=[&](double t){return std::hypot(a*std::sin(t),b*std::cos(t));};
    auto integrate=[&](auto&& self,double l,double r,double fl,double fm,double fr,double whole,double tolerance,int depth)->double {
        auto m=(l+r)/2,lm=(l+m)/2,rm=(m+r)/2;
        auto f1=f(lm),f2=f(rm);
        auto left=(m-l)*(fl+4*f1+fm)/6,right=(r-m)*(fm+4*f2+fr)/6;
        auto delta=left+right-whole;
        if(!depth || std::abs(delta)<=15*tolerance) return left+right+delta/15;
        return self(self,l,m,fl,f1,fm,left,tolerance/2,depth-1)+self(self,m,r,fm,f2,fr,right,tolerance/2,depth-1);
    };
    auto fl=f(0),fm=f(pi/4),fr=f(pi/2),whole=pi/2*(fl+4*fm+fr)/6;
    return 4*integrate(integrate,0,pi/2,fl,fm,fr,whole,std::max(a,b)*1e-12,24);
}
bool ellipse_covers_ellipse(Rect parent,Rect child) {
    auto a=child.width/parent.width,b=child.height/parent.height;
    auto dx=((child.x-parent.x)+(child.width-parent.width)/2)/(parent.width/2);
    auto dy=((child.y-parent.y)+(child.height-parent.height)/2)/(parent.height/2);
    auto aa=a*a,bb=b*b,bx=dx*a,by=dy*b,c=dx*dx+dy*dy;
    double maximum=0;
    if(aa==bb) maximum=aa+c+2*std::hypot(bx,by);
    else if(aa>bb && bx==0 && std::abs(by)<=aa-bb) maximum=aa+c+by*by/(aa-bb);
    else if(bb>aa && by==0 && std::abs(bx)<=bb-aa) maximum=bb+c+bx*bx/(bb-aa);
    else {
        double low=std::max(aa,bb),high=std::max(low+std::hypot(bx,by),std::nextafter(low,std::numeric_limits<double>::infinity()));
        for(int i=0;i<256;++i) {
            auto lambda=(low+high)/2; if(lambda==low || lambda==high) break;
            auto x=bx/(lambda-aa),y=by/(lambda-bb);
            if(x*x+y*y>1) low=lambda; else high=lambda;
        }
        auto lambda=high,x=bx/(lambda-aa),y=by/(lambda-bb),norm=std::hypot(x,y);
        if(!std::isfinite(norm) || norm==0) return false;
        x/=norm; y/=norm; maximum=aa*x*x+bb*y*y+2*bx*x+2*by*y+c;
    }
    return maximum<=1+64*std::numeric_limits<double>::epsilon();
}
#ifdef QUPATHXX_HAS_GEOS
struct Backend {
    GEOSContextHandle_t ctx=nullptr;
    GEOSGeometry* geometry=nullptr;
    const GEOSPreparedGeometry* prepared=nullptr;
    // GEOS contexts may be used by only one thread at a time, including
    // prepared-geometry queries that can populate internal caches.
    mutable std::mutex mutex;
    std::array<char,1024> error{};
    Backend() {
        ctx=GEOS_init_r(); if(!ctx) throw std::bad_alloc();
        GEOSContext_setErrorMessageHandler_r(ctx,[](const char* message,void* user) {
            std::snprintf(static_cast<Backend*>(user)->error.data(),1024,"%s",message);
        },this);
    }
    ~Backend() {
        if(prepared) GEOSPreparedGeom_destroy_r(ctx,prepared);
        if(geometry) GEOSGeom_destroy_r(ctx,geometry);
        if(ctx) GEOS_finish_r(ctx);
    }
    GEOSGeometry* read(const std::string& wkt) {
        auto deleter=[&](GEOSWKTReader* r) { GEOSWKTReader_destroy_r(ctx,r); };
        std::unique_ptr<GEOSWKTReader,decltype(deleter)> reader(GEOSWKTReader_create_r(ctx),deleter);
        if(!reader) throw std::bad_alloc();
        auto* g=GEOSWKTReader_read_r(ctx,reader.get(),wkt.c_str());
        if(!g) throw std::invalid_argument(std::string("GEOS geometry: ")+error.data());
        return g;
    }
    void initialize(const std::string& wkt) {
        geometry=read(wkt);
        if(GEOSisValid_r(ctx,geometry)!=1) {
            auto free_reason=[&](char* p){GEOSFree_r(ctx,p);};
            std::unique_ptr<char,decltype(free_reason)> reason(GEOSisValidReason_r(ctx,geometry),free_reason);
            std::string message=reason?reason.get():error.data();
            throw std::invalid_argument("invalid geometry: "+message);
        }
        if(GEOSisEmpty_r(ctx,geometry)!=0) throw std::invalid_argument("empty geometry is not an ROI");
        prepared=GEOSPrepare_r(ctx,geometry); if(!prepared) throw std::runtime_error(error.data());
    }
    bool predicate(char result) const { if(result==2) throw std::runtime_error(error.data()); return result==1; }
};
#endif
}
struct Roi::Data {
    RoiKind kind;
    Rect bounds;
    std::vector<Ring> paths;
    std::vector<std::size_t> polygon_sizes;
    double area=0,length=0;
    std::string wkt;
#ifdef QUPATHXX_HAS_GEOS
    std::shared_ptr<Backend> backend;
#endif
};
const Roi::Data& Roi::data() const { if(!data_) throw std::invalid_argument("ROI geometry is empty"); return *data_; }
RoiKind Roi::kind() const { return data().kind; }
Rect Roi::bounds() const { return data().bounds; }
double Roi::area() const { return data().area; }
double Roi::perimeter() const { const auto& d=data(); return d.kind==RoiKind::Ellipse?ellipse_length(d.bounds.width/2,d.bounds.height/2):d.length; }
bool Roi::is_area() const { return kind()!=RoiKind::Points && kind()!=RoiKind::Polyline; }
Roi Roi::rectangle(Rect r) {
    r.validate(); auto d=std::make_shared<Data>(); d->kind=RoiKind::Rectangle; d->bounds=r; d->area=r.area(); d->length=2*(r.width+r.height);
    if(d->area<=0 || !std::isfinite(d->length) || r.x+r.width<=r.x || r.y+r.height<=r.y) throw std::invalid_argument("rectangle metrics and extents must be representable");
    return Roi(d);
}
Roi Roi::ellipse(Rect r) {
    r.validate(); auto d=std::make_shared<Data>(); d->kind=RoiKind::Ellipse; d->bounds=r; d->area=r.area()*pi/4;
    if(d->area<=0 || !std::isfinite(2*(r.width+r.height)) || r.width/2==0 || r.height/2==0 || r.x+r.width<=r.x || r.y+r.height<=r.y)
        throw std::invalid_argument("ellipse metrics and extents must be representable");
    return Roi(d);
}
std::vector<Ring> Roi::paths() const {
    const auto& d=data(); if(d.kind==RoiKind::Rectangle) return {box(d.bounds)};
    if(d.kind==RoiKind::Ellipse) return {ellipse_ring(d.bounds)};
    return d.paths;
}
std::vector<PolygonPart> Roi::polygon_parts() const {
    const auto& d=data(); std::vector<PolygonPart> parts; std::size_t offset=0;
    for(auto count:d.polygon_sizes) {
        auto shell=d.paths[offset++]; std::vector<Ring> holes;
        for(std::size_t i=1;i<count;++i) holes.push_back(d.paths[offset++]);
        parts.emplace_back(std::move(shell),std::move(holes));
    }
    return parts;
}
std::string Roi::wkt() const {
    const auto& d=data(); if(!d.wkt.empty()) return d.wkt;
    std::ostringstream out; out.imbue(std::locale::classic()); out.precision(17);
    out<<"POLYGON("; append_ring(out,kind()==RoiKind::Rectangle?box(d.bounds):ellipse_ring(d.bounds),true); out<<')';
    return out.str();
}
Roi Roi::polygon(Ring shell,std::vector<Ring> holes) { return multipolygon({PolygonPart(std::move(shell),std::move(holes))}); }
Roi Roi::multipolygon(std::vector<PolygonPart> parts) {
#ifndef QUPATHXX_HAS_GEOS
    (void)parts; throw std::runtime_error("polygon topology requires QUPATHXX_BUILD_GEOS=ON");
#else
    if(parts.empty() || parts.size()>1000000) throw std::invalid_argument("multipolygon requires polygon parts");
    auto d=std::make_shared<Data>(); d->kind=parts.size()==1?RoiKind::Polygon:RoiKind::MultiPolygon;
    std::ostringstream out; out.imbue(std::locale::classic()); out.precision(17);
    out<<(parts.size()==1?"POLYGON(":"MULTIPOLYGON(");
    for(std::size_t i=0;i<parts.size();++i) {
        if(i) out<<','; if(parts.size()>1) out<<'(';
        auto shell=closed(std::move(parts[i].shell)); append_ring(out,shell,true); d->paths.push_back(std::move(shell));
        d->polygon_sizes.push_back(1+parts[i].holes.size());
        for(auto& hole:parts[i].holes) { out<<','; auto ring=closed(std::move(hole)); append_ring(out,ring,true); d->paths.push_back(std::move(ring)); }
        if(parts.size()>1) out<<')';
    }
    out<<')'; d->bounds=envelope(d->paths); d->wkt=out.str();
    d->backend=std::make_shared<Backend>(); d->backend->initialize(d->wkt);
    if(!GEOSArea_r(d->backend->ctx,d->backend->geometry,&d->area) || !GEOSLength_r(d->backend->ctx,d->backend->geometry,&d->length) ||
        !std::isfinite(d->area) || d->area<=0 || !std::isfinite(d->length))
        throw std::invalid_argument("polygon area/length must be finite and positive");
    return Roi(d);
#endif
}
Roi Roi::polyline(Ring points) {
    check_vertices(points); if(points.size()<2) throw std::invalid_argument("polyline requires at least two points");
    auto d=std::make_shared<Data>(); d->kind=RoiKind::Polyline; d->paths.push_back(std::move(points)); d->bounds=envelope(d->paths);
    const auto& p=d->paths.front();
    for(std::size_t i=1;i<p.size();++i) d->length+=std::hypot(p[i].x-p[i-1].x,p[i].y-p[i-1].y);
    if(!std::isfinite(d->length) || d->length<=0) throw std::invalid_argument("polyline must have positive finite length");
    std::ostringstream out; out.imbue(std::locale::classic()); out.precision(17); out<<"LINESTRING"; append_ring(out,p,false); d->wkt=out.str(); return Roi(d);
}
Roi Roi::points(Ring points) {
    check_vertices(points); if(points.empty()) throw std::invalid_argument("point ROI must not be empty");
    auto d=std::make_shared<Data>(); d->kind=RoiKind::Points; d->paths.push_back(std::move(points)); d->bounds=envelope(d->paths);
    std::ostringstream out; out.imbue(std::locale::classic()); out.precision(17); out<<"MULTIPOINT"; append_ring(out,d->paths.front(),false); d->wkt=out.str(); return Roi(d);
}
bool Roi::contains(Point p) const {
    finite(p); const auto& d=data();
    if(d.kind==RoiKind::Rectangle) return d.bounds.contains(p);
    if(d.kind==RoiKind::Ellipse) { auto x=(p.x-d.bounds.x-d.bounds.width/2)/(d.bounds.width/2),y=(p.y-d.bounds.y-d.bounds.height/2)/(d.bounds.height/2); return x*x+y*y<=1; }
    if(d.kind==RoiKind::Points || d.kind==RoiKind::Polyline) return distance(p)==0;
#ifdef QUPATHXX_HAS_GEOS
    std::lock_guard<std::mutex> lock(d.backend->mutex);
    return d.backend->predicate(GEOSPreparedIntersectsXY_r(d.backend->ctx,d.backend->prepared,p.x,p.y));
#else
    throw std::runtime_error("GEOS topology is disabled");
#endif
}
double Roi::distance(Point p) const {
    finite(p); const auto& d=data();
    if(d.kind==RoiKind::Rectangle) return std::hypot(std::max({d.bounds.x-p.x,0.,p.x-d.bounds.x-d.bounds.width}),std::max({d.bounds.y-p.y,0.,p.y-d.bounds.y-d.bounds.height}));
    if(d.kind==RoiKind::Ellipse && contains(p)) return 0;
    if(d.kind==RoiKind::Ellipse) {
        auto a=d.bounds.width/2,b=d.bounds.height/2;
        auto x=std::abs(p.x-d.bounds.x-a),y=std::abs(p.y-d.bounds.y-b);
        auto scale=std::max({a,b,x,y}); auto ax=a/scale,by=b/scale,px=x/scale,py=y/scale;
        auto a2=ax*ax,b2=by*by;
        if(a2>0 && b2>0) {
            // Solve the outside-point ellipse Lagrange multiplier with a bounded,
            // monotone root search. Scaling prevents coordinate-square overflow.
            double low=0,high=1;
            for(int i=0;i<1200;++i) {
                auto t=(low+high)/2,u=ax*px/(t+a2),v=by*py/(t+b2);
                if(t==low || t==high) break;
                if(u*u+v*v>1) low=t; else high=t;
                if(high-low<=1e-14*high) break;
            }
            auto t=(low+high)/2;
            return std::hypot(px-a2*px/(t+a2),py-b2*py/(t+b2))*scale;
        }
    }
    if(d.kind==RoiKind::Polygon || d.kind==RoiKind::MultiPolygon) {
#ifdef QUPATHXX_HAS_GEOS
        std::lock_guard<std::mutex> lock(d.backend->mutex);
        auto* point=GEOSGeom_createPointFromXY_r(d.backend->ctx,p.x,p.y); if(!point) throw std::bad_alloc();
        double result=0; auto ok=GEOSDistance_r(d.backend->ctx,d.backend->geometry,point,&result); GEOSGeom_destroy_r(d.backend->ctx,point);
        if(!ok) throw std::runtime_error(d.backend->error.data()); return result;
#else
        throw std::runtime_error("GEOS topology is disabled");
#endif
    }
    double result=std::numeric_limits<double>::infinity(); auto paths=this->paths();
    for(const auto& path:paths) {
        for(auto v:path) result=std::min(result,std::hypot(p.x-v.x,p.y-v.y));
        if(d.kind==RoiKind::Points) continue;
        auto count=path.size()+(d.kind==RoiKind::Ellipse?1:0);
        for(std::size_t i=1;i<count;++i) {
            auto a=path[i-1],b=path[i%path.size()]; auto dx=b.x-a.x,dy=b.y-a.y;
            auto scale=std::max(std::abs(dx),std::abs(dy)); if(!scale) continue;
            auto ux=dx/scale,uy=dy/scale;
            auto t=std::clamp(((p.x-a.x)/scale*ux+(p.y-a.y)/scale*uy)/(ux*ux+uy*uy),0.,1.);
            result=std::min(result,std::hypot(p.x-(a.x+t*dx),p.y-(a.y+t*dy)));
        }
    }
    return result;
}
bool Roi::covers(const Roi& other) const {
    auto a=bounds(),b=other.bounds();
    if(b.x<a.x || b.y<a.y || b.x+b.width>a.x+a.width || b.y+b.height>a.y+a.height) return false;
    if(kind()==RoiKind::Rectangle || equals(other)) return true;
    if(kind()==RoiKind::Ellipse && other.kind()==RoiKind::Ellipse) return ellipse_covers_ellipse(a,b);
    if((kind()==RoiKind::Polygon || kind()==RoiKind::MultiPolygon) && other.kind()==RoiKind::Ellipse) {
        Point center{b.x+b.width/2,b.y+b.height/2}; if(!contains(center)) return false;
        const auto rx=b.width/2,ry=b.height/2;
        // The connected ellipse is covered iff its center is covered and no
        // polygon boundary segment enters its open interior (tangency allowed).
        for(const auto& ring:data().paths) for(std::size_t i=0;i<ring.size();++i) {
            auto u=ring[i],v=ring[(i+1)%ring.size()];
            auto x=(u.x-center.x)/rx,y=(u.y-center.y)/ry,dx=(v.x-u.x)/rx,dy=(v.y-u.y)/ry;
            if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx) || !std::isfinite(dy)) throw std::invalid_argument("ellipse containment exceeds floating-point range");
            if((x>1 && x+dx>1) || (x<-1 && x+dx<-1) || (y>1 && y+dy>1) || (y<-1 && y+dy<-1)) continue;
            auto scale=std::max(std::abs(dx),std::abs(dy));
            auto ux=scale?dx/scale:0,uy=scale?dy/scale:0;
            auto t=scale?std::clamp(-((x/scale)*ux+(y/scale)*uy)/(ux*ux+uy*uy),0.,1.):0.;
            auto distance=std::hypot(x+t*dx,y+t*dy);
            if(distance<1-64*std::numeric_limits<double>::epsilon()) return false;
        }
        return true;
    }
    if(kind()==RoiKind::Points) {
        if(other.kind()!=RoiKind::Points) return false;
        for(auto p:other.data().paths.front()) if(distance(p)!=0) return false;
        return true;
    }
    if(kind()==RoiKind::Polyline && other.kind()==RoiKind::Points) {
        for(auto p:other.data().paths.front()) if(distance(p)!=0) return false;
        return true;
    }
    if(kind()==RoiKind::Ellipse && other.kind()!=RoiKind::Ellipse) {
        for(const auto& ring:other.paths()) for(auto point:ring) if(!contains(point)) return false;
        return true; // Ellipses are convex, so contained vertices imply contained segments.
    }
#ifdef QUPATHXX_HAS_GEOS
    auto backend=data().backend;
    if(!backend) { backend=std::make_shared<Backend>(); backend->initialize(wkt()); }
    std::lock_guard<std::mutex> lock(backend->mutex);
    auto deleter=[&](GEOSGeometry* g) { GEOSGeom_destroy_r(backend->ctx,g); };
    std::unique_ptr<GEOSGeometry,decltype(deleter)> geometry(backend->read(other.wkt()),deleter);
    return backend->predicate(GEOSPreparedCovers_r(backend->ctx,backend->prepared,geometry.get()));
#else
    throw std::runtime_error("mixed topology requires QUPATHXX_BUILD_GEOS=ON");
#endif
}
bool Roi::equals(const Roi& other) const {
    if(data_==other.data_) return true;
    const auto& a=data(); const auto& b=other.data();
    if(a.kind!=b.kind || a.bounds.x!=b.bounds.x || a.bounds.y!=b.bounds.y || a.bounds.width!=b.bounds.width || a.bounds.height!=b.bounds.height || a.polygon_sizes!=b.polygon_sizes || a.paths.size()!=b.paths.size()) return false;
    for(std::size_t i=0;i<a.paths.size();++i) if(!equal_ring(a.paths[i],b.paths[i])) return false;
    return true;
}
Roi Roi::translated(double dx,double dy) const {
    finite({dx,dy}); if(dx==0 && dy==0) return *this;
    auto b=bounds(); b.x+=dx; b.y+=dy;
    if(kind()==RoiKind::Rectangle) return rectangle(b);
    if(kind()==RoiKind::Ellipse) return ellipse(b);
    auto rings=paths(); for(auto& ring:rings) for(auto& point:ring) { point.x+=dx; point.y+=dy; finite(point); }
    if(kind()==RoiKind::Points) return points(std::move(rings.front()));
    if(kind()==RoiKind::Polyline) return polyline(std::move(rings.front()));
    std::vector<PolygonPart> parts; std::size_t offset=0;
    for(auto count:data().polygon_sizes) {
        auto shell=std::move(rings[offset++]); std::vector<Ring> holes;
        for(std::size_t i=1;i<count;++i) holes.push_back(std::move(rings[offset++]));
        parts.emplace_back(std::move(shell),std::move(holes));
    }
    return multipolygon(std::move(parts));
}
std::size_t Roi::payload_bytes() const {
    const auto& d=data(); std::size_t size=sizeof(Data)+d.wkt.size()+d.polygon_sizes.size()*sizeof(std::size_t);
    for(const auto& ring:d.paths) size+=sizeof(Ring)+ring.size()*sizeof(Point);
    return size;
}
bool has_geos() {
#ifdef QUPATHXX_HAS_GEOS
    return true;
#else
    return false;
#endif
}
std::string geos_version() {
#ifdef QUPATHXX_HAS_GEOS
    return GEOSversion();
#else
    return "disabled";
#endif
}
}
