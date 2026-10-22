#include "object_classifier_archive.hpp"
#include "object_feature_snapshot.hpp"
#include "utf8_text.hpp"
#include <limits>
#include <stdexcept>
namespace qpx {
void Document::restore_classifier(std::shared_ptr<const ObjectClassifierRecord> record) {
    if(!record || !record->archive || !record->id || record->id>revision_ || record->archive->table()->source_anchor()->document_revision>=record->id || record->archive->table()->object_ids().back()>=next_id_ || record->archive->table()->snapshot_->next_id>next_id_) throw std::invalid_argument("classifier record exceeds document identity/revision bounds");
    const auto& source=record->archive->table()->source_anchor()->source;if(source.width!=metadata().width || source.height!=metadata().height) throw std::invalid_argument("classifier record image dimensions mismatch");
    if(classifiers_.count(record->id) || cells_.count(record->id) || tissue_.count(record->id) || analysis_.count(record->id)) throw std::invalid_argument("duplicate classifier record ID");
    const auto bytes=record->payload_bytes();if(classifiers_.size()>=10000 || bytes>256u*1024*1024-classifiers_bytes_) throw std::length_error("classifier archive exceeds its 10000-record/256 MiB limit");
    classifiers_.emplace(record->id,std::move(record));classifiers_bytes_+=bytes;
}
std::vector<std::shared_ptr<const ObjectClassifierRecord>> Document::classifier_records() const {std::vector<std::shared_ptr<const ObjectClassifierRecord>> out;out.reserve(classifiers_.size());for(const auto& [id,record]:classifiers_) out.push_back(record);return out;}
std::shared_ptr<const ObjectClassifierRecord> Document::classifier_record(std::uint64_t id) const {const auto found=classifiers_.find(id);if(found==classifiers_.end()) throw std::out_of_range("classifier record does not exist");return found->second;}
std::uint64_t Document::keep_classifier_training(const ObjectFeatureRequest& request,std::shared_ptr<const ObjectClassifierArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || archive->result() || !request.matches(*this) || archive->table()->snapshot_!=request.data_) throw std::invalid_argument("training archive does not match a current request");
    if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");
    auto record=std::make_shared<ObjectClassifierRecord>();record->id=revision_+1;record->name=std::move(name);record->archive=std::move(archive);
    ++revision_;try {restore_classifier(record);} catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_classifier_record(std::uint64_t id) {const auto found=classifiers_.find(id);if(found==classifiers_.end()) return false;if(revision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("document revision exhausted");classifiers_bytes_-=found->second->payload_bytes();classifiers_.erase(found);++revision_;return true;}
}
