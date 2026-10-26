#include "qupathxx/raw_pixel_stream_file.hpp"
#include "raw_pixel_stream_internal.hpp"
#include "qupathxx/raw_pixel_stream_contours.hpp"
#include "raw_stream_boundaries.hpp"
#include "raw_pixel_stream_record.hpp"
#include "atomic_export.hpp"
#include "geometry_archive.hpp"
#include "provenance_codec.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>

namespace qpx {
namespace {
using Bytes=std::vector<std::uint8_t>;
constexpr std::uint64_t maximum=1ULL<<46,record_fixed=284,header_limit=128u*1024*1024;
void stop(const std::atomic<bool>* c) {detail::export_checkpoint(c);}
void hash_check(const std::string& value) {if(value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid raw stream hash");}
std::string digest(const void* data,std::size_t n) {Sha256 h;h.update(data,n);return h.finish();}
void text(Sha256& h,const std::string& v) {h.integer(v.size());h.update(v.data(),v.size());}
std::uint64_t add(std::uint64_t a,std::uint64_t b) {if(a>maximum || b>maximum-a) throw std::length_error("raw stream file size overflow");return a+b;}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {if(b && a>maximum/b) throw std::length_error("raw stream file size overflow");return a*b;}
void integer(Bytes& b,std::uint64_t v) {for(unsigned i=0;i<8;++i) b.push_back(std::uint8_t(v>>(8*i)));}
std::uint64_t integer(const std::uint8_t* p) {std::uint64_t v=0;for(unsigned i=0;i<8;++i) v|=std::uint64_t(p[i])<<(8*i);return v;}
void u32(Bytes& b,std::uint32_t v) {for(unsigned i=0;i<4;++i) b.push_back(std::uint8_t(v>>(8*i)));}
std::uint32_t u32(const std::uint8_t* p) {std::uint32_t v=0;for(unsigned i=0;i<4;++i) v|=std::uint32_t(p[i])<<(8*i);return v;}
std::uint32_t bits(float f) {std::uint32_t v;std::memcpy(&v,&f,4);return v;}
float floating(std::uint32_t v) {float f;std::memcpy(&f,&v,4);return f;}
void exact(std::istream& file,void* out,std::size_t n) {file.read(static_cast<char*>(out),std::streamsize(n));if(file.gcount()!=std::streamsize(n)) throw std::runtime_error("truncated raw stream file");}
Bytes read(std::istream& file,std::size_t n) {Bytes b(n);exact(file,b.data(),n);return b;}
std::string read_text(std::istream& file,std::size_t n) {std::string s(n,'\0');exact(file,s.data(),n);return s;}
void write(detail::AtomicExportFile& f,const Bytes& b) {f.write(reinterpret_cast<const char*>(b.data()),b.size());}
void write(detail::AtomicExportFile& f,const std::string& s) {f.write(s.data(),s.size());}
void same(const FileVersion& a,const FileVersion& b) {if(a.bytes!=b.bytes || a.identity_scheme!=b.identity_scheme || a.identity!=b.identity || a.stamp_scheme!=b.stamp_scheme || a.modified_token!=b.modified_token || a.change_token!=b.change_token) throw std::runtime_error("raw stream file changed since opening");}
struct Writer {
    Bytes bytes;std::size_t maximum;
    explicit Writer(std::size_t n):maximum(n) {}
    void raw(const void* p,std::size_t n) {if(!n) return;if(n>maximum-bytes.size()) throw std::length_error("raw stream header exceeds work limit");const auto* first=static_cast<const std::uint8_t*>(p);bytes.insert(bytes.end(),first,first+n);}
    void integer(std::uint64_t v) {Bytes b;qpx::integer(b,v);raw(b.data(),b.size());}
    void signed_integer(std::int64_t v) {std::uint64_t n;std::memcpy(&n,&v,8);integer(n);}
    void real(double v) {std::uint64_t n;std::memcpy(&n,&v,8);integer(n);}
    void blob(const Bytes& b) {integer(b.size());raw(b.data(),b.size());}
    void text(const std::string& s) {integer(s.size());raw(s.data(),s.size());}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    explicit Reader(const Bytes& b):p(b.data()),left(b.size()) {}
    std::uint64_t integer(std::uint64_t maximum=UINT64_MAX) {if(left<8) throw std::invalid_argument("truncated raw stream header integer");const auto v=qpx::integer(p);p+=8;left-=8;if(v>maximum) throw std::invalid_argument("raw stream header integer exceeds limit");return v;}
    std::int64_t signed_integer() {const auto v=integer();std::int64_t n;std::memcpy(&n,&v,8);return n;}
    double real() {const auto n=integer();double v;std::memcpy(&v,&n,8);if(!std::isfinite(v) || (v==0 && std::signbit(v))) throw std::invalid_argument("invalid raw stream numeric parameter");return v;}
    Bytes blob(std::size_t maximum) {const auto n=integer(maximum);if(n>left) throw std::invalid_argument("truncated raw stream header blob");Bytes b(p,p+std::size_t(n));p+=n;left-=std::size_t(n);return b;}
    std::string text(std::size_t maximum) {const auto b=blob(maximum);std::string s(b.begin(),b.end());detail::bounded_utf8(s,maximum);return s;}
};
struct Descriptor : RawPixelStreamGeometry {
    RawPixelClassifier model;std::shared_ptr<const AnalysisProvenance> provenance;std::optional<Roi> mask;std::string epoch,request_sha;
    Descriptor(ScientificRawLattice l,RawPixelStreamRegion r,RawPixelStreamOptions o,RawPixelClassifier m):RawPixelStreamGeometry(std::move(l),r,o),model(std::move(m)) {}
};
std::size_t footer_size(std::size_t classes) {return 292+8*classes;}
std::uint64_t file_size(const RawPixelStreamGeometry& g,std::uint64_t payload,std::uint64_t completed,std::size_t classes) {return add(add(payload,add(multiply(completed,record_fixed),multiply(g.samples_before(completed),9))),footer_size(classes));}
void admit(const Descriptor& d,std::size_t header,const RawPixelStreamFileLimits& limits) {
    if(header>header_limit || limits.work_bytes<131072 || header>(limits.work_bytes-131072)/8) throw std::length_error("raw stream header exceeds file work budget");
    const auto first=d.window(0);const auto count=std::uint64_t(first.core_width())*std::uint64_t(first.core_height());
    if(count>(limits.work_bytes-131072-8*header)/9) throw std::length_error("raw stream historical block exceeds file work budget");
}
void validate_descriptor(const Descriptor& d) {
    hash_check(d.epoch);hash_check(d.request_sha);if(!d.provenance) throw std::invalid_argument("raw stream provenance missing");d.provenance->validate();
    if(d.provenance->source.width!=d.lattice.image_width() || d.provenance->source.height!=d.lattice.image_height() || bool(d.mask)==d.provenance->source_only || (d.mask && !d.mask->is_area())) throw std::invalid_argument("raw stream source/ROI provenance mismatch");
    const GridFraction x(d.lattice.x().extent(),d.lattice.x().samples()),y(d.lattice.y().extent(),d.lattice.y().samples());const auto sx=d.model.recipe().step_x(),sy=d.model.recipe().step_y();
    if(x.numerator()!=sx.numerator() || x.denominator()!=sx.denominator() || y.numerator()!=sy.numerator() || y.denominator()!=sy.denominator()) throw std::invalid_argument("raw stream model/global sampling mismatch");
    const auto& f=d.model.recipe().features();const auto derivative=f.gradient_magnitude || f.laplacian?1:0;if(d.lattice.x().context()<std::ceil(3*f.sigmas.back().x)+derivative || d.lattice.y().context()<std::ceil(3*f.sigmas.back().y)+derivative) throw std::invalid_argument("raw stream feature context mismatch");
    if(raw_pixel_stream_request_sha256(d,d.model,d.provenance->reader,d.epoch,{d.provenance->effective_pixel_width_um,d.provenance->effective_pixel_height_um},d.mask)!=d.request_sha) throw std::invalid_argument("raw stream request commitment mismatch");
}
Bytes encode_header(const Descriptor& d,const RawPixelStreamFileLimits& limits) {
    validate_descriptor(d);const auto capacity=std::min<std::size_t>(header_limit,(limits.work_bytes-131072)/8);
    if(d.model.bytes().size()>limits.work_bytes/16 || d.provenance->payload_bytes()>limits.work_bytes/16 || (d.mask && d.mask->payload_bytes()>limits.work_bytes/32)) throw std::length_error("raw stream header capture exceeds file work budget");
    Writer w(capacity>76?capacity-76:0);const auto& g=d.lattice;
    for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.x().extent(),g.x().samples(),g.x().context(),g.y().origin(),g.y().extent(),g.y().samples(),g.y().context(),d.region.first_x,d.region.first_y,d.region.width,d.region.height,d.options.tile_width,d.options.tile_height}) w.signed_integer(v);
    w.integer(d.options.max_samples);w.integer(d.options.max_windows);w.integer(d.options.work_bytes);w.real(d.options.min_vote_fraction);w.blob(d.model.bytes());w.blob(ProvenanceCodec::encode(*d.provenance,true));w.integer(bool(d.mask));if(d.mask) w.blob(GeometryArchiveCodec::encode(*d.mask));w.text(d.epoch);w.text(d.request_sha);
    Bytes out={'Q','W','S',1};out.reserve(76+w.bytes.size());integer(out,w.bytes.size());out.insert(out.end(),w.bytes.begin(),w.bytes.end());const auto sha=digest(out.data(),out.size());out.insert(out.end(),sha.begin(),sha.end());admit(d,out.size(),limits);return out;
}
std::unique_ptr<Descriptor> decode_header(const Bytes& encoded) {
    Reader r(encoded);const auto width=r.signed_integer(),height=r.signed_integer(),x=r.signed_integer(),ex=r.signed_integer(),nx=r.signed_integer(),cx=r.signed_integer(),y=r.signed_integer(),ey=r.signed_integer(),ny=r.signed_integer(),cy=r.signed_integer();
    ScientificRawLattice lattice(width,height,{x,ex,nx,cx},{y,ey,ny,cy});RawPixelStreamRegion region;region.first_x=r.signed_integer();region.first_y=r.signed_integer();region.width=r.signed_integer();region.height=r.signed_integer();RawPixelStreamOptions options;options.tile_width=r.signed_integer();options.tile_height=r.signed_integer();options.max_samples=r.integer();options.max_windows=r.integer();options.work_bytes=std::size_t(r.integer(1024ULL*1024*1024));options.min_vote_fraction=r.real();
    RawPixelStreamGeometry geometry(lattice,region,options);auto model=RawPixelClassifier::from_bytes(r.blob(64u*1024*1024+128*1024));auto d=std::make_unique<Descriptor>(std::move(lattice),region,options,std::move(model));const auto provenance=r.blob(16u*1024*1024);d->provenance=ProvenanceCodec::decode(provenance.data(),provenance.size(),true);if(r.integer(1)) {const auto mask=r.blob(32u*1024*1024);d->mask=GeometryArchiveCodec::decode(mask.data(),mask.size());}d->epoch=r.text(64);d->request_sha=r.text(64);if(r.left) throw std::invalid_argument("trailing raw stream header data");validate_descriptor(*d);return d;
}
void block_start(Sha256& h,const std::string& header,std::uint64_t index) {text(h,"qpx.raw-pixel-stream-file-block/1");text(h,header);h.integer(index);}
void accumulate(RawPixelStreamFileSummary& s,const RawPixelWindowClassification& b,const std::string& checksum) {
    const auto index=s.completed_windows;s.input_sha256=raw_pixel_stream_chain("qpx.raw-pixel-stream-input-chain/1",s.input_sha256,index,b.input_sha256());s.sha256=raw_pixel_stream_chain("qpx.raw-pixel-stream-result-chain/1",s.sha256,index,b.sha256());s.content_sha256=raw_pixel_stream_chain("qpx.raw-pixel-stream-file-chain/1",s.content_sha256,index,checksum);
    s.valid_samples+=std::uint64_t(std::count(b.valid().begin(),b.valid().end(),std::uint8_t(1)));for(std::size_t i=0;i<b.class_counts().size();++i) {s.class_counts[i]+=b.class_counts()[i];s.classified_samples+=b.class_counts()[i];}++s.completed_windows;s.complete=s.completed_windows==s.total_windows;
}
RawPixelStreamFileSummary initial(const Descriptor& d,const std::string& header) {RawPixelStreamFileSummary s;s.total_windows=d.windows;s.class_counts.resize(d.model.forest().classes().size());s.input_sha256=s.sha256=d.request_sha;s.content_sha256=header;return s;}
void totals(const RawPixelStreamFileSummary& a,const RawPixelStreamFileSummary& b) {if(a.completed_windows!=b.completed_windows || a.valid_samples!=b.valid_samples || a.classified_samples!=b.classified_samples || a.class_counts!=b.class_counts || a.input_sha256!=b.input_sha256 || a.sha256!=b.sha256 || a.content_sha256!=b.content_sha256 || a.complete!=b.complete) throw std::invalid_argument("raw stream prefix totals/commitments mismatch");}
Bytes footer(const RawPixelStreamFileSummary& s,const std::string& header) {Bytes b={'Q','W','E',1};for(auto n:{std::uint64_t(s.complete),s.completed_windows,s.valid_samples,s.classified_samples}) integer(b,n);for(auto n:s.class_counts) integer(b,n);for(const auto* h:{&s.input_sha256,&s.sha256,&s.content_sha256}) b.insert(b.end(),h->begin(),h->end());Sha256 h;text(h,header);h.update(b.data(),b.size());const auto sha=h.finish();b.insert(b.end(),sha.begin(),sha.end());return b;}
}
std::uint64_t raw_pixel_stream_file_bytes(const RawPixelStreamGeometry& g,std::uint64_t header,std::uint64_t completed,std::size_t classes) {
    if(header<76 || header>header_limit || completed>g.windows || classes<2 || classes>256) throw std::invalid_argument("invalid raw stream link framing");return file_size(g,header,completed,classes);
}
void RawPixelStreamFileLimits::validate() const {if(max_bytes<128 || max_bytes>maximum || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid raw stream file limits");}
struct RawPixelStreamFile::Impl {
    std::string path,header_hash;std::shared_ptr<const FileVersion> version;Bytes header,tail;
    std::unique_ptr<Descriptor> descriptor;RawPixelStreamFileSummary summary;RawPixelStreamFileLimits limits;
    mutable std::mutex reader;mutable std::atomic<bool> verified{false};std::atomic<bool> cancelled{false};
};
struct RawPixelStreamFileCodec {
    static void framing(const RawPixelStreamFile::Impl& s,std::istream& file) {
        file.seekg(0);if(read(file,12)!=Bytes(s.header.begin(),s.header.begin()+12)) throw std::runtime_error("raw stream framing changed");file.seekg(std::streamoff(s.header.size()-64));if(read_text(file,64)!=s.header_hash) throw std::runtime_error("raw stream header changed");file.seekg(std::streamoff(s.summary.bytes-s.tail.size()));if(read(file,s.tail.size())!=s.tail) throw std::runtime_error("raw stream completion changed");
    }
    template<class Visitor> static void verified_blocks(RawPixelStreamFile::Impl& s,std::istream& file,Visitor&& visitor,const std::atomic<bool>* c) {
        framing(s,file);auto sum=initial(*s.descriptor,s.header_hash);
        for(std::uint64_t i=0;i<s.summary.completed_windows;++i) {const auto item=block(s,file,i,c);accumulate(sum,*item.first,item.second);visitor(*item.first);}
        totals(sum,s.summary);
    }
    static void validate(const Descriptor& d,const RawPixelWindowClassification& b,const std::atomic<bool>* c) {
        hash_check(b.feature_sha256());hash_check(b.input_sha256());hash_check(b.sha256());if(b.model_sha256()!=d.model.sha256()) throw std::invalid_argument("raw stream block model mismatch");const auto& w=b.window();const auto n=std::size_t(w.core_width()*w.core_height());
        if(b.labels().size()!=n || b.valid().size()!=n || b.vote_fractions().size()!=n || !b.estimated_work_bytes() || b.estimated_work_bytes()>d.options.work_bytes) throw std::invalid_argument("raw stream block dimensions/work mismatch");std::vector<std::uint64_t> counts(d.model.forest().classes().size());const auto trees=double(d.model.forest().tree_count());
        for(std::size_t i=0;i<n;++i) {if((i&4095)==0) stop(c);const auto valid=b.valid()[i];const auto label=b.labels()[i];const auto f=b.vote_fractions()[i];if(valid>1 || label>counts.size()) throw std::invalid_argument("invalid raw stream label/support");
            const auto x=w.x().first_core_index()+std::int64_t(i%std::size_t(w.core_width())),y=w.y().first_core_index()+std::int64_t(i/std::size_t(w.core_width()));
            if(valid) {if(w.x().edge(x).floor()<0 || w.y().edge(y).floor()<0 || w.x().edge(x+1).ceil()>w.image_width() || w.y().edge(y+1).ceil()>w.image_height() || (d.mask && !d.mask->contains({w.x().center(x).binary64(),w.y().center(y).binary64()}))) throw std::invalid_argument("raw stream supported sample outside source/ROI");if(!std::isfinite(f) || f<=0 || f>1) throw std::invalid_argument("invalid raw stream vote fraction");const auto exact=std::round(double(f)*trees)/trees;if(f!=float(exact) || bool(label)!=(exact>=d.options.min_vote_fraction)) throw std::invalid_argument("raw stream votes/threshold mismatch");if(label) ++counts[label-1];}
            else if(label || bits(f)!=0x7fc00000) throw std::invalid_argument("noncanonical raw stream invalid row");
        }
        if(counts!=b.class_counts() || RawPixelWindowPredictionKernel::result_sha256(b,d.options.min_vote_fraction,c)!=b.sha256()) throw std::invalid_argument("raw stream semantic block commitment mismatch");
    }
    static std::pair<std::shared_ptr<const RawPixelWindowClassification>,std::string> block(const RawPixelStreamFile::Impl& s,std::istream& file,std::uint64_t index,const std::atomic<bool>* c) {
        stop(c);stop(&s.cancelled);if(index>=s.summary.completed_windows) throw std::out_of_range("raw stream historical block index");const auto& d=*s.descriptor;const auto window=d.window(index);const auto n=std::size_t(window.core_width()*window.core_height());
        file.seekg(std::streamoff(add(s.header.size(),add(multiply(index,record_fixed),multiply(d.samples_before(index),9)))));const auto metadata=read(file,220);
        if(std::memcmp(metadata.data(),"QWB\1",4) || integer(metadata.data()+4)!=index || integer(metadata.data()+12)!=n) throw std::invalid_argument("raw stream block framing mismatch");
        auto b=std::shared_ptr<RawPixelWindowClassification>(new RawPixelWindowClassification(window));b->capture_.reset();b->work_=std::size_t(integer(metadata.data()+20));b->feature_sha_.assign(reinterpret_cast<const char*>(metadata.data()+28),64);b->input_sha_.assign(reinterpret_cast<const char*>(metadata.data()+92),64);b->sha_.assign(reinterpret_cast<const char*>(metadata.data()+156),64);b->model_sha_=d.model.sha256();b->labels_.resize(n);b->valid_.resize(n);b->fractions_.resize(n);b->counts_.resize(d.model.forest().classes().size());
        Sha256 h;block_start(h,s.header_hash,index);h.update(metadata.data(),metadata.size());std::array<std::uint8_t,65529> buffer{};
        for(std::size_t at=0;at<n;) {stop(c);stop(&s.cancelled);const auto count=std::min<std::size_t>(buffer.size()/9,n-at);exact(file,buffer.data(),count*9);h.update(buffer.data(),count*9);for(std::size_t j=0;j<count;++j) {const auto* p=buffer.data()+j*9;const auto label=u32(p);b->labels_[at+j]=label;b->valid_[at+j]=p[4];b->fractions_[at+j]=floating(u32(p+5));if(label && label<=b->counts_.size()) ++b->counts_[label-1];}at+=count;}
        const auto checksum=read_text(file,64);if(checksum!=h.finish()) throw std::invalid_argument("raw stream block checksum mismatch");validate(d,*b,c);stop(&s.cancelled);return {std::move(b),checksum};
    }
    static std::string write_block(detail::AtomicExportFile& output,const Descriptor& d,const std::string& header,std::uint64_t index,const RawPixelWindowClassification& b,const std::atomic<bool>* c) {
        validate(d,b,c);Bytes metadata={'Q','W','B',1};integer(metadata,index);integer(metadata,b.labels().size());integer(metadata,b.estimated_work_bytes());for(const auto* h:{&b.feature_sha256(),&b.input_sha256(),&b.sha256()}) metadata.insert(metadata.end(),h->begin(),h->end());Sha256 h;block_start(h,header,index);h.update(metadata.data(),metadata.size());write(output,metadata);Bytes buffer;buffer.reserve(65536);
        for(std::size_t i=0;i<b.labels().size();++i) {if(buffer.size()+9>65536) {stop(c);h.update(buffer.data(),buffer.size());write(output,buffer);buffer.clear();}u32(buffer,b.labels()[i]);buffer.push_back(b.valid()[i]);u32(buffer,b.valid()[i]?bits(b.vote_fractions()[i]):0x7fc00000);}
        h.update(buffer.data(),buffer.size());write(output,buffer);const auto checksum=h.finish();write(output,checksum);return checksum;
    }
    static RawPixelStreamFileSummary save(const RawPixelTiledPredictionRequest& request,const std::string& path,RawPixelStreamSaveOptions options,const std::atomic<bool>* c);
};
RawPixelStreamFile::RawPixelStreamFile(const std::string& input,RawPixelStreamFileLimits limits):impl_(std::make_unique<Impl>()) {
    limits.validate();auto& s=*impl_;s.path=detail::export_path(input);s.limits=limits;detail::ReaderFileGuard guard(s.path);s.version=guard.version();if(s.version->bytes>limits.max_bytes || s.version->bytes<76+footer_size(2)) throw std::length_error("raw stream file exceeds size bounds");std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);if(!file) throw std::runtime_error("cannot open raw stream file");auto prefix=read(file,12);if(std::memcmp(prefix.data(),"QWS\1",4)) throw std::invalid_argument("unsupported raw stream encoding");const auto length=integer(prefix.data()+4);
    if(length>header_limit-76 || length>s.version->bytes-76 || length+76>(limits.work_bytes-131072)/8) throw std::length_error("raw stream header exceeds size/work limit");auto encoded=read(file,std::size_t(length));s.header_hash=read_text(file,64);Sha256 h;h.update(prefix.data(),prefix.size());h.update(encoded.data(),encoded.size());if(h.finish()!=s.header_hash) throw std::invalid_argument("raw stream header checksum mismatch");s.descriptor=decode_header(encoded);s.header=std::move(prefix);s.header.reserve(76+std::size_t(length));s.header.insert(s.header.end(),encoded.begin(),encoded.end());s.header.insert(s.header.end(),s.header_hash.begin(),s.header_hash.end());encoded.clear();encoded.shrink_to_fit();admit(*s.descriptor,s.header.size(),limits);
    const auto classes=s.descriptor->model.forest().classes().size(),tail_bytes=footer_size(classes);if(s.version->bytes<s.header.size()+tail_bytes) throw std::invalid_argument("truncated raw stream footer");file.seekg(std::streamoff(s.version->bytes-tail_bytes));s.tail=read(file,tail_bytes);if(std::memcmp(s.tail.data(),"QWE\1",4)) throw std::invalid_argument("unsupported raw stream footer");auto& sum=s.summary;const auto complete=integer(s.tail.data()+4);sum.completed_windows=integer(s.tail.data()+12);sum.valid_samples=integer(s.tail.data()+20);sum.classified_samples=integer(s.tail.data()+28);sum.total_windows=s.descriptor->windows;sum.complete=complete!=0;
    if(complete>1 || sum.completed_windows>sum.total_windows || sum.complete!=(sum.completed_windows==sum.total_windows) || sum.valid_samples>s.descriptor->samples_before(sum.completed_windows)) throw std::invalid_argument("invalid raw stream prefix completion");std::uint64_t total=0;for(std::size_t i=0;i<classes;++i) {const auto count=integer(s.tail.data()+36+i*8);if(count>s.descriptor->samples || total>s.descriptor->samples-count) throw std::invalid_argument("invalid raw stream prefix class count");total+=count;sum.class_counts.push_back(count);}if(total!=sum.classified_samples || total>sum.valid_samples) throw std::invalid_argument("invalid raw stream prefix totals");const auto at=36+classes*8;sum.input_sha256.assign(reinterpret_cast<const char*>(s.tail.data()+at),64);sum.sha256.assign(reinterpret_cast<const char*>(s.tail.data()+at+64),64);sum.content_sha256.assign(reinterpret_cast<const char*>(s.tail.data()+at+128),64);for(const auto* sha:{&sum.input_sha256,&sum.sha256,&sum.content_sha256}) hash_check(*sha);if(footer(sum,s.header_hash)!=s.tail) throw std::invalid_argument("raw stream footer checksum mismatch");sum.bytes=file_size(*s.descriptor,s.header.size(),sum.completed_windows,classes);if(sum.bytes!=s.version->bytes) throw std::invalid_argument("raw stream file length mismatch");guard.verify();
}
RawPixelStreamFile::~RawPixelStreamFile()=default;
const RawPixelStreamFileSummary& RawPixelStreamFile::declared_summary() const {return impl_->summary;}
const RawPixelClassifier& RawPixelStreamFile::model() const {return impl_->descriptor->model;}
const ScientificRawLattice& RawPixelStreamFile::lattice() const {return impl_->descriptor->lattice;}
const RawPixelStreamRegion& RawPixelStreamFile::region() const {return impl_->descriptor->region;}
const RawPixelStreamOptions& RawPixelStreamFile::options() const {return impl_->descriptor->options;}
const RawPixelStreamFileLimits& RawPixelStreamFile::limits() const {return impl_->limits;}
std::size_t RawPixelStreamFile::payload_bytes() const {
    const auto& s=*impl_;const auto& d=*s.descriptor;
    return sizeof(*this)+sizeof(Impl)+sizeof(Descriptor)+s.header.capacity()+s.tail.capacity()+s.path.capacity()+s.header_hash.capacity()+
        4ull*d.model.bytes().size()+64ull*d.model.forest().node_count()+d.provenance->payload_bytes()+(d.mask?d.mask->payload_bytes():0)+65536;
}
const std::shared_ptr<const AnalysisProvenance>& RawPixelStreamFile::provenance() const {return impl_->descriptor->provenance;}
std::shared_ptr<const RawPixelWindowClassification> RawPixelStreamFile::read_block(std::uint64_t index,const std::atomic<bool>* c) const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);try {stop(c);stop(&s.cancelled);detail::ReaderFileGuard guard(s.path);same(*s.version,*guard.version());std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);RawPixelStreamFileCodec::framing(s,file);auto out=RawPixelStreamFileCodec::block(s,file,index,c).first;guard.verify();return out;}catch(...) {s.verified.store(false);throw;}
}
void RawPixelStreamFile::verify(const std::atomic<bool>* c) const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);s.verified.store(false);stop(c);stop(&s.cancelled);detail::ReaderFileGuard guard(s.path);same(*s.version,*guard.version());std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);RawPixelStreamFileCodec::verified_blocks(s,file,[](const RawPixelWindowClassification&) {},c);stop(c);stop(&s.cancelled);guard.verify();s.verified.store(true);
}
void RawPixelStreamFile::cancel() {impl_->cancelled.store(true,std::memory_order_relaxed);impl_->verified.store(false);}
bool RawPixelStreamFile::verified() const {return impl_->verified.load() && !impl_->cancelled.load();}
void RawPixelStreamFile::check_unchanged() const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);try {stop(&s.cancelled);detail::ReaderFileGuard guard(s.path);same(*s.version,*guard.version());guard.verify();}catch(...) {s.verified.store(false);throw;}
}
std::shared_ptr<const RawPixelStreamLink> RawPixelStreamFile::link() const {
    auto& s=*impl_;std::lock_guard<std::mutex> lock(s.reader);
    try {stop(&s.cancelled);if(!s.verified.load()) throw std::logic_error("verify the complete raw stream prefix before preparing a link");
        detail::ReaderFileGuard guard(s.path);same(*s.version,*guard.version());guard.verify();const auto& d=*s.descriptor;
        auto out=std::shared_ptr<RawPixelStreamLink>(new RawPixelStreamLink(d.lattice,d.region,d.options));out->path_=s.path;out->model_=d.model.sha256();out->request_=d.request_sha;out->header_=s.header_hash;out->header_bytes_=s.header.size();out->summary_=s.summary;out->summary_.reused_windows=0;out->object_=d.provenance->object_id;out->revision_=d.provenance->document_revision;out->version_=s.version;out->validate();stop(&s.cancelled);return out;
    }catch(...) {s.verified.store(false);throw;}
}
std::size_t RawPixelFileContours::payload_bytes() const {
    std::size_t bytes=sizeof(*this)+contours_->retained_bytes()+link_->payload_bytes()+classes_.capacity()*sizeof(std::string)+65536;
    for(const auto& name:classes_) bytes+=name.capacity();return bytes;
}
struct RawPixelFileContourKernel {
    static std::shared_ptr<const RawPixelFileContours> run(const RawPixelStreamFile& input,RawPixelStreamContourOptions options,const std::atomic<bool>* c) {
        stop(c);options.validate();if(!has_geos()) throw std::runtime_error("raw historical contours require QUPATHXX_BUILD_GEOS=ON");
        auto& s=*input.impl_;const auto work=131072ull+s.limits.work_bytes+std::max(options.boundary_work_bytes,options.contours.work_bytes);
        if(work>options.work_bytes) throw std::length_error("historical file/boundary/geometry work budget exceeded");
        if(!s.summary.complete) throw std::invalid_argument("historical contours require a complete QWS1 stream");
        auto out=std::shared_ptr<RawPixelFileContours>(new RawPixelFileContours());
        try {
            {
                std::lock_guard<std::mutex> lock(s.reader);s.verified.store(false);stop(&s.cancelled);
                detail::ReaderFileGuard guard(s.path);same(*s.version,*guard.version());std::ifstream file(std::filesystem::u8path(s.path),std::ios::binary);
                const auto& d=*s.descriptor;detail::RawStreamBoundaryBuilder builder(d,d.model.forest().classes().size(),{options.max_boundary_runs,options.boundary_work_bytes});
                RawPixelStreamFileCodec::verified_blocks(s,file,[&](const RawPixelWindowClassification& block) {builder.append(block.window(),block.labels(),c);},c);
                auto network=builder.finish(c);
                if(network.completed_windows!=s.summary.completed_windows || network.completed_samples!=d.samples || network.class_counts!=s.summary.class_counts) throw std::invalid_argument("historical contour census differs from verified file");
                out->contours_=detail::trace_raw_boundary_geometry(network,options.contours,d.mask?&*d.mask:nullptr,c);
                out->classes_=d.model.forest().classes();out->options_=options;out->samples_=network.completed_samples;out->work_=static_cast<std::size_t>(work);
                stop(c);stop(&s.cancelled);guard.verify();s.verified.store(true);
            }
            out->link_=input.link();stop(c);input.check_unchanged();return out;
        }catch(...) {s.verified.store(false);throw;}
    }
};
std::shared_ptr<const RawPixelFileContours> trace_raw_pixel_stream_file(const RawPixelStreamFile& file,RawPixelStreamContourOptions options,const std::atomic<bool>* c) {return RawPixelFileContourKernel::run(file,options,c);}
RawPixelTiledPredictionRequest capture_raw_pixel_stream_replay(const Document& doc,const RawPixelStreamFile& file) {
    if(!file.verified() || !file.declared_summary().complete) throw std::invalid_argument("replay requires a complete verified QWS1 file");
    const auto link=file.link();RawPixelTiledPredictionRequest request(doc,file.model(),file.lattice(),file.options(),file.region(),file.provenance()->object_id);
    if(request.sha256()!=link->request_sha256()) throw std::invalid_argument("current raw replay request differs from saved source/model/ROI/lattice");
    file.check_unchanged();return request;
}
std::shared_ptr<const RawPixelStreamLink> prepare_raw_pixel_stream_link(const std::string& path,RawPixelStreamFileLimits limits,const std::atomic<bool>* c) {stop(c);RawPixelStreamFile file(path,limits);file.verify(c);stop(c);auto out=file.link();stop(c);return out;}
RawPixelStreamFileSummary RawPixelStreamFileCodec::save(const RawPixelTiledPredictionRequest& request,const std::string& input,RawPixelStreamSaveOptions options,const std::atomic<bool>* c) {
    options.limits.validate();stop(c);const auto path=detail::export_path(input);const auto& data=*request.data_;const auto& capture=data.prototype->source;
    if(capture.observed && capture.observed->primary) {detail::guard_file_output(capture.observed->primary->path,capture.observed->primary->identity,path);for(const auto& file:capture.observed->companions) detail::guard_file_output(file.path,file.identity,path);}
    std::unique_ptr<detail::ReaderFileGuard> old;const auto status=std::filesystem::symlink_status(std::filesystem::u8path(path));if(std::filesystem::exists(status) || std::filesystem::is_symlink(status)) {if(!options.replace_existing) throw std::runtime_error("raw stream destination already exists");old=std::make_unique<detail::ReaderFileGuard>(path);if(options.resume_path.empty() || detail::export_path(options.resume_path)!=path) {RawPixelStreamFile previous(path,options.limits);previous.verify(c);}old->verify();}
    std::unique_ptr<RawPixelStreamFile> previous;std::unique_ptr<Descriptor> descriptor;Bytes header;std::uint64_t retained=0;
    if(!options.resume_path.empty()) {
        previous=std::make_unique<RawPixelStreamFile>(options.resume_path,options.limits);const auto& p=*previous->impl_;if(p.descriptor->request_sha!=request.sha256()) throw std::invalid_argument("raw stream resume model/recipe/ROI/coverage/options mismatch");
        RawPixelSourceSnapshot observed;observed.reader=p.descriptor->provenance->reader;observed.observed=p.descriptor->provenance->reader_source;if(!raw_pixel_source_matches(*data.source,observed)) throw std::runtime_error("raw stream resume source identity/files changed");
        descriptor=std::make_unique<Descriptor>(*p.descriptor);retained=p.summary.completed_windows;
    }else {descriptor=std::make_unique<Descriptor>(data.lattice,data.region,data.options,data.prototype->model);descriptor->provenance=capture.provenance;descriptor->mask=data.prototype->mask;descriptor->epoch=capture.epoch;descriptor->request_sha=request.sha256();header=encode_header(*descriptor,options.limits);}
    const auto& header_bytes=previous?previous->impl_->header:header;
    const auto target=retained+std::min(options.max_new_windows,descriptor->windows-retained);const auto expected=file_size(*descriptor,header_bytes.size(),target,descriptor->model.forest().classes().size());if(expected>options.limits.max_bytes) throw std::length_error("raw stream output exceeds byte limit");
    if(!raw_pixel_publication_window_source_matches(*data.source,capture,data.prototype->window)) throw std::runtime_error("raw stream source changed before save");const std::string header_hash(header_bytes.end()-64,header_bytes.end());detail::AtomicExportFile output(path,std::size_t(options.limits.max_bytes),c);write(output,header_bytes);auto summary=initial(*descriptor,header_hash);RawPixelPredictionStream producer(request);
    for(std::uint64_t i=0;i<target;++i) {
        stop(c);std::shared_ptr<const RawPixelWindowClassification> block;
        if(i<retained) {block=previous->read_block(i,c);producer.restore_block(*block,c);}else block=producer.next_impl(c);
        if(!block) throw std::logic_error("raw stream producer exhausted before save target");const auto checksum=write_block(output,*descriptor,header_hash,i,*block,c);accumulate(summary,*block,checksum);
        if(i+1==retained) totals(summary,previous->declared_summary());
    }
    if(!retained && previous) totals(initial(*descriptor,header_hash),previous->declared_summary());
    if(summary.complete && (producer.sha256()!=summary.sha256 || producer.input_sha256()!=summary.input_sha256)) throw std::logic_error("raw stream resumed producer commitment mismatch");
    write(output,footer(summary,header_hash));summary.bytes=output.bytes();summary.reused_windows=retained;if(summary.bytes!=expected) throw std::logic_error("raw stream output size accounting mismatch");stop(c);
    if(!raw_pixel_publication_window_source_matches(*data.source,capture,data.prototype->window)) throw std::runtime_error("raw stream source changed before atomic publication");
    const bool replacing=bool(old);if(old) {old->verify();old.reset();}output.commit(replacing);return summary;
}
RawPixelStreamFileSummary save_raw_pixel_stream(const RawPixelTiledPredictionRequest& request,const std::string& path,RawPixelStreamSaveOptions options,const std::atomic<bool>* c) {return RawPixelStreamFileCodec::save(request,path,std::move(options),c);}
}
