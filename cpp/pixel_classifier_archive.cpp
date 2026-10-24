#include "pixel_classifier_archive.hpp"
#include "pixel_workflow_snapshot.hpp"
#include "geometry_archive.hpp"
#include "provenance_codec.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace qpx {
namespace {
constexpr std::size_t limit=128u*1024*1024;
void stop(const std::atomic<bool>* c) {if(c && c->load()) throw std::runtime_error("pixel archive cancelled");}
std::string digest(const std::uint8_t* p,std::size_t n) {Sha256 s;s.update(p,n);return s.finish();}
struct Writer {
    std::vector<std::uint8_t> b;
    explicit Writer(const char* magic):b(magic,magic+4) {}
    void raw(const std::uint8_t* p,std::size_t n) {if(!n) return;if(n>limit-b.size()) throw std::length_error("pixel archive exceeds 128 MiB");b.insert(b.end(),p,p+n);}
    void integer(std::uint64_t n) {std::uint8_t bytes[8];for(int i=0;i<8;++i) bytes[i]=static_cast<std::uint8_t>(n>>(8*i));raw(bytes,8);}
    void real(double n) {std::uint64_t bits;std::memcpy(&bits,&n,8);integer(bits);}
    void text(const std::string& s) {integer(s.size());raw(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
    void blob(const std::vector<std::uint8_t>& v) {integer(v.size());raw(v.data(),v.size());}
    void finish() {const auto hash=digest(b.data(),b.size());raw(reinterpret_cast<const std::uint8_t*>(hash.data()),hash.size());b.shrink_to_fit();}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    Reader(const std::uint8_t* data,std::size_t size,const char* magic):p(data),left(size>=68?size-68:0) {
        if(!data || size<68 || size>limit || std::memcmp(data,magic,4)) throw std::invalid_argument("unsupported pixel archive encoding");
        if(digest(data,size-64)!=std::string(reinterpret_cast<const char*>(data+size-64),64)) throw std::invalid_argument("pixel archive checksum mismatch");p+=4;
    }
    std::uint64_t integer(std::uint64_t max=std::numeric_limits<std::uint64_t>::max()) {if(left<8) throw std::invalid_argument("truncated pixel archive");std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;if(n>max) throw std::invalid_argument("invalid pixel archive integer");return n;}
    double real() {const auto bits=integer();double n;std::memcpy(&n,&bits,8);if(!std::isfinite(n)) throw std::invalid_argument("nonfinite pixel archive value");return n;}
    std::string text(std::size_t max) {const auto n=integer(max);if(n>left) throw std::invalid_argument("truncated pixel archive text");std::string s(reinterpret_cast<const char*>(p),static_cast<std::size_t>(n));p+=n;left-=n;detail::bounded_utf8(s,max);return s;}
    std::pair<const std::uint8_t*,std::size_t> view(std::size_t max=limit) {const auto n=integer(max);if(n>left) throw std::invalid_argument("truncated pixel archive blob");const auto* start=p;p+=n;left-=n;return {start,static_cast<std::size_t>(n)};}
    std::vector<std::uint8_t> blob(std::size_t max=limit) {const auto v=view(max);return {v.first,v.first+v.second};}
    Roi roi() {const auto v=view(32u*1024*1024);return GeometryArchiveCodec::decode(v.first,v.second);}
    void finish() {if(left) throw std::invalid_argument("trailing pixel archive bytes");}
};

void u32(Writer& w,std::uint32_t n) {std::uint8_t b[4];for(int i=0;i<4;++i) b[i]=static_cast<std::uint8_t>(n>>(8*i));w.raw(b,4);}
std::uint32_t u32(Reader& r) {if(r.left<4) throw std::invalid_argument("truncated pixel array");std::uint32_t n=0;for(int i=0;i<4;++i) n|=std::uint32_t(r.p[i])<<(8*i);r.p+=4;r.left-=4;return n;}
std::uint32_t bits(float f) {std::uint32_t b;std::memcpy(&b,&f,4);return b;}
float number(std::uint32_t b) {float f;std::memcpy(&f,&b,4);return f;}
void text_hash(Sha256& h,const std::string& s) {h.integer(s.size());h.update(s.data(),s.size());}
void real_hash(Sha256& h,double x) {std::uint64_t b;std::memcpy(&b,&x,8);h.integer(b);}
void resolution_hash(Sha256& h,const PixelResolution& r) {real_hash(h,r.downsample);h.integer(r.physical());if(r.physical()) {real_hash(h,*r.pixel_width_um);real_hash(h,*r.pixel_height_um);}}
void hash_check(const std::string& s) {if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("invalid pixel archive hash");}
void geometry(const Roi& roi,const ImageMetadata& m) {const auto b=roi.bounds();if(!roi.is_area() || b.x<0 || b.y<0 || static_cast<long double>(b.x)+b.width>m.width || static_cast<long double>(b.y)+b.height>m.height) throw std::invalid_argument("pixel archive ROI exceeds its image");}
void metadata(const AnalysisProvenance& p) {
    p.validate();const auto& m=p.source;if(m.levels.empty() || m.levels.size()>1024 || m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA || p.nucleus) throw std::invalid_argument("invalid pixel archive source metadata");
    for(std::size_t i=0;i<m.levels.size();++i) {const auto& l=m.levels[i];if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1 || (!i && (l.width!=m.width || l.height!=m.height || l.downsample!=1))) throw std::invalid_argument("invalid pixel archive pyramid");}
}
void resolution(const PixelClassifier& model,const AnalysisProvenance& provenance,std::int32_t level) {
    const auto r=model.resolution();const auto& m=provenance.source;if(!r || level<0 || std::size_t(level)>=m.levels.size()) throw std::invalid_argument("pixel archive requires a natural resolution");
    auto close=[](double a,double b) {return std::abs(a-b)<=1e-9*std::max(std::abs(a),std::abs(b));};
    const auto scale=m.levels[level].downsample;
    if(r->physical()) {if(!provenance.effective_pixel_width_um || !provenance.effective_pixel_height_um || !close(*r->pixel_width_um,*provenance.effective_pixel_width_um*scale) || !close(*r->pixel_height_um,*provenance.effective_pixel_height_um*scale)) throw std::invalid_argument("pixel archive physical resolution mismatch");}
    else if(!close(r->downsample,scale)) throw std::invalid_argument("pixel archive raw resolution mismatch");
}
void annotation_write(Writer& w,const PixelAnnotationSnapshot& a) {detail::bounded_utf8(a.class_name,65536);w.integer(a.id);w.integer(a.revision);w.integer(static_cast<std::uint64_t>(a.kind));w.text(a.class_name);w.blob(GeometryArchiveCodec::encode(a.geometry));}
PixelAnnotationSnapshot annotation_read(Reader& r,const AnalysisProvenance& p) {
    PixelAnnotationSnapshot a;a.id=r.integer();a.revision=r.integer();a.kind=static_cast<ObjectKind>(r.integer(2));a.class_name=r.text(65536);a.geometry=r.roi();
    if(!a.id || !a.revision || a.revision>p.document_revision) throw std::invalid_argument("invalid pixel annotation snapshot revision");geometry(a.geometry,p.source);return a;
}
std::string table_hash(const PixelTrainingTable& t,const std::atomic<bool>* cancelled) {
    Sha256 h;const std::string tag="qpx.annotation-pixel-sampling/1";h.update(tag.data(),tag.size());resolution_hash(h,t.resolution());h.integer(t.row_count());for(const auto& f:t.features()) {text_hash(h,f.name);text_hash(h,f.unit);}for(const auto& name:t.classes()) text_hash(h,name);
    for(std::size_t i=0;i<t.row_count();++i) {if((i&511)==0) stop(cancelled);h.integer(t.labels()[i]);h.integer(t.coordinates()[2*i]);h.integer(t.coordinates()[2*i+1]);h.integer(t.annotation_ids()[i]);for(std::size_t j=0;j<t.features().size();++j) h.integer(bits(t.values()[i*t.features().size()+j]));}return h.finish();
}
void training_identity(const PixelTrainingTable& t,const PixelClassifier& m,const std::atomic<bool>* cancelled) {
    if(t.row_count()!=m.forest().training_rows() || t.classes()!=m.classes() || t.features().size()!=m.features().size() || t.transform().white()!=m.transform().white() || t.transform().stains()!=m.transform().stains()) throw std::invalid_argument("pixel archive training/model mismatch");
    const auto a=t.options().features,b=m.options();if(a.rgb!=b.rgb || a.optical_density!=b.optical_density || a.stains!=b.stains || a.gradient_magnitude!=b.gradient_magnitude || a.laplacian!=b.laplacian || a.sigmas!=b.sigmas || a.work_bytes!=b.work_bytes) throw std::invalid_argument("pixel archive feature recipe mismatch");
    const auto r=m.resolution();const auto tr=t.resolution();if(!r || r->downsample!=tr.downsample || r->pixel_width_um!=tr.pixel_width_um || r->pixel_height_um!=tr.pixel_height_um) throw std::invalid_argument("pixel training resolution mismatch");
    Sha256 h;const std::string tag="qpx.forest.training/1";h.update(tag.data(),tag.size());h.integer(t.row_count());h.integer(t.features().size());h.integer(t.classes().size());
    for(std::size_t i=0;i<t.features().size();++i) {const auto& f=t.features()[i];if(f.name!=m.features()[i].name || f.unit!=m.features()[i].unit) throw std::invalid_argument("pixel training schema mismatch");text_hash(h,f.name);text_hash(h,f.unit);}for(const auto& name:t.classes()) text_hash(h,name);
    for(std::size_t i=0;i<t.values().size();++i) {if((i&4095)==0) stop(cancelled);if(!std::isfinite(t.values()[i])) throw std::invalid_argument("nonfinite pixel training value");h.integer(bits(t.values()[i]));}for(auto label:t.labels()) h.integer(label);
    if(h.finish()!=m.forest().training_sha256()) throw std::invalid_argument("pixel archive training numeric identity mismatch");
}
std::string prediction_hash(const PixelPredictionResult& t,const PixelPredictionRequest::Data& d,const std::atomic<bool>* cancelled) {
    Sha256 h;const std::string tag="qpx.source-pixel-classification/1";h.update(tag.data(),tag.size());const auto& m=t.model();h.update(m.sha256().data(),m.sha256().size());real_hash(h,t.options().min_vote_fraction);const auto& source=t.provenance()->source;const auto& level=source.levels.at(t.level());h.integer(source.width);h.integer(source.height);h.integer(level.width);h.integer(level.height);h.integer(t.level());real_hash(h,t.downsample());for(auto v:{d.x,d.y,d.w,d.h}) h.integer(v);
    for(std::size_t i=0;i<t.labels().size();++i) {if((i&4095)==0) stop(cancelled);h.integer(t.valid()[i]);h.integer(t.labels()[i]);h.integer(std::isfinite(t.vote_fractions()[i])?bits(t.vote_fractions()[i]):0x7fc00000u);}return h.finish();
}
void samples_check(const PixelTrainingTable& t,const std::vector<PixelAnnotationSnapshot>& annotations,const std::atomic<bool>* cancelled) {
    const auto p=t.options();const auto& level=t.provenance()->source.levels.at(p.level);const auto classes=t.classes().size(),rows=t.row_count(),columns=t.features().size();
    if(rows<4 || rows>p.max_samples || t.values().size()!=rows*columns || t.coordinates().size()!=rows*2 || t.annotation_ids().size()!=rows || t.eligible_class_pixels().size()!=classes || t.sampled_class_counts().size()!=classes || !t.tiles() || !t.scanned_pixels() || t.scanned_pixels()>p.max_scanned_pixels || t.ambiguous_pixels()>t.scanned_pixels() || t.estimated_work_bytes()>p.work_bytes || t.estimated_work_bytes()<t.retained_bytes() || t.tiles()>t.scanned_pixels() || !std::isfinite(t.elapsed_ms()) || t.elapsed_ms()<0) throw std::invalid_argument("invalid pixel training dimensions/statistics");
    std::map<std::uint64_t,const PixelAnnotationSnapshot*> objects;std::size_t complexity=0;std::uint64_t previous=0;
    for(const auto& a:annotations) {if(a.id<=previous || !std::binary_search(t.classes().begin(),t.classes().end(),a.class_name)) throw std::invalid_argument("invalid pixel training annotation order/class");previous=a.id;objects.emplace(a.id,&a);for(const auto& path:a.geometry.paths()) complexity+=path.size();}
    if(annotations.empty() || complexity>100000000/rows) throw std::length_error("pixel archive cohort geometry verification exceeds operation budget");
    std::set<std::pair<std::int64_t,std::int64_t>> centers;std::vector<std::uint64_t> counts(classes);std::uint32_t prior=0;
    for(std::size_t i=0;i<rows;++i) {if((i&511)==0) stop(cancelled);const auto label=t.labels()[i];const auto x=t.coordinates()[i*2],y=t.coordinates()[i*2+1];const auto obj=objects.find(t.annotation_ids()[i]);
        if(label>=classes || label<prior || x<0 || x>=level.width || y<0 || y>=level.height || obj==objects.end() || obj->second->class_name!=t.classes()[label] || !centers.emplace(x,y).second || !obj->second->geometry.contains({(double(x)+.5)*level.downsample,(double(y)+.5)*level.downsample})) throw std::invalid_argument("invalid pixel training sample identity/coordinate");prior=label;++counts[label];
    }
    std::uint64_t used=0;const auto cap=std::min(p.max_samples_per_class,p.max_samples/classes);
    for(std::size_t i=0;i<classes;++i) {if(counts[i]!=t.sampled_class_counts()[i] || counts[i]<2 || counts[i]>cap || counts[i]>t.eligible_class_pixels()[i] || t.eligible_class_pixels()[i]>t.scanned_pixels() || (p.balanced && counts[i]!=counts[0])) throw std::invalid_argument("invalid pixel training class counts");used+=t.eligible_class_pixels()[i];}if(used>t.scanned_pixels()-t.ambiguous_pixels()) throw std::invalid_argument("pixel training eligibility exceeds scan");
    hash_check(t.sha256());hash_check(t.input_sha256());if(table_hash(t,cancelled)!=t.sha256()) throw std::invalid_argument("pixel training table hash mismatch");
}
void mask_check(const PixelPredictionResult& t,const PixelPredictionRequest::Data& d,const std::atomic<bool>* cancelled) {
    const auto p=t.options();const auto n=t.labels().size();const auto classes=t.model().classes().size();
    if(n!=std::uint64_t(t.width())*t.height() || t.valid().size()!=n || t.vote_fractions().size()!=n || t.class_counts().size()!=classes || t.estimated_work_bytes()>p.work_bytes || !std::isfinite(t.elapsed_ms()) || t.elapsed_ms()<0) throw std::invalid_argument("invalid pixel prediction dimensions/statistics");
    const auto cost=t.roi().payload_bytes()/sizeof(Point)+1;if(n && cost>100000000/n) throw std::length_error("pixel archive mask geometry verification exceeds operation budget");
    std::vector<std::uint64_t> counts(classes);std::uint64_t valid=0,classified=0;const auto trees=t.model().forest().tree_count();
    for(std::size_t i=0;i<n;++i) {if((i&4095)==0) stop(cancelled);const auto label=t.labels()[i];const auto f=t.vote_fractions()[i];
        if(t.valid()[i]>1 || label>classes || (!t.valid()[i] && (label || !std::isnan(f))) || (t.valid()[i] && (!std::isfinite(f) || f<0 || f>1 || std::abs(f*trees-std::round(f*trees))>1e-6*trees || f*trees+1e-5<std::ceil(double(trees)/classes) || (label && f+1e-7<p.min_vote_fraction) || (!label && f>p.min_vote_fraction+1e-7)))) throw std::invalid_argument("invalid pixel mask label/vote");
        if(t.valid()[i] && !t.roi().contains({(double(d.x+static_cast<std::int64_t>(i%std::size_t(d.w)))+.5)*t.downsample(),(double(d.y+static_cast<std::int64_t>(i/std::size_t(d.w)))+.5)*t.downsample()})) throw std::invalid_argument("pixel mask marks a center outside its ROI as valid");
        valid+=t.valid()[i];if(label) {++classified;++counts[label-1];}
    }
    if(counts!=t.class_counts() || valid!=t.valid_pixels() || classified!=t.classified_pixels()) throw std::invalid_argument("pixel mask class counts mismatch");hash_check(t.sha256());hash_check(t.input_sha256());if(prediction_hash(t,d,cancelled)!=t.sha256()) throw std::invalid_argument("pixel prediction hash mismatch");
}
void options_write(Writer& w,const PixelTrainingOptions& p) {w.integer(p.level);w.integer(p.calibrated);w.integer(p.balanced);w.integer(p.tile_size);w.integer(p.max_samples);w.integer(p.max_samples_per_class);w.integer(p.work_bytes);w.integer(p.seed);w.integer(p.max_scanned_pixels);}
PixelTrainingOptions options_read(Reader& r,const PixelClassifier& m) {PixelTrainingOptions p;p.features=m.options();p.level=static_cast<std::int32_t>(r.integer(1023));p.calibrated=r.integer(1)!=0;p.balanced=r.integer(1)!=0;p.tile_size=r.integer(512);p.max_samples=r.integer(100000);p.max_samples_per_class=r.integer(50000);p.work_bytes=r.integer(1024ull*1024*1024);p.seed=r.integer();p.max_scanned_pixels=r.integer(100000000);p.validate();return p;}
}
const std::shared_ptr<const AnalysisProvenance>& PixelClassifierArchive::provenance() const {return table_?table_->provenance():result_->provenance();}
std::shared_ptr<const PixelClassifierArchive> PixelClassifierArchive::from_bytes(const std::vector<std::uint8_t>& b) {return PixelClassifierArchiveCodec::decode(b.data(),b.size(),nullptr);}
std::size_t PixelClassifierRecord::payload_bytes() const {return sizeof(PixelClassifierRecord)+name.size()+(archive?archive->payload_bytes():0);}
void PixelClassifierArchiveCodec::seal(PixelClassifierArchive& out,std::vector<std::uint8_t> bytes) {
    if(bytes.size()>limit-1024) throw std::length_error("pixel artifact exceeds its 128 MiB encoding limit");out.bytes_=std::move(bytes);out.sha_=digest(out.bytes_.data(),out.bytes_.size());
    out.payload_=sizeof(PixelClassifierArchive)+out.bytes_.capacity()+3*out.model_->bytes().size();
    if(out.table_) out.payload_+=out.table_->retained_bytes();
    if(out.result_) out.payload_+=sizeof(PixelPredictionRequest::Data)+out.result_->labels().capacity()*4+out.result_->valid().capacity()+out.result_->vote_fractions().capacity()*4+out.result_->roi().payload_bytes()+out.result_->provenance()->payload_bytes()+262144;
    for(const auto& a:out.annotations_) out.payload_+=sizeof(PixelAnnotationSnapshot)+a.class_name.size()+a.geometry.payload_bytes();
    if(out.payload_>256u*1024*1024) throw std::length_error("pixel artifact exceeds 256 MiB retained payload");
}
std::shared_ptr<const PixelClassifierArchive> archive_pixel_training(std::shared_ptr<const PixelTrainingTable> table,const PixelClassifier& model,const std::atomic<bool>* cancelled) {return PixelClassifierArchiveCodec::training(std::move(table),model,cancelled);}
std::shared_ptr<const PixelClassifierArchive> archive_pixel_prediction(std::shared_ptr<const PixelPredictionResult> result,const std::atomic<bool>* cancelled) {return PixelClassifierArchiveCodec::prediction(std::move(result),cancelled);}

std::shared_ptr<const PixelClassifierArchive> PixelClassifierArchiveCodec::training(std::shared_ptr<const PixelTrainingTable> table,const PixelClassifier& model,const std::atomic<bool>* cancelled) {
    stop(cancelled);if(!table || !table->snapshot_) throw std::invalid_argument("pixel archive requires a training snapshot");const auto& d=*table->snapshot_;metadata(*d.provenance);resolution(model,*d.provenance,d.options.level);training_identity(*table,model,cancelled);
    auto out=std::shared_ptr<PixelClassifierArchive>(new PixelClassifierArchive());out->table_=table;out->model_=std::make_shared<const PixelClassifier>(model);out->annotations_.reserve(d.rows.size());
    for(const auto& a:d.rows) out->annotations_.push_back({a.id,a.revision,a.kind,d.classes.at(a.label),a.roi});
    samples_check(*table,out->annotations_,cancelled);
    Writer w("QPA\1");w.integer(1);w.blob(model.bytes());w.blob(ProvenanceCodec::encode(*d.provenance));w.integer(out->annotations_.size());for(const auto& a:out->annotations_) {stop(cancelled);annotation_write(w,a);}
    options_write(w,d.options);w.integer(d.bytes);w.real(d.capture_ms);w.integer(table->ambiguous_pixels());w.integer(table->scanned_pixels());w.integer(table->tiles());w.integer(table->estimated_work_bytes());w.real(table->elapsed_ms());w.text(table->sha256());w.text(table->input_sha256());
    for(std::size_t i=0;i<d.classes.size();++i) {w.integer(table->eligible_class_pixels()[i]);w.integer(table->sampled_class_counts()[i]);}
    w.integer(table->row_count());for(std::size_t i=0;i<table->row_count();++i) {if((i&511)==0) stop(cancelled);u32(w,table->labels()[i]);w.integer(table->coordinates()[i*2]);w.integer(table->coordinates()[i*2+1]);w.integer(table->annotation_ids()[i]);for(std::size_t c=0;c<table->features().size();++c) u32(w,bits(table->values()[i*table->features().size()+c]));}
    w.finish();seal(*out,std::move(w.b));stop(cancelled);return out;
}
std::shared_ptr<const PixelClassifierArchive> PixelClassifierArchiveCodec::prediction(std::shared_ptr<const PixelPredictionResult> result,const std::atomic<bool>* cancelled) {
    stop(cancelled);if(!result || !result->snapshot_) throw std::invalid_argument("pixel archive requires a prediction snapshot");const auto& d=*result->snapshot_;metadata(*d.provenance);resolution(d.model,*d.provenance,d.level);mask_check(*result,d,cancelled);
    auto out=std::shared_ptr<PixelClassifierArchive>(new PixelClassifierArchive());out->result_=result;out->model_=std::make_shared<const PixelClassifier>(d.model);out->annotations_.push_back({d.id,d.provenance->object_revision,d.kind,d.class_name,d.roi});
    Writer w(d.windowed?"QPA\2":"QPA\1");w.integer(0);w.blob(d.model.bytes());w.blob(ProvenanceCodec::encode(*d.provenance));w.integer(1);annotation_write(w,out->annotations_.front());w.integer(d.level);for(auto n:{d.x,d.y,d.w,d.h}) w.integer(n);w.integer(d.options.tile_size);w.integer(d.options.work_bytes);w.real(d.options.min_vote_fraction);w.integer(d.fixed);w.integer(d.work);w.integer(d.feature_budget);w.integer(d.tiles);w.integer(result->valid_pixels());w.integer(result->classified_pixels());w.real(result->elapsed_ms());w.text(result->sha256());w.text(result->input_sha256());for(auto n:result->class_counts()) w.integer(n);
    for(std::size_t i=0;i<result->labels().size();++i) {if((i&4095)==0) stop(cancelled);u32(w,result->labels()[i]);const auto valid=result->valid()[i];w.raw(&valid,1);u32(w,std::isfinite(result->vote_fractions()[i])?bits(result->vote_fractions()[i]):0x7fc00000u);}
    w.finish();seal(*out,std::move(w.b));stop(cancelled);return out;
}
void PixelClassifierArchiveCodec::validate_prediction(const PixelPredictionResult& result,const std::atomic<bool>* cancelled) {
    if(!result.snapshot_) throw std::invalid_argument("pixel block has no snapshot");mask_check(result,*result.snapshot_,cancelled);
}
std::shared_ptr<const PixelClassifierArchive> PixelClassifierArchiveCodec::decode(const std::uint8_t* data,std::size_t size,const std::atomic<bool>* cancelled) {
    stop(cancelled);const bool windowed=data && size>=4 && std::memcmp(data,"QPA\2",4)==0;Reader r(data,size,windowed?"QPA\2":"QPA\1");const bool training=r.integer(1)!=0;if(windowed && training) throw std::invalid_argument("windowed pixel archive cannot contain a training cohort");auto model=PixelClassifier::from_bytes(r.blob(64u*1024*1024+4096));const auto pv=r.view(4u*1024*1024);auto provenance=ProvenanceCodec::decode(pv.first,pv.second);metadata(*provenance);
    auto out=std::shared_ptr<PixelClassifierArchive>(new PixelClassifierArchive());out->model_=std::make_shared<const PixelClassifier>(model);const auto annotations=r.integer(4096);if(!annotations || (!training && annotations!=1)) throw std::invalid_argument("invalid pixel annotation snapshot count");out->annotations_.reserve(annotations);
    std::size_t snapshot_min=0;for(std::size_t i=0;i<annotations;++i) {stop(cancelled);auto a=annotation_read(r,*provenance);snapshot_min+=a.geometry.payload_bytes()+a.class_name.size()+sizeof(std::string);out->annotations_.push_back(std::move(a));}
    const auto& first=out->annotations_.front();if(first.id!=provenance->object_id || first.revision!=provenance->object_revision) throw std::invalid_argument("pixel archive source anchor mismatch");
    if(training) {
        auto options=options_read(r,model);resolution(model,*provenance,options.level);if(model.resolution()->physical()!=bool(options.calibrated && provenance->effective_pixel_width_um && provenance->effective_pixel_height_um)) throw std::invalid_argument("pixel training calibration preset mismatch");if(model.resolution()->downsample!=provenance->source.levels[options.level].downsample) throw std::invalid_argument("pixel training grid mismatch");
        auto d=std::make_shared<PixelTrainingRequest::Data>(options,model.transform());d->provenance=provenance;d->metadata=provenance->source;d->revision=provenance->document_revision;d->resolution=*model.resolution();d->classes=model.classes();d->bytes=r.integer(std::min<std::size_t>(64u*1024*1024,options.work_bytes));d->capture_ms=r.real();
        if(d->capture_ms<0 || d->bytes<sizeof(*d)+annotations*sizeof(PixelTrainingRequest::Data::Row)+snapshot_min+provenance->payload_bytes()) throw std::invalid_argument("invalid pixel training snapshot budget");
        d->rows.reserve(annotations);for(const auto& a:out->annotations_) {const auto found=std::lower_bound(d->classes.begin(),d->classes.end(),a.class_name);if(found==d->classes.end() || *found!=a.class_name) throw std::invalid_argument("unknown pixel training annotation class");d->rows.push_back({a.id,a.geometry,static_cast<std::uint32_t>(found-d->classes.begin()),a.revision,a.kind});}
        auto table=std::shared_ptr<PixelTrainingTable>(new PixelTrainingTable());table->snapshot_=d;table->features_=model.features();table->ambiguous_=r.integer(options.max_scanned_pixels);table->scanned_=r.integer(options.max_scanned_pixels);table->tiles_=r.integer(options.max_scanned_pixels);table->work_=r.integer(options.work_bytes);table->ms_=r.real();table->sha_=r.text(64);table->input_sha_=r.text(64);for(std::size_t i=0;i<d->classes.size();++i) {table->eligible_.push_back(r.integer(options.max_scanned_pixels));table->counts_.push_back(r.integer(options.max_samples_per_class));}
        const auto rows=r.integer(options.max_samples);
        const auto columns=table->features_.size();if(rows<4 || rows>(r.left/(28+4*columns))) throw std::invalid_argument("truncated pixel training arrays");
        if(size+3*model.bytes().size()+snapshot_min+d->bytes+rows*(columns*4+28)+524288>256u*1024*1024) throw std::length_error("decoded pixel cohort exceeds retained budget");table->values_.resize(rows*columns);table->labels_.resize(rows);table->coordinates_.resize(rows*2);table->ids_.resize(rows);
        for(std::size_t i=0;i<rows;++i) {if((i&511)==0) stop(cancelled);table->labels_[i]=u32(r);table->coordinates_[i*2]=static_cast<std::int64_t>(r.integer(std::numeric_limits<std::int64_t>::max()));table->coordinates_[i*2+1]=static_cast<std::int64_t>(r.integer(std::numeric_limits<std::int64_t>::max()));table->ids_[i]=r.integer();for(std::size_t c=0;c<columns;++c) table->values_[i*columns+c]=number(u32(r));}
        samples_check(*table,out->annotations_,cancelled);training_identity(*table,model,cancelled);out->table_=std::move(table);
    }else {
        const auto level=static_cast<std::int32_t>(r.integer(1023));resolution(model,*provenance,level);const auto& grid=provenance->source.levels.at(level);std::int64_t dims[4];for(auto& n:dims) n=static_cast<std::int64_t>(r.integer(std::numeric_limits<std::int64_t>::max()));
        if(dims[0]>=grid.width || dims[1]>=grid.height || !dims[2] || !dims[3] || dims[2]>grid.width-dims[0] || dims[3]>grid.height-dims[1] || std::uint64_t(dims[2])>limit/9/std::uint64_t(dims[3])) throw std::invalid_argument("invalid pixel prediction grid");
        PixelPredictionOptions options;options.tile_size=r.integer(512);options.work_bytes=r.integer(1024ull*1024*1024);options.min_vote_fraction=r.real();options.validate();auto d=std::make_shared<PixelPredictionRequest::Data>(model,options);d->provenance=provenance;d->metadata=provenance->source;d->level=level;d->id=first.id;d->revision=provenance->document_revision;d->kind=first.kind;d->class_name=first.class_name;d->roi=first.geometry;d->windowed=windowed;d->x=dims[0];d->y=dims[1];d->w=dims[2];d->h=dims[3];d->fixed=r.integer(options.work_bytes);d->work=r.integer(options.work_bytes);d->feature_budget=r.integer(options.work_bytes);d->tiles=r.integer(100000000);
        const auto n=std::size_t(d->w)*d->h;if(d->fixed<9*n+sizeof(*d)+d->roi.payload_bytes()+provenance->payload_bytes()+d->class_name.size()+262144 || d->fixed>d->work || d->feature_budget<1024*1024 || d->feature_budget>model.options().work_bytes || d->tiles!=((std::uint64_t(d->w)+options.tile_size-1)/options.tile_size)*((std::uint64_t(d->h)+options.tile_size-1)/options.tile_size)) throw std::invalid_argument("invalid pixel prediction work metadata");
        auto edge=[&](double x,std::int64_t maximum) {const auto v=std::ceil(static_cast<long double>(x)/grid.downsample-.5L);return v<=0?std::int64_t(0):v>=maximum?maximum:static_cast<std::int64_t>(v);};const auto b=d->roi.bounds();const auto left=edge(b.x,grid.width),top=edge(b.y,grid.height),right=edge(b.x+b.width,grid.width),bottom=edge(b.y+b.height,grid.height);if(windowed?(d->x<left || d->y<top || d->x+d->w>right || d->y+d->h>bottom):(d->x!=left || d->y!=top || d->x+d->w!=right || d->y+d->h!=bottom)) throw std::invalid_argument("pixel prediction grid/ROI mismatch");
        auto result=std::shared_ptr<PixelPredictionResult>(new PixelPredictionResult());result->snapshot_=d;result->valid_pixels_=r.integer(n);result->classified_pixels_=r.integer(n);result->ms_=r.real();result->sha_=r.text(64);result->input_sha_=r.text(64);for(std::size_t i=0;i<model.classes().size();++i) result->counts_.push_back(r.integer(n));
        if(size+3*model.bytes().size()+snapshot_min+provenance->payload_bytes()+n*9+524288>256u*1024*1024) throw std::length_error("decoded pixel mask exceeds retained budget");
        if(n>r.left/9) throw std::invalid_argument("truncated pixel prediction arrays");result->labels_.resize(n);result->valid_.resize(n);result->fractions_.resize(n);
        for(std::size_t i=0;i<n;++i) {if((i&4095)==0) stop(cancelled);result->labels_[i]=u32(r);if(!r.left) throw std::invalid_argument("truncated pixel validity array");result->valid_[i]=*r.p++;--r.left;const auto fraction=u32(r);if(!result->valid_[i] && fraction!=0x7fc00000u) throw std::invalid_argument("noncanonical invalid pixel vote");result->fractions_[i]=number(fraction);}
        mask_check(*result,*d,cancelled);out->result_=std::move(result);
    }
    r.finish();seal(*out,std::vector<std::uint8_t>(data,data+size));stop(cancelled);return out;
}
std::vector<std::uint8_t> PixelClassifierArchiveCodec::encode(const PixelClassifierRecord& record,const std::atomic<bool>* cancelled) {
    stop(cancelled);detail::bounded_utf8(record.name,256);if(!record.id || !record.captured_next_id || record.name.empty() || !record.archive || record.archive->provenance()->document_revision>=record.id) throw std::invalid_argument("invalid pixel record metadata");Writer w("QPR\1");w.integer(record.id);w.integer(record.captured_next_id);w.text(record.name);w.blob(record.archive->bytes());w.finish();stop(cancelled);return std::move(w.b);
}
std::shared_ptr<const PixelClassifierRecord> PixelClassifierArchiveCodec::record(const std::uint8_t* data,std::size_t size,const ImageMetadata& metadata,const std::atomic<bool>* cancelled) {
    stop(cancelled);Reader r(data,size,"QPR\1");auto record=std::make_shared<PixelClassifierRecord>();record->id=r.integer();record->captured_next_id=r.integer();record->name=r.text(256);const auto v=r.view();record->archive=decode(v.first,v.second,cancelled);r.finish();const auto& p=*record->archive->provenance();if(!record->id || !record->captured_next_id || record->name.empty() || p.document_revision>=record->id || p.source.width!=metadata.width || p.source.height!=metadata.height) throw std::invalid_argument("invalid pixel record source/revision");for(const auto& a:record->archive->annotations()) if(a.id>=record->captured_next_id) throw std::invalid_argument("pixel archive exceeds captured object counter");return record;
}
}
