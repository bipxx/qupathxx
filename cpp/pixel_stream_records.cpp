#include "pixel_stream_record.hpp"
#include "atomic_export.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <filesystem>
#include <algorithm>
#include <limits>
#include <numeric>
#include <cstring>
namespace qpx {
namespace {
constexpr std::size_t limit=128*1024;
void hash(const std::string& s) {if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid pixel stream link hash");}
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','S','L',1};
    void integer(std::uint64_t n) {for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
    void text(const std::string& s) {if(s.size()>limit-8 || bytes.size()>limit-s.size()-8) throw std::length_error("pixel stream link exceeds size limit");integer(s.size());bytes.insert(bytes.end(),s.begin(),s.end());}
};
struct Reader {
    const std::uint8_t* data;std::size_t size,at=4;
    Reader(const std::uint8_t* p,std::size_t n):data(p),size(n) {
        if(n<68 || n>limit || std::memcmp(p,"QSL\1",4)) throw std::invalid_argument("invalid pixel stream record encoding");Sha256 h;h.update(p,n-64);if(h.finish()!=std::string(reinterpret_cast<const char*>(p+n-64),64)) throw std::invalid_argument("pixel stream record checksum mismatch");size-=64;
    }
    std::uint64_t integer() {if(at>size || size-at<8) throw std::invalid_argument("truncated pixel stream record");std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(data[at++])<<(8*i);return n;}
    std::string text(std::size_t maximum) {const auto n=integer();if(n>maximum || at>size || n>size-at) throw std::invalid_argument("invalid pixel stream record text length");std::string s(reinterpret_cast<const char*>(data+at),static_cast<std::size_t>(n));at+=static_cast<std::size_t>(n);detail::bounded_utf8(s,maximum);return s;}
};

}
std::size_t PixelStreamLink::payload_bytes() const {return sizeof(*this)+path_.capacity()+model_.capacity()+summary_.sha256.capacity()+summary_.class_counts.capacity()*sizeof(std::uint64_t);}
std::size_t PixelStreamRecord::payload_bytes() const {return sizeof(*this)+name.capacity()+(link?link->payload_bytes():0);}
void PixelStreamLink::validate() const {
    detail::bounded_utf8(path_,32768);if(path_.empty() || width_<=0 || height_<=0 || !object_ || !block_ || block_>512 || !summary_.blocks || summary_.blocks>100000000 || summary_.bytes<128 || summary_.bytes>(1ULL<<46) || summary_.class_counts.empty() || summary_.class_counts.size()>1024) throw std::invalid_argument("invalid pixel stream link metadata");
    hash(model_);hash(summary_.sha256);if(!grid_[2] || !grid_[3] || grid_[0]>=std::uint64_t(width_) || grid_[1]>=std::uint64_t(height_) || grid_[2]>std::uint64_t(width_)-grid_[0] || grid_[3]>std::uint64_t(height_)-grid_[1]) throw std::invalid_argument("pixel stream link grid exceeds source");
    if(grid_[2]>(1ULL<<46)/grid_[3]) throw std::invalid_argument("pixel stream link grid overflow");const auto pixels=grid_[2]*grid_[3];
    if(summary_.valid_pixels>pixels || summary_.classified_pixels>summary_.valid_pixels || ((grid_[2]+block_-1)/block_)*((grid_[3]+block_-1)/block_)!=summary_.blocks) throw std::invalid_argument("invalid pixel stream link coverage");
    std::uint64_t sum=0;for(auto count:summary_.class_counts) {if(count>summary_.classified_pixels-sum) throw std::invalid_argument("invalid pixel stream link class counts");sum+=count;}if(sum!=summary_.classified_pixels) throw std::invalid_argument("invalid pixel stream link class totals");
}
bool PixelStreamLink::same_content(const PixelStreamLink& b) const {
    return model_==b.model_ && summary_.sha256==b.summary_.sha256 && summary_.bytes==b.summary_.bytes && summary_.blocks==b.summary_.blocks && summary_.valid_pixels==b.summary_.valid_pixels && summary_.classified_pixels==b.summary_.classified_pixels && summary_.class_counts==b.summary_.class_counts && width_==b.width_ && height_==b.height_ && object_==b.object_ && revision_==b.revision_ && std::equal(std::begin(grid_),std::end(grid_),std::begin(b.grid_)) && windowed_==b.windowed_ && block_==b.block_;
}
std::shared_ptr<const PixelStreamLink> prepare_pixel_stream_link(const std::string& path,std::uint64_t max_bytes,const std::atomic<bool>* cancelled) {PixelStreamFile file(path,max_bytes);file.verify(cancelled);detail::export_checkpoint(cancelled);return file.link();}
std::vector<std::uint8_t> PixelStreamRecordCodec::encode(const PixelStreamRecord& record,const std::string& directory,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);detail::bounded_utf8(record.name,256);if(!record.id || record.name.empty() || !record.link) throw std::invalid_argument("invalid pixel stream record");const auto& l=*record.link;l.validate();Writer w;w.integer(record.id);w.text(record.name);
    auto path=std::filesystem::u8path(l.path_);const auto relative=path.lexically_relative(std::filesystem::u8path(directory));if(!relative.empty()) path=relative;w.text(path.generic_u8string());
    w.text(l.model_);w.text(l.summary_.sha256);for(auto v:{std::uint64_t(l.width_),std::uint64_t(l.height_),l.object_,l.revision_,l.grid_[0],l.grid_[1],l.grid_[2],l.grid_[3],std::uint64_t(l.block_),std::uint64_t(l.windowed_),l.summary_.bytes,l.summary_.blocks,l.summary_.valid_pixels,l.summary_.classified_pixels,std::uint64_t(l.summary_.class_counts.size())}) w.integer(v);for(auto n:l.summary_.class_counts) w.integer(n);
    if(w.bytes.size()>limit-64) throw std::length_error("pixel stream record exceeds byte limit");Sha256 h;h.update(w.bytes.data(),w.bytes.size());const auto sha=h.finish();w.bytes.insert(w.bytes.end(),sha.begin(),sha.end());detail::export_checkpoint(cancel);return std::move(w.bytes);
}
std::shared_ptr<const PixelStreamRecord> PixelStreamRecordCodec::decode(const std::uint8_t* data,std::size_t size,const std::string& directory,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);Reader r(data,size);auto record=std::make_shared<PixelStreamRecord>();auto link=std::make_shared<PixelStreamLink>();record->id=r.integer();record->name=r.text(256);const auto stored_path=r.text(32768);if(stored_path.empty()) throw std::invalid_argument("empty pixel stream link path");auto path=std::filesystem::u8path(stored_path);if(path.is_relative()) path=std::filesystem::u8path(directory)/path;link->path_=detail::export_path(path.u8string());link->model_=r.text(64);link->summary_.sha256=r.text(64);
    const auto width=r.integer(),height=r.integer();if(width>INT64_MAX || height>INT64_MAX) throw std::invalid_argument("pixel stream source dimensions overflow");link->width_=static_cast<std::int64_t>(width);link->height_=static_cast<std::int64_t>(height);link->object_=r.integer();link->revision_=r.integer();for(auto& n:link->grid_) n=r.integer();const auto step=r.integer(),windowed=r.integer();if(step>512 || windowed>1) throw std::invalid_argument("invalid pixel stream grid flags");link->block_=static_cast<std::size_t>(step);link->windowed_=windowed!=0;
    link->summary_.bytes=r.integer();link->summary_.blocks=r.integer();link->summary_.valid_pixels=r.integer();link->summary_.classified_pixels=r.integer();const auto count=r.integer();if(!count || count>1024) throw std::invalid_argument("invalid pixel stream class count");for(std::uint64_t i=0;i<count;++i) link->summary_.class_counts.push_back(r.integer());if(r.at!=r.size || !record->id || record->name.empty()) throw std::invalid_argument("invalid pixel stream record trailer");link->validate();record->link=std::move(link);detail::export_checkpoint(cancel);return record;
}
void Document::restore_pixel_stream(std::shared_ptr<const PixelStreamRecord> record) {
    if(!record || !record->id || record->id>revision_ || !record->link || record->link->source_width()!=metadata().width || record->link->source_height()!=metadata().height) throw std::invalid_argument("pixel stream record source/revision mismatch");
    detail::bounded_utf8(record->name,256);if(record->name.empty()) throw std::invalid_argument("pixel stream record name is empty");
    if(streams_.count(record->id) || raw_pixels_.count(record->id) || pixels_.count(record->id) || classifiers_.count(record->id) || cells_.count(record->id) || tissue_.count(record->id) || analysis_.count(record->id)) throw std::invalid_argument("duplicate pixel stream record ID");
    const auto bytes=record->payload_bytes();if(streams_.size()>=10000 || bytes>64u*1024*1024-streams_bytes_) throw std::length_error("pixel stream records exceed 10000-record/64 MiB limit");streams_.emplace(record->id,std::move(record));streams_bytes_+=bytes;
}
std::vector<std::shared_ptr<const PixelStreamRecord>> Document::pixel_stream_records() const {std::vector<std::shared_ptr<const PixelStreamRecord>> out;out.reserve(streams_.size());for(const auto& [id,r]:streams_) out.push_back(r);return out;}
std::shared_ptr<const PixelStreamRecord> Document::pixel_stream_record(std::uint64_t id) const {const auto r=streams_.find(id);if(r==streams_.end()) throw std::out_of_range("pixel stream record does not exist");return r->second;}
std::uint64_t Document::keep_pixel_stream(std::shared_ptr<const PixelStreamLink> link,std::string name) {
    if(!link) throw std::invalid_argument("pixel stream link is required");if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<PixelStreamRecord>();record->id=revision_+1;record->name=std::move(name);record->link=std::move(link);++revision_;try {restore_pixel_stream(record);}catch(...) {--revision_;throw;}return record->id;
}
bool Document::remove_pixel_stream_record(std::uint64_t id) {const auto r=streams_.find(id);if(r==streams_.end()) return false;if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");streams_bytes_-=r->second->payload_bytes();streams_.erase(r);++revision_;return true;}
void Document::relink_pixel_stream_record(std::uint64_t id,std::shared_ptr<const PixelStreamLink> link) {
    const auto old=pixel_stream_record(id);if(!link || !old->link->same_content(*link)) throw std::invalid_argument("replacement stream content does not match saved link");if(revision_==UINT64_MAX) throw std::overflow_error("document revision exhausted");auto record=std::make_shared<PixelStreamRecord>();record->id=id;record->name=old->name;record->link=std::move(link);const auto bytes=record->payload_bytes();if(bytes>64u*1024*1024-(streams_bytes_-old->payload_bytes())) throw std::length_error("pixel stream links exceed memory limit");streams_[id]=record;streams_bytes_=streams_bytes_-old->payload_bytes()+bytes;++revision_;
}
}
