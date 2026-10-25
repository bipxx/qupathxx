#include "object_classifier_archive.hpp"
#include "object_feature_snapshot.hpp"
#include "geometry_archive.hpp"
#include "provenance_codec.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include "cell_recipe.hpp"
#include "scientific_cells.hpp"
#include "grid_cell_descriptor.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <map>
#include <tuple>
#include <stdexcept>
namespace qpx {
namespace {
constexpr std::size_t limit=160u*1024*1024,artifact_limit=128u*1024*1024;
void stopped(const std::atomic<bool>* c) {if(c && c->load()) throw std::runtime_error("classifier archive cancelled");}
std::string digest(const std::uint8_t* p,std::size_t n) {Sha256 s;s.update(p,n);return s.finish();}
struct Writer {
    std::vector<std::uint8_t> b;
    explicit Writer(const char* magic):b(magic,magic+4) {}
    void raw(const std::uint8_t* p,std::size_t n) {if(!n) return;if(n>limit-b.size()) throw std::length_error("classifier archive exceeds 160 MiB");b.insert(b.end(),p,p+n);}
    void integer(std::uint64_t n) {std::uint8_t bytes[8];for(int i=0;i<8;++i) bytes[i]=static_cast<std::uint8_t>(n>>(8*i));raw(bytes,8);}
    void real(double n) {std::uint64_t bits;std::memcpy(&bits,&n,8);integer(bits);}
    void text(const std::string& s) {integer(s.size());raw(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
    void blob(const std::vector<std::uint8_t>& v) {integer(v.size());raw(v.data(),v.size());}
    void finish() {const auto hash=digest(b.data(),b.size());raw(reinterpret_cast<const std::uint8_t*>(hash.data()),hash.size());b.shrink_to_fit();}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    Reader(const std::uint8_t* data,std::size_t size,const char* magic):p(data),left(size>=68?size-68:0) {
        if(!data || size<68 || size>limit || std::memcmp(data,magic,4)) throw std::invalid_argument("unsupported classifier archive encoding");
        if(digest(data,size-64)!=std::string(reinterpret_cast<const char*>(data+size-64),64)) throw std::invalid_argument("classifier archive checksum mismatch");p+=4;
    }
    std::uint64_t integer(std::uint64_t max=std::numeric_limits<std::uint64_t>::max()) {if(left<8) throw std::invalid_argument("truncated classifier archive");std::uint64_t n=0;for(int i=0;i<8;++i) n|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;if(n>max) throw std::invalid_argument("invalid classifier archive integer");return n;}
    double real() {const auto bits=integer();double n;std::memcpy(&n,&bits,8);if(!std::isfinite(n)) throw std::invalid_argument("nonfinite classifier archive value");return n;}
    std::string text(std::size_t max) {const auto n=integer(max);if(n>left) throw std::invalid_argument("truncated classifier archive text");std::string s(reinterpret_cast<const char*>(p),static_cast<std::size_t>(n));p+=n;left-=n;detail::bounded_utf8(s,max);return s;}
    std::pair<const std::uint8_t*,std::size_t> view(std::size_t max=limit) {const auto n=integer(max);if(n>left) throw std::invalid_argument("truncated classifier archive blob");const auto* start=p;p+=n;left-=n;return {start,static_cast<std::size_t>(n)};}
    std::vector<std::uint8_t> blob(std::size_t max=limit) {const auto v=view(max);return {v.first,v.first+v.second};}
    Roi roi() {const auto v=view(32u*1024*1024);return GeometryArchiveCodec::decode(v.first,v.second);}
    void finish() {if(left) throw std::invalid_argument("trailing classifier archive bytes");}
};
void hash_text(Sha256& s,const std::string& v) {s.integer(v.size());s.update(v.data(),v.size());}
std::uint32_t bits(float f) {std::uint32_t b;std::memcpy(&b,&f,4);return b;}
std::string table_digest(const ObjectFeatureTable& t) {
    Sha256 hash;const std::string domain="qpx.object-features/1";hash.update(domain.data(),domain.size());hash.integer(t.object_ids().size());hash.integer(t.features().size());
    for(const auto& f:t.features()) {hash_text(hash,f.name);hash_text(hash,f.unit);}
    for(std::size_t i=0;i<t.object_ids().size();++i) {hash.integer(t.object_ids()[i]);hash.integer(t.object_revisions()[i]);hash.integer(static_cast<std::uint64_t>(t.object_kinds()[i]));hash_text(hash,t.class_names()[i]);}
    for(auto f:t.values()) hash.integer(bits(f));return hash.finish();
}
void training_identity(const ObjectFeatureTable& table,const ForestModel& model) {
    std::size_t rows=0;std::vector<std::uint32_t> labels;std::vector<std::size_t> counts(model.classes().size());
    for(const auto& label:table.class_names()) if(!label.empty()) {
        const auto found=std::lower_bound(model.classes().begin(),model.classes().end(),label);
        if(found==model.classes().end() || *found!=label) throw std::invalid_argument("training archive class does not belong to model");
        const auto index=static_cast<std::uint32_t>(found-model.classes().begin());labels.push_back(index);++counts[index];++rows;
    }
    if(rows!=model.training_rows() || std::any_of(counts.begin(),counts.end(),[](auto n){return n<2;})) throw std::invalid_argument("training archive cohort does not match model");
    Sha256 hash;const std::string domain="qpx.forest.training/1";hash.update(domain.data(),domain.size());hash.integer(rows);hash.integer(table.features().size());hash.integer(model.classes().size());
    for(const auto& f:table.features()) {hash_text(hash,f.name);hash_text(hash,f.unit);}for(const auto& name:model.classes()) hash_text(hash,name);
    const auto cols=table.features().size();for(std::size_t row=0;row<table.object_ids().size();++row) if(!table.class_names()[row].empty()) for(std::size_t c=0;c<cols;++c) hash.integer(bits(table.values()[row*cols+c]));
    for(auto label:labels) hash.integer(label);if(hash.finish()!=model.training_sha256()) throw std::invalid_argument("training archive numeric identity mismatch");
}
void geometry(const Roi& roi,const ImageMetadata& m) {
    const auto b=roi.bounds();if(!roi.is_area() || b.x<0 || b.y<0 || static_cast<long double>(b.x)+b.width>m.width || static_cast<long double>(b.y)+b.height>m.height) throw std::invalid_argument("classifier archive geometry exceeds its source image");
}
void schema(const ObjectFeatureOptions& options,bool physical,const ForestModel& model) {
    const auto expected=ObjectFeatureOptions::from_model(model);
    if(expected.calibrated!=physical || expected.nucleus_shape!=options.nucleus_shape || expected.cell_rgb!=options.cell_rgb || expected.cytoplasm_rgb!=options.cytoplasm_rgb) throw std::invalid_argument("classifier archive feature preset mismatch");
}
void source_write(Writer& w,const ObjectCellFeatureSource& s,bool extended) {
    if(s.scientific_grid || s.grid_parameters) throw std::invalid_argument("grid classifier sources require QCA5 typed metadata");
    s.parameters.validate();if(s.algorithm!=cell_detail::algorithm_id(s.parameters.algorithm)) throw std::invalid_argument("classifier cell algorithm/recipe mismatch");
    detail::bounded_utf8(s.producer,256);detail::bounded_utf8(s.algorithm,256);detail::bounded_utf8(s.input_sha256,64);
    w.integer(s.record_id);w.integer(s.object_id);w.integer(s.object_revision);w.integer(s.document_revision);w.integer(s.cell_count);w.text(s.producer);w.text(s.algorithm);w.text(s.input_sha256);w.blob(GeometryArchiveCodec::encode(s.roi));w.integer(s.level);w.real(s.downsample);
    const auto& p=s.parameters;w.integer(static_cast<std::uint64_t>(p.feature));for(auto v:{p.threshold,p.sigma,p.min_peak_radius,p.min_seed_distance,p.cell_expansion}) w.real(v);
    for(auto v:{p.min_nucleus_pixels,p.max_nucleus_pixels,std::uint64_t(p.connectivity),std::uint64_t(p.level+1),std::uint64_t(p.tile_size),std::uint64_t(p.max_cells),std::uint64_t(p.work_bytes),std::uint64_t(p.contours.clip_to_roi),std::uint64_t(p.contours.max_components),std::uint64_t(p.contours.max_edges),std::uint64_t(p.contours.max_vertices),std::uint64_t(p.contours.work_bytes)}) w.integer(v);
    if(extended) {w.integer(static_cast<std::uint64_t>(p.algorithm));w.real(p.log_prominence);w.integer(p.context_padding);}
    for(auto v:s.white) w.real(v);for(const auto& stain:s.stains) for(auto v:stain) w.real(v);w.integer(bool(s.provenance));if(s.provenance) w.blob(ProvenanceCodec::encode(*s.provenance));
}
std::shared_ptr<const ObjectCellFeatureSource> source_read(Reader& r,const ImageMetadata& m,unsigned version) {
    auto s=std::make_shared<ObjectCellFeatureSource>();s->record_id=r.integer();s->object_id=r.integer();s->object_revision=r.integer();s->document_revision=r.integer();s->cell_count=r.integer(1000000);s->producer=r.text(256);s->algorithm=r.text(256);s->input_sha256=r.text(64);s->roi=r.roi();s->level=static_cast<std::int32_t>(r.integer(65535));s->downsample=r.real();
    auto& p=s->parameters;p.feature=static_cast<TissueFeature>(r.integer(3));p.threshold=r.real();p.sigma=r.real();p.min_peak_radius=r.real();p.min_seed_distance=r.real();p.cell_expansion=r.real();p.min_nucleus_pixels=r.integer();p.max_nucleus_pixels=r.integer();p.connectivity=static_cast<int>(r.integer(8));p.level=static_cast<std::int32_t>(r.integer(65536))-1;p.tile_size=r.integer();p.max_cells=r.integer();p.work_bytes=r.integer();p.contours.clip_to_roi=r.integer(1)!=0;p.contours.max_components=r.integer();p.contours.max_edges=r.integer();p.contours.max_vertices=r.integer();p.contours.work_bytes=r.integer();p.validate();
    if(version>=2) {p.algorithm=static_cast<CellAlgorithm>(r.integer(version-1));p.log_prominence=r.real();p.context_padding=static_cast<std::uint32_t>(r.integer(4096));p.validate();}
    for(auto& v:s->white) v=r.real();for(auto& stain:s->stains) for(auto& v:stain) v=r.real();StainTransform::validate_normalized(s->stains,s->white);
    if(r.integer(1)) {const auto v=r.view(4u*1024*1024);s->provenance=ProvenanceCodec::decode(v.first,v.second);}
    if(!s->record_id || !s->object_id || !s->object_revision || s->object_revision>s->document_revision || s->document_revision>=s->record_id || !s->cell_count || s->cell_count>s->parameters.max_cells || s->producer.empty() || s->algorithm!=cell_detail::algorithm_id(s->parameters.algorithm) || s->input_sha256.size()!=64 || s->input_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos || std::size_t(s->level)>=m.levels.size() || s->downsample!=m.levels[s->level].downsample) throw std::invalid_argument("invalid classifier cell source manifest");
    geometry(s->roi,m);if(s->provenance) {const auto& a=*s->provenance;if(a.object_id!=s->object_id || a.object_revision!=s->object_revision || a.document_revision!=s->document_revision || a.source.width!=m.width || a.source.height!=m.height) throw std::invalid_argument("classifier cell source provenance mismatch");}return s;
}
auto resolved(const ResolvedGridCellParameters& p) {return std::make_tuple(p.step_x,p.step_y,p.sigma_x,p.sigma_y,p.min_nucleus_samples,p.max_nucleus_samples);}
void grid_source_validate(const ObjectCellFeatureSource& s,const ImageMetadata& m,const std::atomic<bool>* cancelled) {
    if(!s.scientific_grid || !s.grid_parameters || !s.provenance || s.level!=0 || !std::isnan(s.downsample) || s.parameters.algorithm!=CellAlgorithm::GridLogWatershedBodies)
        throw std::invalid_argument("classifier grid source is missing typed metadata");
    const auto& g=*s.scientific_grid;const auto& p=*s.grid_parameters;const auto& a=*s.provenance;p.validate();a.validate();
    detail::bounded_utf8(s.producer,256);detail::bounded_utf8(s.algorithm,256);detail::bounded_utf8(s.input_sha256,64);
    if(!s.record_id || !s.object_id || !s.object_revision || s.object_revision>s.document_revision || s.document_revision>=s.record_id || !s.cell_count ||
       s.cell_count>p.max_cells || s.producer.empty() || s.algorithm!="qpx.grid-log-watershed-bodies/1" || s.input_sha256.size()!=64 || s.input_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos ||
       a.object_id!=s.object_id || a.object_revision!=s.object_revision || a.document_revision!=s.document_revision || a.nucleus ||
       g.image_width()!=m.width || g.image_height()!=m.height || a.source.width!=m.width || a.source.height!=m.height)
        throw std::invalid_argument("invalid classifier grid source identity");
    geometry(s.roi,m);StainTransform::validate_normalized(s.stains,s.white);
    const auto q=resolve_grid_cell_parameters(g,p,a);
    if(resolved(q)!=resolved(s.resolved_grid_parameters) || s.cell_count>std::uint64_t(g.x().samples())*g.y().samples()/q.min_nucleus_samples)
        throw std::invalid_argument("classifier grid source has inconsistent resolved parameters or cell count");
    grid_cell_work_bytes(g,p,q,s.roi,a,a.source,cancelled);
}
void grid_source_write(Writer& w,const ObjectCellFeatureSource& s,const std::atomic<bool>* cancelled) {
    if(!s.provenance) throw std::invalid_argument("classifier grid source has no provenance");grid_source_validate(s,s.provenance->source,cancelled);
    w.integer(s.record_id);w.integer(s.object_id);w.integer(s.object_revision);w.integer(s.document_revision);w.integer(s.cell_count);
    w.text(s.producer);w.text(s.algorithm);w.text(s.input_sha256);w.blob(GeometryArchiveCodec::encode(s.roi));
    cell_detail::grid_parameters_write(w,*s.grid_parameters);const auto& g=*s.scientific_grid;
    for(auto v:{g.image_width(),g.image_height(),g.x().origin(),g.y().origin(),g.x().extent(),g.y().extent(),g.x().samples(),g.y().samples(),g.x().context(),g.y().context()}) w.integer(std::uint64_t(v));
    w.blob(ProvenanceCodec::encode(*s.provenance));for(auto v:s.white) w.real(v);for(const auto& row:s.stains) for(auto v:row) w.real(v);
    const auto& q=s.resolved_grid_parameters;for(auto v:{q.step_x,q.step_y,q.sigma_x,q.sigma_y}) w.real(v);w.integer(q.min_nucleus_samples);w.integer(q.max_nucleus_samples);
}
std::shared_ptr<const ObjectCellFeatureSource> grid_source_read(Reader& r,const ImageMetadata& m,const std::atomic<bool>* cancelled) {
    auto s=std::make_shared<ObjectCellFeatureSource>();s->record_id=r.integer();s->object_id=r.integer();s->object_revision=r.integer();s->document_revision=r.integer();s->cell_count=r.integer(1000000);
    s->producer=r.text(256);s->algorithm=r.text(256);s->input_sha256=r.text(64);s->roi=r.roi();s->grid_parameters=cell_detail::grid_parameters_read(r);
    std::array<std::int64_t,10> values;for(auto& v:values) {const auto bits=r.integer();std::memcpy(&v,&bits,8);}
    s->scientific_grid=ScientificGrid(values[0],values[1],{values[2],values[4],values[6],values[8]},{values[3],values[5],values[7],values[9]},s->grid_parameters->work_bytes);
    const auto provenance=r.view(4u*1024*1024);s->provenance=ProvenanceCodec::decode(provenance.first,provenance.second);
    for(auto& v:s->white) v=r.real();for(auto& row:s->stains) for(auto& v:row) v=r.real();
    auto& q=s->resolved_grid_parameters;q.step_x=r.real();q.step_y=r.real();q.sigma_x=r.real();q.sigma_y=r.real();q.min_nucleus_samples=r.integer();q.max_nucleus_samples=r.integer();
    s->parameters.algorithm=CellAlgorithm::GridLogWatershedBodies;s->downsample=std::numeric_limits<double>::quiet_NaN();grid_source_validate(*s,m,cancelled);return s;
}
void cell_write(Writer& w,const CellFeature& c) {
    w.integer(c.nucleus_pixels);w.integer(c.cell_pixels);for(const auto& vector:{c.cell_rgb,c.nucleus_rgb,c.cell_channels,c.nucleus_channels}) for(auto v:vector) w.real(v);
    w.integer(bool(c.cytoplasm_rgb));if(c.cytoplasm_rgb) {for(auto v:*c.cytoplasm_rgb) w.real(v);for(auto v:*c.cytoplasm_channels) w.real(v);}
}
CellFeature cell_read(Reader& r,bool cytoplasm) {
    CellFeature c;c.nucleus_pixels=r.integer();c.cell_pixels=r.integer();for(auto* vector:{&c.cell_rgb,&c.nucleus_rgb,&c.cell_channels,&c.nucleus_channels}) for(auto& v:*vector) v=r.real();
    if(r.integer(1)) {c.cytoplasm_rgb=ColorVector{};c.cytoplasm_channels=ColorVector{};for(auto& v:*c.cytoplasm_rgb) v=r.real();for(auto& v:*c.cytoplasm_channels) v=r.real();}
    if(!c.nucleus_pixels || c.cell_pixels<c.nucleus_pixels || bool(c.cytoplasm_rgb)!=(c.cell_pixels>c.nucleus_pixels) || (cytoplasm && !c.cytoplasm_rgb)) throw std::invalid_argument("invalid classifier cell measurements");
    for(const auto& vector:{c.cell_rgb,c.nucleus_rgb}) for(auto v:vector) if(v<0 || v>255) throw std::invalid_argument("classifier RGB measurement exceeds device range");
    if(c.cytoplasm_rgb) for(auto v:*c.cytoplasm_rgb) if(v<0 || v>255) throw std::invalid_argument("classifier cytoplasm measurement exceeds device range");return c;
}
void record_metadata(const ObjectClassifierRecord& r) {
    detail::bounded_utf8(r.name,256);if(r.name.empty() || !r.id || !r.archive || r.archive->table()->source_anchor()->document_revision>=r.id || !std::isfinite(r.minimum_vote_fraction) || r.minimum_vote_fraction<0 || r.minimum_vote_fraction>1) throw std::invalid_argument("invalid classifier record metadata");
    if(!r.applied) {if(!r.changed_objects.empty() || r.minimum_vote_fraction!=0) throw std::invalid_argument("unapplied classifier record has class changes");return;}
    const auto result=r.archive->result();if(!result) throw std::invalid_argument("training record cannot contain applied predictions");
    std::size_t changed=0;const auto& table=*result->table();
    for(std::size_t i=0;i<table.object_ids().size();++i) {const auto& prediction=result->predictions().at(i);const auto& label=result->model()->classes().at(prediction.class_index);
        if(prediction.vote_fraction>=r.minimum_vote_fraction && table.class_names()[i]!=label) {if(changed>=r.changed_objects.size() || r.changed_objects[changed++]!=table.object_ids()[i]) throw std::invalid_argument("classifier record changed IDs mismatch");}}
    if(changed!=r.changed_objects.size()) throw std::invalid_argument("classifier record has extra changed IDs");
}
}
std::shared_ptr<const ObjectClassifierArchive> archive_object_classifier(std::shared_ptr<const ObjectFeatureTable> table,const ForestModel& model,std::shared_ptr<const ObjectClassificationResult> result,const std::atomic<bool>* cancelled) {
    return ObjectClassifierArchiveCodec::prepare(std::move(table),model,std::move(result),cancelled);
}
std::shared_ptr<const ObjectClassifierArchive> ObjectClassifierArchiveCodec::prepare(std::shared_ptr<const ObjectFeatureTable> table,const ForestModel& model,std::shared_ptr<const ObjectClassificationResult> result,const std::atomic<bool>* cancelled) {
    stopped(cancelled);if(!table || !table->snapshot_ || !table->anchor_) throw std::invalid_argument("classifier archive requires a feature snapshot");
    const auto& data=*table->snapshot_;schema(data.options,data.physical,model);
    if(table->features().size()!=model.features().size()) throw std::invalid_argument("classifier archive model schema mismatch");
    for(std::size_t i=0;i<model.features().size();++i) if(table->features()[i].name!=model.features()[i].name || table->features()[i].unit!=model.features()[i].unit) throw std::invalid_argument("classifier archive model schema mismatch");
    if(result) {if(result->table()!=table || result->model_sha256()!=model.sha256()) throw std::invalid_argument("prediction archive result/model mismatch");}
    else training_identity(*table,model);
    const bool grid=std::any_of(data.origins.begin(),data.origins.end(),[](const auto& origin) {return bool(origin.source->scientific_grid);});
    const bool extended=std::any_of(data.origins.begin(),data.origins.end(),[](const auto& origin) {return origin.source->parameters.algorithm!=CellAlgorithm::DistanceWatershed;});
    const bool bodies=std::any_of(data.origins.begin(),data.origins.end(),[](const auto& origin) {return origin.source->parameters.algorithm==CellAlgorithm::LogWatershedBodies;});
    const bool sampled=std::any_of(data.origins.begin(),data.origins.end(),[](const auto& origin) {return origin.source->parameters.algorithm==CellAlgorithm::SampledLogWatershedBodies;});
    Writer w(grid?"QCA\5":(sampled?"QCA\4":(bodies?"QCA\3":(extended?"QCA\2":"QCA\1"))));w.integer(bool(result));w.blob(model.bytes());w.blob(ProvenanceCodec::encode(*data.anchor));
    w.integer(std::uint64_t(data.options.calibrated) | (std::uint64_t(data.options.nucleus_shape)<<1) | (std::uint64_t(data.options.cell_rgb)<<2) | (std::uint64_t(data.options.cytoplasm_rgb)<<3));w.integer(data.options.snapshot_bytes);w.integer(data.physical);w.real(data.sx);w.real(data.sy);w.integer(data.revision);w.integer(data.next_id);
    w.integer(data.rows.size());
    for(std::size_t i=0;i<data.rows.size();++i) {stopped(cancelled);const auto& row=data.rows[i];w.integer(row.id);w.integer(row.revision);w.integer(static_cast<std::uint64_t>(row.kind));w.text(row.label);w.blob(GeometryArchiveCodec::encode(row.body));w.integer(bool(row.nucleus));if(row.nucleus) w.blob(GeometryArchiveCodec::encode(*row.nucleus));
        if(data.options.cell_rgb) {cell_write(w,data.cells[i]);w.integer(data.origins[i].record_id);w.integer(data.origins[i].label);}}
    std::map<std::uint64_t,std::shared_ptr<const ObjectCellFeatureSource>> origins;for(const auto& o:data.origins) origins.emplace(o.record_id,o.source);
    w.integer(origins.size());for(const auto& [id,p]:origins) {stopped(cancelled);if(grid) w.integer(bool(p->scientific_grid));if(p->scientific_grid) grid_source_write(w,*p,cancelled);else source_write(w,*p,extended);}
    w.text(table->sha256());w.integer(table->values().size());for(std::size_t i=0;i<table->values().size();++i) {if((i&511)==0) stopped(cancelled);w.integer(bits(table->values()[i]));}
    if(result) w.real(result->elapsed_ms());w.finish();if(w.b.size()>artifact_limit) throw std::length_error("classifier artifact exceeds 128 MiB");stopped(cancelled);
    auto out=std::shared_ptr<ObjectClassifierArchive>(new ObjectClassifierArchive());out->table_=std::move(table);out->model_=std::make_shared<const ForestModel>(model);out->result_=std::move(result);out->bytes_=std::move(w.b);out->sha256_=digest(out->bytes_.data(),out->bytes_.size());
    out->payload_bytes_=sizeof(ObjectClassifierArchive)+out->bytes_.capacity()+data.bytes+out->table_->values().capacity()*sizeof(float)+data.rows.size()*(sizeof(std::string)+3*sizeof(std::uint64_t)+sizeof(ObjectKind)+256)+2*model.bytes().size();
    if(out->result_) for(const auto& prediction:out->result_->predictions()) out->payload_bytes_+=sizeof(ClassifierPrediction)+prediction.votes.capacity()*sizeof(std::uint32_t);
    if(out->payload_bytes_>256u*1024*1024) throw std::length_error("classifier archive exceeds 256 MiB retained payload");return out;
}
std::size_t ObjectCellFeatureSource::payload_bytes() const {return sizeof(ObjectCellFeatureSource)+128+producer.capacity()+algorithm.capacity()+input_sha256.capacity()+roi.payload_bytes()+(provenance?provenance->payload_bytes():0);}
std::size_t ObjectClassifierRecord::payload_bytes() const {if(!archive) throw std::invalid_argument("classifier record has no archive");return sizeof(ObjectClassifierRecord)+128+name.capacity()+changed_objects.capacity()*sizeof(std::uint64_t)+archive->payload_bytes();}
std::vector<std::uint8_t> ObjectClassifierArchiveCodec::encode(const ObjectClassifierRecord& record,const std::atomic<bool>* cancelled) {
    stopped(cancelled);record_metadata(record);Writer w("QCR\1");w.integer(record.id);w.text(record.name);w.integer(record.applied);w.real(record.minimum_vote_fraction);w.integer(record.changed_objects.size());for(auto id:record.changed_objects) w.integer(id);w.blob(record.archive->bytes_);w.finish();stopped(cancelled);return std::move(w.b);
}
std::shared_ptr<const ObjectClassifierRecord> ObjectClassifierArchiveCodec::decode(const std::uint8_t* bytes,std::size_t size,const ImageMetadata& metadata,const std::atomic<bool>* cancelled) {
    stopped(cancelled);Reader record_reader(bytes,size,"QCR\1");auto record=std::make_shared<ObjectClassifierRecord>();record->id=record_reader.integer();record->name=record_reader.text(256);record->applied=record_reader.integer(1)!=0;record->minimum_vote_fraction=record_reader.real();const auto changed=record_reader.integer(100000);if(changed>record_reader.left/8) throw std::invalid_argument("truncated changed classifier IDs");record->changed_objects.reserve(changed);for(std::size_t i=0;i<changed;++i) record->changed_objects.push_back(record_reader.integer());
    const auto artifact_view=record_reader.view(artifact_limit);record_reader.finish();if(artifact_view.second<4) throw std::invalid_argument("truncated classifier artifact");const auto version=artifact_view.first[3];if(version<1 || version>5) throw std::invalid_argument("unsupported classifier archive version");Reader r(artifact_view.first,artifact_view.second,version==5?"QCA\5":(version==4?"QCA\4":(version==3?"QCA\3":(version==2?"QCA\2":"QCA\1"))));const bool prediction=r.integer(1)!=0;auto model=ForestModel::from_bytes(r.blob(64u*1024*1024));auto data=std::make_shared<ObjectFeatureRequest::Data>();
    const auto anchor=r.view(4u*1024*1024);data->anchor=ProvenanceCodec::decode(anchor.first,anchor.second);
    if(data->anchor->source.width!=metadata.width || data->anchor->source.height!=metadata.height) throw std::invalid_argument("classifier archive source dimensions mismatch");
    const auto flags=r.integer(15);data->options.calibrated=flags&1;data->options.nucleus_shape=flags&2;data->options.cell_rgb=flags&4;data->options.cytoplasm_rgb=flags&8;data->options.snapshot_bytes=static_cast<std::size_t>(r.integer(256u*1024*1024));data->options.validate();data->physical=r.integer(1)!=0;data->sx=r.real();data->sy=r.real();data->revision=r.integer();data->next_id=r.integer();if(!data->next_id) throw std::invalid_argument("invalid classifier source ID namespace");schema(data->options,data->physical,model);
    const auto& p=*data->anchor;const bool physical=data->options.calibrated && p.effective_pixel_width_um && p.effective_pixel_height_um;
    if(data->revision!=p.document_revision || data->physical!=physical || data->sx!=(physical?*p.effective_pixel_width_um:1) || data->sy!=(physical?*p.effective_pixel_height_um:1)) throw std::invalid_argument("classifier archive calibration/revision mismatch");
    const auto count=r.integer(100000);if(!count || count>r.left/64) throw std::invalid_argument("invalid classifier archive row count");
    auto charge=[&](std::size_t n) {if(n>data->options.snapshot_bytes-data->bytes) throw std::length_error("classifier feature archive exceeds snapshot budget");data->bytes+=n;};
    charge(sizeof(ObjectFeatureRequest::Data)+count*sizeof(ObjectFeatureRequest::Data::Row)+data->anchor->payload_bytes());data->rows.reserve(count);if(data->options.cell_rgb) {charge(count*(sizeof(CellFeature)+sizeof(ObjectFeatureRequest::Data::Origin)));data->cells.reserve(count);data->origins.reserve(count);}
    for(std::size_t i=0;i<count;++i) {stopped(cancelled);const auto id=r.integer(),revision=r.integer();const auto kind=static_cast<ObjectKind>(r.integer(2));auto label=r.text(256);auto body=r.roi();std::optional<Roi> nucleus;if(r.integer(1)) nucleus=r.roi();
        if(!id || id>=data->next_id || !revision || revision>data->revision || (!data->rows.empty() && id<=data->rows.back().id) || ((data->options.nucleus_shape || data->options.cell_rgb) && !nucleus) || (!data->options.nucleus_shape && !data->options.cell_rgb && nucleus)) throw std::invalid_argument("invalid classifier archive row identity");geometry(body,metadata);if(nucleus) geometry(*nucleus,metadata);charge(label.size()+body.payload_bytes()+(nucleus?nucleus->payload_bytes():0));data->rows.push_back({id,revision,std::move(label),std::move(body),std::move(nucleus),kind});
        if(data->options.cell_rgb) {if(kind!=ObjectKind::Cell) throw std::invalid_argument("RGB archive requires cell rows");auto cell=cell_read(r,data->options.cytoplasm_rgb);const auto record_id=r.integer();cell.label=static_cast<std::uint32_t>(r.integer(1000000));if(!record_id || record_id>=revision || !cell.label) throw std::invalid_argument("invalid classifier cell feature origin");data->origins.push_back({record_id,cell.label,{}});data->cells.push_back(std::move(cell));}}
    if(data->rows.front().id!=p.object_id || data->rows.front().revision!=p.object_revision || p.nucleus) throw std::invalid_argument("classifier source anchor does not identify its first row");
    std::map<std::uint64_t,std::shared_ptr<const ObjectCellFeatureSource>> origins;const auto origin_count=r.integer(count);
    for(std::size_t i=0;i<origin_count;++i) {stopped(cancelled);const bool grid=version==5 && r.integer(1);auto source=grid?grid_source_read(r,metadata,cancelled):source_read(r,metadata,std::min<unsigned>(version,4));const auto id=source->record_id;if(source->object_id>=data->next_id) throw std::invalid_argument("classifier cell source exceeds captured ID namespace");charge(source->payload_bytes());if(!origins.emplace(id,std::move(source)).second) throw std::invalid_argument("duplicate classifier feature source manifest");}
    std::map<std::uint64_t,bool> used;for(std::size_t row=0;row<data->origins.size();++row) {auto& origin=data->origins[row];const auto found=origins.find(origin.record_id);if(found==origins.end()) throw std::invalid_argument("missing classifier feature source manifest");origin.source=found->second;if(origin.label>origin.source->cell_count) throw std::invalid_argument("classifier cell label exceeds source run");
        if(origin.source->scientific_grid) {
            const auto& g=*origin.source->scientific_grid;const auto& q=origin.source->resolved_grid_parameters;const auto& f=data->cells[row];
            if(f.cell_pixels>std::uint64_t(g.x().samples())*g.y().samples() || f.nucleus_pixels<q.min_nucleus_samples || (q.max_nucleus_samples && f.nucleus_pixels>q.max_nucleus_samples))
                throw std::invalid_argument("classifier cell counts exceed their scientific grid or area cutoffs");
        } else {
            const auto& level=metadata.levels.at(origin.source->level);const auto width=static_cast<std::uint64_t>(level.width),height=static_cast<std::uint64_t>(level.height);
            if(width<=std::numeric_limits<std::uint64_t>::max()/height && data->cells[row].cell_pixels>width*height) throw std::invalid_argument("classifier cell pixel count exceeds source level");
        }
        used.emplace(origin.record_id,true);}if(used.size()!=origins.size()) throw std::invalid_argument("orphaned classifier feature source manifest");
    auto table=std::shared_ptr<ObjectFeatureTable>(new ObjectFeatureTable());table->snapshot_=data;table->anchor_=data->anchor;table->features_=model.features();const auto expected_digest=r.text(64);const auto values=r.integer(100000*128);if(values!=count*model.features().size() || values>r.left/8) throw std::invalid_argument("invalid classifier feature matrix dimensions");table->values_.reserve(values);
    for(std::size_t i=0;i<values;++i) {if((i&511)==0) stopped(cancelled);const auto value_bits=static_cast<std::uint32_t>(r.integer(std::numeric_limits<std::uint32_t>::max()));float value;std::memcpy(&value,&value_bits,4);if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max()/4) throw std::invalid_argument("invalid archived classifier feature");table->values_.push_back(value);}
    for(const auto& row:data->rows) {table->ids_.push_back(row.id);table->revisions_.push_back(row.revision);table->kinds_.push_back(row.kind);table->labels_.push_back(row.label);}table->digest_=table_digest(*table);if(table->digest_!=expected_digest) throw std::invalid_argument("classifier feature table checksum mismatch");
    std::shared_ptr<ObjectClassificationResult> result;if(prediction) {const auto elapsed=r.real();if(elapsed<0) throw std::invalid_argument("invalid classifier prediction duration");result=std::shared_ptr<ObjectClassificationResult>(new ObjectClassificationResult());result->table_=table;result->model_=std::make_shared<const ForestModel>(model);result->model_sha256_=model.sha256();result->elapsed_ms_=elapsed;
        const auto cols=table->features().size();for(std::size_t row=0;row<count;row+=512) {stopped(cancelled);const auto end=std::min<std::size_t>(count,row+512);std::vector<float> matrix(table->values_.begin()+row*cols,table->values_.begin()+end*cols);auto predictions=model.predict(matrix,table->features());result->predictions_.insert(result->predictions_.end(),std::make_move_iterator(predictions.begin()),std::make_move_iterator(predictions.end()));}}
    r.finish();record->archive=prepare(std::move(table),model,std::move(result),cancelled);
    // Canonical re-encoding detects alternate encodings and unvalidated fields.
    if(record->archive->bytes_.size()!=artifact_view.second || !std::equal(record->archive->bytes_.begin(),record->archive->bytes_.end(),artifact_view.first)) throw std::invalid_argument("noncanonical classifier archive");record_metadata(*record);return record;
}
}
