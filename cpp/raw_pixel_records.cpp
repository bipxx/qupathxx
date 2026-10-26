#include "raw_pixel_archive.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "utf8_text.hpp"
#include <limits>
namespace qpx {
void Document::restore_raw_pixel(std::shared_ptr<const RawPixelRecord> record) {
    if(!record || !record->archive || !record->id || record->id>revision_ || !record->captured_next_id || record->captured_next_id>next_id_ || record->archive->provenance()->document_revision>=record->id || record->archive->provenance()->object_id>=record->captured_next_id || record->archive->grid().image_width()!=metadata().width || record->archive->grid().image_height()!=metadata().height) throw std::invalid_argument("raw pixel record identity/source bounds mismatch");
    const auto id=record->id;if(raw_pixels_.count(id) || raw_analysis_.count(id) || streams_.count(id) || pixels_.count(id) || classifiers_.count(id) || cells_.count(id) || tissue_.count(id) || analysis_.count(id)) throw std::invalid_argument("duplicate raw pixel record ID");
    if(record->archive->table()) for(auto object:record->archive->table()->annotation_ids()) if(object>=record->captured_next_id) throw std::invalid_argument("raw pixel record exceeds captured object IDs");
    const auto bytes=record->payload_bytes();if(raw_pixels_.size()>=10000 || bytes>256u*1024*1024-raw_pixels_bytes_) throw std::length_error("raw pixel records exceed 10000/256 MiB retained limit");raw_pixels_.emplace(id,std::move(record));raw_pixels_bytes_+=bytes;
}
std::vector<std::shared_ptr<const RawPixelRecord>> Document::raw_pixel_records() const {std::vector<std::shared_ptr<const RawPixelRecord>> out;out.reserve(raw_pixels_.size());for(const auto& [id,record]:raw_pixels_) out.push_back(record);return out;}
std::shared_ptr<const RawPixelRecord> Document::raw_pixel_record(std::uint64_t id) const {const auto found=raw_pixels_.find(id);if(found==raw_pixels_.end()) throw std::out_of_range("raw pixel record does not exist");return found->second;}
std::uint64_t Document::keep_raw_pixel_training(const RawPixelTrainingRequest& request,std::shared_ptr<const RawPixelArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->table() || !archive->capture_ || archive->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel training archive is not a current captured result");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);++revision_;try {restore_raw_pixel(record);}catch(...) {--revision_;throw;}return record->id;
}
std::uint64_t Document::keep_raw_pixel_prediction(const RawPixelPredictionRequest& request,std::shared_ptr<const RawPixelArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->result() || !archive->capture_ || archive->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel prediction archive is not a current captured result");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);++revision_;try {restore_raw_pixel(record);}catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_raw_pixel_record(std::uint64_t id) {const auto found=raw_pixels_.find(id);if(found==raw_pixels_.end()) return false;if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");raw_pixels_bytes_-=found->second->payload_bytes();raw_pixels_.erase(found);++revision_;return true;}
}
