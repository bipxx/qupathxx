#include "qupathxx/raw_pixel_training_checkpoint.hpp"
#include "raw_pixel_tiled_training_internal.hpp"
#include "raw_pixel_archive_binary.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <fstream>

namespace qpx {
using namespace detail::raw_archive;
void RawPixelTrainingCheckpointLimits::validate() const {
    if(max_bytes<256 || max_bytes>32u*1024*1024 || work_bytes<1024*1024 || work_bytes>1024u*1024*1024 || max_bytes>work_bytes/8)
        throw std::invalid_argument("invalid raw training checkpoint byte/work limits");
}
struct RawPixelTrainingCheckpoint::Data {
    ScientificRawLattice lattice;RawPixelSourceSnapshot source;
    std::uint64_t completed=0,total=0;std::string request,prefix;
    std::vector<std::uint8_t> bytes;
    Data(ScientificRawLattice l,RawPixelSourceSnapshot s):lattice(std::move(l)),source(std::move(s)) {}
};
RawPixelTrainingCheckpoint::RawPixelTrainingCheckpoint(std::shared_ptr<const Data> d):data_(std::move(d)) {}
const std::vector<std::uint8_t>& RawPixelTrainingCheckpoint::bytes() const {return data_->bytes;}
std::uint64_t RawPixelTrainingCheckpoint::completed_windows() const {return data_->completed;}
std::uint64_t RawPixelTrainingCheckpoint::total_windows() const {return data_->total;}
bool RawPixelTrainingCheckpoint::complete() const {return data_->completed==data_->total;}
const std::string& RawPixelTrainingCheckpoint::request_sha256() const {return data_->request;}
const std::string& RawPixelTrainingCheckpoint::prefix_sha256() const {return data_->prefix;}
std::shared_ptr<const RawPixelTrainingCheckpoint> RawPixelTrainingCheckpoint::from_bytes(const std::vector<std::uint8_t>& bytes,RawPixelTrainingCheckpointLimits limits,const std::atomic<bool>* c) {
    limits.validate();Reader r(bytes.data(),bytes.size(),"QTC\1",c,limits.max_bytes);
    const auto g=lattice(r);auto s=source(r,g);auto d=std::make_shared<Data>(g,std::move(s));
    if(!d->source.provenance->source_only || d->source.provenance->nucleus || raw_pixel_source_snapshot_bytes(d->source)>limits.work_bytes/4)
        throw std::invalid_argument("invalid raw training checkpoint source capture");
    d->total=r.integer(1000000);d->completed=r.integer(d->total);d->request=r.text(64);d->prefix=r.text(64);r.finish();
    hash_check(d->request);hash_check(d->prefix);
    if(!d->total || (!d->completed && d->request!=d->prefix)) throw std::invalid_argument("invalid raw training checkpoint prefix");
    d->bytes=bytes;stop(c);return std::shared_ptr<const RawPixelTrainingCheckpoint>(new RawPixelTrainingCheckpoint(std::move(d)));
}
std::shared_ptr<const RawPixelTrainingCheckpoint> load_raw_pixel_training_checkpoint(const std::string& input,RawPixelTrainingCheckpointLimits limits,const std::atomic<bool>* c) {
    limits.validate();stop(c);const auto path=detail::export_path(input);detail::ReaderFileGuard guard(path);
    const auto n=guard.version()->bytes;if(n>limits.max_bytes || n<256) throw std::length_error("raw training checkpoint file size exceeds limits");
    std::ifstream file(std::filesystem::u8path(path),std::ios::binary);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));
    for(std::size_t at=0;at<bytes.size();) {stop(c);const auto count=std::min<std::size_t>(65536,bytes.size()-at);file.read(reinterpret_cast<char*>(bytes.data()+at),std::streamsize(count));if(file.gcount()!=std::streamsize(count)) throw std::runtime_error("raw training checkpoint read failed");at+=count;}
    if(file.peek()!=std::char_traits<char>::eof() || file.bad()) throw std::runtime_error("raw training checkpoint file changed");guard.verify();
    auto out=RawPixelTrainingCheckpoint::from_bytes(bytes,limits,c);guard.verify();return out;
}
struct RawPixelTrainingCheckpointCodec {
    static RawPixelTrainingCheckpointResult save(const RawPixelTiledTrainingRequest& request,const std::string& input,RawPixelTrainingCheckpointOptions o,const std::atomic<bool>* c) {
        o.limits.validate();stop(c);const auto path=detail::export_path(input);const auto& d=*request.data_;
        if(d.source.observed && d.source.observed->primary) {detail::guard_file_output(d.source.observed->primary->path,d.source.observed->primary->identity,path);for(const auto& file:d.source.observed->companions) detail::guard_file_output(file.path,file.identity,path);}
        std::unique_ptr<detail::ReaderFileGuard> old;const auto status=std::filesystem::symlink_status(std::filesystem::u8path(path));
        if(std::filesystem::exists(status) || std::filesystem::is_symlink(status)) {if(!o.replace_existing) throw std::runtime_error("raw training checkpoint destination already exists");old=std::make_unique<detail::ReaderFileGuard>(path);load_raw_pixel_training_checkpoint(path,o.limits,c);old->verify();}
        const auto request_sha=raw_pixel_tiled_request_sha256(d);
        RawPixelTiledTrainingPrefix prefix;RawPixelTrainingCheckpointResult out;
        std::shared_ptr<const RawPixelTrainingCheckpoint> previous;
        // Preserve original observation bytes on resume for deterministic output
        // across a file reader close/reopen. Fresh capture remains on the table.
        const RawPixelSourceSnapshot* observation=&d.source;
        if(!o.resume_path.empty()) {
            previous=load_raw_pixel_training_checkpoint(o.resume_path,o.limits,c);const auto& p=*previous->data_;
            if(p.request!=request_sha || p.total!=d.plan.size() || p.source.epoch!=d.source.epoch || p.source.calibration.x!=d.source.calibration.x || p.source.calibration.y!=d.source.calibration.y)
                throw std::invalid_argument("raw training checkpoint request/selection/calibration mismatch");
            if(!raw_pixel_source_matches(*request.source_,p.source)) throw std::runtime_error("raw training checkpoint source observations changed");
            prefix.expected_windows=p.completed;prefix.expected_sha=p.prefix;observation=&p.source;out.replayed_windows=p.completed;
        }
        prefix.target=prefix.expected_windows+std::min<std::uint64_t>(o.max_new_windows,d.plan.size()-prefix.expected_windows);
        if(!raw_pixel_publication_lattice_source_matches(*request.source_,d.source,d.lattice)) throw std::runtime_error("raw training source changed before checkpoint execution");
        // Admit metadata encoding before pixel IO, independently of live work.
        if(raw_pixel_source_snapshot_bytes(*observation)>o.limits.work_bytes/8) throw std::length_error("raw training checkpoint source exceeds file work budget");
        Writer w("QTC\1",c,o.limits.max_bytes);grid(w,d.lattice);source(w,*observation);w.integer(d.plan.size());w.integer(prefix.target);w.text(request_sha);w.text(request_sha);
        // Prefix hash has the same fixed width as the admitted placeholder.
        const auto hash_at=w.bytes.size()-64;
        if(w.bytes.size()>o.limits.max_bytes-64) throw std::length_error("raw training checkpoint exceeds file byte limit");
        out.table=RawPixelTiledTrainingKernel::run(request.data_,*request.source_,c,nullptr,nullptr,&prefix);
        std::copy(prefix.sha.begin(),prefix.sha.end(),w.bytes.begin()+hash_at);w.finish();
        if(prefix.completed!=prefix.target || bool(out.table)!=(prefix.target==d.plan.size())) throw std::logic_error("raw training checkpoint execution accounting mismatch");
        out.checkpoint=RawPixelTrainingCheckpoint::from_bytes(w.bytes,o.limits,c);
        detail::AtomicExportFile file(path,o.limits.max_bytes,c);file.write(reinterpret_cast<const char*>(w.bytes.data()),w.bytes.size());stop(c);
        if(!raw_pixel_publication_lattice_source_matches(*request.source_,d.source,d.lattice)) throw std::runtime_error("raw training source changed before checkpoint publication");
        const bool replacing=bool(old);if(old) {old->verify();old.reset();}file.commit(replacing);return out;
    }
};
RawPixelTrainingCheckpointResult save_raw_pixel_training_checkpoint(const RawPixelTiledTrainingRequest& request,const std::string& path,RawPixelTrainingCheckpointOptions options,const std::atomic<bool>* c) {return RawPixelTrainingCheckpointCodec::save(request,path,std::move(options),c);}
}
