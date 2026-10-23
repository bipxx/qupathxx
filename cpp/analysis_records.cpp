#include "qupathxx/analysis.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace qpx {
namespace {
void advance(std::uint64_t revision) {if(revision==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");}
void check_result(const Document& document,const AnalysisRequest& request,const AnalysisResult& result) {
    if(!request.matches(document,request.object_id())) throw std::invalid_argument("analysis request is stale");
    if(result.object_id!=request.object_id() || result.object_revision!=request.object_revision() || result.document_revision!=request.document_revision() ||
       result.provenance!=request.provenance() || result.algorithm!=roi_analysis_algorithm(request.sampling_policy()) || result.kind!=request.kind() || result.nucleus!=request.nucleus() || result.white!=request.transform().white() || result.stains!=request.transform().stains())
        throw std::invalid_argument("analysis result does not match its request");
}
}
std::size_t AnalysisRecord::payload_bytes() const {return sizeof(AnalysisRecord)+(result.provenance?result.provenance->payload_bytes():0)+192+result.algorithm.size()+result.input_sha256.size()+roi.payload_bytes();}
void AnalysisRecord::validate(const ImageMetadata& metadata) const {
    const auto& r=result;
    if(r.provenance) {r.provenance->validate();if(r.provenance->object_id!=r.object_id || r.provenance->object_revision!=r.object_revision || r.provenance->document_revision!=r.document_revision || r.provenance->nucleus!=r.nucleus || r.provenance->source.width!=metadata.width || r.provenance->source.height!=metadata.height) throw std::invalid_argument("analysis provenance identity mismatch");}
    (void)r.sampling_policy();
    if(!id || !r.object_id || r.object_revision>r.document_revision || r.document_revision>=id ||
       (r.kind!=AnalysisKind::OpticalDensityMeans && r.kind!=AnalysisKind::StainMeans) || !r.samples || !r.tiles ||
       tile_size<16 || tile_size>1024 || !std::isfinite(r.elapsed_ms) || r.elapsed_ms<0)
        throw std::invalid_argument("invalid analysis record metadata");
    if(r.input_sha256.size()!=64 || !std::all_of(r.input_sha256.begin(),r.input_sha256.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        throw std::invalid_argument("invalid analysis input SHA-256");
    for(auto value:r.means) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite analysis mean");
    StainTransform::validate_normalized(r.stains,r.white);
    if(!roi.is_area()) throw std::invalid_argument("analysis record requires an area ROI");
    const auto b=roi.bounds();
    if(b.x<0 || b.y<0 || b.x+b.width>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
       b.y+b.height>=static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
       static_cast<long double>(b.x+b.width)>metadata.width || static_cast<long double>(b.y+b.height)>metadata.height)
        throw std::invalid_argument("analysis record ROI must lie inside the image");
    const auto w=static_cast<std::uint64_t>(std::ceil(b.x+b.width-.5)-std::ceil(b.x-.5));
    const auto h=static_cast<std::uint64_t>(std::ceil(b.y+b.height-.5)-std::ceil(b.y-.5));
    if(!w || !h || w>std::numeric_limits<std::uint64_t>::max()/h || r.samples>w*h ||
       r.tiles!=((w+tile_size-1)/tile_size)*((h+tile_size-1)/tile_size)) throw std::invalid_argument("invalid analysis record sample/tile counts");
}
void Document::restore_analysis(AnalysisRecord record) {
    record.validate(metadata());
    if(record.id>revision_ || record.result.object_id>=next_id_) throw std::invalid_argument("analysis record exceeds document identity/revision bounds");
    const auto bytes=record.payload_bytes();
    if(analysis_.size()>=100000 || bytes>256u*1024u*1024u-analysis_bytes_) throw std::length_error("analysis archive exceeds its 100000-record/256 MiB limit");
    const auto key=std::make_pair(record.result.object_id,record.id); const auto id=record.id;
    if(analysis_.count(id) || tissue_.count(id) || cells_.count(id) || classifiers_.count(id)) throw std::invalid_argument("duplicate analysis record ID");
    auto immutable=std::make_shared<const AnalysisRecord>(std::move(record));
    const bool reuse=!analysis_free_slots_.empty();
    const auto slot=reuse?analysis_free_slots_.back():analysis_next_slot_;
    const auto block_index=slot/256;
    std::shared_ptr<AnalysisBlock> block;
    auto& blocks=analysis_blocks_.values;
    const bool new_block=block_index==blocks.size();
    if(new_block) {
        block=std::make_shared<AnalysisBlock>();
        if(blocks.size()==blocks.capacity()) blocks.reserve(std::max<std::size_t>(4,blocks.capacity()*2));
    } else if(!blocks[block_index]->sealed) block=blocks[block_index];
    else {block=std::make_shared<AnalysisBlock>(*blocks[block_index]);block->sealed=false;}
    // Allocate every fallible container change before publishing a slot. The
    // worker's const block views never observe a partially inserted record.
    analysis_by_object_.emplace(key,true);
    try {analysis_.emplace(id,StoredAnalysis{immutable,{},{},false,slot});}
    catch(...) {analysis_by_object_.erase(key);throw;}
    if(new_block) blocks.push_back(block);else blocks[block_index]=block;
    block->records[slot%256]=std::move(immutable);
    if(reuse) analysis_free_slots_.pop_back();else ++analysis_next_slot_;
    analysis_bytes_+=bytes;
}
std::uint64_t Document::keep_analysis(const AnalysisRequest& request,const AnalysisResult& result) {
    check_result(*this,request,result); advance(revision_);
    AnalysisRecord record{revision_+1,result,request.roi(),request.tile_size()};record.validate(metadata());
    // Insertion uses the prospective canonical revision; roll it back on failure.
    ++revision_;
    try {restore_analysis(std::move(record));} catch(...) {--revision_;throw;}
    auto& stored=analysis_.at(revision_);stored.identity=analysis_identity_.token;stored.source=server_;stored.verified=true;
    return revision_;
}
bool Document::verify_analysis(std::uint64_t id,const AnalysisRequest& request,const AnalysisResult& result) {
    check_result(*this,request,result);
    auto it=analysis_.find(id);if(it==analysis_.end()) throw std::out_of_range("analysis record does not exist");
    auto& stored=it->second;const auto& old=stored.value->result;
    AnalysisRecord check{stored.value->id,result,request.roi(),request.tile_size()};
    check.result.document_revision=old.document_revision;check.result.provenance=old.provenance;
    // Reverification is derived state; validate against the archived capture
    // bounds without advancing or exhausting the live canonical revision.
    check.validate(metadata());
    if(old.algorithm!=result.algorithm || old.object_id!=result.object_id || old.object_revision!=result.object_revision || old.kind!=result.kind || old.nucleus!=result.nucleus ||
       old.white!=result.white || old.stains!=result.stains || stored.value->tile_size!=request.tile_size() || !stored.value->roi.equals(request.roi()))
        throw std::invalid_argument("verification request does not match saved analysis parameters");
    stored.identity=analysis_identity_.token;stored.source=server_;
    stored.verified=old.input_sha256==result.input_sha256 && old.means==result.means && old.samples==result.samples && old.tiles==result.tiles;
    return stored.verified;
}
std::vector<AnalysisRecord> Document::analysis_records() const {std::vector<AnalysisRecord> result;result.reserve(analysis_.size());for(const auto& [id,entry]:analysis_) result.push_back(*entry.value);return result;}
AnalysisRecord Document::analysis_record(std::uint64_t id) const {auto it=analysis_.find(id);if(it==analysis_.end()) throw std::out_of_range("analysis record does not exist");return *it->second.value;}
std::optional<AnalysisRecord> Document::latest_analysis(std::uint64_t object_id) const {
    auto it=analysis_by_object_.upper_bound({object_id,std::numeric_limits<std::uint64_t>::max()});
    if(it==analysis_by_object_.begin() || (--it)->first.first!=object_id) return std::nullopt;
    return *analysis_.at(it->first.second).value;
}
std::string Document::analysis_status(std::uint64_t id) const {
    auto it=analysis_.find(id);if(it==analysis_.end()) throw std::out_of_range("analysis record does not exist");
    const auto& stored=it->second;const auto& result=stored.value->result;
    auto object=objects_.find(result.object_id);
    if(object==objects_.end() || object->second.revision!=result.object_revision) return "object_changed";
    const auto roi=result.nucleus?object->second.nucleus:std::optional<Roi>(object->second.geometry);
    if(!roi || !roi->equals(stored.value->roi)) return "object_changed";
    if(!stored.identity || stored.identity!=analysis_identity_.token) return "unverified";
    if(stored.source.lock().get()!=server_.get()) return "source_changed";
    if(server_->closed()) return "source_closed";
    return stored.verified?"current":"verification_failed";
}
bool Document::remove_analysis(std::uint64_t id) {
    auto it=analysis_.find(id);if(it==analysis_.end()) return false;advance(revision_);
    if(analysis_.size()==1) {
        analysis_.clear();analysis_by_object_.clear();analysis_blocks_.values.clear();
        std::vector<std::size_t>().swap(analysis_free_slots_);analysis_next_slot_=0;analysis_bytes_=0;++revision_;return true;
    }
    const auto slot=it->second.slot,block_index=slot/256;
    auto& blocks=analysis_blocks_.values;
    auto block=blocks[block_index];
    if(block->sealed) {block=std::make_shared<AnalysisBlock>(*block);block->sealed=false;}
    if(analysis_free_slots_.size()==analysis_free_slots_.capacity()) analysis_free_slots_.reserve(std::max<std::size_t>(64,analysis_free_slots_.capacity()*2));
    blocks[block_index]=block;block->records[slot%256].reset();analysis_free_slots_.push_back(slot);
    analysis_by_object_.erase({it->second.value->result.object_id,id});analysis_bytes_-=it->second.value->payload_bytes();analysis_.erase(it);++revision_;return true;
}
}
