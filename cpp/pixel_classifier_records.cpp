#include "pixel_classifier_archive.hpp"
#include "pixel_workflow_snapshot.hpp"
#include "utf8_text.hpp"
#include <limits>
#include <stdexcept>
namespace qpx {
void Document::restore_pixel_classifier(std::shared_ptr<const PixelClassifierRecord> record) {
    if(!record || !record->archive || !record->id || record->id>revision_ || !record->captured_next_id || record->captured_next_id>next_id_ || record->archive->provenance()->document_revision>=record->id) throw std::invalid_argument("pixel record exceeds document revision bounds");
    const auto& source=record->archive->provenance()->source;if(source.width!=metadata().width || source.height!=metadata().height) throw std::invalid_argument("pixel record image dimensions mismatch");
    for(const auto& a:record->archive->annotations()) if(a.id>=record->captured_next_id) throw std::invalid_argument("pixel record exceeds document object identity bounds");
    if(pixels_.count(record->id) || classifiers_.count(record->id) || cells_.count(record->id) || tissue_.count(record->id) || analysis_.count(record->id)) throw std::invalid_argument("duplicate pixel record ID");
    const auto bytes=record->payload_bytes();if(pixels_.size()>=10000 || bytes>256u*1024*1024-pixels_bytes_) throw std::length_error("pixel archives exceed 10000-record/256 MiB retained limit");
    pixels_.emplace(record->id,std::move(record));pixels_bytes_+=bytes;
}
std::vector<std::shared_ptr<const PixelClassifierRecord>> Document::pixel_classifier_records() const {std::vector<std::shared_ptr<const PixelClassifierRecord>> out;out.reserve(pixels_.size());for(const auto& [id,p]:pixels_) out.push_back(p);return out;}
std::shared_ptr<const PixelClassifierRecord> Document::pixel_classifier_record(std::uint64_t id) const {const auto found=pixels_.find(id);if(found==pixels_.end()) throw std::out_of_range("pixel classifier record does not exist");return found->second;}
std::uint64_t Document::keep_pixel_training(const PixelTrainingRequest& request,std::shared_ptr<const PixelClassifierArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->table() || !request.matches(*this) || archive->table()->snapshot_!=request.data_) throw std::invalid_argument("pixel training archive does not match a current request");
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<PixelClassifierRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);
    ++revision_;try {restore_pixel_classifier(record);}catch(...) {--revision_;throw;}return record->id;
}
std::uint64_t Document::keep_pixel_prediction(const PixelPredictionRequest& request,std::shared_ptr<const PixelClassifierArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->result() || !request.matches(*this) || !archive->result()->matches(request)) throw std::invalid_argument("pixel prediction archive does not match a current request");
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<PixelClassifierRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);
    ++revision_;try {restore_pixel_classifier(record);}catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_pixel_classifier_record(std::uint64_t id) {const auto found=pixels_.find(id);if(found==pixels_.end()) return false;if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");pixels_bytes_-=found->second->payload_bytes();pixels_.erase(found);++revision_;return true;}
}
