#include "tma_internal.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace qpx {
namespace {
void label(const std::string& value,std::size_t limit) {
    detail::bounded_utf8(value,limit);if(value.empty()) throw std::invalid_argument("TMA labels must not be empty");
}
void axis(const std::vector<std::string>& values,std::size_t count) {
    if(values.size()!=count) throw std::invalid_argument("TMA axis label count does not match grid dimensions");
    std::set<std::string> seen;for(const auto& value:values) {label(value,128);if(!seen.insert(value).second) throw std::invalid_argument("TMA axis labels must be unique");}
}
std::string alphabet(std::size_t index) {
    std::string out;for(++index;index;index=(index-1)/26) out.insert(out.begin(),char('A'+(index-1)%26));return out;
}
std::vector<std::string> labels(std::size_t count,bool rows,const std::vector<std::string>& supplied) {
    if(!supplied.empty()) return supplied;std::vector<std::string> out;out.reserve(count);
    for(std::size_t i=0;i<count;++i) out.push_back(rows?alphabet(i):std::to_string(i+1));return out;
}
std::size_t position(const TmaCoreInfo& value) {return value.row()*value.grid_info()->columns()+value.column();}
bool structural(const std::shared_ptr<const TmaCoreInfo>& a,const std::shared_ptr<const TmaCoreInfo>& b) {
    if(bool(a)!=bool(b)) return true;return a && (a->grid_info()!=b->grid_info() || a->row()!=b->row() || a->column()!=b->column());
}
void add(detail::TmaIndex& index,const Annotation& annotation) {
    const auto& info=*annotation.tma_core;info.validate();
    if(annotation.kind!=ObjectKind::Annotation || annotation.geometry.kind()!=RoiKind::Ellipse) throw std::invalid_argument("TMA cores must be elliptical annotations");
    if(!index.info) {index.info=info.grid_info();index.info->validate();}
    else if(index.info!=info.grid_info() && !index.info->same_layout(*info.grid_info())) throw std::invalid_argument("all TMA cores must share one grid layout");
    if(!index.slots.emplace(position(info),annotation.id).second) throw std::invalid_argument("duplicate TMA grid position");
    if(index.slots.size()>10000) throw std::length_error("TMA grid exceeds 10000 cores");
}
}
void TmaGridOptions::validate() const {
    if(!rows || !columns || rows>512 || columns>512 || rows>10000/columns) throw std::invalid_argument("TMA grid must contain 1..10000 cores with at most 512 rows or columns");
    label(name,256);if(!row_labels.empty()) axis(row_labels,rows);if(!column_labels.empty()) axis(column_labels,columns);
    if(!std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0 || width>1e12 || height>1e12) throw std::invalid_argument("invalid TMA core dimensions");
    for(const auto p:{origin,row_step,column_step}) if(!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x)>1e15 || std::abs(p.y)>1e15) throw std::invalid_argument("invalid TMA grid coordinates");
    const auto r=std::hypot(row_step.x,row_step.y),c=std::hypot(column_step.x,column_step.y);
    if((rows>1 && !r) || (columns>1 && !c) || (rows>1 && columns>1 && std::abs(row_step.x*column_step.y-row_step.y*column_step.x)<=1e-12*r*c)) throw std::invalid_argument("TMA grid step vectors are degenerate");
    for(auto row:{std::size_t(0),rows-1}) for(auto column:{std::size_t(0),columns-1}) {
        const auto x=origin.x+row*row_step.x+column*column_step.x,y=origin.y+row*row_step.y+column*column_step.y;
        if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x)+width/2>1e15 || std::abs(y)+height/2>1e15) throw std::invalid_argument("TMA grid exceeds coordinate range");
    }
}
std::size_t TmaGridInfo::payload_bytes() const {
    auto n=sizeof(*this)+64+name_.capacity()+(rows_.capacity()+columns_.capacity())*sizeof(std::string);
    for(const auto* values:{&rows_,&columns_}) for(const auto& value:*values) n+=value.capacity();return n;
}
bool TmaGridInfo::same_layout(const TmaGridInfo& other) const {return name_==other.name_ && rows_==other.rows_ && columns_==other.columns_;}
void TmaGridInfo::validate() const {
    if(rows_.empty() || columns_.empty() || rows_.size()>512 || columns_.size()>512 || rows_.size()>10000/columns_.size()) throw std::invalid_argument("invalid TMA grid layout");
    label(name_,256);axis(rows_,rows_.size());axis(columns_,columns_.size());
}
std::size_t TmaCoreInfo::payload_bytes() const {return sizeof(*this)+64+case_.capacity();}
bool TmaCoreInfo::same_metadata(const TmaCoreInfo& b) const {
    return row_==b.row_ && column_==b.column_ && missing_==b.missing_ && case_==b.case_ && (grid_==b.grid_ || (grid_ && b.grid_ && grid_->same_layout(*b.grid_)));
}
void TmaCoreInfo::validate() const {
    if(!grid_ || row_>=grid_->rows() || column_>=grid_->columns()) throw std::invalid_argument("TMA core position exceeds grid");detail::bounded_utf8(case_,256);
}
std::shared_ptr<const TmaCoreInfo> TmaCoreInfo::with_missing(bool value) const {auto out=std::make_shared<TmaCoreInfo>(*this);out->missing_=value;return out;}
std::shared_ptr<const TmaCoreInfo> TmaCoreInfo::with_case_id(std::string value) const {auto out=std::make_shared<TmaCoreInfo>(*this);out->case_=std::move(value);out->validate();return out;}
std::optional<Annotation> TmaGrid::core(std::size_t row,std::size_t column) const {
    if(row>=info_->rows() || column>=info_->columns()) throw std::out_of_range("TMA core position exceeds grid");const auto key=row*info_->columns()+column;
    const auto found=std::lower_bound(cores_.begin(),cores_.end(),key,[](const auto& a,std::size_t b){return position(*a.tma_core)<b;});
    return found==cores_.end() || position(*found->tma_core)!=key?std::nullopt:std::optional<Annotation>(*found);
}
std::shared_ptr<const detail::TmaIndex> Document::prepare_tma(const ObjectPatch& before,const ObjectPatch& after) const {
    bool changed=false;for(const auto& [id,a]:after) {const auto& old=before.at(id);if(structural(old?old->tma_core:nullptr,a?a->tma_core:nullptr)) {changed=true;break;}}
    if(!changed) return tma_index_;
    auto next=std::make_shared<detail::TmaIndex>();if(tma_index_) next->slots=tma_index_->slots;
    for(const auto& [id,a]:before) if(a && a->tma_core) next->slots.erase(position(*a->tma_core));
    // First reserve each changed position. Revalidate layouts against the final
    // patch, permitting an atomic rename/relabel or a permutation of slots.
    for(const auto& [id,a]:after) if(a && a->tma_core) {a->tma_core->validate();if(!next->slots.emplace(position(*a->tma_core),id).second) throw std::invalid_argument("duplicate TMA grid position");}
    if(next->slots.empty()) return {};
    if(next->slots.size()>10000) throw std::length_error("TMA grid exceeds 10000 cores");
    std::set<const TmaGridInfo*> configurations;
    for(const auto& [slot,id]:next->slots) {
        const auto patch=after.find(id);const auto& a=patch==after.end()?find(id):*patch->second;const auto& info=*a.tma_core;
        if(!next->info) {next->info=info.grid_info();next->info->validate();}
        else if(next->info!=info.grid_info() && !next->info->same_layout(*info.grid_info())) throw std::invalid_argument("all TMA cores must share one grid layout");
        if(configurations.insert(info.grid_info().get()).second) next->grid_bytes+=info.grid_info()->payload_bytes();
        if(slot!=position(info)) throw std::logic_error("TMA index is inconsistent");
    }
    return next;
}
void Document::restore_tma_index() {
    auto index=std::make_shared<detail::TmaIndex>();std::set<const TmaGridInfo*> configurations;
    for(const auto& [id,a]:objects_) if(a.tma_core) {add(*index,a);if(configurations.insert(a.tma_core->grid_info().get()).second) index->grid_bytes+=a.tma_core->grid_info()->payload_bytes();}
    if(index->slots.empty()) {tma_index_.reset();tma_revision_=0;return;}tma_index_=std::move(index);tma_revision_=revision_;
}
std::size_t Document::tma_core_count() const {return tma_index_?tma_index_->slots.size():0;}
std::vector<std::uint64_t> Document::tma_core_ids(bool include_missing) const {
    std::vector<std::uint64_t> out;if(tma_index_) {out.reserve(tma_index_->slots.size());for(const auto& [slot,id]:tma_index_->slots) if(include_missing || !find(id).tma_core->missing()) out.push_back(id);}return out;
}
std::shared_ptr<const TmaGrid> Document::tma_grid(std::size_t limit) const {
    if(!limit || limit>256*1024*1024) throw std::invalid_argument("invalid TMA snapshot limit");if(!tma_index_) return {};
    auto out=std::shared_ptr<TmaGrid>(new TmaGrid());out->info_=tma_index_->info;out->revision_=revision_;out->bytes_=sizeof(TmaGrid)+tma_index_->grid_bytes;
    if(out->bytes_>limit) throw std::length_error("TMA snapshot exceeds byte limit");
    for(const auto& [slot,id]:tma_index_->slots) {const auto n=object_payload(find(id));if(n>limit-out->bytes_) throw std::length_error("TMA snapshot exceeds byte limit");out->bytes_+=n;}
    out->cores_.reserve(tma_index_->slots.size());for(const auto& [slot,id]:tma_index_->slots) out->cores_.push_back(find(id));return out;
}
std::shared_ptr<const TmaGridInfo> TmaCodec::grid(const TmaGridOptions& options) {
    options.validate();
    auto info=std::shared_ptr<TmaGridInfo>(new TmaGridInfo());info->name_=options.name;info->rows_=labels(options.rows,true,options.row_labels);info->columns_=labels(options.columns,false,options.column_labels);info->validate();
    return info;
}
std::vector<ObjectEdit> tma_grid_edits(const TmaGridOptions& options) {
    const auto info=TmaCodec::grid(options);
    std::vector<ObjectEdit> edits;edits.reserve(options.rows*options.columns);
    for(std::size_t row=0;row<options.rows;++row) for(std::size_t column=0;column<options.columns;++column) {
        const auto x=options.origin.x+row*options.row_step.x+column*options.column_step.x,y=options.origin.y+row*options.row_step.y+column*options.column_step.y;
        auto edit=ObjectEdit::create_geometry(Roi::ellipse({x-options.width/2,y-options.height/2,options.width,options.height}),info->row_labels()[row]+"-"+info->column_labels()[column]);
        edit.value.tma_core=TmaCodec::core(info,row,column,false,"");edits.push_back(std::move(edit));
    }
    return edits;
}
std::vector<std::uint64_t> Document::create_tma_grid(const TmaGridOptions& options) {
    if(tma_index_) throw std::invalid_argument("document already has a TMA grid; clear or edit it explicitly");return execute(tma_grid_edits(options),"Create TMA grid");
}
std::uint64_t Document::add_tma_core(std::size_t row,std::size_t column,Rect bounds,std::string case_id,bool missing) {
    if(!tma_index_) throw std::invalid_argument("document has no TMA grid");
    const auto info=TmaCodec::core(tma_index_->info,row,column,missing,std::move(case_id));
    if(tma_index_->slots.count(position(*info))) throw std::invalid_argument("TMA grid position is occupied");
    auto edit=ObjectEdit::create_geometry(Roi::ellipse(bounds),info->row_label()+"-"+info->column_label());edit.value.tma_core=info;return execute({edit},"Add TMA core").at(0);
}
void Document::translate_tma_grid(double dx,double dy,bool descendants) {
    if(!std::isfinite(dx) || !std::isfinite(dy)) throw std::invalid_argument("invalid TMA translation");if(!tma_index_) throw std::invalid_argument("document has no TMA grid");
    auto ids=tma_core_ids(true);std::set<std::uint64_t> seen(ids.begin(),ids.end());
    if(descendants) for(std::size_t i=0;i<ids.size();++i) for(auto child:children(ids[i])) if(seen.insert(child).second) ids.push_back(child);
    std::vector<ObjectEdit> edits;edits.reserve(ids.size());for(auto id:ids) {const auto a=find(id);auto edit=ObjectEdit::replace_geometry(id,a.geometry.translated(dx,dy),a.name,a.parent_id,a.class_name,a.kind,a.nucleus?std::optional<Roi>(a.nucleus->translated(dx,dy)):std::nullopt);edit.value.tma_core=a.tma_core;edits.push_back(std::move(edit));}execute(edits,"Move TMA grid");
}
void Document::set_tma_missing(const std::vector<std::uint64_t>& ids,bool missing) {
    std::vector<ObjectEdit> edits;std::set<std::uint64_t> seen;edits.reserve(ids.size());
    for(auto id:ids) if(seen.insert(id).second) {const auto& old=find(id);if(!old.tma_core) throw std::invalid_argument("object is not a TMA core");auto info=std::make_shared<TmaCoreInfo>(*old.tma_core);info->missing_=missing;edits.push_back(ObjectEdit::tma_metadata(id,std::move(info)));}
    execute(edits,"Set missing TMA cores");
}
void Document::set_tma_case_ids(const std::map<std::uint64_t,std::string>& values) {
    std::vector<ObjectEdit> edits;edits.reserve(values.size());for(const auto& [id,value]:values) {const auto& old=find(id);if(!old.tma_core) throw std::invalid_argument("object is not a TMA core");auto info=std::make_shared<TmaCoreInfo>(*old.tma_core);info->case_=value;info->validate();edits.push_back(ObjectEdit::tma_metadata(id,std::move(info)));}execute(edits,"Set TMA case IDs");
}
void Document::set_tma_labels(std::string name,const std::vector<std::string>& rows,const std::vector<std::string>& columns,bool rename) {
    if(!tma_index_) throw std::invalid_argument("document has no TMA grid");auto info=std::shared_ptr<TmaGridInfo>(new TmaGridInfo());const auto& previous=*tma_index_->info;
    info->name_=name.empty()?previous.name():std::move(name);info->rows_=rows.empty()?previous.row_labels():rows;info->columns_=columns.empty()?previous.column_labels():columns;info->validate();
    if(info->rows()!=previous.rows() || info->columns()!=previous.columns()) throw std::invalid_argument("relabeling cannot change TMA dimensions");
    const std::shared_ptr<const TmaGridInfo> shared=info->same_layout(previous)?tma_index_->info:info;std::vector<ObjectEdit> edits;edits.reserve(tma_core_count()*(rename?2:1));
    for(const auto& [slot,id]:tma_index_->slots) {auto a=find(id);auto core=std::make_shared<TmaCoreInfo>(*a.tma_core);core->grid_=shared;edits.push_back(ObjectEdit::tma_metadata(id,core));
        if(rename) {auto edit=ObjectEdit::replace_geometry(id,a.geometry,core->row_label()+"-"+core->column_label(),a.parent_id,a.class_name,a.kind,a.nucleus);edit.value.tma_core=core;edits.push_back(std::move(edit));}}
    execute(edits,"Relabel TMA grid");
}
void Document::clear_tma_grid(bool delete_cores) {
    if(!tma_index_) return;const auto ids=tma_core_ids(true);if(delete_cores) {remove_objects(ids);return;}
    std::vector<ObjectEdit> edits;edits.reserve(ids.size());for(auto id:ids) edits.push_back(ObjectEdit::tma_metadata(id,{}));execute(edits,"Clear TMA grid");
}
std::shared_ptr<const TmaCoreInfo> TmaCodec::core(std::shared_ptr<const TmaGridInfo> grid,std::size_t row,std::size_t column,bool missing,std::string case_id) {
    auto out=std::shared_ptr<TmaCoreInfo>(new TmaCoreInfo());out->grid_=std::move(grid);out->row_=row;out->column_=column;out->missing_=missing;out->case_=std::move(case_id);out->validate();return out;
}
std::vector<std::uint8_t> TmaCodec::encode(const TmaGridInfo& value) {
    value.validate();std::vector<std::uint8_t> out={'Q','T','G',1};
    auto number=[&](std::uint32_t n){for(int i=0;i<4;++i) out.push_back(std::uint8_t(n>>(8*i)));};
    auto text=[&](const std::string& s){number(static_cast<std::uint32_t>(s.size()));out.insert(out.end(),s.begin(),s.end());};
    text(value.name_);number(static_cast<std::uint32_t>(value.rows()));number(static_cast<std::uint32_t>(value.columns()));for(const auto* values:{&value.rows_,&value.columns_}) for(const auto& s:*values) text(s);
    Sha256 hash;hash.update(out.data(),out.size());const auto digest=hash.finish();out.insert(out.end(),digest.begin(),digest.end());return out;
}
std::shared_ptr<const TmaGridInfo> TmaCodec::decode(const void* data,std::size_t size) {
    const auto* bytes=static_cast<const std::uint8_t*>(data);if(!bytes || size<80 || size>256*1024 || std::memcmp(bytes,"QTG\1",4)) throw std::invalid_argument("invalid TMA grid encoding");
    Sha256 hash;hash.update(bytes,size-64);if(hash.finish()!=std::string(reinterpret_cast<const char*>(bytes+size-64),64)) throw std::invalid_argument("TMA grid checksum mismatch");std::size_t at=4;const auto end=size-64;
    auto number=[&] {if(at>end || end-at<4) throw std::invalid_argument("truncated TMA grid");std::uint32_t n=0;for(int i=0;i<4;++i) n|=std::uint32_t(bytes[at++])<<(8*i);return n;};
    auto text=[&](std::size_t maximum) {const auto n=number();if(n>maximum || at>end || n>end-at) throw std::invalid_argument("invalid TMA grid label length");std::string s(reinterpret_cast<const char*>(bytes+at),n);at+=n;return s;};
    auto out=std::shared_ptr<TmaGridInfo>(new TmaGridInfo());out->name_=text(256);const auto rows=number(),columns=number();if(!rows || !columns || rows>512 || columns>512 || rows>10000/columns) throw std::invalid_argument("invalid TMA grid dimensions");
    out->rows_.reserve(rows);out->columns_.reserve(columns);for(std::uint32_t i=0;i<rows;++i) out->rows_.push_back(text(128));for(std::uint32_t i=0;i<columns;++i) out->columns_.push_back(text(128));if(at!=end) throw std::invalid_argument("trailing TMA grid data");out->validate();return out;
}
}
