#include "raw_pixel_stream_record.hpp"
#include "raw_pixel_stream_internal.hpp"
#include "raw_pixel_archive_binary.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "filesystem_access.hpp"

namespace qpx {
using namespace detail::raw_archive;
namespace {
constexpr std::size_t link_limit=128u*1024;
template<class Axis> bool same_axis(const Axis& a,const Axis& b) {return a.origin()==b.origin() && a.extent()==b.extent() && a.samples()==b.samples() && a.context()==b.context();}
void stream_options(Writer& w,const RawPixelStreamOptions& o) {w.integer(o.tile_width);w.integer(o.tile_height);w.integer(o.max_samples);w.integer(o.max_windows);w.integer(o.work_bytes);w.real(o.min_vote_fraction);}
RawPixelStreamOptions stream_options(Reader& r) {RawPixelStreamOptions o;o.tile_width=std::int64_t(r.integer(4096));o.tile_height=std::int64_t(r.integer(4096));o.max_samples=r.integer(1000000000000ULL);o.max_windows=r.integer(1000000000000ULL);o.work_bytes=std::size_t(r.integer(1024ULL*1024*1024));o.min_vote_fraction=r.real();o.validate();return o;}
}
std::size_t RawPixelStreamLink::payload_bytes() const {
    std::size_t n=sizeof(*this)+path_.capacity()+model_.capacity()+request_.capacity()+header_.capacity()+summary_.class_counts.capacity()*sizeof(std::uint64_t)+summary_.input_sha256.capacity()+summary_.sha256.capacity()+summary_.content_sha256.capacity();
    if(version_) n+=sizeof(FileVersion)+version_->path.capacity()+version_->identity_scheme.capacity()+version_->identity.capacity()+version_->stamp_scheme.capacity()+version_->modified_token.capacity()+version_->change_token.capacity();return n;
}
void RawPixelStreamLink::validate() const {
    detail::bounded_utf8(path_,32768);if(path_.empty() || !std::filesystem::u8path(path_).is_absolute() || (object_ && !revision_)) throw std::invalid_argument("invalid raw stream link path/source history");
    const RawPixelStreamGeometry g(lattice_,region_,options_);const auto& s=summary_;
    for(const auto* h:{&model_,&request_,&header_,&s.input_sha256,&s.sha256,&s.content_sha256}) hash_check(*h);
    if(s.total_windows!=g.windows || s.completed_windows>s.total_windows || s.complete!=(s.completed_windows==s.total_windows) || s.reused_windows || s.class_counts.size()<2 || s.class_counts.size()>256 || s.valid_samples>g.samples_before(s.completed_windows) || s.classified_samples>s.valid_samples || (options_.min_vote_fraction==0 && std::signbit(options_.min_vote_fraction))) throw std::invalid_argument("invalid raw stream link coverage/totals");
    if(s.bytes!=raw_pixel_stream_file_bytes(g,header_bytes_,s.completed_windows,s.class_counts.size())) throw std::invalid_argument("raw stream link file size mismatch");
    std::uint64_t sum=0;for(auto count:s.class_counts) {if(count>s.classified_samples-sum) throw std::invalid_argument("raw stream link class count overflow");sum+=count;}if(sum!=s.classified_samples) throw std::invalid_argument("raw stream link class totals mismatch");
    if(!s.completed_windows && (s.input_sha256!=request_ || s.sha256!=request_ || s.content_sha256!=header_)) throw std::invalid_argument("raw stream link empty-prefix commitment mismatch");
}
bool RawPixelStreamLink::same_content(const RawPixelStreamLink& b) const {
    const auto& a=summary_;const auto& s=b.summary_;
    return model_==b.model_ && request_==b.request_ && header_==b.header_ && header_bytes_==b.header_bytes_ && object_==b.object_ && revision_==b.revision_
        && lattice_.image_width()==b.lattice_.image_width() && lattice_.image_height()==b.lattice_.image_height() && same_axis(lattice_.x(),b.lattice_.x()) && same_axis(lattice_.y(),b.lattice_.y())
        && region_.first_x==b.region_.first_x && region_.first_y==b.region_.first_y && region_.width==b.region_.width && region_.height==b.region_.height
        && options_.tile_width==b.options_.tile_width && options_.tile_height==b.options_.tile_height && options_.max_samples==b.options_.max_samples && options_.max_windows==b.options_.max_windows && options_.work_bytes==b.options_.work_bytes && options_.min_vote_fraction==b.options_.min_vote_fraction
        && a.bytes==s.bytes && a.total_windows==s.total_windows && a.completed_windows==s.completed_windows && a.valid_samples==s.valid_samples && a.classified_samples==s.classified_samples && a.class_counts==s.class_counts && a.complete==s.complete && a.input_sha256==s.input_sha256 && a.sha256==s.sha256 && a.content_sha256==s.content_sha256;
}
void RawPixelStreamLink::check_verified_file() const {
    if(!version_) throw std::invalid_argument("verify the external raw stream before keeping or relinking its historical metadata");
    detail::ReaderFileGuard guard(path_);const auto& a=*version_;const auto& b=*guard.version();
    if(a.bytes!=b.bytes || a.identity_scheme!=b.identity_scheme || a.identity!=b.identity || a.stamp_scheme!=b.stamp_scheme || a.modified_token!=b.modified_token || a.change_token!=b.change_token) throw std::runtime_error("verified raw stream changed before record publication");guard.verify();
}
std::vector<std::uint8_t> RawPixelStreamRecordCodec::encode(const RawPixelStreamRecord& record,const std::string& directory,const std::atomic<bool>* c) {
    stop(c);detail::bounded_utf8(record.name,256);if(!record.id || record.name.empty() || !record.link) throw std::invalid_argument("invalid raw stream record");const auto& l=*record.link;l.validate();Writer w("QWL\1",c,link_limit);w.integer(record.id);w.text(record.name);
    auto path=std::filesystem::u8path(l.path_);const auto relative=detail::relative_locator(path,std::filesystem::u8path(directory));if(!relative.empty()) path=relative;w.text(path.generic_u8string());
    grid(w,l.lattice_);for(auto v:{l.region_.first_x,l.region_.first_y,l.region_.width,l.region_.height}) w.signed_integer(v);stream_options(w,l.options_);w.integer(l.object_);w.integer(l.revision_);w.integer(l.header_bytes_);w.text(l.model_);w.text(l.request_);w.text(l.header_);
    const auto& s=l.summary_;for(auto n:{s.bytes,s.total_windows,s.completed_windows,s.valid_samples,s.classified_samples,std::uint64_t(s.complete)}) w.integer(n);w.text(s.input_sha256);w.text(s.sha256);w.text(s.content_sha256);w.integer(s.class_counts.size());for(auto count:s.class_counts) w.integer(count);w.finish();return std::move(w.bytes);
}
std::shared_ptr<const RawPixelStreamRecord> RawPixelStreamRecordCodec::decode(const std::uint8_t* bytes,std::size_t size,const std::string& directory,const std::atomic<bool>* c) {
    Reader r(bytes,size,"QWL\1",c,link_limit);auto record=std::make_shared<RawPixelStreamRecord>();record->id=r.integer();record->name=r.text(256);const auto stored=r.text(32768);if(stored.empty()) throw std::invalid_argument("empty raw stream link path");auto path=std::filesystem::u8path(stored);if(path.is_relative()) path=std::filesystem::u8path(directory)/path;
    const auto g=lattice(r);RawPixelStreamRegion region;region.first_x=r.signed_integer();region.first_y=r.signed_integer();region.width=r.signed_integer();region.height=r.signed_integer();const auto options=stream_options(r);auto link=std::shared_ptr<RawPixelStreamLink>(new RawPixelStreamLink(g,region,options));link->path_=detail::export_path(path.u8string());link->object_=r.integer();link->revision_=r.integer();link->header_bytes_=r.integer(128u*1024*1024);link->model_=r.text(64);link->request_=r.text(64);link->header_=r.text(64);
    auto& s=link->summary_;s.bytes=r.integer(1ULL<<46);s.total_windows=r.integer(1000000000000ULL);s.completed_windows=r.integer(s.total_windows);s.valid_samples=r.integer(1000000000000ULL);s.classified_samples=r.integer(s.valid_samples);s.complete=r.integer(1)!=0;s.input_sha256=r.text(64);s.sha256=r.text(64);s.content_sha256=r.text(64);const auto count=r.integer(256);if(count<2) throw std::invalid_argument("invalid raw stream link class count");s.class_counts.reserve(std::size_t(count));for(std::uint64_t i=0;i<count;++i) s.class_counts.push_back(r.integer(s.classified_samples));r.finish();if(!record->id || record->name.empty()) throw std::invalid_argument("invalid raw stream record identity/name");link->validate();record->link=std::move(link);return record;
}
void Document::restore_raw_pixel_stream(std::shared_ptr<const RawPixelStreamRecord> record) {
    if(!record || !record->id || record->id>revision_ || !record->link || record->link->lattice().image_width()!=metadata().width || record->link->lattice().image_height()!=metadata().height) throw std::invalid_argument("raw stream record source/revision mismatch");
    detail::bounded_utf8(record->name,256);if(record->name.empty()) throw std::invalid_argument("raw stream record name is empty");record->link->validate();const auto id=record->id;
    if(raw_streams_.count(id) || streams_.count(id) || raw_pixels_.count(id) || raw_analysis_.count(id) || pixels_.count(id) || classifiers_.count(id) || cells_.count(id) || tissue_.count(id) || analysis_.count(id)) throw std::invalid_argument("duplicate raw stream record ID");
    const auto bytes=record->payload_bytes();if(raw_streams_.size()>=10000 || raw_streams_bytes_>64u*1024*1024 || bytes>64u*1024*1024-raw_streams_bytes_) throw std::length_error("raw stream records exceed 10000-record/64 MiB limit");raw_streams_.emplace(id,std::move(record));raw_streams_bytes_+=bytes;
}
std::vector<std::shared_ptr<const RawPixelStreamRecord>> Document::raw_pixel_stream_records() const {std::vector<std::shared_ptr<const RawPixelStreamRecord>> out;out.reserve(raw_streams_.size());for(const auto& entry:raw_streams_) out.push_back(entry.second);return out;}
std::shared_ptr<const RawPixelStreamRecord> Document::raw_pixel_stream_record(std::uint64_t id) const {const auto found=raw_streams_.find(id);if(found==raw_streams_.end()) throw std::out_of_range("raw stream record does not exist");return found->second;}
std::uint64_t Document::keep_raw_pixel_stream(std::shared_ptr<const RawPixelStreamLink> link,std::string name) {
    if(!link) throw std::invalid_argument("raw stream link is required");link->check_verified_file();if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelStreamRecord>();record->id=revision_+1;record->name=std::move(name);record->link=std::move(link);++revision_;try {restore_raw_pixel_stream(record);}catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_raw_pixel_stream_record(std::uint64_t id) {const auto found=raw_streams_.find(id);if(found==raw_streams_.end()) return false;if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");raw_streams_bytes_-=found->second->payload_bytes();raw_streams_.erase(found);++revision_;return true;}
void Document::relink_raw_pixel_stream_record(std::uint64_t id,std::shared_ptr<const RawPixelStreamLink> link) {
    const auto old=raw_pixel_stream_record(id);if(!link || !old->link->same_content(*link)) throw std::invalid_argument("replacement raw stream content does not match saved link");link->check_verified_file();if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<RawPixelStreamRecord>();record->id=id;record->name=old->name;record->link=std::move(link);const auto bytes=record->payload_bytes();if(bytes>64u*1024*1024-(raw_streams_bytes_-old->payload_bytes())) throw std::length_error("raw stream records exceed memory limit");raw_streams_[id]=record;raw_streams_bytes_=raw_streams_bytes_-old->payload_bytes()+bytes;++revision_;
}
}
