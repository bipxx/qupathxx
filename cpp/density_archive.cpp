#include "density_archive.hpp"
#include "density_snapshot.hpp"
#include "spatial_snapshot.hpp"
#include "geometry_archive.hpp"
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
    std::vector<std::uint8_t> bytes={'Q','D','N',1};const std::atomic<bool>* cancelled;
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
std::string geometry_hash(const Roi& roi) {const auto b=GeometryArchiveCodec::encode(roi);Sha256 h;h.update(b.data(),b.size());return h.finish();}
std::size_t vertices(const Roi& roi) {
    if(roi.kind()==RoiKind::Ellipse) return 512;
    if(roi.kind()==RoiKind::Rectangle) return 4;
    std::size_t n=0;for(const auto& path:roi.paths()) n+=path.size();return n;
}
double physical_area(double area,double sx,double sy) {
    int a=0,x=0,y=0;const auto mantissa=std::frexp(area,&a)*std::frexp(sx,&x)*std::frexp(sy,&y)/1e6;
    const auto result=std::ldexp(mantissa,a+x+y);
    if(!std::isfinite(result) || result<=0) throw std::invalid_argument("invalid density calibrated area");return result;
}
}
std::vector<std::uint8_t> SpatialDensityArchiveCodec::encode(const SpatialDensityRecord& record,const std::atomic<bool>* c) {
    stop(c);if(!record.result || !record.id || !record.captured_next_id || record.result->document_revision()>=record.id)
        throw std::invalid_argument("invalid density archive record");
    detail::bounded_utf8(record.name,256);detail::bounded_utf8(record.producer,256);
    if(record.name.empty() || record.producer.empty()) throw std::invalid_argument("density archive name/producer is empty");
    const auto& r=*record.result;const auto& n=*r.snapshot_;const auto& d=*n.capture.data_;const auto& o=n.options;o.validate();
    Writer w{{'Q','D','N',1},c};w.integer(record.id);w.integer(record.captured_next_id);w.text(record.name);w.text(record.producer);
    w.integer(o.prefer_nucleus);w.integer(o.polygonize_curves);
    for(auto v:{std::uint64_t(o.snapshot_bytes),std::uint64_t(o.work_bytes),o.max_membership_tests,o.max_geometry_operations,
        std::uint64_t(o.max_geometry_vertices),d.revision,std::uint64_t(d.bytes),std::uint64_t(d.work),std::uint64_t(n.bytes),std::uint64_t(n.work)}) w.integer(v);
    w.real(d.sx);w.real(d.sy);w.real(d.capture_ms);w.real(n.capture_ms);w.text(d.digest);w.text(n.digest);w.text(r.sha256_);
    w.integer(r.membership_tests_);w.integer(r.geometry_operations_);w.real(r.elapsed_ms_);
    const auto provenance=ProvenanceCodec::encode(*d.provenance,true);w.integer(provenance.size());w.raw(provenance.data(),provenance.size());
    w.integer(d.objects.size());
    for(const auto& a:d.objects) {
        stop(c);if(a.id>=record.captured_next_id) throw std::invalid_argument("density object exceeds captured identity bounds");
        w.integer(a.id);w.integer(a.revision);w.real(a.centroid.x);w.real(a.centroid.y);w.integer(unsigned(a.kind));w.integer(a.nucleus);w.text(a.geometry_sha256);
    }
    for(const auto* ids:{&d.sources,&d.targets}) {w.integer(ids->size());for(auto i:*ids) {stop(c);w.integer(i);}}
    auto geometry=[&](const Roi& roi) {stop(c);const auto b=GeometryArchiveCodec::encode(roi);w.integer(b.size());w.raw(b.data(),b.size());};
    for(const auto* group:{&n.windows,&n.tissue,&n.excluded}) {
        w.integer(group->size());for(const auto& a:*group) {stop(c);w.integer(a.id);w.integer(a.revision);geometry(a.geometry);}
    }
    for(const auto& row:r.rows_) {
        stop(c);w.integer(row.count);w.real(row.area_pixels);w.real(row.area_mm2);w.integer(bool(row.density_per_mm2));
        if(row.density_per_mm2) {w.real(*row.density_per_mm2);geometry(*row.footprint);}
    }
    const auto hash=checksum(w.bytes.data(),w.bytes.size(),c);w.raw(hash.data(),hash.size());return std::move(w.bytes);
}
std::shared_ptr<const SpatialDensityRecord> SpatialDensityArchiveCodec::decode(const std::uint8_t* p,std::size_t size,const std::atomic<bool>* c) {
    stop(c);if(!p || size<68 || size>limit || std::memcmp(p,"QDN\1",4) ||
        checksum(p,size-64,c)!=std::string(reinterpret_cast<const char*>(p+size-64),64)) throw std::invalid_argument("invalid density archive header/checksum");
    Reader rd{p+4,size-68};auto record=std::make_shared<SpatialDensityRecord>();
    record->id=rd.integer();record->captured_next_id=rd.integer();record->name=rd.text(256);record->producer=rd.text(256);
    SpatialDensityOptions o;o.prefer_nucleus=rd.integer(1)!=0;o.polygonize_curves=rd.integer(1)!=0;
    o.snapshot_bytes=std::size_t(rd.integer(256u*1024*1024));o.work_bytes=std::size_t(rd.integer(1024u*1024*1024));
    o.max_membership_tests=rd.integer();o.max_geometry_operations=rd.integer();o.max_geometry_vertices=std::size_t(rd.integer(1000000));o.validate();
    auto d=std::make_shared<SpatialDistanceRequest::Data>();
    d->options.units=SpatialDistanceUnits::Micrometres;d->options.prefer_nucleus=o.prefer_nucleus;d->options.exclude_self=false;
    d->options.snapshot_bytes=o.snapshot_bytes;d->options.work_bytes=o.work_bytes;d->options.max_distance_tests=o.max_membership_tests;
    d->revision=rd.integer();d->bytes=std::size_t(rd.integer(o.snapshot_bytes));d->work=std::size_t(rd.integer(o.work_bytes));
    const auto bytes=std::size_t(rd.integer(o.snapshot_bytes)),work=std::size_t(rd.integer(o.work_bytes));
    d->sx=rd.real();d->sy=rd.real();d->capture_ms=rd.real();const auto capture_ms=rd.real();d->digest=rd.text(64);hex(d->digest);
    const auto input=rd.text(64),result_hash=rd.text(64);hex(input);hex(result_hash);
    const auto tests=rd.integer(o.max_membership_tests),operations=rd.integer(o.max_geometry_operations);const auto elapsed_ms=rd.real();
    const auto provenance=rd.view(1024*1024);d->provenance=ProvenanceCodec::decode(provenance.first,provenance.second,true);
    if(!record->id || !record->captured_next_id || record->name.empty() || record->producer.empty() || d->revision>=record->id ||
       !d->provenance->source_only || d->provenance->document_revision!=d->revision || d->capture_ms<0 || capture_ms<0 || elapsed_ms<0 || d->sx<=0 || d->sy<=0 ||
       d->provenance->effective_pixel_width_um!=std::optional<double>(d->sx) || d->provenance->effective_pixel_height_um!=std::optional<double>(d->sy))
        throw std::invalid_argument("invalid density archive metadata/calibration");
    const auto count=std::size_t(rd.integer(130000));
    const auto base=sizeof(*d)+sizeof(SpatialDistanceRequest)+256+d->provenance->payload_bytes();
    if(!count || count>rd.left/120 || base>d->bytes || count>(d->bytes-base)/(sizeof(SpatialObjectSnapshot)+128)) throw std::invalid_argument("density objects exceed admitted payload");
    d->objects.reserve(count);
    for(std::size_t i=0;i<count;++i) {
        stop(c);SpatialObjectSnapshot a;a.id=rd.integer();a.revision=rd.integer(d->revision);a.centroid={rd.real(),rd.real()};
        a.kind=ObjectKind(rd.integer(2));a.nucleus=rd.integer(1)!=0;a.geometry_sha256=rd.text(64);hex(a.geometry_sha256);
        if(!a.id || a.id>=record->captured_next_id || (i && a.id<=d->objects.back().id) || (a.nucleus && (a.kind!=ObjectKind::Cell || !o.prefer_nucleus)) ||
           a.centroid.x<0 || a.centroid.y<0 || a.centroid.x>d->provenance->source.width || a.centroid.y>d->provenance->source.height)
            throw std::invalid_argument("invalid density object snapshot");
        d->objects.push_back(std::move(a));
    }
    std::vector<bool> used(count,false);
    for(auto* ids:{&d->sources,&d->targets}) {
        const auto length=std::size_t(rd.integer(ids==&d->sources?30000:100000));
        if(length>rd.left/8 || (ids==&d->sources && !length)) throw std::invalid_argument("invalid density cohort length");ids->reserve(length);
        for(std::size_t i=0;i<length;++i) {
            stop(c);const auto index=std::size_t(rd.integer(count-1));if(i && index<=ids->back()) throw std::invalid_argument("unordered density cohort");
            if(ids==&d->targets && d->objects[index].kind!=ObjectKind::Cell) throw std::invalid_argument("density target is not a Cell");
            ids->push_back(index);used[index]=true;
        }
    }
    const auto minimum=base+count*(sizeof(SpatialObjectSnapshot)+128)+(d->sources.size()+d->targets.size())*sizeof(std::size_t);
    if(std::find(used.begin(),used.end(),false)!=used.end() || d->bytes<minimum ||
       d->work<d->bytes+sizeof(SpatialDistanceResult)+d->sources.size()*sizeof(SpatialDistanceRow)+SpatialIndex::estimated_bytes(d->targets.size())+8192 || d->digest!=d->input_digest())
        throw std::invalid_argument("density centroid capture identity/payload mismatch");
    auto snapshot=std::make_shared<SpatialDensityRequest::Data>(SpatialDistanceRequest(d),o);
    snapshot->bytes=bytes;snapshot->work=work;snapshot->digest=input;snapshot->capture_ms=capture_ms;
    std::size_t admitted=d->bytes+sizeof(SpatialDensityRequest::Data)+sizeof(SpatialDensityRequest)+512,total_vertices=0;
    std::vector<std::uint64_t> source_ids;
    auto geometry=[&]() {stop(c);const auto blob=rd.view(std::min(limit,o.snapshot_bytes));auto roi=GeometryArchiveCodec::decode(blob.first,blob.second);stop(c);return roi;};
    for(auto* group:{&snapshot->windows,&snapshot->tissue,&snapshot->excluded}) {
        const auto length=std::size_t(rd.integer(10000));
        if((group!=&snapshot->excluded && !length) || length>rd.left/32) throw std::invalid_argument("invalid density area role length");
        const auto overhead=length*(sizeof(SpatialDensityAreaSnapshot)+128);
        if(admitted>bytes || overhead>bytes-admitted) throw std::invalid_argument("density area metadata exceeds admitted payload");admitted+=overhead;group->reserve(length);
        for(std::size_t i=0;i<length;++i) {
            stop(c);const auto id=rd.integer(),revision=rd.integer(d->revision);auto roi=geometry();
            const auto b=roi.bounds();const auto n=vertices(roi);
            if(!roi.is_area() || (roi.kind()==RoiKind::Ellipse && !o.polygonize_curves) || n>o.max_geometry_vertices-total_vertices ||
               b.x<0 || b.y<0 || b.x+b.width>d->provenance->source.width || b.y+b.height>d->provenance->source.height ||
               (i && id<=group->back().id)) throw std::invalid_argument("invalid density area geometry");
            const auto object=std::lower_bound(d->objects.begin(),d->objects.end(),id,[](const auto& a,auto key) {return a.id<key;});
            if(object==d->objects.end() || object->id!=id || object->revision!=revision) throw std::invalid_argument("density area object identity mismatch");
            const auto hash=geometry_hash(roi);const auto center=roi.centroid();
            if(!object->nucleus && (object->geometry_sha256!=hash || center.x!=object->centroid.x || center.y!=object->centroid.y)) throw std::invalid_argument("density area body identity mismatch");
            if(roi.payload_bytes()>bytes-admitted) throw std::invalid_argument("density geometry exceeds admitted payload");
            total_vertices+=n;admitted+=roi.payload_bytes();source_ids.push_back(id);group->push_back({id,revision,std::move(roi),hash});
        }
    }
    std::sort(source_ids.begin(),source_ids.end());source_ids.erase(std::unique(source_ids.begin(),source_ids.end()),source_ids.end());
    if(source_ids.size()!=d->sources.size()) throw std::invalid_argument("density area roles disagree with capture");
    for(std::size_t i=0;i<source_ids.size();++i) if(source_ids[i]!=d->objects[d->sources[i]].id) throw std::invalid_argument("density area roles disagree with capture");
    if(admitted!=bytes || work!=snapshot->work_bound() || input!=snapshot->input_digest()) throw std::invalid_argument("density snapshot identity/admission mismatch");
    auto result=std::shared_ptr<SpatialDensityResult>(new SpatialDensityResult());result->snapshot_=snapshot;result->rows_.reserve(snapshot->windows.size());
    result->sha256_=result_hash;result->membership_tests_=tests;result->geometry_operations_=operations;result->elapsed_ms_=elapsed_ms;
    total_vertices=0;
    for(const auto& window:snapshot->windows) {
        stop(c);SpatialDensityRow row;row.window_id=window.id;row.window_revision=window.revision;
        row.count=rd.integer(d->targets.size());row.area_pixels=rd.real();row.area_mm2=rd.real();const bool valid=rd.integer(1)!=0;
        if(valid) {
            row.density_per_mm2=rd.real();auto roi=geometry();const auto n=vertices(roi);
            if(!roi.is_area() || roi.kind()==RoiKind::Ellipse || n>o.max_geometry_vertices-total_vertices || row.area_pixels<=0 || row.area_mm2<=0 || *row.density_per_mm2<0 ||
               row.area_pixels!=roi.area() || row.area_mm2!=physical_area(row.area_pixels,d->sx,d->sy) || *row.density_per_mm2!=row.count/row.area_mm2)
                throw std::invalid_argument("invalid density row area/value");
            total_vertices+=n;row.geometry_sha256=geometry_hash(roi);row.footprint=std::move(roi);
        }else if(row.count || row.area_pixels!=0 || row.area_mm2!=0) throw std::invalid_argument("invalid empty density row");
        result->rows_.push_back(std::move(row));
    }
    if(rd.left || result->result_digest()!=result_hash ||
       result->payload_bytes()+SpatialIndex::estimated_bytes(d->targets.size())+3*d->targets.size()*sizeof(std::uint64_t)+o.max_geometry_vertices*1024+8192>o.work_bytes)
        throw std::invalid_argument("density result identity/payload mismatch");
    record->result=std::move(result);return record;
}
}
