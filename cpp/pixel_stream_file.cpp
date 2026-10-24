#include "qupathxx/pixel_stream_file.hpp"
#include "pixel_classifier_archive.hpp"
#include "pixel_workflow_snapshot.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "pixel_stream_digest.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>
namespace qpx {
namespace {
constexpr std::uint64_t maximum=1ULL<<46,header_limit=128u*1024*1024,record_fixed=200;
using Bytes=std::vector<std::uint8_t>;
void stop(const std::atomic<bool>* c) {detail::export_checkpoint(c);}
void limit(std::uint64_t n) {if(n<128 || n>maximum) throw std::invalid_argument("invalid pixel stream file byte limit");}
std::uint64_t add(std::uint64_t a,std::uint64_t b) {if(a>maximum || b>maximum-a) throw std::length_error("pixel stream file size overflow");return a+b;}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {if(b && a>maximum/b) throw std::length_error("pixel stream file size overflow");return a*b;}
void integer(Bytes& b,std::uint64_t n) {for(int i=0;i<8;++i) b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::uint64_t integer(const std::uint8_t* b) {std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(b[i])<<(8*i);return n;}
void u32(Bytes& b,std::uint32_t n) {for(int i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::uint32_t u32(const std::uint8_t* b) {std::uint32_t n=0;for(int i=0;i<4;++i) n|=std::uint32_t(b[i])<<(8*i);return n;}
std::uint32_t bits(float f) {std::uint32_t n;std::memcpy(&n,&f,4);return n;}
float number(std::uint32_t n) {float f;std::memcpy(&f,&n,4);return f;}
void hex_hash(const std::string& value) {if(value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid pixel stream content hash");}
void exact(std::istream& file,void* data,std::size_t n) {file.read(static_cast<char*>(data),static_cast<std::streamsize>(n));if(file.gcount()!=static_cast<std::streamsize>(n)) throw std::runtime_error("truncated pixel stream file");}
Bytes read(std::istream& file,std::size_t n) {Bytes b(n);exact(file,b.data(),n);return b;}
std::string text(std::istream& file,std::size_t n) {std::string b(n,'\0');exact(file,b.data(),n);return b;}
void write(detail::AtomicExportFile& file,const Bytes& b) {file.write(reinterpret_cast<const char*>(b.data()),b.size());}
void write(detail::AtomicExportFile& file,const std::string& s) {file.write(s.data(),s.size());}
void same(const FileVersion& a,const FileVersion& b) {if(a.bytes!=b.bytes || a.identity_scheme!=b.identity_scheme || a.identity!=b.identity || a.stamp_scheme!=b.stamp_scheme || a.modified_token!=b.modified_token || a.change_token!=b.change_token) throw std::runtime_error("pixel stream file changed since opening");}
struct Grid {
    std::uint64_t x=0,y=0,w=0,h=0,step=0,blocks=0,columns=0;bool windowed=false;
    std::array<std::uint64_t,4> block(std::uint64_t i) const {const auto xx=(i%columns)*step,yy=(i/columns)*step;return {x+xx,y+yy,std::min(step,w-xx),std::min(step,h-yy)};}
    std::uint64_t first_pixels() const {return std::min(w,step)*std::min(h,step);}
    std::uint64_t pixels_before(std::uint64_t i) const {const auto row=i/columns,col=i%columns;return add(multiply(row*step,w),multiply(col*step,std::min(step,h-row*step)));}
};
void validate_grid(Grid& g,const PixelPredictionResult& prototype,const std::array<std::uint64_t,4>& actual) {
    if(!g.w || !g.h || !g.step || g.step>512 || g.blocks>100000000) throw std::invalid_argument("invalid pixel stream grid");
    const auto& source=prototype.provenance()->source.levels.at(prototype.level());
    if(g.x>=std::uint64_t(source.width) || g.y>=std::uint64_t(source.height) || g.w>std::uint64_t(source.width)-g.x || g.h>std::uint64_t(source.height)-g.y) throw std::invalid_argument("pixel stream grid exceeds source");
    g.columns=(g.w+g.step-1)/g.step;if(multiply(g.columns,(g.h+g.step-1)/g.step)!=g.blocks) throw std::invalid_argument("pixel stream block count mismatch");
    auto edge=[&](double v,std::int64_t n) {const auto p=std::ceil(static_cast<long double>(v)/source.downsample-.5L);return p<=0?std::uint64_t(0):p>=n?std::uint64_t(n):static_cast<std::uint64_t>(p);};
    const auto b=prototype.roi().bounds();const auto left=edge(b.x,source.width),top=edge(b.y,source.height),right=edge(b.x+b.width,source.width),bottom=edge(b.y+b.height,source.height);
    if(g.windowed?(g.x<left || g.y<top || g.x+g.w>right || g.y+g.h>bottom):(g.x!=left || g.y!=top || g.x+g.w!=right || g.y+g.h!=bottom)) throw std::invalid_argument("pixel stream grid/ROI mismatch");
    const auto first=g.block(0);if(!prototype.windowed() || actual!=first) throw std::invalid_argument("pixel stream prototype grid mismatch");
    (void)multiply(multiply(g.w,g.h),9);
}
std::uint64_t footer_size(std::size_t classes) {return 4+16+classes*8+128;}
std::uint64_t file_size(const Grid& g,std::uint64_t payload,std::size_t classes) {return add(add(payload,add(multiply(g.blocks-1,record_fixed),multiply(multiply(g.w,g.h)-g.first_pixels(),9))),footer_size(classes));}
void ordered_start(Sha256& h,const std::string& header,const std::string& first) {const char tag[]="qpx.pixel-stream-content/1";h.update(tag,sizeof(tag)-1);h.update(header.data(),header.size());h.update(first.data(),first.size());}
void record_start(Sha256& h,std::uint64_t i) {const char tag[]="qpx.pixel-stream-block/1";h.update(tag,sizeof(tag)-1);h.integer(i);}
}
struct PixelStreamFile::Impl {
    std::string path,header_hash,footer_checksum,input_sha;Bytes prefix;std::shared_ptr<const FileVersion> version;std::shared_ptr<const PixelClassifierArchive> prototype;
    Grid grid;std::uint64_t payload=0;PixelStreamFileSummary summary;mutable std::mutex reader;mutable std::atomic<bool> verified{false};std::atomic<bool> cancelled{false};
};
struct PixelStreamFileCodec {
    static void framing(PixelStreamFile::Impl& s,std::istream& file,bool full_header,const std::atomic<bool>* cancel) {
        stop(cancel);stop(&s.cancelled);file.seekg(0);const auto prefix=read(file,68);
        auto changed=[] {throw std::runtime_error("pixel stream file changed since opening");};
        if(prefix!=s.prefix) changed();
        if(full_header) {
            Sha256 head;head.update(prefix.data(),prefix.size());std::array<std::uint8_t,65536> buffer{};auto remaining=s.payload-132;
            while(remaining) {stop(cancel);stop(&s.cancelled);const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,buffer.size()));exact(file,buffer.data(),count);head.update(buffer.data(),count);remaining-=count;}
            if(head.finish()!=s.header_hash) changed();
        }else file.seekg(static_cast<std::streamoff>(s.payload-64));
        if(text(file,64)!=s.header_hash) changed();
        const auto bytes=footer_size(s.summary.class_counts.size());file.seekg(static_cast<std::streamoff>(s.summary.bytes-bytes));const auto footer=read(file,static_cast<std::size_t>(bytes-64));const auto checksum=text(file,64);
        Sha256 tail;tail.update(s.header_hash.data(),s.header_hash.size());tail.update(footer.data(),footer.size());if(checksum!=s.footer_checksum || tail.finish()!=checksum) changed();
        stop(cancel);stop(&s.cancelled);
    }
    static std::array<std::uint64_t,4> coordinates(const PixelPredictionResult& block) {const auto& d=*block.snapshot_;return {std::uint64_t(d.x),std::uint64_t(d.y),std::uint64_t(d.w),std::uint64_t(d.h)};}
    static std::shared_ptr<PixelPredictionResult> blank(const PixelPredictionResult& first,const std::array<std::uint64_t,4>& grid) {
        auto out=std::shared_ptr<PixelPredictionResult>(new PixelPredictionResult());auto d=std::make_shared<PixelPredictionRequest::Data>(*first.snapshot_);
        d->x=grid[0];d->y=grid[1];d->w=grid[2];d->h=grid[3];d->windowed=true;d->tiles=((grid[2]+d->options.tile_size-1)/d->options.tile_size)*((grid[3]+d->options.tile_size-1)/d->options.tile_size);out->snapshot_=d;
        const auto n=static_cast<std::size_t>(d->w*d->h);out->labels_.resize(n);out->valid_.resize(n);out->fractions_.resize(n);out->counts_.resize(d->model.classes().size());return out;
    }
    static std::pair<std::shared_ptr<const PixelPredictionResult>,std::string> block(PixelStreamFile::Impl& s,std::istream& file,std::uint64_t index,bool seek,const std::atomic<bool>* cancel) {
        stop(cancel);stop(&s.cancelled);if(index>=s.grid.blocks) throw std::out_of_range("pixel stream block index out of range");if(!index) return {s.prototype->result(),s.prototype->sha256()};
        const auto grid=s.grid.block(index);if(seek) {const auto offset=add(s.payload,add(multiply(index-1,record_fixed),multiply(s.grid.pixels_before(index)-s.grid.first_pixels(),9)));file.seekg(static_cast<std::streamoff>(offset));if(!file) throw std::runtime_error("cannot seek pixel stream block");}
        const auto metadata=read(file,136);Sha256 digest;record_start(digest,index);digest.update(metadata.data(),metadata.size());
        auto out=blank(*s.prototype->result(),grid);out->sha_.assign(reinterpret_cast<const char*>(metadata.data()),64);out->input_sha_.assign(reinterpret_cast<const char*>(metadata.data()+64),64);const auto ms=integer(metadata.data()+128);std::memcpy(&out->ms_,&ms,8);
        std::array<std::uint8_t,65529> buffer{};const auto n=out->labels_.size();
        for(std::size_t at=0;at<n;) {stop(cancel);stop(&s.cancelled);const auto count=std::min<std::size_t>(buffer.size()/9,n-at);exact(file,buffer.data(),count*9);digest.update(buffer.data(),count*9);
            for(std::size_t j=0;j<count;++j) {const auto* p=buffer.data()+j*9;const auto label=u32(p),fraction=u32(p+5);const auto valid=p[4];if(valid>1 || label>out->counts_.size() || (!valid && fraction!=0x7fc00000u)) throw std::invalid_argument("invalid pixel stream mask value");out->labels_[at+j]=label;out->valid_[at+j]=valid;out->fractions_[at+j]=number(fraction);out->valid_pixels_+=valid;if(label) {++out->classified_pixels_;++out->counts_[label-1];}}
            at+=count;
        }
        const auto expected=text(file,64),actual=digest.finish();if(expected!=actual) throw std::invalid_argument("pixel stream block checksum mismatch");PixelClassifierArchiveCodec::validate_prediction(*out,cancel?cancel:&s.cancelled);stop(&s.cancelled);return {std::move(out),actual};
    }
};
PixelStreamFile::PixelStreamFile(const std::string& input,std::uint64_t max_bytes):impl_(std::make_unique<Impl>()) {
    limit(max_bytes);auto& s=*impl_;s.path=detail::export_path(input);detail::ReaderFileGuard observed(s.path);s.version=observed.version();if(s.version->bytes>max_bytes) throw std::length_error("pixel stream file exceeds byte limit");
    std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);if(!file) throw std::runtime_error("cannot open pixel stream file");auto prefix=read(file,68);if(std::memcmp(prefix.data(),"QPS\1",4)) throw std::invalid_argument("unsupported pixel stream file encoding");
    s.grid.x=integer(prefix.data()+4);s.grid.y=integer(prefix.data()+12);s.grid.w=integer(prefix.data()+20);s.grid.h=integer(prefix.data()+28);s.grid.step=integer(prefix.data()+36);s.grid.blocks=integer(prefix.data()+44);const auto windowed=integer(prefix.data()+52),length=integer(prefix.data()+60);
    if(s.version->bytes<132 || length>s.version->bytes-132 || windowed>1 || length>header_limit || length<68 || length>max_bytes-std::min<std::uint64_t>(max_bytes,132)) throw std::invalid_argument("invalid pixel stream header size/flags");s.grid.windowed=windowed!=0;
    Sha256 header;header.update(prefix.data(),prefix.size());auto archive=read(file,static_cast<std::size_t>(length));header.update(archive.data(),archive.size());s.header_hash=text(file,64);if(s.header_hash!=header.finish()) throw std::invalid_argument("pixel stream header checksum mismatch");
    s.prototype=PixelClassifierArchive::from_bytes(archive);if(!s.prototype->result()) throw std::invalid_argument("pixel stream header requires prediction values");validate_grid(s.grid,*s.prototype->result(),PixelStreamFileCodec::coordinates(*s.prototype->result()));s.payload=132+length;
    const auto classes=s.prototype->model()->classes().size();
    const auto expected=file_size(s.grid,s.payload,classes);if(expected!=s.version->bytes) throw std::invalid_argument("pixel stream file length mismatch");
    file.seekg(static_cast<std::streamoff>(expected-footer_size(classes)));const auto footer=read(file,static_cast<std::size_t>(footer_size(classes)-64));const auto checksum=text(file,64);Sha256 tail;tail.update(s.header_hash.data(),64);tail.update(footer.data(),footer.size());if(std::memcmp(footer.data(),"QPE\1",4) || checksum!=tail.finish()) throw std::invalid_argument("pixel stream footer checksum mismatch");
    s.summary.bytes=expected;s.summary.blocks=s.grid.blocks;s.summary.valid_pixels=integer(footer.data()+4);s.summary.classified_pixels=integer(footer.data()+12);std::uint64_t sum=0;
    for(std::size_t i=0;i<classes;++i) {const auto count=integer(footer.data()+20+i*8);if(count>multiply(s.grid.w,s.grid.h)) throw std::invalid_argument("pixel stream footer count exceeds grid");sum+=count;s.summary.class_counts.push_back(count);}
    if(sum!=s.summary.classified_pixels || sum>s.summary.valid_pixels || s.summary.valid_pixels>multiply(s.grid.w,s.grid.h)) throw std::invalid_argument("invalid pixel stream completion totals");
    s.summary.sha256.assign(reinterpret_cast<const char*>(footer.data()+20+classes*8),64);hex_hash(s.summary.sha256);s.prefix=std::move(prefix);s.footer_checksum=checksum;observed.verify();
}
PixelStreamFile::~PixelStreamFile()=default;
std::shared_ptr<const PixelPredictionResult> PixelStreamFile::read_block(std::uint64_t index) const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);try {stop(&s.cancelled);detail::ReaderFileGuard observed(s.path);same(*s.version,*observed.version());std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);if(!file) throw std::runtime_error("cannot reopen pixel stream file");PixelStreamFileCodec::framing(s,file,index==0,nullptr);auto result=PixelStreamFileCodec::block(s,file,index,true,nullptr);observed.verify();stop(&s.cancelled);return result.first;}catch(...) {s.verified.store(false);throw;}
}
void PixelStreamFile::verify(const std::atomic<bool>* cancel) const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);s.verified.store(false);stop(cancel);detail::ReaderFileGuard observed(s.path);same(*s.version,*observed.version());std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);if(!file) throw std::runtime_error("cannot reopen pixel stream file");PixelStreamFileCodec::framing(s,file,true,cancel);file.seekg(static_cast<std::streamoff>(s.payload));Sha256 ordered;ordered_start(ordered,s.header_hash,s.prototype->sha256());Sha256 inputs;detail::pixel_stream_input_header(inputs,s.prototype->model()->sha256(),s.grid.blocks);std::vector<std::uint64_t> counts(s.summary.class_counts.size());std::uint64_t valid=0,classified=0;
    for(std::uint64_t i=0;i<s.grid.blocks;++i) {auto block=PixelStreamFileCodec::block(s,file,i,false,cancel);detail::pixel_stream_input_block(inputs,i,block.first->input_sha256());if(i) ordered.update(block.second.data(),block.second.size());valid+=block.first->valid_pixels();classified+=block.first->classified_pixels();for(std::size_t j=0;j<counts.size();++j) counts[j]+=block.first->class_counts()[j];}
    if(ordered.finish()!=s.summary.sha256 || counts!=s.summary.class_counts || valid!=s.summary.valid_pixels || classified!=s.summary.classified_pixels) throw std::invalid_argument("pixel stream completion verification failed");observed.verify();stop(cancel);stop(&s.cancelled);s.input_sha=inputs.finish();s.verified.store(true);
}
std::string PixelStreamFile::input_sha256() const {auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);stop(&s.cancelled);if(!s.verified.load()) throw std::logic_error("verify all stream blocks before reading the input digest");return s.input_sha;}
void PixelStreamFile::cancel() {impl_->cancelled.store(true);impl_->verified.store(false);}
bool PixelStreamFile::cancelled() const {return impl_->cancelled.load();}
bool PixelStreamFile::verified() const {return !impl_->cancelled.load() && impl_->verified.load();}
Rect PixelStreamFile::level_bounds() const {const auto& g=impl_->grid;return {double(g.x),double(g.y),double(g.w),double(g.h)};}
std::size_t PixelStreamFile::block_size() const {return static_cast<std::size_t>(impl_->grid.step);}
bool PixelStreamFile::windowed() const {return impl_->grid.windowed;}
const PixelStreamFileSummary& PixelStreamFile::declared_summary() const {return impl_->summary;}
std::shared_ptr<const PixelStreamLink> PixelStreamFile::link() const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);stop(&s.cancelled);if(!s.verified.load()) throw std::logic_error("verify all stream blocks before preparing a project link");
    auto link=std::make_shared<PixelStreamLink>();link->path_=s.path;link->summary_=s.summary;const auto& first=*s.prototype->result();link->model_=first.model().sha256();const auto& source=first.provenance()->source;link->width_=source.width;link->height_=source.height;link->object_=first.object_id();link->revision_=first.provenance()->document_revision;
    const std::uint64_t grid[]={s.grid.x,s.grid.y,s.grid.w,s.grid.h};std::copy(std::begin(grid),std::end(grid),std::begin(link->grid_));link->windowed_=s.grid.windowed;link->block_=static_cast<std::size_t>(s.grid.step);link->validate();stop(&s.cancelled);return link;
}
PixelStreamFileSummary save_pixel_stream(PixelPredictionStream& stream,const std::string& input,bool replace,std::uint64_t max_bytes,const std::atomic<bool>* cancel) {
    limit(max_bytes);stop(cancel);if(stream.completed_blocks() || stream.state()!="ready") throw std::invalid_argument("saving requires a fresh pixel producer");const auto path=detail::export_path(input);
    std::unique_ptr<detail::ReaderFileGuard> old;const auto status=std::filesystem::symlink_status(std::filesystem::u8path(path));
    if(std::filesystem::exists(status) || std::filesystem::is_symlink(status)) {if(!replace) throw std::runtime_error("pixel stream destination already exists");old=std::make_unique<detail::ReaderFileGuard>(path);PixelStreamFile existing(path,max_bytes);existing.verify(cancel);old->verify();}
    const auto bounds=stream.level_bounds();Grid g;g.x=static_cast<std::uint64_t>(bounds.x);g.y=static_cast<std::uint64_t>(bounds.y);g.w=static_cast<std::uint64_t>(bounds.width);g.h=static_cast<std::uint64_t>(bounds.height);g.blocks=stream.total_blocks();g.windowed=stream.windowed();
    if(add(multiply(multiply(g.w,g.h),9),multiply(g.blocks-1,record_fixed))>max_bytes) throw std::length_error("pixel stream output exceeds byte limit");
    auto first=stream.next();if(!first || stream.completed_blocks()!=1) throw std::runtime_error("pixel stream producer changed during save");const auto first_grid=PixelStreamFileCodec::coordinates(*first);g.x=first_grid[0];g.y=first_grid[1];g.step=std::max(first_grid[2],first_grid[3]);validate_grid(g,*first,first_grid);stop(cancel);
    auto prototype=archive_pixel_prediction(first,cancel);Bytes prefix={'Q','P','S',1};for(auto n:{g.x,g.y,g.w,g.h,g.step,g.blocks,std::uint64_t(g.windowed),std::uint64_t(prototype->bytes().size())}) integer(prefix,n);
    Sha256 head;head.update(prefix.data(),prefix.size());head.update(prototype->bytes().data(),prototype->bytes().size());const auto header_hash=head.finish();const auto payload=132+prototype->bytes().size();const auto expected=file_size(g,payload,first->model().classes().size());if(expected>max_bytes) throw std::length_error("pixel stream output exceeds byte limit");
    detail::AtomicExportFile output(path,static_cast<std::size_t>(max_bytes),cancel);write(output,prefix);write(output,prototype->bytes());write(output,header_hash);Sha256 ordered;ordered_start(ordered,header_hash,prototype->sha256());prototype.reset();first.reset();
    for(std::uint64_t i=1;i<g.blocks;++i) {stop(cancel);auto block=stream.next();if(!block || stream.completed_blocks()!=i+1) throw std::runtime_error("pixel stream producer changed during save");const auto actual=PixelStreamFileCodec::coordinates(*block);const auto wanted=g.block(i);if(actual!=wanted) throw std::runtime_error("pixel stream block coverage changed");PixelClassifierArchiveCodec::validate_prediction(*block,cancel);
        Bytes metadata(block->sha256().begin(),block->sha256().end());metadata.insert(metadata.end(),block->input_sha256().begin(),block->input_sha256().end());std::uint64_t ms;const auto value=block->elapsed_ms();std::memcpy(&ms,&value,8);integer(metadata,ms);Sha256 digest;record_start(digest,i);digest.update(metadata.data(),metadata.size());write(output,metadata);
        Bytes buffer;buffer.reserve(65536);for(std::size_t j=0;j<block->labels().size();++j) {if(buffer.size()+9>65536) {stop(cancel);digest.update(buffer.data(),buffer.size());write(output,buffer);buffer.clear();}u32(buffer,block->labels()[j]);buffer.push_back(block->valid()[j]);u32(buffer,std::isfinite(block->vote_fractions()[j])?bits(block->vote_fractions()[j]):0x7fc00000u);}
        digest.update(buffer.data(),buffer.size());write(output,buffer);const auto checksum=digest.finish();write(output,checksum);ordered.update(checksum.data(),checksum.size());
    }
    if(stream.state()!="complete") throw std::runtime_error("pixel stream did not complete");PixelStreamFileSummary summary;summary.blocks=g.blocks;summary.valid_pixels=stream.valid_pixels();summary.classified_pixels=stream.classified_pixels();summary.class_counts=stream.class_counts();summary.sha256=ordered.finish();
    Bytes footer={'Q','P','E',1};integer(footer,summary.valid_pixels);integer(footer,summary.classified_pixels);for(auto n:summary.class_counts) integer(footer,n);footer.insert(footer.end(),summary.sha256.begin(),summary.sha256.end());Sha256 tail;tail.update(header_hash.data(),header_hash.size());tail.update(footer.data(),footer.size());write(output,footer);write(output,tail.finish());summary.bytes=output.bytes();if(summary.bytes!=expected) throw std::logic_error("pixel stream file size accounting mismatch");stop(cancel);const bool replacing=bool(old);if(old) {old->verify();old.reset();}output.commit(replacing);return summary;
}
}
