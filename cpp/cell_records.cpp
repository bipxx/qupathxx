#include "qupathxx/cells.hpp"
#include "cell_object_status.hpp"
#include "scientific_cells.hpp"
#include "qupathxx/provenance.hpp"
#include <limits>
#include <stdexcept>
#include <tuple>
namespace qpx {
namespace {
void advance(std::uint64_t r) {if(r==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");}
auto parameters(const CellParameters& p) {return std::make_tuple(p.algorithm,p.log_prominence,p.context_padding,p.feature,p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,p.min_nucleus_pixels,p.max_nucleus_pixels,
    p.connectivity,p.level,p.tile_size,p.max_cells,p.work_bytes,p.contours.clip_to_roi,p.contours.max_components,p.contours.max_edges,p.contours.max_vertices,p.contours.work_bytes);}
auto feature(const CellFeature& f) {return std::tie(f.label,f.nucleus_pixels,f.cell_pixels,f.nucleus_rgb,f.cell_rgb,f.nucleus_channels,f.cell_channels,f.cytoplasm_rgb,f.cytoplasm_channels);}
auto marker(const CellMarker& m) {return std::tie(m.label,m.result_label,m.pixel_index,m.center.x,m.center.y,m.radius);}
auto grid_parameters(const GridCellParameters& p) {return std::make_tuple(p.processing,p.units,p.feature,p.threshold,p.sigma,p.log_prominence,p.min_peak_radius,p.min_seed_distance,p.cell_expansion,
    p.min_nucleus_area,p.max_nucleus_area,p.sampling_policy,p.max_cells,p.work_bytes,p.contours.clip_to_roi,p.contours.max_components,p.contours.max_edges,p.contours.max_vertices,p.contours.work_bytes);}
auto grid(const ScientificGrid& g) {return std::make_tuple(g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context());}
auto resolved(const ResolvedGridCellParameters& p) {return std::make_tuple(p.step_x,p.step_y,p.sigma_x,p.sigma_y,p.min_nucleus_samples,p.max_nucleus_samples);}
auto nominal(const NominalCellSampling& n) {
    const auto& r=n.source_request();return std::make_tuple(r.x_px,r.y_px,r.width_px,r.height_px,r.downsample,n.context_x(),n.context_y(),r.sampling);
}
bool same_sampling(const TissueResult& a,const TissueResult& b) {
    if(a.nominal_sampling() || b.nominal_sampling()) return a.nominal_sampling() && b.nominal_sampling() && nominal(*a.nominal_sampling())==nominal(*b.nominal_sampling());
    return a.scientific_grid() && b.scientific_grid() && grid(*a.scientific_grid())==grid(*b.scientific_grid());
}
bool same_grid_request(const CellResult& a,const CellResult& b) {
    const auto& x=a.nuclei()->provenance();const auto& y=b.nuclei()->provenance();
    return a.grid_parameters() && b.grid_parameters() && same_sampling(*a.nuclei(),*b.nuclei()) && x && y &&
        grid_parameters(*a.grid_parameters())==grid_parameters(*b.grid_parameters()) &&
        resolved(*a.resolved_grid_parameters())==resolved(*b.resolved_grid_parameters()) && x->effective_pixel_width_um==y->effective_pixel_width_um && x->effective_pixel_height_um==y->effective_pixel_height_um;
}
bool same_request(const CellResult& a,const CellResult& b) {
    return a.nuclei()->object_id()==b.nuclei()->object_id() && a.nuclei()->object_revision()==b.nuclei()->object_revision() &&
        a.nuclei()->roi().equals(b.nuclei()->roi()) &&
        a.nuclei()->white()==b.nuclei()->white() && a.nuclei()->stains()==b.nuclei()->stains() &&
        a.nuclei()->level()==b.nuclei()->level() &&
        ((a.grid_parameters() || b.grid_parameters())?same_grid_request(a,b):
            (parameters(a.parameters())==parameters(b.parameters()) && a.nuclei()->downsample()==b.nuclei()->downsample()));
}
bool equivalent(const CellResult& a,const CellResult& b) {
    if(!same_request(a,b) || std::string(a.algorithm())!=b.algorithm() || a.nuclei()->input_sha256()!=b.nuclei()->input_sha256() || a.nuclei()->samples()!=b.nuclei()->samples() ||
       a.nuclei()->threshold_pixels()!=b.nuclei()->threshold_pixels() || a.nuclei()->labels()!=b.nuclei()->labels() || a.cells()->labels()!=b.cells()->labels() ||
       a.raw_peaks()!=b.raw_peaks() || a.features().size()!=b.features().size() || a.markers().size()!=b.markers().size()) return false;
    for(std::size_t i=0;i<a.features().size();++i) if(feature(a.features()[i])!=feature(b.features()[i])) return false;
    for(std::size_t i=0;i<a.markers().size();++i) if(marker(a.markers()[i])!=marker(b.markers()[i])) return false;
    return true;
}
}
std::size_t CellRecord::payload_bytes() const {
    if(!result || !result->nuclei() || !result->cells()) throw std::invalid_argument("cell record has no rasters");
    return sizeof(CellRecord)+512+producer.capacity()+result->retained_bytes()+2*result->nuclei()->roi().payload_bytes();
}
void Document::restore_cells(std::shared_ptr<const CellRecord> record) {
    if(!record || !record->result || !record->result->nuclei() || !record->result->cells() || !record->id || record->id>revision_ || record->result->nuclei()->document_revision()>=record->id ||
       !record->result->nuclei()->object_id() || record->result->nuclei()->object_id()>=next_id_ || record->producer.empty() || record->producer.size()>256 || record->producer.find('\0')!=std::string::npos)
        throw std::invalid_argument("invalid cell archive record");
    const auto bytes=record->payload_bytes();
    if(cells_.size()>=10000 || bytes>256u*1024u*1024u-cells_bytes_) throw std::length_error("cell archive exceeds its 10000-record/256 MiB limit");
    const auto id=record->id;const auto key=std::make_pair(record->result->nuclei()->object_id(),id);
    if(cells_.count(id) || tissue_.count(id) || analysis_.count(id) || classifiers_.count(id)) throw std::invalid_argument("duplicate cell record ID");
    cells_by_object_.emplace(key,true);
    try {cells_.emplace(id,StoredCells{std::move(record),{},{},false});} catch(...) {cells_by_object_.erase(key);throw;}
    cells_bytes_+=bytes;
}
std::uint64_t Document::keep_cells(const CellRequest& request,std::shared_ptr<const CellResult> result) {
    if(!result || !request.matches(*this,request.object_id()) || request.ticket_!=result->ticket_) throw std::invalid_argument("cell result does not match its current request");
    advance(revision_);auto record=std::make_shared<CellRecord>();record->id=revision_+1;record->result=std::move(result);
    ++revision_;try {restore_cells(record);} catch(...) {--revision_;throw;}
    auto& stored=cells_.at(record->id);stored.identity=analysis_identity_.token;stored.source=server_;stored.verified=true;return record->id;
}
std::vector<std::uint64_t> Document::publish_cell_record(std::uint64_t id,std::string prefix,std::string classification) {
    const auto record=cell_record(id);const auto& r=*record->result;
    if(cell_status(id)!="current") throw std::invalid_argument("cell publication requires a current verified run");
    if(!r.cell_contours() || !r.nucleus_contours()) throw std::runtime_error("cell publication requires available validated contours (GEOS)");
    std::vector<ObjectEdit> edits;edits.reserve(r.features().size());
    for(std::size_t i=0;i<r.features().size();++i) {
        const auto& body=r.cell_contours()->components().at(i);const auto& nucleus=r.nucleus_contours()->components().at(i);
        if(body.label!=i+1 || nucleus.label!=i+1 || r.features()[i].label!=i+1) throw std::runtime_error("cell contour label validation failed");
        auto edit=ObjectEdit::create_geometry(body.geometry,prefix+" "+std::to_string(i+1),r.nuclei()->object_id(),classification,ObjectKind::Cell,nucleus.geometry);
        edit.value.cell_feature_link=CellFeatureLink{id,static_cast<std::uint32_t>(i+1)};edits.push_back(std::move(edit));
    }
    return execute(edits,"Add detected cells");
}
std::vector<std::uint64_t> Document::publish_cells(const CellRequest& request,std::shared_ptr<const CellResult> result,std::string prefix,std::string classification) {
    if(!result || !request.matches(*this,request.object_id()) || request.ticket_!=result->ticket_) throw std::invalid_argument("cell result does not match its current request");
    if(!result->cell_contours() || !result->nucleus_contours()) throw std::runtime_error("cell publication requires available validated contours (GEOS)");
    if(result->features().empty()) return {};
    // Reuse a retained copy of this exact run, rather than duplicating its rasters.
    auto it=cells_by_object_.lower_bound({request.object_id(),0});
    for(;it!=cells_by_object_.end() && it->first.first==request.object_id();++it) {
        const auto id=it->first.second;if(cell_record(id)->result==result && cell_status(id)=="current") return publish_cell_record(id,std::move(prefix),std::move(classification));
    }
    const auto before=revision_;const auto id=keep_cells(request,std::move(result));
    try {return publish_cell_record(id,std::move(prefix),std::move(classification));}
    catch(...) {
        // execute has a strong exception guarantee; roll back the private archive
        // insertion as well, preserving IDs, history and the document revision.
        const auto record=cell_record(id);cells_bytes_-=record->payload_bytes();cells_by_object_.erase({request.object_id(),id});cells_.erase(id);revision_=before;throw;
    }
}
CellFeature Document::cell_object_feature(std::uint64_t id) const {
    const auto& a=find(id);if(!a.cell_feature_link) throw std::invalid_argument("object has no cell feature link");
    const auto& link=*a.cell_feature_link;const auto record=cell_record(link.record_id);
    if(!link.label || link.label>record->result->features().size()) throw std::invalid_argument("invalid cell feature label");
    return record->result->features().at(link.label-1);
}
std::string Document::cell_object_status(std::uint64_t id) const {
    const auto& a=find(id);if(!a.cell_feature_link) return "no_features";
    const auto& link=*a.cell_feature_link;const auto it=cells_.find(link.record_id);if(it==cells_.end()) return "archive_missing";
    return detail::cell_geometry_status(a,*it->second.value->result,cell_status(link.record_id));
}
bool Document::verify_cells(std::uint64_t id,const CellRequest& request,const CellResult& result) {
    if(!request.matches(*this,request.object_id()) || request.ticket_!=result.ticket_) throw std::invalid_argument("cell verification result does not match its current request");
    auto it=cells_.find(id);if(it==cells_.end()) throw std::out_of_range("cell record does not exist");
    auto& stored=it->second;
    if(!same_request(*stored.value->result,result)) throw std::invalid_argument("verification request does not match saved cell parameters");
    stored.identity=analysis_identity_.token;stored.source=server_;
    stored.verified=stored.value->result->domain_bits_==result.domain_bits_ && equivalent(*stored.value->result,result);
    return stored.verified;
}
std::shared_ptr<const CellRecord> Document::cell_record(std::uint64_t id) const {auto it=cells_.find(id);if(it==cells_.end()) throw std::out_of_range("cell record does not exist");return it->second.value;}
std::vector<std::shared_ptr<const CellRecord>> Document::cell_records() const {std::vector<std::shared_ptr<const CellRecord>> records;records.reserve(cells_.size());for(const auto& entry:cells_) records.push_back(entry.second.value);return records;}
std::shared_ptr<const CellRecord> Document::latest_cells(std::uint64_t id) const {
    auto it=cells_by_object_.upper_bound({id,std::numeric_limits<std::uint64_t>::max()});
    if(it==cells_by_object_.begin() || (--it)->first.first!=id) return {};return cells_.at(it->first.second).value;
}
std::string Document::cell_status(std::uint64_t id) const {
    const auto record=cell_record(id);const auto& stored=cells_.at(id);const auto& result=*record->result->nuclei();const auto object=objects_.find(result.object_id());
    if(object==objects_.end() || object->second.revision!=result.object_revision() || !object->second.geometry.equals(result.roi())) return "object_changed";
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(stored.source.lock().get()!=server_.get()) return "source_changed";
    if(server_->closed()) return "source_closed";
    if(record->result->grid_parameters()) {
        const auto& p=result.provenance();
        if(!p || p->effective_pixel_width_um!=pixel_width_um() || p->effective_pixel_height_um!=pixel_height_um()) return "calibration_changed";
    }
    return stored.verified?"current":"verification_failed";
}
bool Document::remove_cells(std::uint64_t id) {
    auto it=cells_.find(id);if(it==cells_.end()) return false;advance(revision_);
    cells_by_object_.erase({it->second.value->result->nuclei()->object_id(),id});cells_bytes_-=it->second.value->payload_bytes();cells_.erase(it);++revision_;return true;
}
CellRequest CellRequest::replay(const Document& document,std::uint64_t id) {
    const auto record=document.cell_record(id);const auto& r=*record->result->nuclei();
    if(record->result->grid_parameters()) {
        if((!r.scientific_grid() && !r.nominal_sampling()) || !r.provenance()) throw std::invalid_argument("saved grid cell geometry or provenance is missing");
        const auto transform=StainTransform::from_normalized(r.stains(),r.white());
        CellRequest request=r.nominal_sampling()?CellRequest(document,r.object_id(),transform,*r.nominal_sampling(),*record->result->grid_parameters()):
            CellRequest(document,r.object_id(),transform,*r.scientific_grid(),*record->result->grid_parameters());
        const auto& p=request.scientific_->source.provenance();
        if(document.annotation(r.object_id()).revision!=r.object_revision() || !request.roi().equals(r.roi()) ||
           p->effective_pixel_width_um!=r.provenance()->effective_pixel_width_um || p->effective_pixel_height_um!=r.provenance()->effective_pixel_height_um ||
           resolved(*request.resolved_grid_parameters())!=resolved(*record->result->resolved_grid_parameters())) throw std::invalid_argument("saved grid cell object, calibration or resolved geometry changed");
        return request;
    }
    CellRequest request(document,r.object_id(),StainTransform::from_normalized(r.stains(),r.white()),record->result->parameters());
    if(document.annotation(r.object_id()).revision!=r.object_revision() || !request.roi().equals(r.roi()) || request.level()!=r.level() || request.downsample()!=r.downsample())
        throw std::invalid_argument("saved cell object or pyramid has changed");
    return request;
}
}
