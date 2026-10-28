#include "spatial_archive.hpp"
#include "spatial_snapshot.hpp"
#include "provenance_codec.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <limits>
namespace qpx {
namespace {
constexpr std::size_t limit=128u*1024*1024;
void stop(const std::atomic<bool>* c) {if(c && c->load(std::memory_order_relaxed)) throw std::runtime_error("spatial archive cancelled");}
void hash_text(Sha256& h,const std::string& v) {h.integer(v.size());h.update(v.data(),v.size());}
void hash_real(Sha256& h,double v) {std::uint64_t bits;std::memcpy(&bits,&v,8);h.integer(bits);}
std::string checksum(const std::uint8_t* p,std::size_t n,const std::atomic<bool>* c) {
    Sha256 h;for(std::size_t at=0;at<n;) {stop(c);const auto count=std::min<std::size_t>(65536,n-at);h.update(p+at,count);at+=count;}stop(c);return h.finish();
}
void hex(const std::string& v) {if(v.size()!=64 || v.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid spatial archive hash");}
struct Writer {
    std::vector<std::uint8_t> bytes={'Q','S','D',1};const std::atomic<bool>* cancelled;
    void raw(const void* data,std::size_t n) {
        if(n>limit-bytes.size()) throw std::length_error("spatial archive exceeds 128 MiB limit");
        const auto* p=static_cast<const std::uint8_t*>(data);
        for(std::size_t at=0;at<n;) {stop(cancelled);const auto count=std::min<std::size_t>(65536,n-at);bytes.insert(bytes.end(),p+at,p+at+count);at+=count;}
    }
    void integer(std::uint64_t n) {std::uint8_t b[8];for(int i=0;i<8;++i) b[i]=std::uint8_t(n>>(8*i));raw(b,8);}
    void real(double v) {if(!std::isfinite(v)) throw std::invalid_argument("nonfinite spatial archive number");std::uint64_t bits;std::memcpy(&bits,&v,8);integer(bits);}
    void text(const std::string& v) {integer(v.size());raw(v.data(),v.size());}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer(std::uint64_t max=UINT64_MAX) {
        if(left<8) throw std::invalid_argument("truncated spatial archive integer");
        std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;
        if(n>max) throw std::invalid_argument("spatial archive integer exceeds limit");return n;
    }
    double real() {const auto bits=integer();double v;std::memcpy(&v,&bits,8);if(!std::isfinite(v)) throw std::invalid_argument("nonfinite spatial archive number");return v;}
    std::pair<const std::uint8_t*,std::size_t> view(std::size_t max) {
        const auto n=integer(max);if(n>left) throw std::invalid_argument("truncated spatial archive text/blob");
        const auto v=std::make_pair(p,std::size_t(n));p+=n;left-=n;return v;
    }
    std::string text(std::size_t max) {const auto v=view(max);std::string s(reinterpret_cast<const char*>(v.first),v.second);detail::bounded_utf8(s,max);return s;}
};
}
std::vector<std::uint8_t> SpatialDistanceArchiveCodec::encode(const SpatialDistanceRecord& record,const std::atomic<bool>* c) {
    stop(c);if(!record.result || !record.id || !record.captured_next_id || record.result->document_revision()>=record.id) throw std::invalid_argument("invalid spatial archive record");
    detail::bounded_utf8(record.name,256);detail::bounded_utf8(record.producer,256);
    if(record.name.empty() || record.producer.empty()) throw std::invalid_argument("spatial archive name/producer is empty");
    const auto& r=*record.result;const auto& d=*r.snapshot_;d.options.validate();
    Writer w{{'Q','S','D',1},c};w.integer(record.id);w.integer(record.captured_next_id);w.text(record.name);w.text(record.producer);
    w.integer(unsigned(d.options.units));w.integer(d.options.prefer_nucleus);w.integer(d.options.exclude_self);
    for(auto v:{std::uint64_t(d.options.snapshot_bytes),std::uint64_t(d.options.work_bytes),d.options.max_distance_tests,d.revision,std::uint64_t(d.bytes),std::uint64_t(d.work)}) w.integer(v);
    w.real(d.sx);w.real(d.sy);w.real(d.capture_ms);w.text(d.digest);w.text(r.sha256_);w.integer(r.distance_tests_);w.real(r.elapsed_ms_);
    const auto provenance=ProvenanceCodec::encode(*d.provenance,true);w.integer(provenance.size());w.raw(provenance.data(),provenance.size());
    w.integer(d.objects.size());
    for(const auto& a:d.objects) {
        stop(c);if(a.id>=record.captured_next_id) throw std::invalid_argument("spatial archive object exceeds captured identity bounds");
        w.integer(a.id);w.integer(a.revision);w.real(a.centroid.x);w.real(a.centroid.y);w.integer(unsigned(a.kind));w.integer(a.nucleus);w.text(a.geometry_sha256);
    }
    for(const auto* ids:{&d.sources,&d.targets}) {w.integer(ids->size());for(auto i:*ids) {stop(c);w.integer(i);}}
    for(const auto& row:r.rows_) {
        stop(c);w.integer(bool(row.target_id));if(row.target_id) {w.integer(*row.target_id);w.real(*row.distance);}
    }
    const auto hash=checksum(w.bytes.data(),w.bytes.size(),c);w.raw(hash.data(),hash.size());return std::move(w.bytes);
}
std::shared_ptr<const SpatialDistanceRecord> SpatialDistanceArchiveCodec::decode(const std::uint8_t* p,std::size_t n,const std::atomic<bool>* c) {
    stop(c);if(!p || n<68 || n>limit || std::memcmp(p,"QSD\1",4) || checksum(p,n-64,c)!=std::string(reinterpret_cast<const char*>(p+n-64),64)) throw std::invalid_argument("invalid spatial archive header/checksum");
    Reader rd{p+4,n-68};auto record=std::make_shared<SpatialDistanceRecord>();record->id=rd.integer();record->captured_next_id=rd.integer();record->name=rd.text(256);record->producer=rd.text(256);
    auto d=std::make_shared<SpatialDistanceRequest::Data>();auto& o=d->options;
    o.units=SpatialDistanceUnits(rd.integer(1));o.prefer_nucleus=rd.integer(1)!=0;o.exclude_self=rd.integer(1)!=0;
    o.snapshot_bytes=std::size_t(rd.integer(256u*1024*1024));o.work_bytes=std::size_t(rd.integer(1024u*1024*1024));o.max_distance_tests=rd.integer();o.validate();
    d->revision=rd.integer();d->bytes=std::size_t(rd.integer(o.snapshot_bytes));d->work=std::size_t(rd.integer(o.work_bytes));d->sx=rd.real();d->sy=rd.real();d->capture_ms=rd.real();d->digest=rd.text(64);hex(d->digest);
    auto result=std::shared_ptr<SpatialDistanceResult>(new SpatialDistanceResult());result->snapshot_=d;result->sha256_=rd.text(64);hex(result->sha256_);
    result->distance_tests_=rd.integer(o.max_distance_tests);result->elapsed_ms_=rd.real();
    const auto provenance=rd.view(1024*1024);d->provenance=ProvenanceCodec::decode(provenance.first,provenance.second,true);
    if(!record->id || !record->captured_next_id || record->name.empty() || record->producer.empty() || d->revision>=record->id || !d->provenance->source_only || d->provenance->document_revision!=d->revision || d->capture_ms<0 || result->elapsed_ms_<0 || d->sx<=0 || d->sy<=0) throw std::invalid_argument("invalid spatial archive metadata");
    if(o.units==SpatialDistanceUnits::Pixels) {if(d->sx!=1 || d->sy!=1) throw std::invalid_argument("invalid spatial pixel scales");}
    else if(d->provenance->effective_pixel_width_um!=std::optional<double>(d->sx) || d->provenance->effective_pixel_height_um!=std::optional<double>(d->sy)) throw std::invalid_argument("invalid spatial calibrated scales");
    const auto count=std::size_t(rd.integer(200000));
    const auto base=sizeof(*d)+sizeof(SpatialDistanceRequest)+256+d->provenance->payload_bytes();
    if(!count || count>rd.left/120 || base>d->bytes || count>(d->bytes-base)/(sizeof(SpatialObjectSnapshot)+128)) throw std::invalid_argument("spatial objects exceed admitted payload");
    d->objects.reserve(count);
    for(std::size_t i=0;i<count;++i) {
        stop(c);SpatialObjectSnapshot a;a.id=rd.integer();a.revision=rd.integer(d->revision);a.centroid={rd.real(),rd.real()};a.kind=ObjectKind(rd.integer(2));a.nucleus=rd.integer(1)!=0;a.geometry_sha256=rd.text(64);hex(a.geometry_sha256);
        if(!a.id || a.id>=record->captured_next_id || (i && a.id<=d->objects.back().id) || (a.nucleus && (a.kind!=ObjectKind::Cell || !o.prefer_nucleus)) || a.centroid.x<0 || a.centroid.y<0 || a.centroid.x>d->provenance->source.width || a.centroid.y>d->provenance->source.height) throw std::invalid_argument("invalid spatial object snapshot");
        d->objects.push_back(std::move(a));
    }
    std::vector<bool> used(count,false);
    for(auto* ids:{&d->sources,&d->targets}) {
        const auto size=std::size_t(rd.integer(100000));if(size>rd.left/8 || (ids==&d->sources && !size)) throw std::invalid_argument("invalid spatial cohort length");ids->reserve(size);
        for(std::size_t i=0;i<size;++i) {stop(c);const auto index=std::size_t(rd.integer(count-1));if(i && index<=ids->back()) throw std::invalid_argument("unordered spatial cohort");ids->push_back(index);used[index]=true;}
    }
    const auto minimum=base+count*(sizeof(SpatialObjectSnapshot)+128)+(d->sources.size()+d->targets.size())*sizeof(std::size_t);
    if(std::find(used.begin(),used.end(),false)!=used.end() || d->bytes<minimum || d->work<d->bytes+sizeof(SpatialDistanceResult)+d->sources.size()*sizeof(SpatialDistanceRow)+SpatialIndex::estimated_bytes(d->targets.size())+8192 || d->digest!=d->input_digest()) throw std::invalid_argument("spatial capture identity/payload mismatch");
    if(d->sources.size()>rd.left/8) throw std::invalid_argument("truncated spatial rows");result->rows_.reserve(d->sources.size());
    Sha256 hash;hash_text(hash,"qpx.centroid-nearest-result/1");hash_text(hash,d->digest);hash.integer(d->sources.size());std::uint64_t matched=0;
    for(auto i:d->sources) {
        stop(c);const auto& a=d->objects[i];SpatialDistanceRow row;row.source_id=a.id;row.source_revision=a.revision;row.source_centroid=a.centroid;row.source_kind=a.kind;row.source_nucleus=a.nucleus;
        const bool present=rd.integer(1)!=0;hash.integer(a.id);hash.integer(present);
        const bool eligible=!d->targets.empty() && (!o.exclude_self || d->targets.size()>1 || d->targets[0]!=i);
        if(present!=eligible) throw std::invalid_argument("spatial row target status disagrees with cohort");
        if(present) {
            const auto id=rd.integer();const auto t=std::lower_bound(d->objects.begin(),d->objects.end(),id,[](const auto& a,auto id){return a.id<id;});
            if(t==d->objects.end() || t->id!=id || !std::binary_search(d->targets.begin(),d->targets.end(),std::size_t(t-d->objects.begin())) || (o.exclude_self && id==a.id)) throw std::invalid_argument("spatial row references an ineligible target");
            row.distance=rd.real();const auto expected=std::hypot(std::abs((t->centroid.x-a.centroid.x)*d->sx),std::abs((t->centroid.y-a.centroid.y)*d->sy));
            // libm hypot may differ by a few ULPs across supported platforms.
            // Keep the archived float64 bytes/hash; this is structural validation,
            // not a relaxation of the independent geometry comparison tolerance.
            if(!std::isfinite(expected) || *row.distance<0 || (expected==0?*row.distance!=0:std::abs(*row.distance-expected)>8*std::numeric_limits<double>::epsilon()*expected)) throw std::invalid_argument("spatial row distance disagrees with captured centroids");
            row.target_id=t->id;row.target_revision=t->revision;row.target_centroid=t->centroid;row.target_kind=t->kind;row.target_nucleus=t->nucleus;hash.integer(id);hash_real(hash,*row.distance);++matched;
        }
        result->rows_.push_back(row);
    }
    if(rd.left || hash.finish()!=result->sha256_ || result->distance_tests_<matched || result->payload_bytes()>o.work_bytes) throw std::invalid_argument("spatial result identity/payload mismatch");
    record->result=std::move(result);return record;
}
}
