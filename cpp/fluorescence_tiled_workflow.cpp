#include "qupathxx/fluorescence_tiled_workflow.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/workflow.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "fluorescence_cell_internal.hpp"
#include "scientific_cell_numeric.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace qpx {
namespace {
void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
void real(Sha256& h,double x){if(x==0)x=0;std::uint64_t bits;std::memcpy(&bits,&x,8);h.integer(bits);}
void file(Sha256& h,const FileVersion& f) {
    text(h,f.path);text(h,f.identity_scheme);text(h,f.identity);text(h,f.stamp_scheme);text(h,f.modified_token);text(h,f.change_token);h.integer(f.bytes);
}
void region(Sha256& h,const RawRegionRequest& r) {
    for(auto v:{std::uint64_t(r.region.x),std::uint64_t(r.region.y),std::uint64_t(r.region.width),std::uint64_t(r.region.height),std::uint64_t(r.first_channel),std::uint64_t(r.channels),std::uint64_t(r.z),std::uint64_t(r.t)})h.integer(v);
}
void parameters(Sha256& h,const FluorescenceCellOptions& p) {
    h.integer(p.segmentation_channel);h.integer(std::uint64_t(p.threshold_method));h.integer(std::uint64_t(p.units));
    for(auto v:{p.threshold,p.sigma,p.peak_prominence,p.min_nucleus_area,p.cell_expansion})real(h,v);
    h.integer(bool(p.max_nucleus_area));if(p.max_nucleus_area)real(h,*p.max_nucleus_area);h.integer(p.max_cells);h.integer(p.work_bytes);
}
bool inside(std::int64_t x,std::int64_t y,const RegionRequest& r){return x>=r.x && y>=r.y && x-r.x<r.width && y-r.y<r.height;}
}
void FluorescenceTileOptions::validate()const {
    if(!width || !height || width>4096 || height>4096 || halo>4096 || !max_tiles || max_tiles>100000)
        throw std::invalid_argument("Invalid fluorescence tile dimensions/halo/count limit");
}
struct FluorescenceTiledRequest::Data {
    std::thread::id owner=std::this_thread::get_id();RawAnalysisRequest authority;
    std::shared_ptr<ImageServer> source;ScientificRawLattice lattice;RawPixelSourceSnapshot snapshot;
    RawRegionRequest analysis;FluorescenceCellOptions options;FluorescenceTileOptions tiles;
    std::optional<std::array<double,2>> calibration;std::size_t columns=0,count=0,fixed=0;
    std::string sha,source_sha;
    Data(const Document& doc,RawRegionRequest selection,std::uint64_t object,FluorescenceCellOptions p,FluorescenceTileOptions t):
        authority(doc,selection,object,false,256,1000000000000ULL),source(doc.server()),
        lattice(doc.metadata().width,doc.metadata().height,ScientificGridAxis(0,doc.metadata().width,1),ScientificGridAxis(0,doc.metadata().height,1)),
        snapshot(raw_pixel_capture_lattice_source(doc,lattice,p.work_bytes)),analysis(authority.region()),options(p),tiles(t) {
        p.validate();t.validate();analysis.validate(snapshot.metadata);const auto& r=analysis.region;
        if(r.level || r.x<0 || r.y<0 || r.x>=snapshot.metadata.width || r.y>=snapshot.metadata.height ||
            r.width>snapshot.metadata.width-r.x || r.height>snapshot.metadata.height-r.y)throw std::invalid_argument("Tiled fluorescence requires an in-image level-zero ROI");
        if(snapshot.metadata.width!=doc.metadata().width || snapshot.metadata.height!=doc.metadata().height || snapshot.metadata.levels.empty() || snapshot.metadata.levels[0].downsample!=1)
            throw std::invalid_argument("Tiled fluorescence raw/display dimensions differ");
        if(p.segmentation_channel>=analysis.channels)throw std::invalid_argument("Tiled segmentation channel is outside selected block");
        if(snapshot.calibration.x && snapshot.calibration.y)calibration=std::array<double,2>{*snapshot.calibration.x,*snapshot.calibration.y};
        if(p.units==FluorescenceCellUnits::Micrometres && !calibration)throw std::invalid_argument("Physical tiled cells require both calibration axes");
        const auto spacing=p.units==FluorescenceCellUnits::Pixels?std::array<double,2>{1,1}:*calibration;
        const cell_detail::PhysicalGridMetric metric(spacing[0],spacing[1]);const auto s=metric.sigma_pixels(p.sigma);
        const auto halo=[&](double scale,int radius){const auto n=2*std::ceil(p.cell_expansion/scale)+radius+1;if(!std::isfinite(n) || n>t.halo)throw std::invalid_argument("Tiled fluorescence halo does not cover smoothing/expansion support");};
        halo(spacing[0],cell_detail::scientific_gaussian_radius(s.first));halo(spacing[1],cell_detail::scientific_gaussian_radius(s.second));
        columns=std::size_t((r.width-1)/t.width+1);const auto rows=std::size_t((r.height-1)/t.height+1);
        if(columns>t.max_tiles/rows)throw WorkflowCaptureLimit("Fluorescence tile count exceeds capture limit");count=columns*rows;
        fixed=sizeof(Data)+2*raw_pixel_source_snapshot_bytes(snapshot)+2*authority.object_context().payload_bytes()+65536;
        if(authority.mask())workflow_capture_charge(fixed,3*authority.mask()->payload_bytes(),p.work_bytes);
        workflow_capture_charge(fixed,count*(sizeof(std::string)+128),p.work_bytes);
        Sha256 source_hash;text(source_hash,"qpx.tiled-fluorescence-source/1");text(source_hash,snapshot.epoch);text(source_hash,snapshot.reader);
        if(!snapshot.observed)throw std::invalid_argument("Tiled fluorescence requires observable source identity");
        const auto& observed=*snapshot.observed;text(source_hash,observed.reader);text(source_hash,observed.scope);source_hash.integer(bool(observed.primary));
        if(observed.primary){file(source_hash,*observed.primary);for(const auto& f:observed.companions)file(source_hash,f);}
        else{text(source_hash,observed.generation);text(source_hash,observed.locator);}
        for(auto v:{snapshot.calibration.x,snapshot.calibration.y}){source_hash.integer(bool(v));if(v)real(source_hash,*v);}source_sha=source_hash.finish();
        Sha256 h;text(h,"qpx.tiled-fluorescence-request/1");text(h,source_sha);region(h,analysis);parameters(h,p);
        for(auto v:{std::uint64_t(t.width),std::uint64_t(t.height),std::uint64_t(t.halo),std::uint64_t(t.max_tiles)})h.integer(v);
        h.integer(bool(authority.mask()));if(authority.mask()){const auto bytes=GeometryArchiveCodec::encode(*authority.mask());h.integer(bytes.size());h.update(bytes.data(),bytes.size());}sha=h.finish();
    }
    RegionRequest core(std::size_t i)const {
        const auto& r=analysis.region;const auto x=r.x+std::int64_t(i%columns)*tiles.width,y=r.y+std::int64_t(i/columns)*tiles.height;
        return {x,y,0,std::min<std::int64_t>(tiles.width,r.x+r.width-x),std::min<std::int64_t>(tiles.height,r.y+r.height-y)};
    }
    struct Window {RawRegionRequest request;RegionRequest core;RawPixelBuffer pixels;std::vector<std::uint8_t> domain;};
    Window read(std::size_t i,const std::atomic<bool>* c)const {
        workflow_checkpoint(c);if(!raw_pixel_publication_lattice_source_matches(*source,snapshot,lattice))throw WorkflowInputChanged("Tiled fluorescence source changed before read");
        const auto ownership=core(i);auto request=analysis;
        const auto x=std::max<std::int64_t>(0,ownership.x-tiles.halo),y=std::max<std::int64_t>(0,ownership.y-tiles.halo);
        const auto right=ownership.x+ownership.width+std::min<std::int64_t>(tiles.halo,snapshot.metadata.width-ownership.x-ownership.width);
        const auto bottom=ownership.y+ownership.height+std::min<std::int64_t>(tiles.halo,snapshot.metadata.height-ownership.y-ownership.height);
        request.region={x,y,0,right-x,bottom-y};
        detail::fluorescence_cell_work_bytes(right-x,bottom-y,request.channels,snapshot.metadata.sample_type,options,calibration,0,0,fixed);
        auto raw=source->read_raw(request,c);
        if(raw.width()!=right-x || raw.height()!=bottom-y || raw.channels()!=request.channels || raw.sample_type()!=snapshot.metadata.sample_type)
            throw WorkflowInputChanged("Tiled fluorescence decoder returned another lattice/type");
        if(!raw_pixel_publication_lattice_source_matches(*source,snapshot,lattice))throw WorkflowInputChanged("Tiled fluorescence source changed during read");
        std::vector<std::uint8_t> domain(std::size_t(raw.width()*raw.height()));
        for(std::int64_t dy=0;dy<raw.height();++dy)for(std::int64_t dx=0;dx<raw.width();++dx) {
            const auto at=std::size_t(dy*raw.width()+dx);if(!(at&4095))workflow_checkpoint(c);
            const auto px=x+dx,py=y+dy;domain[at]=inside(px,py,analysis.region) && (!authority.mask() || authority.mask()->contains_sample({double(px)+.5,double(py)+.5},RoiSamplingPolicy::GeometryCoverage));
        }
        return {request,ownership,std::move(raw),std::move(domain)};
    }
};
FluorescenceTiledRequest::FluorescenceTiledRequest(const Document& doc,RawRegionRequest r,std::uint64_t object,FluorescenceCellOptions p,FluorescenceTileOptions t) {
    p.validate();t.validate();data_=std::make_shared<const Data>(doc,r,object,p,t);if(!matches(doc))throw WorkflowInputChanged("Tiled fluorescence changed during capture");
}
void FluorescenceTiledRequest::require_owner()const{workflow_require_owner(data_->owner);}
bool FluorescenceTiledRequest::matches(const Document& doc)const{require_owner();return data_->authority.matches(doc) && raw_pixel_source_matches(*doc.server(),data_->snapshot);}
bool FluorescenceTiledRequest::publication_matches(const Document& doc)const{return matches(doc) && raw_pixel_publication_lattice_source_matches(*doc.server(),data_->snapshot,data_->lattice);}
std::size_t FluorescenceTiledRequest::tile_count()const{return data_->count;}
const std::string& FluorescenceTiledRequest::sha256()const{return data_->sha;}
const std::string& FluorescenceTiledRequest::source_sha256()const{return data_->source_sha;}
struct FluorescenceTiledKernel {
    static FluorescenceTiledReport run(const FluorescenceTiledRequest& request,const std::function<void(std::size_t,const FluorescenceTileResult&)>& sink,const std::atomic<bool>* cancelled) {
        const auto began=std::chrono::steady_clock::now();const auto& d=*request.data_;workflow_checkpoint(cancelled);
        FluorescenceTiledReport report;report.request_sha256=d.sha;report.source_sha256=d.source_sha;report.resolved_threshold=d.options.threshold;
        std::vector<std::string> hashes(d.count);double lo=0,hi=0;std::uint64_t count=0;
        if(d.options.threshold_method==FluorescenceThreshold::Otsu256) {
            std::array<std::uint64_t,256> bins{};
            for(unsigned pass=0;pass<2;++pass) {
                for(std::size_t i=0;i<d.count;++i) {
                    auto w=d.read(i,cancelled);const auto digest=detail::fluorescence_input_sha256(w.pixels,w.domain,cancelled);
                    if(!pass)hashes[i]=digest;else if(hashes[i]!=digest)throw WorkflowInputChanged("Raw fluorescence changed during global threshold resolution");
                    const auto scores=detail::fluorescence_scores(w.pixels,w.domain,d.options,d.calibration,d.fixed,cancelled);
                    for(std::size_t at=0;at<scores.values.size();++at) {
                        if(!(at&4095))workflow_checkpoint(cancelled);
                        if(!scores.eligible[at])continue;
                        const auto x=w.request.region.x+std::int64_t(at%std::size_t(w.pixels.width())),y=w.request.region.y+std::int64_t(at/std::size_t(w.pixels.width()));
                        if(!inside(x,y,w.core))continue;const auto value=scores.values[at];
                        if(!pass){if(!count)lo=hi=value;else{lo=std::min(lo,value);hi=std::max(hi,value);}++count;}
                        else{const auto scale=std::max(std::abs(lo),std::abs(hi));++bins[std::size_t(std::clamp((value/scale-lo/scale)/(hi/scale-lo/scale)*256,0.,255.))];}
                    }
                }
                if(!count)throw std::invalid_argument("No finite fluorescence segmentation pixels in ROI");
                if(lo==hi)break;
            }
            report.resolved_threshold=detail::fluorescence_otsu_threshold(lo,hi,count,bins);
        }
        auto p=d.options;p.threshold_method=FluorescenceThreshold::Explicit;p.threshold=report.resolved_threshold;
        Sha256 input;text(input,"qpx.tiled-fluorescence-input/1");text(input,d.sha);
        for(std::size_t i=0;i<d.count;++i) {
            auto w=d.read(i,cancelled);
            const auto tile=detail::segment_fluorescence_tile_reserved(w.pixels,w.domain,w.request,w.core,d.analysis.region,d.source_sha,p,d.calibration,d.fixed,cancelled);
            if(!hashes[i].empty() && tile.input_sha256!=hashes[i])throw WorkflowInputChanged("Raw fluorescence changed after global threshold resolution");
            report.accounted_work_bytes=std::max(report.accounted_work_bytes,tile.accounted_work_bytes);report.finite_pixels+=tile.finite_segmentation_pixels_in_core;
            report.cells+=tile.cells.size();report.unresolved_tiles+=!tile.complete();++report.tiles;text(input,tile.input_sha256);
            if(sink)sink(i,tile);workflow_checkpoint(cancelled);
        }
        if(!report.finite_pixels)throw std::invalid_argument("No finite fluorescence segmentation pixels in ROI");
        if(!raw_pixel_publication_lattice_source_matches(*d.source,d.snapshot,d.lattice))throw WorkflowInputChanged("Tiled fluorescence source changed before delivery");
        report.input_sha256=input.finish();report.state=report.unresolved_tiles?"unresolved":"ready";
        report.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return report;
    }
};
FluorescenceTiledReport detect_tiled_fluorescence_cells(const FluorescenceTiledRequest& request,const std::function<void(std::size_t,const FluorescenceTileResult&)>& sink,const std::atomic<bool>* cancelled) {
    return FluorescenceTiledKernel::run(request,sink,cancelled);
}
}
