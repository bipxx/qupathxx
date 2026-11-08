#include "density_snapshot.hpp"
#include "spatial_snapshot.hpp"
#include "utf8_text.hpp"
#include <stdexcept>
namespace qpx {
void Document::restore_density(std::shared_ptr<const SpatialDensityRecord> record) {
    if(!record || !record->result || !record->id || record->id>revision_ || !record->captured_next_id || record->captured_next_id>next_id_ || record->result->document_revision()>=record->id)
        throw std::invalid_argument("invalid spatial record identity/revision bounds");
    detail::bounded_utf8(record->name,256);detail::bounded_utf8(record->producer,256);
    if(record->name.empty() || record->producer.empty()) throw std::invalid_argument("spatial record requires a name and producer");
    const auto& d=*record->result->snapshot_->capture.data_;
    if(d.provenance->source.width!=metadata().width || d.provenance->source.height!=metadata().height) throw std::invalid_argument("spatial record image dimensions mismatch");
    for(const auto& a:d.objects) if(a.id>=record->captured_next_id) throw std::invalid_argument("spatial record exceeds captured object identity bounds");
    const auto id=record->id;
    if(density_records_.count(id) || neighborhood_records_.count(id) || spatial_records_.count(id) || analysis_.count(id) || raw_analysis_.count(id) || tissue_.count(id) || cells_.count(id) || classifiers_.count(id) || pixels_.count(id) || raw_pixels_.count(id) || streams_.count(id) || raw_streams_.count(id)) throw std::invalid_argument("duplicate spatial record ID");
    const auto bytes=record->payload_bytes();if(density_records_.size()>=10000 || bytes>256u*1024*1024-density_bytes_) throw std::length_error("spatial records exceed 10000-record/256 MiB retained limit");
    density_records_.emplace(id,StoredDensity{std::move(record),{},false});density_bytes_+=bytes;
}
std::uint64_t Document::keep_density(const SpatialDensityRequest& request,std::shared_ptr<const SpatialDensityResult> result,std::string name) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial result does not match its current request");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");
    auto record=std::make_shared<SpatialDensityRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->result=std::move(result);
    ++revision_;try {restore_density(record);}catch(...) {--revision_;throw;}
    auto& stored=density_records_.at(record->id);stored.identity=analysis_identity_.token;stored.verified=true;return record->id;
}
std::shared_ptr<const SpatialDensityRecord> Document::density_record(std::uint64_t id) const {
    const auto it=density_records_.find(id);if(it==density_records_.end()) throw std::out_of_range("spatial record does not exist");return it->second.value;
}
std::vector<std::shared_ptr<const SpatialDensityRecord>> Document::density_records() const {
    std::vector<std::shared_ptr<const SpatialDensityRecord>> out;out.reserve(density_records_.size());for(const auto& entry:density_records_) out.push_back(entry.second.value);return out;
}
std::string Document::density_status(std::uint64_t id) const {
    const auto record=density_record(id);const auto& stored=density_records_.at(id);
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(!record->result->snapshot_->capture.data_->matches_objects(*this)) return "objects_or_calibration_changed";
    return stored.verified?"current":"verification_failed";
}
bool Document::verify_density(std::uint64_t id,const SpatialDensityRequest& request,std::shared_ptr<const SpatialDensityResult> result) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial verification requires a current captured result");
    const auto record=density_record(id);const auto& old=*record->result;
    if(!old.snapshot_->same_geometry(*result->snapshot_)) throw std::invalid_argument("verification request does not match saved spatial geometry/parameters");
    bool same=old.rows().size()==result->rows().size();
    for(std::size_t i=0;same && i<old.rows().size();++i) {
        const auto& a=old.rows()[i];const auto& b=result->rows()[i];
        same=a.window_id==b.window_id && a.window_revision==b.window_revision && a.count==b.count &&
            a.area_pixels==b.area_pixels && a.area_mm2==b.area_mm2 && a.density_per_mm2==b.density_per_mm2 && a.geometry_sha256==b.geometry_sha256;
    }
    auto& stored=density_records_.at(id);stored.identity=analysis_identity_.token;stored.verified=same;return same;
}
bool Document::remove_density(std::uint64_t id) {
    const auto it=density_records_.find(id);if(it==density_records_.end()) return false;
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");density_bytes_-=it->second.value->payload_bytes();density_records_.erase(it);++revision_;return true;
}
SpatialDensityRequest SpatialDensityRequest::replay(const Document& doc,std::uint64_t id) {
    const auto result=doc.density_record(id)->result;const auto& d=*result->snapshot_;
    std::vector<std::uint64_t> windows,tissue,excluded,cells;
    for(const auto& a:d.windows) windows.push_back(a.id);
    for(const auto& a:d.tissue) tissue.push_back(a.id);
    for(const auto& a:d.excluded) excluded.push_back(a.id);
    for(std::size_t i=0;i<result->cell_count();++i) cells.push_back(result->cell_object(i).id);
    SpatialDensityRequest request(doc,windows,tissue,excluded,cells,result->options());
    if(!d.same_geometry(*request.data_)) throw std::invalid_argument("saved density geometry or calibration has changed");return request;
}
}
