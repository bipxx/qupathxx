#include "raw_pixel_archive.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "raw_pixel_tiled_training_internal.hpp"
#include "utf8_text.hpp"
#include "geometry_archive.hpp"
#include <limits>
namespace qpx {
namespace {
template<class Grid> bool same_grid(const Grid& a,const Grid& b) {
    return a.image_width()==b.image_width() && a.image_height()==b.image_height() &&
        a.x().origin()==b.x().origin() && a.x().extent()==b.x().extent() && a.x().samples()==b.x().samples() && a.x().context()==b.x().context() &&
        a.y().origin()==b.y().origin() && a.y().extent()==b.y().extent() && a.y().samples()==b.y().samples() && a.y().context()==b.y().context();
}
bool same_mask(const std::optional<Roi>& a,const std::optional<Roi>& b) {return bool(a)==bool(b) && (!a || GeometryArchiveCodec::encode(*a)==GeometryArchiveCodec::encode(*b));}
bool replay_source(const RawPixelSourceSnapshot& a,const RawPixelSourceSnapshot& b) {
    if(a.reader!=b.reader || a.epoch!=b.epoch || a.calibration.x!=b.calibration.x || a.calibration.y!=b.calibration.y || bool(a.observed)!=bool(b.observed)) return false;
    if(!a.observed) return true;
    const auto& x=*a.observed;const auto& y=*b.observed;
    if(x.reader!=y.reader || x.locator!=y.locator || x.scope!=y.scope || bool(x.primary)!=bool(y.primary) || x.companions.size()!=y.companions.size()) return false;
    if(x.primary && x.primary->path!=y.primary->path) return false;
    for(std::size_t i=0;i<x.companions.size();++i) if(x.companions[i].path!=y.companions[i].path) return false;
    // Reopen creates a new generation/file observation. Content commitments,
    // not an old process token, establish equality after a fresh bounded read.
    return true;
}
bool same_training(const RawPixelTrainingRequest::Data& a,const RawPixelTrainingRequest::Data& b) {
    if(!same_grid(a.grid,b.grid) || a.recipe.bytes()!=b.recipe.bytes() || !replay_source(a.source,b.source) || a.classes!=b.classes || a.rows.size()!=b.rows.size()) return false;
    const auto& x=a.options;const auto& y=b.options;
    if(x.max_samples!=y.max_samples || x.max_samples_per_class!=y.max_samples_per_class || x.work_bytes!=y.work_bytes || x.seed!=y.seed || x.max_scanned_samples!=y.max_scanned_samples || x.balanced!=y.balanced) return false;
    for(std::size_t i=0;i<a.rows.size();++i) {const auto& x=a.rows[i];const auto& y=b.rows[i];if(x.id!=y.id || x.revision!=y.revision || x.kind!=y.kind || x.label!=y.label || GeometryArchiveCodec::encode(x.roi)!=GeometryArchiveCodec::encode(y.roi)) return false;}
    return true;
}
bool same_tiled_training(const RawPixelTiledTrainingRequest::Data& a,const RawPixelTiledTrainingRequest::Data& b) {
    if(!same_grid(a.lattice,b.lattice) || a.recipe.bytes()!=b.recipe.bytes() || !replay_source(a.source,b.source) || a.classes!=b.classes || a.annotations->rows.size()!=b.annotations->rows.size() || a.tiles.width!=b.tiles.width || a.tiles.height!=b.tiles.height || a.tiles.max_windows!=b.tiles.max_windows) return false;
    const auto& x=a.options;const auto& y=b.options;if(x.max_samples!=y.max_samples || x.max_samples_per_class!=y.max_samples_per_class || x.work_bytes!=y.work_bytes || x.seed!=y.seed || x.max_scanned_samples!=y.max_scanned_samples || x.balanced!=y.balanced) return false;
    for(std::size_t i=0;i<a.annotations->rows.size();++i) {const auto& x=a.annotations->rows[i];const auto& y=b.annotations->rows[i];if(x.id!=y.id || x.revision!=y.revision || x.kind!=y.kind || x.label!=y.label || GeometryArchiveCodec::encode(x.body)!=GeometryArchiveCodec::encode(y.body)) return false;}return true;
}
bool same_prediction(const RawPixelPredictionRequest::Data& a,const RawPixelPredictionRequest::Data& b) {
    const auto& x=*a.source.provenance;const auto& y=*b.source.provenance;
    return same_grid(a.grid,b.grid) && a.model.sha256()==b.model.sha256() && a.minimum==b.minimum && a.budget==b.budget &&
        replay_source(a.source,b.source) && same_mask(a.mask,b.mask) && x.source_only==y.source_only && x.object_id==y.object_id && x.object_revision==y.object_revision;
}
}
RawPixelTrainingRequest RawPixelTrainingRequest::replay(const Document& document,std::uint64_t id) {
    const auto archive=document.raw_pixel_record(id)->archive;if(!archive->table_) throw std::invalid_argument("raw pixel record is not a training table");
    const auto& before=*archive->table_->snapshot_;std::vector<std::uint64_t> ids;ids.reserve(before.rows.size());for(const auto& row:before.rows) ids.push_back(row.id);
    RawPixelTrainingRequest request(document,ids,before.grid,before.recipe.selection(),before.recipe.requested_features(),before.options,before.recipe.physical_sigmas());
    if(!same_training(before,*request.data_)) throw std::invalid_argument("raw pixel replay annotations, source or recipe no longer match capture");return request;
}
RawPixelTiledTrainingRequest RawPixelTiledTrainingRequest::replay(const Document& doc,std::uint64_t id) {
    const auto archive=doc.raw_pixel_record(id)->archive;if(!archive->tiled_table_) throw std::invalid_argument("raw record is not a sparse tiled training table");const auto& before=*archive->tiled_table_->snapshot_;std::vector<std::uint64_t> ids;ids.reserve(before.annotations->rows.size());for(const auto& a:before.annotations->rows) ids.push_back(a.id);
    RawPixelTiledTrainingRequest request(doc,ids,before.lattice,before.recipe.selection(),before.recipe.requested_features(),before.options,before.tiles,before.recipe.physical_sigmas());if(!same_tiled_training(before,*request.data_)) throw std::invalid_argument("raw tiled replay annotations, source or recipe differ");return request;
}
RawPixelPredictionRequest RawPixelPredictionRequest::replay(const Document& document,std::uint64_t id) {
    const auto archive=document.raw_pixel_record(id)->archive;if(!archive->prediction_) throw std::invalid_argument("raw pixel record is not a prediction");
    const auto& before=*archive->prediction_;RawPixelPredictionRequest request(document,before.model,before.grid,before.source.provenance->object_id,before.minimum,before.budget);
    if(!same_prediction(before,*request.data_)) throw std::invalid_argument("raw pixel replay object, source or recipe no longer match capture");return request;
}
bool Document::verify_raw_pixel_training(std::uint64_t id,const RawPixelTrainingRequest& request,std::shared_ptr<const RawPixelTrainingTable> table,std::shared_ptr<const RawPixelClassifier> model) const {
    if(!table || table->snapshot_->capture!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel verification table is not a current captured result");
    const auto archive=raw_pixel_record(id)->archive;if(!archive->table_ || !same_training(*archive->table_->snapshot_,*request.data_)) throw std::invalid_argument("raw pixel verification recipe differs from retained training");
    const auto& before=*archive->table_;return (!model || (archive->model_ && model->sha256()==archive->model_->sha256())) && before.sha256()==table->sha256() && before.input_sha256()==table->input_sha256() && before.eligible_class_samples()==table->eligible_class_samples() && before.sampled_class_counts()==table->sampled_class_counts() && before.ambiguous_samples()==table->ambiguous_samples();
}
bool Document::verify_raw_pixel_tiled_training(std::uint64_t id,const RawPixelTiledTrainingRequest& request,std::shared_ptr<const RawPixelTiledTrainingTable> table,std::shared_ptr<const RawPixelClassifier> model) const {
    if(!table || !table->snapshot_->capture || table->snapshot_->capture!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw tiled verification needs a current captured table");const auto archive=raw_pixel_record(id)->archive;if(!archive->tiled_table_ || !same_tiled_training(*archive->tiled_table_->snapshot_,*request.data_)) throw std::invalid_argument("raw tiled verification recipe differs from retained table");const auto& before=*archive->tiled_table_;
    return (!model || (archive->model_ && model->sha256()==archive->model_->sha256())) && before.sha256()==table->sha256() && before.input_sha256()==table->input_sha256() && before.eligible_class_samples()==table->eligible_class_samples() && before.sampled_class_counts()==table->sampled_class_counts() && before.ambiguous_samples()==table->ambiguous_samples();
}
bool Document::verify_raw_pixel_prediction(std::uint64_t id,const RawPixelPredictionRequest& request,std::shared_ptr<const RawPixelClassification> result) const {
    if(!result || result->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel verification prediction is not a current captured result");
    const auto archive=raw_pixel_record(id)->archive;if(!archive->result_ || !same_prediction(*archive->prediction_,*request.data_)) throw std::invalid_argument("raw pixel verification recipe differs from retained prediction");
    return archive->result_->sha256()==result->sha256() && archive->result_->input_sha256()==result->input_sha256() && archive->result_->model_sha256()==result->model_sha256();
}
void Document::restore_raw_pixel(std::shared_ptr<const RawPixelRecord> record) {
    if(!record || record->id>revision_ || record->captured_next_id>next_id_) throw std::invalid_argument("raw pixel record exceeds document identity bounds");RawPixelArchiveCodec::validate_record_bounds(*record,metadata());
    const auto id=record->id;if(raw_pixels_.count(id) || raw_analysis_.count(id) || streams_.count(id) || pixels_.count(id) || classifiers_.count(id) || cells_.count(id) || tissue_.count(id) || analysis_.count(id)) throw std::invalid_argument("duplicate raw pixel record ID");
    const auto bytes=record->payload_bytes();if(raw_pixels_.size()>=10000 || bytes>256u*1024*1024-raw_pixels_bytes_) throw std::length_error("raw pixel records exceed 10000/256 MiB retained limit");raw_pixels_.emplace(id,std::move(record));raw_pixels_bytes_+=bytes;
}
std::vector<std::shared_ptr<const RawPixelRecord>> Document::raw_pixel_records() const {std::vector<std::shared_ptr<const RawPixelRecord>> out;out.reserve(raw_pixels_.size());for(const auto& [id,record]:raw_pixels_) out.push_back(record);return out;}
std::shared_ptr<const RawPixelRecord> Document::raw_pixel_record(std::uint64_t id) const {const auto found=raw_pixels_.find(id);if(found==raw_pixels_.end()) throw std::out_of_range("raw pixel record does not exist");return found->second;}
std::uint64_t Document::keep_raw_pixel_training(const RawPixelTrainingRequest& request,std::shared_ptr<const RawPixelArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->table() || !archive->capture_ || archive->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel training archive is not a current captured result");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);++revision_;try {restore_raw_pixel(record);}catch(...) {--revision_;throw;}return record->id;
}
std::uint64_t Document::keep_raw_pixel_tiled_training(const RawPixelTiledTrainingRequest& request,std::shared_ptr<const RawPixelArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->tiled_table() || !archive->capture_ || archive->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw tiled training archive is not a current captured result");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);++revision_;try {restore_raw_pixel(record);}catch(...) {--revision_;throw;}return record->id;
}
std::uint64_t Document::keep_raw_pixel_prediction(const RawPixelPredictionRequest& request,std::shared_ptr<const RawPixelArchive> archive,std::string name) {
    detail::bounded_utf8(name,256);if(name.empty() || !archive || !archive->result() || !archive->capture_ || archive->capture_!=request.data_->capture || !request.publication_matches(*this)) throw std::invalid_argument("raw pixel prediction archive is not a current captured result");
    if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelRecord>();record->id=revision_+1;record->captured_next_id=next_id_;record->name=std::move(name);record->archive=std::move(archive);++revision_;try {restore_raw_pixel(record);}catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_raw_pixel_record(std::uint64_t id) {const auto found=raw_pixels_.find(id);if(found==raw_pixels_.end()) return false;if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");raw_pixels_bytes_-=found->second->payload_bytes();raw_pixels_.erase(found);++revision_;return true;}
}
