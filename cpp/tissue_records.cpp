#include "qupathxx/tissue.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>
namespace qpx {
namespace {
void advance(std::uint64_t revision) {if(revision==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");}
auto parameters(const TissueParameters& p) {return std::make_tuple(p.feature,p.threshold,p.connectivity,p.level,p.open_radius,p.close_radius,p.fill_holes,p.max_hole_pixels,p.min_pixels,p.max_pixels,p.tile_size,p.max_components,p.work_bytes,p.sampling_policy);}
bool equivalent(const TissueResult& a,const TissueResult& b) {
    return a.algorithm()==b.algorithm() && a.input_sha256()==b.input_sha256() && a.object_id()==b.object_id() &&
        a.object_revision()==b.object_revision() && a.nucleus()==b.nucleus() && a.roi().equals(b.roi()) &&
        parameters(a.parameters())==parameters(b.parameters()) && a.white()==b.white() && a.stains()==b.stains() &&
        a.level()==b.level() && a.downsample()==b.downsample() && a.samples()==b.samples() &&
        a.threshold_pixels()==b.threshold_pixels() && a.cleaned_pixels()==b.cleaned_pixels() && a.filled_pixels()==b.filled_pixels() &&
        a.foreground_pixels()==b.foreground_pixels() && a.raw_components()==b.raw_components() && a.labels()==b.labels();
}
}
std::size_t TissueRecord::payload_bytes() const {if(!result) throw std::invalid_argument("tissue record has no mask");return sizeof(TissueRecord)+192+producer.capacity()+result->retained_bytes()+result->roi().payload_bytes()+result->algorithm().capacity()+result->input_sha256().capacity();}
void Document::restore_tissue(std::shared_ptr<const TissueRecord> record) {
    if(!record || !record->result || !record->id || record->id>revision_ || record->result->document_revision()>=record->id ||
       !record->result->object_id() || record->result->object_id()>=next_id_ || record->producer.empty() || record->producer.size()>256 || record->producer.find('\0')!=std::string::npos || record->result->scientific_grid())
        throw std::invalid_argument("invalid tissue archive record");
    const auto bytes=record->payload_bytes();
    if(tissue_.size()>=10000 || bytes>256u*1024u*1024u-tissue_bytes_) throw std::length_error("tissue archive exceeds its 10000-record/256 MiB limit");
    const auto id=record->id;const auto key=std::make_pair(record->result->object_id(),id);
    if(tissue_.count(id) || analysis_.count(id) || cells_.count(id) || classifiers_.count(id)) throw std::invalid_argument("duplicate tissue record ID");
    const bool reuse=!tissue_free_slots_.empty();const auto slot=reuse?tissue_free_slots_.back():tissue_next_slot_;
    const auto index=slot/256;auto& blocks=tissue_blocks_.values;const bool fresh=index==blocks.size();
    std::shared_ptr<TissueBlock> block;
    if(fresh) {block=std::make_shared<TissueBlock>();if(blocks.size()==blocks.capacity()) blocks.reserve(std::max<std::size_t>(4,blocks.capacity()*2));}
    else if(!blocks[index]->sealed) block=blocks[index];
    else {block=std::make_shared<TissueBlock>(*blocks[index]);block->sealed=false;}
    tissue_by_object_.emplace(key,true);
    try {tissue_.emplace(id,StoredTissue{record,{},{},false,slot});} catch(...) {tissue_by_object_.erase(key);throw;}
    if(fresh) blocks.push_back(block);else blocks[index]=block;
    block->records[slot%256]=std::move(record);
    if(reuse) tissue_free_slots_.pop_back();else ++tissue_next_slot_;
    tissue_bytes_+=bytes;
}
std::uint64_t Document::keep_tissue(const TissueRequest& request,std::shared_ptr<const TissueResult> result) {
    if(result && result->scientific_grid()) throw std::invalid_argument("explicit-grid masks require their versioned cell record");
    if(!result || !request.matches(*this,request.object_id()) || request.ticket_!=result->ticket_) throw std::invalid_argument("tissue result does not match its current request");
    advance(revision_);auto record=std::make_shared<TissueRecord>();record->id=revision_+1;record->result=std::move(result);
    ++revision_;try {restore_tissue(record);} catch(...) {--revision_;throw;}
    auto& stored=tissue_.at(record->id);stored.identity=analysis_identity_.token;stored.source=server_;stored.verified=true;return record->id;
}
bool Document::verify_tissue(std::uint64_t id,const TissueRequest& request,const TissueResult& result) {
    if(!request.matches(*this,request.object_id()) || request.ticket_!=result.ticket_) throw std::invalid_argument("tissue verification result does not match its current request");
    auto it=tissue_.find(id);if(it==tissue_.end()) throw std::out_of_range("tissue record does not exist");
    auto& stored=it->second;const auto& old=*stored.value->result;
    if(old.object_id()!=result.object_id() || old.object_revision()!=result.object_revision() || old.nucleus()!=result.nucleus() ||
       !old.roi().equals(result.roi()) || parameters(old.parameters())!=parameters(result.parameters()) || old.white()!=result.white() || old.stains()!=result.stains())
        throw std::invalid_argument("verification request does not match saved tissue parameters");
    stored.identity=analysis_identity_.token;stored.source=server_;stored.verified=equivalent(old,result);return stored.verified;
}
std::shared_ptr<const TissueRecord> Document::tissue_record(std::uint64_t id) const {auto it=tissue_.find(id);if(it==tissue_.end()) throw std::out_of_range("tissue record does not exist");return it->second.value;}
std::vector<std::shared_ptr<const TissueRecord>> Document::tissue_records() const {std::vector<std::shared_ptr<const TissueRecord>> records;records.reserve(tissue_.size());for(const auto& [id,entry]:tissue_) records.push_back(entry.value);return records;}
std::shared_ptr<const TissueRecord> Document::latest_tissue(std::uint64_t id) const {
    auto it=tissue_by_object_.upper_bound({id,std::numeric_limits<std::uint64_t>::max()});
    if(it==tissue_by_object_.begin() || (--it)->first.first!=id) return {};return tissue_.at(it->first.second).value;
}
std::string Document::tissue_status(std::uint64_t id) const {
    const auto record=tissue_record(id);const auto& stored=tissue_.at(id);const auto& result=*record->result;
    const auto object=objects_.find(result.object_id());
    if(object==objects_.end() || object->second.revision!=result.object_revision()) return "object_changed";
    const auto roi=result.nucleus()?object->second.nucleus:std::optional<Roi>(object->second.geometry);
    if(!roi || !roi->equals(result.roi())) return "object_changed";
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(stored.source.lock().get()!=server_.get()) return "source_changed";
    if(server_->closed()) return "source_closed";
    return stored.verified?"current":"verification_failed";
}
bool Document::remove_tissue(std::uint64_t id) {
    auto it=tissue_.find(id);if(it==tissue_.end()) return false;advance(revision_);
    if(tissue_.size()==1) {tissue_.clear();tissue_by_object_.clear();tissue_blocks_.values.clear();std::vector<std::size_t>().swap(tissue_free_slots_);tissue_next_slot_=0;tissue_bytes_=0;++revision_;return true;}
    const auto slot=it->second.slot,index=slot/256;auto& blocks=tissue_blocks_.values;auto block=blocks[index];
    if(block->sealed) {block=std::make_shared<TissueBlock>(*block);block->sealed=false;}
    if(tissue_free_slots_.size()==tissue_free_slots_.capacity()) tissue_free_slots_.reserve(std::max<std::size_t>(64,tissue_free_slots_.capacity()*2));
    blocks[index]=block;block->records[slot%256].reset();tissue_free_slots_.push_back(slot);
    tissue_by_object_.erase({it->second.value->result->object_id(),id});tissue_bytes_-=it->second.value->payload_bytes();tissue_.erase(it);++revision_;return true;
}
TissueRequest TissueRequest::replay(const Document& document,std::uint64_t id) {
    const auto record=document.tissue_record(id);const auto& r=*record->result;
    TissueRequest request(document,r.object_id(),StainTransform::from_normalized(r.stains(),r.white()),r.parameters(),r.nucleus());
    const auto object=document.annotation(r.object_id());
    if(object.revision!=r.object_revision() || !request.roi().equals(r.roi()) || request.level()!=r.level() || request.downsample()!=r.downsample())
        throw std::invalid_argument("saved tissue object or pyramid has changed");
    return request;
}
}
