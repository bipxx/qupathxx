#include "neighborhood_snapshot.hpp"
#include "spatial_snapshot.hpp"
#include "utf8_text.hpp"
#include <stdexcept>
namespace qpx {
void Document::restore_neighborhood(std::shared_ptr<const SpatialNeighborhoodRecord> record) {
    if(!record || !record->result || !record->id || record->id>revision_ || !record->captured_next_id || record->captured_next_id>next_id_ || record->result->document_revision()>=record->id)
        throw std::invalid_argument("invalid spatial record identity/revision bounds");
    detail::bounded_utf8(record->name,256);detail::bounded_utf8(record->producer,256);
    if(record->name.empty() || record->producer.empty()) throw std::invalid_argument("spatial record requires a name and producer");
    const auto& d=*record->result->snapshot_->capture.data_;
    if(d.provenance->source.width!=metadata().width || d.provenance->source.height!=metadata().height) throw std::invalid_argument("spatial record image dimensions mismatch");
    for(const auto& a:d.objects) if(a.id>=record->captured_next_id) throw std::invalid_argument("spatial record exceeds captured object identity bounds");
    const auto id=record->id;
    if(density_records_.count(id) || neighborhood_records_.count(id) || spatial_records_.count(id) || analysis_.count(id) || raw_analysis_.count(id) || tissue_.count(id) || cells_.count(id) || classifiers_.count(id) || pixels_.count(id) || raw_pixels_.count(id) || streams_.count(id) || raw_streams_.count(id)) throw std::invalid_argument("duplicate spatial record ID");
    const auto bytes=record->payload_bytes();if(neighborhood_records_.size()>=10000 || bytes>256u*1024*1024-neighborhood_bytes_) throw std::length_error("spatial records exceed 10000-record/256 MiB retained limit");
    neighborhood_records_.emplace(id,StoredNeighborhood{std::move(record),{},false});neighborhood_bytes_+=bytes;
}
std::uint64_t Document::keep_neighborhood(const SpatialNeighborhoodRequest& request,std::shared_ptr<const SpatialNeighborhoodResult> result,std::string name) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial result does not match its current request");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");
    auto record=std::make_shared<SpatialNeighborhoodRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->result=std::move(result);
    ++revision_;try {restore_neighborhood(record);}catch(...) {--revision_;throw;}
    auto& stored=neighborhood_records_.at(record->id);stored.identity=analysis_identity_.token;stored.verified=true;return record->id;
}
std::shared_ptr<const SpatialNeighborhoodRecord> Document::neighborhood_record(std::uint64_t id) const {
    const auto it=neighborhood_records_.find(id);if(it==neighborhood_records_.end()) throw std::out_of_range("spatial record does not exist");return it->second.value;
}
std::vector<std::shared_ptr<const SpatialNeighborhoodRecord>> Document::neighborhood_records() const {
    std::vector<std::shared_ptr<const SpatialNeighborhoodRecord>> out;out.reserve(neighborhood_records_.size());for(const auto& entry:neighborhood_records_) out.push_back(entry.second.value);return out;
}
std::string Document::neighborhood_status(std::uint64_t id) const {
    const auto record=neighborhood_record(id);const auto& stored=neighborhood_records_.at(id);
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(!record->result->snapshot_->capture.data_->matches_objects(*this)) return "objects_or_calibration_changed";
    return stored.verified?"current":"verification_failed";
}
bool Document::verify_neighborhood(std::uint64_t id,const SpatialNeighborhoodRequest& request,std::shared_ptr<const SpatialNeighborhoodResult> result) {
    if(!result || !request.matches(*this) || !result->belongs_to(request)) throw std::invalid_argument("spatial verification requires a current captured result");
    const auto record=neighborhood_record(id);const auto& old=*record->result;
    if(old.options().radius!=result->options().radius || !old.snapshot_->capture.data_->same_geometry(*result->snapshot_->capture.data_)) throw std::invalid_argument("verification request does not match saved spatial geometry/parameters");
    bool same=old.rows().size()==result->rows().size();
    for(std::size_t i=0;same && i<old.rows().size();++i) same=old.rows()[i].count==result->rows()[i].count;
    auto& stored=neighborhood_records_.at(id);stored.identity=analysis_identity_.token;stored.verified=same;return same;
}
bool Document::remove_neighborhood(std::uint64_t id) {
    const auto it=neighborhood_records_.find(id);if(it==neighborhood_records_.end()) return false;
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");neighborhood_bytes_-=it->second.value->payload_bytes();neighborhood_records_.erase(it);++revision_;return true;
}
SpatialNeighborhoodRequest SpatialNeighborhoodRequest::replay(const Document& doc,std::uint64_t id) {
    const auto result=doc.neighborhood_record(id)->result;const auto& d=*result->snapshot_->capture.data_;
    std::vector<std::uint64_t> sources,targets;sources.reserve(d.sources.size());targets.reserve(d.targets.size());
    for(auto i:d.sources) sources.push_back(d.objects[i].id);for(auto i:d.targets) targets.push_back(d.objects[i].id);
    SpatialNeighborhoodRequest request(doc,sources,targets,result->options());
    if(!d.same_geometry(*request.data_->capture.data_)) throw std::invalid_argument("saved spatial geometry or calibration has changed");return request;
}
}
