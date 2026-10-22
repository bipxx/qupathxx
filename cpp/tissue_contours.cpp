#include "qupathxx/tissue.hpp"
#include "qupathxx/pixel_contours.hpp"
#include "qupathxx/provenance.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#ifdef QUPATHXX_HAS_GEOS
#include <geos_c.h>
#endif
namespace qpx {
namespace {
void checkpoint(const std::atomic<bool>* flag) {if(flag && flag->load(std::memory_order_relaxed)) throw std::runtime_error("tissue contour conversion cancelled");}
// Ordered adjacent-label pairs split straight runs at every raster junction.
// Thus diagonal contacts are noded without materializing one edge per pixel.
template<class Mask,class Emit> void boundary_runs(const Mask& mask,Emit&& emit,const std::atomic<bool>* cancelled) {
    const auto w=mask.width(),h=mask.height();const auto& labels=mask.labels();
    for(std::int64_t y=0;y<=h;++y) {
        checkpoint(cancelled);std::uint32_t a=0,b=0;std::int64_t start=0;
        for(std::int64_t x=0;x<=w;++x) {
            if((x&4095)==0) checkpoint(cancelled);
            auto next_a=x<w && y?labels[static_cast<std::size_t>((y-1)*w+x)]:0;
            auto next_b=x<w && y<h?labels[static_cast<std::size_t>(y*w+x)]:0;
            if(next_a==next_b) next_a=next_b=0;
            if(a!=next_a || b!=next_b) {if(a || b) emit(start,y,x,y);start=x;a=next_a;b=next_b;}
        }
    }
    struct Active {std::uint32_t a=0,b=0;std::int64_t start=0;};
    for(std::int64_t block=0;block<=w;block+=64) {
        std::array<Active,64> active{};const auto columns=std::min<std::int64_t>(64,w+1-block);
        for(std::int64_t y=0;y<=h;++y) {
            if((y&63)==0) checkpoint(cancelled);
            for(std::int64_t offset=0;offset<columns;++offset) {
                const auto x=block+offset;
                auto a=y<h && x?labels[static_cast<std::size_t>(y*w+x-1)]:0;
                auto b=y<h && x<w?labels[static_cast<std::size_t>(y*w+x)]:0;
                if(a==b) a=b=0;
                auto& prior=active[offset];
                if(prior.a!=a || prior.b!=b) {if(prior.a || prior.b) emit(x,prior.start,x,y);prior={a,b,y};}
            }
        }
    }
}
#ifdef QUPATHXX_HAS_GEOS
struct Destroy {GEOSContextHandle_t ctx;void operator()(GEOSGeometry* g) const {if(g) GEOSGeom_destroy_r(ctx,g);}};
using Geometry=std::unique_ptr<GEOSGeometry,Destroy>;
struct Backend {
    GEOSContextHandle_t ctx=nullptr;
    std::array<char,1024> error{};
    const std::atomic<bool>* cancelled;
    explicit Backend(const std::atomic<bool>* flag):cancelled(flag) {
        ctx=GEOS_init_r();if(!ctx) throw std::bad_alloc();
        GEOSContext_setErrorMessageHandler_r(ctx,[](const char* message,void* user){std::snprintf(static_cast<Backend*>(user)->error.data(),1024,"%s",message);},this);
#if GEOS_VERSION_MAJOR>3 || (GEOS_VERSION_MAJOR==3 && GEOS_VERSION_MINOR>=14)
        GEOSContext_setInterruptCallback_r(ctx,[](void* user)->int {auto* b=static_cast<Backend*>(user);return b->cancelled && b->cancelled->load(std::memory_order_relaxed);},this);
#endif
    }
    ~Backend() {GEOS_finish_r(ctx);}
    Geometry own(GEOSGeometry* value) {
        if(!value) {checkpoint(cancelled);throw std::runtime_error(std::string("tissue contour topology: ")+error.data());}
        return Geometry(value,Destroy{ctx});
    }
    Geometry read(const std::string& text) {
        auto deleter=[&](GEOSWKTReader* r){GEOSWKTReader_destroy_r(ctx,r);};
        std::unique_ptr<GEOSWKTReader,decltype(deleter)> reader(GEOSWKTReader_create_r(ctx),deleter);
        if(!reader) throw std::bad_alloc();return own(GEOSWKTReader_read_r(ctx,reader.get(),text.c_str()));
    }
};
bool same(Point a,Point b) {return a.x==b.x && a.y==b.y;}
bool straight(Point a,Point b,Point c) {
    return ((a.x==b.x && b.x==c.x) || (a.y==b.y && b.y==c.y));
}
Ring read_ring(Backend& backend,const GEOSGeometry* ring,std::size_t& extracted,std::size_t limit) {
    const auto sequence=GEOSGeom_getCoordSeq_r(backend.ctx,ring);unsigned count=0;
    if(!sequence || !GEOSCoordSeq_getSize_r(backend.ctx,sequence,&count) || count<4) throw std::runtime_error("invalid tissue contour ring");
    if(count-1>limit-extracted) throw std::length_error("tissue contour vertex limit exceeded");
    extracted+=count-1;Ring output;output.reserve(count-1);
    for(unsigned i=0;i+1<count;++i) {
        if((i&4095)==0) checkpoint(backend.cancelled);
        double x,y;if(!GEOSCoordSeq_getXY_r(backend.ctx,sequence,i,&x,&y)) throw std::runtime_error("could not read tissue contour coordinates");
        Point p{x,y};if(!output.empty() && same(output.back(),p)) continue;
        while(output.size()>1 && straight(output[output.size()-2],output.back(),p)) output.pop_back();
        output.push_back(p);
    }
    while(output.size()>3 && straight(output[output.size()-2],output.back(),output.front())) output.pop_back();
    std::size_t first=0;while(output.size()-first>3 && straight(output.back(),output[first],output[first+1])) ++first;
    if(first) output.erase(output.begin(),output.begin()+first);
    return output;
}
void extract(Backend& backend,const GEOSGeometry* geometry,std::vector<PolygonPart>& parts,std::size_t& vertices,std::size_t limit,int depth=0) {
    checkpoint(backend.cancelled);if(depth>256) throw std::length_error("tissue contour geometry nesting limit exceeded");
    const auto empty=GEOSisEmpty_r(backend.ctx,geometry);if(empty==1) return;if(empty!=0) throw std::runtime_error("invalid tissue contour emptiness query");
    const auto type=GEOSGeomTypeId_r(backend.ctx,geometry);
    if(type==GEOS_POLYGON) {
        auto shell=read_ring(backend,GEOSGetExteriorRing_r(backend.ctx,geometry),vertices,limit);
        const auto holes=GEOSGetNumInteriorRings_r(backend.ctx,geometry);if(holes<0) throw std::runtime_error("invalid tissue contour holes");
        std::vector<Ring> rings;rings.reserve(holes);
        for(int i=0;i<holes;++i) rings.push_back(read_ring(backend,GEOSGetInteriorRingN_r(backend.ctx,geometry,i),vertices,limit));
        parts.emplace_back(std::move(shell),std::move(rings));
    } else if(type==GEOS_MULTIPOLYGON || type==GEOS_GEOMETRYCOLLECTION) {
        const auto count=GEOSGetNumGeometries_r(backend.ctx,geometry);if(count<0) throw std::runtime_error("invalid tissue contour collection");
        for(int i=0;i<count;++i) extract(backend,GEOSGetGeometryN_r(backend.ctx,geometry,i),parts,vertices,limit,depth+1);
    } else if(type<0) throw std::runtime_error("invalid tissue contour geometry");
    // Clipping can also produce zero-area points/lines, which are not annotations.
}
#endif
}
void TissueContourOptions::validate() const {
    if(!max_components || max_components>1000000 || !max_edges || max_edges>1000000 || !max_vertices || max_vertices>1000000)
        throw std::invalid_argument("tissue contour limits must be 1..1000000");
    if(!work_bytes || work_bytes>512u*1024u*1024u) throw std::invalid_argument("tissue contour work budget must be 1 byte..512 MiB");
}
struct TissueContourKernel {
    template<class Mask> static std::shared_ptr<const TissueContours> run(const Mask& mask,TissueContourOptions options,const std::atomic<bool>* cancelled) {
        options.validate();checkpoint(cancelled);
#ifndef QUPATHXX_HAS_GEOS
        (void)mask;throw std::runtime_error("tissue contours require QUPATHXX_BUILD_GEOS=ON");
#else
        const auto began=std::chrono::steady_clock::now();
        if(mask.components().size()>options.max_components) throw std::length_error("tissue contour component limit exceeded");
        const auto base=65536ull+mask.components().size()*512ull+(options.clip_to_roi?2ull*mask.roi().payload_bytes():0);
        if(base>options.work_bytes) throw std::length_error("tissue contour work budget exceeded");
        auto result=std::shared_ptr<TissueContours>(new TissueContours());result->options_=options;result->input_sha256_=mask.input_sha256();result->object_id_=mask.object_id();
        if(mask.foreground_pixels()) boundary_runs(mask,[&](auto,auto,auto,auto) {
            if(++result->edges_>options.max_edges) throw std::length_error("tissue contour edge limit exceeded");
            if(base+result->edges_*512ull>options.work_bytes) throw std::length_error("tissue contour work budget exceeded");
        },cancelled);
        result->work_=static_cast<std::size_t>(base+result->edges_*512ull);result->components_.reserve(mask.components().size());
        result->retained_=sizeof(TissueContours)+result->components_.capacity()*sizeof(TissueContour)+result->input_sha256_.capacity();
        if(result->edges_) {
            Backend backend(cancelled);std::vector<Geometry> owned;std::vector<const GEOSGeometry*> lines;
            owned.reserve(result->edges_);lines.reserve(result->edges_);
            boundary_runs(mask,[&](std::int64_t x0,std::int64_t y0,std::int64_t x1,std::int64_t y1) {
                auto destroy=[&](GEOSCoordSequence* s){GEOSCoordSeq_destroy_r(backend.ctx,s);};
                std::unique_ptr<GEOSCoordSequence,decltype(destroy)> seq(GEOSCoordSeq_create_r(backend.ctx,2,2),destroy);
                if(!seq || !GEOSCoordSeq_setXY_r(backend.ctx,seq.get(),0,double(x0),double(y0)) || !GEOSCoordSeq_setXY_r(backend.ctx,seq.get(),1,double(x1),double(y1))) throw std::runtime_error("could not build tissue contour edge");
                auto line=backend.own(GEOSGeom_createLineString_r(backend.ctx,seq.release()));lines.push_back(line.get());owned.push_back(std::move(line));
            },cancelled);
            checkpoint(cancelled);auto faces=backend.own(GEOSPolygonize_r(backend.ctx,lines.data(),static_cast<unsigned>(lines.size())));
            const auto image=mask.image_bounds();auto clip=backend.own(GEOSGeom_createRectangle_r(backend.ctx,0,0,image.width,image.height));
            if(options.clip_to_roi) {auto roi=backend.read(mask.roi().wkt());clip=backend.own(GEOSIntersection_r(backend.ctx,clip.get(),roi.get()));}
            struct Mapping {Rect bounds;double scale;const std::atomic<bool>* cancelled;};Mapping mapping{mask.bounds(),mask.downsample(),cancelled};
            const auto far_x=mapping.bounds.x+(mask.width()-1)*mapping.scale,far_y=mapping.bounds.y+(mask.height()-1)*mapping.scale;
            if(!std::isfinite(far_x+mapping.scale) || !std::isfinite(far_y+mapping.scale) || mapping.bounds.x+mapping.scale<=mapping.bounds.x || mapping.bounds.y+mapping.scale<=mapping.bounds.y || far_x+mapping.scale<=far_x || far_y+mapping.scale<=far_y)
                throw std::invalid_argument("tissue contour grid loses coordinate precision");
            std::vector<std::vector<PolygonPart>> grouped(mask.components().size());std::vector<std::uint64_t> areas(mask.components().size());
            std::size_t extracted=0;const auto count=GEOSGetNumGeometries_r(backend.ctx,faces.get());if(count<0) throw std::runtime_error("invalid tissue contour faces");
            for(int i=0;i<count;++i) {
                checkpoint(cancelled);const auto* face=GEOSGetGeometryN_r(backend.ctx,faces.get(),i);
                auto point=backend.own(GEOSPointOnSurface_r(backend.ctx,face));double x,y;
                if(!GEOSGeomGetX_r(backend.ctx,point.get(),&x) || !GEOSGeomGetY_r(backend.ctx,point.get(),&y) || !std::isfinite(x) || !std::isfinite(y) || x<0 || y<0 || x>=mask.width() || y>=mask.height()) throw std::runtime_error("invalid tissue contour interior point");
                const auto label=mask.labels()[static_cast<std::size_t>(std::floor(y))*static_cast<std::size_t>(mask.width())+static_cast<std::size_t>(std::floor(x))];if(!label) continue;
                if(label>grouped.size() || GEOSisValid_r(backend.ctx,face)!=1) throw std::runtime_error("invalid tissue contour face");
                double area=0;if(!GEOSArea_r(backend.ctx,face,&area) || !std::isfinite(area) || area<0 || area>mask.foreground_pixels() || std::abs(area-std::round(area))>1e-6) throw std::runtime_error("invalid tissue contour raster area");
                areas[label-1]+=static_cast<std::uint64_t>(std::llround(area));
                auto world=backend.own(GEOSGeom_transformXY_r(backend.ctx,face,[](double* xx,double* yy,void* user)->int {
                    const auto& m=*static_cast<Mapping*>(user);if(m.cancelled && m.cancelled->load(std::memory_order_relaxed)) return 0;
                    *xx=m.bounds.x+*xx*m.scale;*yy=m.bounds.y+*yy*m.scale;return std::isfinite(*xx) && std::isfinite(*yy);
                },&mapping));
                auto clipped=backend.own(GEOSIntersection_r(backend.ctx,world.get(),clip.get()));
                extract(backend,clipped.get(),grouped[label-1],extracted,options.max_vertices);
            }
            for(std::size_t i=0;i<grouped.size();++i) {
                checkpoint(cancelled);const auto& component=mask.components()[i];
                if(areas[i]!=component.pixels) throw std::runtime_error("tissue contour area does not match label raster");
                if(grouped[i].empty()) continue;
                auto geometry=Roi::multipolygon(std::move(grouped[i]));
                for(const auto& path:geometry.paths()) result->vertices_+=path.size();
                result->retained_+=geometry.payload_bytes();if(result->retained_>options.work_bytes) throw std::length_error("tissue contour retained geometry exceeds work budget");
                result->components_.push_back({component.label,component.pixels,std::move(geometry)});
            }
        }
        checkpoint(cancelled);result->elapsed_ms_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
#endif
    }
};
namespace {
// Borrow the immutable raster; only the <=256-entry class summary is copied.
struct PixelRaster {
    const PixelPredictionResult& result;
    struct Component {std::uint32_t label;std::uint64_t pixels;};
    std::vector<Component> classes;
    explicit PixelRaster(const PixelPredictionResult& r):result(r) {
        for(std::size_t i=0;i<r.class_counts().size();++i) classes.push_back({static_cast<std::uint32_t>(i+1),r.class_counts()[i]});
    }
    const auto& components() const {return classes;}
    const auto& labels() const {return result.labels();}
    auto width() const {return result.width();} auto height() const {return result.height();}
    auto foreground_pixels() const {return result.classified_pixels();}
    const auto& roi() const {return result.roi();}
    auto bounds() const {return result.bounds();} auto downsample() const {return result.downsample();}
    Rect image_bounds() const {const auto& s=result.provenance()->source;return {0,0,double(s.width),double(s.height)};}
    const auto& input_sha256() const {return result.sha256();}
    auto object_id() const {return result.object_id();}
};
}
std::shared_ptr<const PixelContours> trace_pixel_prediction(std::shared_ptr<const PixelPredictionResult> result,TissueContourOptions options,const std::atomic<bool>* cancelled) {
    if(!result) throw std::invalid_argument("pixel prediction is required");
    auto contours=TissueContourKernel::run(PixelRaster(*result),options,cancelled);
    return std::shared_ptr<const PixelContours>(new PixelContours(std::move(result),std::move(contours)));
}
std::vector<std::uint64_t> publish_pixel_contours(Document& document,const PixelPredictionRequest& request,const PixelContours& traced,std::string prefix) {
    const auto& result=*traced.prediction_;
    if(!request.matches(document) || !result.matches(request)) throw std::logic_error("pixel contours no longer match the current document and request");
    std::vector<ObjectEdit> edits;edits.reserve(traced.contours_->components().size());
    const auto parent=traced.contours_->options().clip_to_roi?result.object_id():0;
    for(const auto& c:traced.contours_->components()) {
        const auto& classification=result.model().classes().at(c.label-1);
        edits.push_back(ObjectEdit::create_geometry(c.geometry,prefix+" "+classification,parent,classification));
    }
    return document.execute(edits,"Add pixel classifier annotations");
}
std::shared_ptr<const TissueContours> trace_tissue(const TissueResult& result,TissueContourOptions options,const std::atomic<bool>* cancelled) {return TissueContourKernel::run(result,options,cancelled);}
}
