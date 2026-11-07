#include "cell_contours.hpp"
#include "raster_boundary_runs.hpp"
#include "cell_numeric.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <stdexcept>
namespace qpx {
namespace {
using cell_detail::checkpoint;
}
struct CellContourKernel {
    static void run(CellResult& result,const std::atomic<bool>* cancelled) {
        const auto& p=result.parameters_;const auto count=result.features_.size();
        if(has_geos()) {
            const bool concurrent=count>1 && result.nuclei_->labels().size()>=256u*256u;
            if(concurrent) {
                // Independent immutable rasters and separate GEOS contexts.
                // Admission already charges both contour work allowances.
                auto nuclei=std::async(std::launch::async,[&] {
                    return trace_tissue(*result.nuclei_,p.contours,cancelled);
                });
                auto bodies=trace_tissue(*result.cells_,p.contours,cancelled);
                auto nucleus_contours=nuclei.get();
                result.nucleus_contours_=std::move(nucleus_contours);
                result.cell_contours_=std::move(bodies);
            } else {
                result.nucleus_contours_=trace_tissue(*result.nuclei_,p.contours,cancelled);
                result.cell_contours_=trace_tissue(*result.cells_,p.contours,cancelled);
            }
            if(result.nucleus_contours_->components().size()!=count || result.cell_contours_->components().size()!=count) throw std::runtime_error("cell contours did not preserve every label");
            if(!count) return;
            // Avoid full-mask scans for sparse large rasters; their few ordinary
            // GEOS predicates cost less. This selects execution, not semantics.
            if(result.nuclei_->labels().size()/count<=1024 &&
                verify_integer_cell_containment(*result.nuclei_,*result.nucleus_contours_,*result.cells_,*result.cell_contours_,cancelled)) return;
            // Predicates are independent and already lock their GEOS contexts.
            // Keep normalization and aggregate retained-size updates sequential.
            std::vector<std::uint8_t> covered(count);
            auto inspect=[&](std::size_t first,std::size_t last) {
                for(auto i=first;i<last;++i) {
                    checkpoint(cancelled);const auto& body=result.cell_contours_->components()[i];const auto& nucleus=result.nucleus_contours_->components()[i];
                    if(body.label!=i+1 || nucleus.label!=i+1) throw std::runtime_error("cell contour label validation failed");
                    covered[i]=body.geometry.covers(nucleus.geometry);
                }
            };
            if(concurrent) {
                auto checks=std::async(std::launch::async,[&]{inspect(0,count/2);});
                inspect(count/2,count);checks.get();
            } else inspect(0,count);
            for(std::size_t i=0;i<count;++i) {checkpoint(cancelled);const auto& body=result.cell_contours_->components()[i];const auto& nucleus=result.nucleus_contours_->components()[i];
                if(!covered[i]) {
                    // Independent ROI intersections can leave zero-area boundary
                    // remnants that fail GEOS Covers. Normalize this fresh body
                    // before publication, without relaxing containment predicates.
                    if(nucleus.geometry.boolean_operation(body.geometry,BooleanOp::Difference)) throw std::runtime_error("cell nucleus extends outside its body contour");
                    auto united=body.geometry.boolean_operation(nucleus.geometry,BooleanOp::Union);checkpoint(cancelled);
                    if(!united || !united->covers(body.geometry) || !united->covers(nucleus.geometry) || !result.nuclei_->roi().covers(*united)) throw std::runtime_error("cell contour boundary normalization failed");
                    std::size_t before=0,after=0;for(const auto& path:body.geometry.paths()) before+=path.size();for(const auto& path:united->paths()) after+=path.size();
                    // trace_tissue allocated mutable native storage; this result
                    // has not escaped the worker or acquired any external owner.
                    auto repaired=std::const_pointer_cast<TissueContours>(result.cell_contours_);
                    const auto vertices=repaired->vertices_-before+after,retained=repaired->retained_-body.geometry.payload_bytes()+united->payload_bytes();
                    if(vertices>p.contours.max_vertices || retained>p.contours.work_bytes) throw std::length_error("cell contour normalization exceeds geometry budget");
                    repaired->components_[i].geometry=std::move(*united);repaired->vertices_=vertices;repaired->retained_=retained;
                }
            }
        }
    }
    static void restore_archive(CellResult& result,const std::atomic<bool>* cancelled) {
        // Historical labels stay readable even if this build cannot reconstruct
        // derived geometry within their original contour allowances.
        try {run(result,cancelled);}
        catch(const std::length_error&) {result.nucleus_contours_.reset();result.cell_contours_.reset();}
        catch(const std::runtime_error&) {result.nucleus_contours_.reset();result.cell_contours_.reset();}
        checkpoint(cancelled);
    }
};
bool verify_integer_cell_containment(const TissueResult& nuclei,const TissueContours& nucleus_geometry,
    const TissueResult& bodies,const TissueContours& body_geometry,const std::atomic<bool>* cancelled) {
    checkpoint(cancelled);
    const auto* n=nuclei.nominal_sampling();const auto* b=bodies.nominal_sampling();
    if(!n || !b || n->source_request().downsample!=1 || b->source_request().downsample!=1 ||
        nuclei.roi().kind()!=RoiKind::Rectangle || !nuclei.roi().equals(bodies.roi()) ||
        nuclei.width()!=bodies.width() || nuclei.height()!=bodies.height()) return false;
    const auto integer=[](double v){return std::isfinite(v) && std::trunc(v)==v;};
    const auto roi=nuclei.roi().bounds();const auto ox=n->edge_x(0),oy=n->edge_y(0);
    const auto width=nuclei.width(),height=nuclei.height();
    if(!integer(roi.x) || !integer(roi.y) || !integer(roi.width) || !integer(roi.height) ||
        !integer(ox) || !integer(oy) || ox!=b->edge_x(0) || oy!=b->edge_y(0) ||
        std::abs(ox)>4503599627370496. || std::abs(oy)>4503599627370496. ||
        std::abs(ox+width)>4503599627370496. || std::abs(oy+height)>4503599627370496.) return false;
    const auto count=nuclei.components().size();
    if(count!=bodies.components().size() || count!=nucleus_geometry.components().size() || count!=body_geometry.components().size() ||
        nuclei.labels().size()!=bodies.labels().size()) throw std::runtime_error("integer cell geometry dimensions differ");
    if(!count) return true;
    for(std::size_t i=0;i<nuclei.labels().size();++i) {
        if((i&4095)==0) checkpoint(cancelled);
        if(nuclei.labels()[i] && nuclei.labels()[i]!=bodies.labels()[i]) throw std::runtime_error("cell body raster loses a nucleus");
    }
    auto agrees=[&](const TissueResult& mask,const TissueContours& contours) {
        std::vector<std::uint64_t> expected(count+1);
        detail::for_each_label_boundary_run(mask,[&](auto x0,auto y0,auto x1,auto y1,auto a,auto b) {
            const auto length=std::uint64_t(x1-x0+y1-y0);
            if(a>count || b>count) throw std::runtime_error("integer cell boundary label exceeds components");
            if(a) expected[a]+=length;if(b) expected[b]+=length;
        },cancelled);
        auto label_at=[&](std::int64_t x,std::int64_t y) {
            return x<0 || y<0 || x>=width || y>=height?std::uint32_t(0):mask.labels()[std::size_t(y)*std::size_t(width)+std::size_t(x)];
        };
        for(std::size_t i=0;i<count;++i) {
            checkpoint(cancelled);const auto label=std::uint32_t(i+1);const auto& component=contours.components()[i];
            if(component.label!=label || mask.components()[i].label!=label || component.pixels!=mask.components()[i].pixels)
                throw std::runtime_error("integer cell contour label or pixel count differs");
            if(component.geometry.area()!=double(component.pixels)) throw std::runtime_error("integer cell contour area differs from raster");
            std::uint64_t length=0;
            for(const auto& ring:component.geometry.paths()) for(std::size_t j=0;j<ring.size();++j) {
                const auto a=ring[j],b=ring[(j+1)%ring.size()];
                const auto ax=a.x-ox,ay=a.y-oy,bx=b.x-ox,by=b.y-oy;
                if(!integer(ax) || !integer(ay) || !integer(bx) || !integer(by) ||
                    ax<0 || ax>width || bx<0 || bx>width || ay<0 || ay>height || by<0 || by>height || (ax!=bx && ay!=by)) return false;
                if(ay==by) {
                    const auto y=std::int64_t(ay),first=std::int64_t(std::min(ax,bx)),last=std::int64_t(std::max(ax,bx));
                    for(auto x=first;x<last;++x) {
                        if((length++&4095)==0) checkpoint(cancelled);
                        if((label_at(x,y-1)==label)==(label_at(x,y)==label)) throw std::runtime_error("cell contour edge differs from raster");
                    }
                } else {
                    const auto x=std::int64_t(ax),first=std::int64_t(std::min(ay,by)),last=std::int64_t(std::max(ay,by));
                    for(auto y=first;y<last;++y) {
                        if((length++&4095)==0) checkpoint(cancelled);
                        if((label_at(x-1,y)==label)==(label_at(x,y)==label)) throw std::runtime_error("cell contour edge differs from raster");
                    }
                }
            }
            // Valid polygon boundaries cannot repeat unit edges. Membership
            // plus equal length therefore proves complete boundary equality.
            if(length!=expected[label]) throw std::runtime_error("cell contour boundary length differs from raster");
        }
        return true;
    };
    // Both valid, bounded orthogonal geometries exactly match their pixel
    // unions. Pixel-wise inclusion is consequently continuous containment,
    // including every boundary point; no general-purpose relate query is needed.
    return agrees(nuclei,nucleus_geometry) && agrees(bodies,body_geometry);
}
void restore_cell_contours(CellResult& result,const std::atomic<bool>* cancelled) {CellContourKernel::run(result,cancelled);}
void restore_archived_cell_contours(CellResult& result,const std::atomic<bool>* cancelled) {CellContourKernel::restore_archive(result,cancelled);}
}
