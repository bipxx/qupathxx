#include "neighborhood_archive.hpp"
#include "neighborhood_snapshot.hpp"
#include "spatial_snapshot.hpp"
#include "provenance_codec.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
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
    std::vector<std::uint8_t> bytes={'Q','S','N',1};const std::atomic<bool>* cancelled;
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
std::vector<std::uint8_t> SpatialNeighborhoodArchiveCodec::encode(const SpatialNeighborhoodRecord& record,const std::atomic<bool>* c) {
    stop(c);if(!record.result || !record.id || !record.captured_next_id || record.result->document_revision()>=record.id)
        throw std::invalid_argument("invalid neighborhood archive record");
    detail::bounded_utf8(record.name,256);detail::bounded_utf8(record.producer,256);
    if(record.name.empty() || record.producer.empty()) throw std::invalid_argument("neighborhood archive name/producer is empty");
    const auto& r=*record.result;const auto& n=*r.snapshot_;const auto& d=*n.capture.data_;n.options.validate();
    Writer w{{'Q','S','N',1},c};w.integer(record.id);w.integer(record.captured_next_id);w.text(record.name);w.text(record.producer);w.real(n.options.radius);
    w.integer(unsigned(d.options.units));w.integer(d.options.prefer_nucleus);w.integer(d.options.exclude_self);
    for(auto v:{std::uint64_t(d.options.snapshot_bytes),std::uint64_t(d.options.work_bytes),d.options.max_distance_tests,d.revision,
        std::uint64_t(d.bytes),std::uint64_t(d.work),std::uint64_t(n.bytes),std::uint64_t(n.work)}) w.integer(v);
    w.real(d.sx);w.real(d.sy);w.real(d.capture_ms);w.text(d.digest);w.text(n.digest);w.text(r.sha256_);w.integer(r.distance_tests_);w.real(r.elapsed_ms_);
    const auto provenance=ProvenanceCodec::encode(*d.provenance,true);w.integer(provenance.size());w.raw(provenance.data(),provenance.size());
    w.integer(d.objects.size());
    for(const auto& a:d.objects) {
        stop(c);if(a.id>=record.captured_next_id) throw std::invalid_argument("neighborhood object exceeds captured identity bounds");
        w.integer(a.id);w.integer(a.revision);w.real(a.centroid.x);w.real(a.centroid.y);w.integer(unsigned(a.kind));w.integer(a.nucleus);w.text(a.geometry_sha256);
    }
    for(const auto* ids:{&d.sources,&d.targets}) {w.integer(ids->size());for(auto i:*ids) {stop(c);w.integer(i);}}
    for(const auto& row:r.rows_) {stop(c);w.integer(row.count);}
    const auto hash=checksum(w.bytes.data(),w.bytes.size(),c);w.raw(hash.data(),hash.size());return std::move(w.bytes);
}
std::shared_ptr<const SpatialNeighborhoodRecord> SpatialNeighborhoodArchiveCodec::decode(const std::uint8_t* p,std::size_t size,const std::atomic<bool>* c) {
    stop(c);if(!p || size<68 || size>limit || std::memcmp(p,"QSN\1",4) ||
        checksum(p,size-64,c)!=std::string(reinterpret_cast<const char*>(p+size-64),64)) throw std::invalid_argument("invalid neighborhood archive header/checksum");
    Reader rd{p+4,size-68};auto record=std::make_shared<SpatialNeighborhoodRecord>();record->id=rd.integer();record->captured_next_id=rd.integer();record->name=rd.text(256);record->producer=rd.text(256);
    SpatialNeighborhoodOptions options;options.radius=rd.real();
    auto d=std::make_shared<SpatialDistanceRequest::Data>();auto& o=d->options;
    o.units=SpatialDistanceUnits(rd.integer(1));o.prefer_nucleus=rd.integer(1)!=0;o.exclude_self=rd.integer(1)!=0;
    o.snapshot_bytes=std::size_t(rd.integer(256u*1024*1024));o.work_bytes=std::size_t(rd.integer(1024u*1024*1024));o.max_distance_tests=rd.integer();
    static_cast<SpatialDistanceOptions&>(options)=o;options.validate();
    d->revision=rd.integer();d->bytes=std::size_t(rd.integer(o.snapshot_bytes));d->work=std::size_t(rd.integer(o.work_bytes));
    const auto bytes=std::size_t(rd.integer(o.snapshot_bytes)),work=std::size_t(rd.integer(o.work_bytes));
    d->sx=rd.real();d->sy=rd.real();d->capture_ms=rd.real();d->digest=rd.text(64);hex(d->digest);
    const auto input=rd.text(64),result_hash=rd.text(64);hex(input);hex(result_hash);
    const auto tests=rd.integer(o.max_distance_tests);const auto elapsed_ms=rd.real();
    const auto provenance=rd.view(1024*1024);d->provenance=ProvenanceCodec::decode(provenance.first,provenance.second,true);
    if(!record->id || !record->captured_next_id || record->name.empty() || record->producer.empty() || d->revision>=record->id ||
       !d->provenance->source_only || d->provenance->document_revision!=d->revision || d->capture_ms<0 || elapsed_ms<0 || d->sx<=0 || d->sy<=0)
        throw std::invalid_argument("invalid neighborhood archive metadata");
    if(o.units==SpatialDistanceUnits::Pixels) {if(d->sx!=1 || d->sy!=1) throw std::invalid_argument("invalid neighborhood pixel scales");}
    else if(d->provenance->effective_pixel_width_um!=std::optional<double>(d->sx) || d->provenance->effective_pixel_height_um!=std::optional<double>(d->sy))
        throw std::invalid_argument("invalid neighborhood calibrated scales");
    const auto count=std::size_t(rd.integer(200000));
    const auto base=sizeof(*d)+sizeof(SpatialDistanceRequest)+256+d->provenance->payload_bytes();
    if(!count || count>rd.left/120 || base>d->bytes || count>(d->bytes-base)/(sizeof(SpatialObjectSnapshot)+128)) throw std::invalid_argument("neighborhood objects exceed admitted payload");
    d->objects.reserve(count);
    for(std::size_t i=0;i<count;++i) {
        stop(c);SpatialObjectSnapshot a;a.id=rd.integer();a.revision=rd.integer(d->revision);a.centroid={rd.real(),rd.real()};a.kind=ObjectKind(rd.integer(2));a.nucleus=rd.integer(1)!=0;a.geometry_sha256=rd.text(64);hex(a.geometry_sha256);
        if(!a.id || a.id>=record->captured_next_id || (i && a.id<=d->objects.back().id) || (a.nucleus && (a.kind!=ObjectKind::Cell || !o.prefer_nucleus)) ||
           a.centroid.x<0 || a.centroid.y<0 || a.centroid.x>d->provenance->source.width || a.centroid.y>d->provenance->source.height)
            throw std::invalid_argument("invalid neighborhood object snapshot");
        d->objects.push_back(std::move(a));
    }
    std::vector<bool> used(count,false);
    for(auto* ids:{&d->sources,&d->targets}) {
        const auto length=std::size_t(rd.integer(100000));if(length>rd.left/8 || (ids==&d->sources && !length)) throw std::invalid_argument("invalid neighborhood cohort length");ids->reserve(length);
        for(std::size_t i=0;i<length;++i) {stop(c);const auto index=std::size_t(rd.integer(count-1));if(i && index<=ids->back()) throw std::invalid_argument("unordered neighborhood cohort");ids->push_back(index);used[index]=true;}
    }
    const auto minimum=base+count*(sizeof(SpatialObjectSnapshot)+128)+(d->sources.size()+d->targets.size())*sizeof(std::size_t);
    if(std::find(used.begin(),used.end(),false)!=used.end() || d->bytes<minimum ||
       d->work<d->bytes+sizeof(SpatialDistanceResult)+d->sources.size()*sizeof(SpatialDistanceRow)+SpatialIndex::estimated_bytes(d->targets.size())+8192 || d->digest!=d->input_digest())
        throw std::invalid_argument("neighborhood centroid capture identity/payload mismatch");
    auto snapshot=std::make_shared<SpatialNeighborhoodRequest::Data>(SpatialDistanceRequest(d),options);
    snapshot->bytes=bytes;snapshot->work=work;snapshot->digest=input;
    const auto extra=sizeof(SpatialNeighborhoodRequest::Data)+sizeof(SpatialNeighborhoodRequest)+sizeof(SpatialNeighborhoodResult)+512;
    if(bytes<d->bytes+extra || work<d->work+extra+d->sources.size()*sizeof(SpatialNeighborhoodRow)) throw std::invalid_argument("neighborhood wrapper exceeds admitted payload");
    Sha256 identity;hash_text(identity,"qpx.centroid-radius-count-input/1");hash_text(identity,d->digest);hash_real(identity,options.radius);
    if(identity.finish()!=input || d->sources.size()>rd.left/8) throw std::invalid_argument("invalid neighborhood input identity/rows");
    auto result=std::shared_ptr<SpatialNeighborhoodResult>(new SpatialNeighborhoodResult());result->snapshot_=snapshot;result->rows_.reserve(d->sources.size());
    result->sha256_=result_hash;result->distance_tests_=tests;result->elapsed_ms_=elapsed_ms;
    Sha256 hash;hash_text(hash,"qpx.centroid-radius-count-result/1");hash_text(hash,input);hash.integer(d->sources.size());
    for(auto i:d->sources) {
        stop(c);const auto& a=d->objects[i];SpatialNeighborhoodRow row;
        row.source_id=a.id;row.source_revision=a.revision;row.source_centroid=a.centroid;row.source_kind=a.kind;row.source_nucleus=a.nucleus;
        const auto eligible=d->targets.size()-(o.exclude_self && std::binary_search(d->targets.begin(),d->targets.end(),i)?1:0);
        row.count=rd.integer(eligible);hash.integer(a.id);hash.integer(row.count);result->rows_.push_back(row);
    }
    if(rd.left || hash.finish()!=result_hash || result->payload_bytes()+SpatialIndex::estimated_bytes(d->targets.size())+8192>o.work_bytes)
        throw std::invalid_argument("neighborhood result identity/payload mismatch");
    record->result=std::move(result);return record;
}
}
