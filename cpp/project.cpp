#include "qupathxx/project.hpp"
#include "tissue_archive.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#ifdef QUPATHXX_HAS_PROJECT
#include <sqlite3.h>
#endif

namespace qpx {
namespace {
namespace fs=std::filesystem;
void text(const std::string& value,std::size_t limit=4096) {
    if(value.size()>limit || value.find('\0')!=std::string::npos) throw std::invalid_argument("invalid project text or path");
}
std::string absolute(const std::string& path) {
    text(path,32768); if(path.empty()) throw std::invalid_argument("project/image path must not be empty");
    return fs::absolute(fs::u8path(path)).lexically_normal().u8string();
}
std::string reader_name(const std::string& path,std::string reader) {
    if(reader=="auto") {
        auto ext=fs::u8path(path).extension().u8string();
        std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        reader=ext==".ppm"?"ppm":"openslide";
    }
    if(reader!="ppm" && reader!="openslide") throw std::invalid_argument("image reader must be auto, ppm or openslide");
    return reader;
}
std::shared_ptr<ImageServer> read_image(const std::string& path,const std::string& reader) {
    return reader=="ppm"?dense_image_server(Image::read_ppm(path)):open_slide(path);
}
void validate_metadata(const ImageMetadata& m) {
    if(m.pixel_type!=PixelType::UInt8 || m.layout!=PixelLayout::RGBA || m.width<=0 || m.height<=0 || m.levels.empty() || m.levels.size()>65536 ||
        m.levels.front().width!=m.width || m.levels.front().height!=m.height || m.levels.front().downsample!=1)
        throw std::invalid_argument("invalid project image metadata");
    text(m.vendor);
    for(const auto& l:m.levels) if(l.width<=0 || l.height<=0 || !std::isfinite(l.downsample) || l.downsample<1)
        throw std::invalid_argument("invalid project pyramid level");
    for(auto c:{m.pixel_width_um,m.pixel_height_um}) if(c && (!std::isfinite(*c) || *c<=0))
        throw std::invalid_argument("invalid project image calibration");
}
void compatible(const ImageMetadata& expected,const ImageMetadata& actual) {
    if(expected.width!=actual.width || expected.height!=actual.height || expected.levels.size()!=actual.levels.size() ||
        expected.pixel_type!=actual.pixel_type || expected.layout!=actual.layout)
        throw std::invalid_argument("relinked image dimensions or pyramid do not match project");
    for(std::size_t i=0;i<expected.levels.size();++i) {
        const auto& a=expected.levels[i]; const auto& b=actual.levels[i];
        if(a.width!=b.width || a.height!=b.height || a.downsample!=b.downsample)
            throw std::invalid_argument("relinked image pyramid does not match project");
    }
}
class LinkedServer final : public ImageServer {
    ImageMetadata metadata_;
    std::string path_,reader_;
    mutable std::shared_ptr<ImageServer> server_;
    mutable std::mutex mutex_;
    std::atomic<bool> closed_{false};
public:
    LinkedServer(ImageMetadata m,std::string path,std::string reader) : metadata_(std::move(m)),path_(std::move(path)),reader_(std::move(reader)) { validate_metadata(metadata_); }
    const ImageMetadata& metadata() const override { return metadata_; }
    PixelBuffer read_region(const RegionRequest& request) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closed_) throw std::runtime_error("image server is closed");
        if(!server_) { auto candidate=read_image(path_,reader_); compatible(metadata_,candidate->metadata()); server_=std::move(candidate); }
        return server_->read_region(request);
    }
    bool closed() const override { return closed_.load(); }
    void close() override { std::lock_guard<std::mutex> lock(mutex_); if(server_) server_->close(); closed_=true; }
};
void advance(std::uint64_t value) { if(value==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("project identity/revision exhausted"); }
}
Project::Project() {
    std::random_device random; constexpr char hex[]="0123456789abcdef";
    for(int i=0;i<32;++i) identity_.push_back(hex[random()&15]);
    instance_identity_=identity_;
}
std::unique_ptr<Project> Project::snapshot_for_save(std::size_t budget,std::size_t& bytes) const {
    bytes=0;
    auto add=[&](std::size_t count) { if(count>budget-bytes) throw std::length_error("project snapshot exceeds memory budget"); bytes+=count; };
    add(sizeof(Project)+path_.size()+identity_.size());
    for(const auto& [id,e]:entries_) {
        add(sizeof(Entry)+sizeof(Document)+128+e.info.name.size()+e.info.source_path.size()+e.info.reader.size());
        if(e.info.embedded) add(e.document->image().pixels().size());
        else add(sizeof(LinkedServer)+e.document->metadata().levels.size()*sizeof(ImageLevel)+e.document->metadata().vendor.size()+e.info.source_path.size()+e.info.reader.size());
        for(const auto& [key,a]:e.document->objects_) add(sizeof(Annotation)+64+a.name.size()+a.class_name.size()+a.geometry.payload_bytes()+(a.nucleus?a.nucleus->payload_bytes():0));
        for(const auto& [name,c]:e.document->classes_) add(sizeof(ObjectClass)+96+name.size()+c.name.size());
        add(e.document->analysis_bytes_);
        add(e.document->tissue_bytes_);
        add(e.document->tissue_blocks_.values.size()*(sizeof(Document::TissueBlock)+sizeof(std::shared_ptr<const Document::TissueBlock>)));
        add(e.document->analysis_blocks_.values.size()*(sizeof(Document::AnalysisBlock)+sizeof(std::shared_ptr<const Document::AnalysisBlock>)));
    }
    for(const auto& [id,v]:views_) add(sizeof(ProjectView)+64);
    auto snapshot=std::make_unique<Project>();
    snapshot->identity_=identity_; snapshot->path_=path_; snapshot->generation_=generation_;
    snapshot->next_id_=next_id_; snapshot->active_image_=active_image_; snapshot->revision_=revision_; snapshot->views_=views_;
    for(const auto& [id,e]:entries_) {
        auto doc=std::shared_ptr<Document>(new Document());
        // Save-only authority: no mutable indexes, selection or edit history enter
        // the worker. Immutable ROI storage and dense source pixels are shared.
        doc->server_=e.info.embedded?e.document->server_:std::make_shared<LinkedServer>(e.document->metadata(),e.info.source_path,e.info.reader);
        doc->objects_=e.document->objects_; doc->classes_=e.document->classes_;
        doc->next_id_=e.document->next_id_; doc->revision_=e.document->revision_;
        doc->pixel_width_um_=e.document->pixel_width_um_; doc->pixel_height_um_=e.document->pixel_height_um_;
        const auto& blocks=e.document->analysis_blocks_.values;
        doc->analysis_snapshot_.assign(blocks.begin(),blocks.end());
        for(const auto& block:blocks) if(!block->sealed) block->sealed=true;
        doc->analysis_snapshot_count_=e.document->analysis_.size();
        doc->analysis_bytes_=e.document->analysis_bytes_;
        const auto& masks=e.document->tissue_blocks_.values;
        doc->tissue_snapshot_.assign(masks.begin(),masks.end());
        for(const auto& block:masks) block->sealed=true;
        doc->tissue_snapshot_count_=e.document->tissue_.size();
        doc->tissue_bytes_=e.document->tissue_bytes_;
        snapshot->entries_.emplace(id,Entry{e.info,std::move(doc)});
    }
    return snapshot;
}
void Project::restore_recovery(const std::string& path,const std::string& base) {
    auto restored=recover(path,base);
    adopt_workspace(*restored);
}
void Project::adopt_workspace(Project& candidate) noexcept {
    entries_.swap(candidate.entries_); views_.swap(candidate.views_); saved_revisions_.swap(candidate.saved_revisions_);
    path_.swap(candidate.path_); identity_.swap(candidate.identity_); instance_identity_.swap(candidate.instance_identity_); recovery_source_.swap(candidate.recovery_source_);
    std::swap(next_id_,candidate.next_id_); std::swap(active_image_,candidate.active_image_); std::swap(generation_,candidate.generation_);
    std::swap(revision_,candidate.revision_); std::swap(saved_revision_,candidate.saved_revision_);
}
bool Project::can_retire_on_worker() const {
    return std::all_of(entries_.begin(),entries_.end(),[](const auto& value){return value.second.native_lifetime;});
}
const Project::Entry& Project::entry(std::uint64_t id) const {
    auto it=entries_.find(id); if(it==entries_.end()) throw std::out_of_range("project image does not exist"); return it->second;
}
std::shared_ptr<Document> Project::document(std::uint64_t id) const { return entry(id).document; }
std::vector<ProjectImage> Project::images() const {
    std::vector<ProjectImage> result; result.reserve(entries_.size());
    for(const auto& [id,e]:entries_) {
        auto info=e.info; std::error_code error;
        info.available=info.embedded || fs::is_regular_file(fs::u8path(info.source_path),error); result.push_back(std::move(info));
    }
    return result;
}
std::uint64_t Project::add_image(const std::string& path,std::string reader,std::string name) {
    auto source=absolute(path); reader=reader_name(source,std::move(reader));
    auto id=add_document(std::make_shared<Document>(read_image(source,reader)),std::move(name),source,reader);
    entries_.at(id).native_lifetime=true;
    return id;
}
std::uint64_t Project::add_document(std::shared_ptr<Document> doc,std::string name,std::string source,std::string reader) {
    if(!doc) throw std::invalid_argument("project requires a document");
    for(const auto& [id,e]:entries_) if(e.document.get()==doc.get()) throw std::invalid_argument("document already belongs to this project");
    advance(next_id_); advance(revision_); validate_metadata(doc->metadata());
    const bool embedded=source.empty();
    if(embedded) {
        const auto& image=doc->image();
        if(image.pixels().size()>256*1024*1024) throw std::length_error("embedded image exceeds 256 MiB; use a file link");
        reader="embedded";
    } else {
        source=absolute(source); reader=reader_name(source,std::move(reader));
        if(fs::exists(fs::u8path(source))) compatible(doc->metadata(),read_image(source,reader)->metadata());
    }
    if(name.empty()) name=embedded?"Image":fs::u8path(source).filename().u8string(); text(name);
    auto id=next_id_;
    entries_.emplace(id,Entry{{id,std::move(name),std::move(source),std::move(reader),doc->metadata().width,doc->metadata().height,embedded,true},std::move(doc)});
    ++next_id_; ++revision_; if(!active_image_) active_image_=id; return id;
}
void Project::remove_image(std::uint64_t id) {
    entry(id); advance(revision_); entries_.erase(id); views_.erase(id); ++revision_;
    if(active_image_==id) active_image_=entries_.empty()?0:entries_.begin()->first;
}
void ProjectView::validate() const {
    static_assert(static_cast<int>(DisplayMode::RGB)==0 && static_cast<int>(DisplayMode::Gray)==1 &&
        static_cast<int>(DisplayMode::Red)==2 && static_cast<int>(DisplayMode::Green)==3 && static_cast<int>(DisplayMode::Blue)==4,
        "Persisted schema-2 display codes require an explicit migration before changing");
    display.validate();
    const double limit=std::numeric_limits<float>::max()/4;
    if(!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(zoom) || zoom<=0 ||
       !std::isfinite(8192./zoom) || zoom>limit || std::abs(center.x*zoom)>limit || std::abs(center.y*zoom)>limit)
        throw std::invalid_argument("project view exceeds finite screen coordinate range");
}
bool ProjectView::operator==(const ProjectView& b) const {
    return center.x==b.center.x && center.y==b.center.y && zoom==b.zoom && display==b.display;
}
namespace {
void validate_view(const ProjectView& view,const ImageMetadata& image) {
    view.validate();
    if(static_cast<double>(std::max(image.width,image.height))*view.zoom>std::numeric_limits<float>::max()/4)
        throw std::invalid_argument("project view exceeds image screen coordinate range");
}
}
std::optional<ProjectView> Project::view(std::uint64_t id) const {
    entry(id); auto it=views_.find(id); return it==views_.end()?std::nullopt:std::optional<ProjectView>(it->second);
}
void Project::set_view(std::uint64_t id,ProjectView view) {
    validate_view(view,entry(id).document->metadata());
    auto existing=views_.find(id); if(existing!=views_.end() && existing->second==view) return;
    advance(revision_); views_.insert_or_assign(id,std::move(view)); ++revision_;
}
void Project::clear_view(std::uint64_t id) {
    entry(id); if(!views_.count(id)) return;
    advance(revision_); views_.erase(id); ++revision_;
}
void Project::set_active_image(std::uint64_t id) {
    if(id) entry(id); if(active_image_==id) return;
    advance(revision_); active_image_=id; ++revision_;
}
void Project::rename_image(std::uint64_t id,std::string name) {
    const auto& old=entry(id); text(name);
    if(name.empty()) throw std::invalid_argument("image name must not be empty");
    if(old.info.name==name) return;
    advance(revision_); entries_.at(id).info.name.swap(name); ++revision_;
}
void Project::relink(std::uint64_t id,const std::string& path) {
    const auto& old=entry(id); if(old.info.embedded) throw std::invalid_argument("embedded images do not require relinking");
    auto source=absolute(path); auto server=read_image(source,old.info.reader); compatible(old.document->metadata(),server->metadata());
    // Keep recorded source metadata and manual calibration separate from new
    // reader metadata. Existing external server references retain their source.
    auto linked=std::make_shared<LinkedServer>(old.document->metadata(),source,old.info.reader);
    advance(revision_);
    entries_.at(id).info.source_path.swap(source); entries_.at(id).document->server_=std::move(linked); ++revision_;
}
bool Project::dirty() const {
    if(path_.empty() || revision_!=saved_revision_ || saved_revisions_.size()!=entries_.size()) return true;
    for(const auto& [id,e]:entries_) { auto old=saved_revisions_.find(id); if(old==saved_revisions_.end() || old->second!=e.document->revision()) return true; }
    return false;
}
bool has_project() {
#ifdef QUPATHXX_HAS_PROJECT
    return true;
#else
    return false;
#endif
}
std::string sqlite_version() {
#ifdef QUPATHXX_HAS_PROJECT
    return sqlite3_libversion();
#else
    return "disabled";
#endif
}
#ifndef QUPATHXX_HAS_PROJECT
void Project::save(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
void Project::save_impl(const std::string&,const std::string&,const std::atomic<bool>*,const RecoveryInfo*) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
void Project::save_recovery(const std::string&,const std::string&,const RecoveryInfo&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::open(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::open_impl(const std::string&,bool,const std::string&,LoadProgress*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::shared_ptr<Project> Project::recover(const std::string&,const std::string&) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
std::pair<std::string,std::uint64_t> Project::read_save_identity(const std::string&) { throw std::runtime_error("project storage requires QUPATHXX_BUILD_PROJECT=ON"); }
std::optional<RecoveryInfo> Project::inspect_recovery_work(const std::string&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
RecoveryInfo Project::inspect_recovery_header(const std::string&,const std::atomic<bool>*) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
RecoveryInfo Project::inspect_recovery(const std::string&) { throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON"); }
#else
namespace {
constexpr int application_id=1364219992;
constexpr int recovery_application_id=1364218456; // QPRX, distinct from ordinary QPXX projects.
struct Database {
    sqlite3* db=nullptr;
    Database(const std::string& path,bool write,const std::atomic<bool>* cancelled=nullptr) {
        auto connect=[&](int flags) {
            auto code=sqlite3_open_v2(path.c_str(),&db,flags|SQLITE_OPEN_NOMUTEX,nullptr);
            if(code!=SQLITE_OK) throw std::runtime_error(std::string("SQLite open: ")+(db?sqlite3_errmsg(db):"out of memory"));
            sqlite3_extended_result_codes(db,1); sqlite3_busy_timeout(db,1000);
            if(cancelled) {
                // Initial header reads can wait on another writer before the
                // usual progress handler is installed. Keep that wait cancellable.
                sqlite3_busy_handler(db,[](void* value,int retry) {
                    auto* flag=static_cast<const std::atomic<bool>*>(value);
                    if(retry>=200 || flag->load()) return 0;
                    sqlite3_sleep(5);
                    return flag->load()?0:1;
                },const_cast<std::atomic<bool>*>(cancelled));
                cancellation(cancelled);
            }
            sqlite3_limit(db,SQLITE_LIMIT_LENGTH,272*1024*1024);
        };
        try {
            connect(write?SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:SQLITE_OPEN_READONLY);
            if(!write) {
                // A killed writer can leave a hot rollback journal. A read-only
                // connection cannot repair it. Retry only SQLite's precise
                // recovery-required condition, without creating a new file.
                auto code=sqlite3_exec(db,"PRAGMA user_version",nullptr,nullptr,nullptr);
                if(code!=SQLITE_OK && sqlite3_extended_errcode(db)==SQLITE_READONLY_ROLLBACK) {
                    sqlite3_close(db); db=nullptr;
                    connect(SQLITE_OPEN_READWRITE);
                    if(sqlite3_exec(db,"PRAGMA user_version",nullptr,nullptr,nullptr)!=SQLITE_OK)
                        throw std::runtime_error(std::string("SQLite rollback recovery: ")+sqlite3_errmsg(db));
                    sqlite3_close(db); db=nullptr;
                    connect(SQLITE_OPEN_READONLY);
                } else if(code!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));
            }
        } catch(...) {if(db) sqlite3_close(db); db=nullptr; throw;}
    }
    ~Database() { if(db) sqlite3_close(db); }
    Database(const Database&)=delete;
    void cancellation(const std::atomic<bool>* flag) {
        if(flag) sqlite3_progress_handler(db,1000,[](void* value){return static_cast<const std::atomic<bool>*>(value)->load()?1:0;},const_cast<std::atomic<bool>*>(flag));
    }
    void exec(const char* sql) {
        char* error=nullptr; auto code=sqlite3_exec(db,sql,nullptr,nullptr,&error);
        if(code!=SQLITE_OK) { std::string message=error?error:sqlite3_errmsg(db); sqlite3_free(error); throw std::runtime_error("SQLite: "+message); }
    }
};
struct Statement {
    Database& owner; sqlite3_stmt* stmt=nullptr;
    Statement(Database& db,const char* sql):owner(db) { if(sqlite3_prepare_v2(db.db,sql,-1,&stmt,nullptr)!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db.db)); }
    ~Statement() { if(stmt) sqlite3_finalize(stmt); }
    void check(int code) { if(code!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(owner.db)); }
    void string(int i,const std::string& s) { check(sqlite3_bind_text64(stmt,i,s.data(),s.size(),SQLITE_TRANSIENT,SQLITE_UTF8)); }
    void integer(int i,std::int64_t v) { check(sqlite3_bind_int64(stmt,i,v)); }
    void real(int i,double v) { check(sqlite3_bind_double(stmt,i,v)); }
    void optional(int i,std::optional<double> v) { if(v) real(i,*v); else check(sqlite3_bind_null(stmt,i)); }
    void blob(int i,const std::vector<std::uint8_t>& v) { check(sqlite3_bind_blob64(stmt,i,v.data(),v.size(),SQLITE_STATIC)); }
    bool row() { auto code=sqlite3_step(stmt); if(code==SQLITE_ROW) return true; if(code==SQLITE_DONE) return false; throw std::runtime_error(sqlite3_errmsg(owner.db)); }
    void done() { if(row()) throw std::runtime_error("unexpected SQLite row"); check(sqlite3_reset(stmt)); check(sqlite3_clear_bindings(stmt)); }
    int type(int i) const { return sqlite3_column_type(stmt,i); }
    std::string string(int i) const {
        if(type(i)!=SQLITE_TEXT) throw std::invalid_argument("project text column has invalid type");
        auto bytes=sqlite3_column_bytes(stmt,i); if(bytes>32768) throw std::length_error("project text too large");
        auto* p=sqlite3_column_text(stmt,i); return std::string(reinterpret_cast<const char*>(p),static_cast<std::size_t>(bytes));
    }
    std::int64_t integer(int i) const { if(type(i)!=SQLITE_INTEGER) throw std::invalid_argument("invalid project integer"); return sqlite3_column_int64(stmt,i); }
    double real(int i) const { if(type(i)!=SQLITE_FLOAT && type(i)!=SQLITE_INTEGER) throw std::invalid_argument("invalid project number"); auto v=sqlite3_column_double(stmt,i); if(!std::isfinite(v)) throw std::invalid_argument("nonfinite project number"); return v; }
    std::optional<double> optional(int i) const { return type(i)==SQLITE_NULL?std::nullopt:std::optional<double>(real(i)); }
};
struct Transaction {
    Database& db; bool committed=false;
    Transaction(Database& d,bool write):db(d) { db.exec(write?"BEGIN IMMEDIATE":"BEGIN"); }
    ~Transaction() { if(!committed) sqlite3_exec(db.db,"ROLLBACK",nullptr,nullptr,nullptr); }
    void commit() { db.exec("COMMIT"); committed=true; }
};
std::int64_t scalar(Database& db,const char* sql) { Statement s(db,sql); if(!s.row()) throw std::runtime_error("missing SQLite scalar"); return s.integer(0); }
std::uint64_t number(const std::string& text) {
    std::uint64_t value=0; auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || std::to_string(value)!=text) throw std::invalid_argument("invalid project unsigned identity");
    return value;
}
int schema(Database& db,bool allow_recovery=false) {
    auto version=scalar(db,"PRAGMA user_version");
    auto app=scalar(db,"PRAGMA application_id");
    if((app!=application_id && !(allow_recovery && app==recovery_application_id)) || (version!=1 && version!=2 && version!=3 && version!=4))
        throw std::invalid_argument("not a supported qupathxx project schema (expected version 1, 2, 3 or 4)");
    return static_cast<int>(version);
}
void create_views(Database& db) {
    db.exec("CREATE TABLE views(image_id TEXT PRIMARY KEY NOT NULL REFERENCES images(id),center_x REAL NOT NULL,center_y REAL NOT NULL,zoom REAL NOT NULL,mode INTEGER NOT NULL,black REAL NOT NULL,white REAL NOT NULL,gamma REAL NOT NULL) STRICT; PRAGMA user_version=2;");
}
void create_analysis(Database& db) {
    db.exec("CREATE TABLE analysis_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,object_id TEXT NOT NULL,object_revision TEXT NOT NULL,document_revision TEXT NOT NULL,algorithm TEXT NOT NULL,input_sha256 TEXT NOT NULL,kind INTEGER NOT NULL,nucleus INTEGER NOT NULL,tile_size INTEGER NOT NULL,samples TEXT NOT NULL,tiles TEXT NOT NULL,elapsed_ms REAL NOT NULL,mean_r REAL NOT NULL,mean_g REAL NOT NULL,mean_b REAL NOT NULL,white_r REAL NOT NULL,white_g REAL NOT NULL,white_b REAL NOT NULL,s00 REAL NOT NULL,s01 REAL NOT NULL,s02 REAL NOT NULL,s10 REAL NOT NULL,s11 REAL NOT NULL,s12 REAL NOT NULL,s20 REAL NOT NULL,s21 REAL NOT NULL,s22 REAL NOT NULL,geometry BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=3;");
}
void create_tissue(Database& db) {
    db.exec("CREATE TABLE tissue_records(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,producer TEXT NOT NULL,geometry BLOB NOT NULL,mask BLOB NOT NULL,PRIMARY KEY(image_id,id)) STRICT; PRAGMA user_version=4;");
}
bool recovery_table(Database& db) { return scalar(db,"SELECT count(*) FROM sqlite_schema WHERE type='table' AND name='recovery'")!=0; }
RecoveryInfo recovery_header(Database& db,const std::string& path) {
    if(scalar(db,"PRAGMA application_id")!=recovery_application_id || schema(db,true)<2 || !recovery_table(db)) throw std::invalid_argument("not a supported recovery snapshot");
    Statement row(db,"SELECT identity,session,base_path,base_generation,created_ms,image_count,object_count FROM recovery");
    if(!row.row()) throw std::invalid_argument("missing recovery metadata");
    RecoveryInfo info; info.path=path; info.identity=row.string(0); info.session=row.string(1); info.base_path=row.string(2);
    info.base_generation=number(row.string(3)); info.created_unix_ms=row.integer(4);
    auto images=row.integer(5),objects=row.integer(6);
    auto valid_id=[](const std::string& id){return id.size()==32 && id.find_first_not_of("0123456789abcdef")==std::string::npos;};
    text(info.base_path,32768);
    if(!valid_id(info.identity) || !valid_id(info.session) || info.created_unix_ms<=0 || images<0 || objects<0 || row.row() ||
       (info.base_path.empty()?info.base_generation!=0:(!fs::u8path(info.base_path).is_absolute() || !info.base_generation || info.base_generation>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))))
        throw std::invalid_argument("invalid recovery metadata");
    info.images=static_cast<std::size_t>(images); info.objects=static_cast<std::size_t>(objects);
    Statement meta(db,"SELECT identity FROM project");
    if(!meta.row() || meta.string(0)!=info.identity || meta.row() || scalar(db,"SELECT count(*) FROM images")!=images || scalar(db,"SELECT count(*) FROM objects")!=objects)
        throw std::invalid_argument("recovery metadata does not match snapshot");
    return info;
}
struct Encoder {
    std::vector<std::uint8_t> bytes={'Q','R','O',1};
    void u32(std::size_t value) { if(value>1000000) throw std::length_error("geometry count exceeds limit"); for(int i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i))); }
    void real(double v) { std::uint64_t bits; static_assert(sizeof(v)==sizeof(bits),"64-bit doubles required"); std::memcpy(&bits,&v,8); for(int i=0;i<8;++i) bytes.push_back(static_cast<std::uint8_t>(bits>>(8*i))); }
    void ring(const Ring& ring) { u32(ring.size()); for(auto p:ring) { real(p.x); real(p.y); } }
};
std::vector<std::uint8_t> encode(const Roi& roi) {
    Encoder e; e.bytes.push_back(static_cast<std::uint8_t>(roi.kind()));
    if(roi.kind()==RoiKind::Rectangle || roi.kind()==RoiKind::Ellipse) { auto r=roi.bounds(); for(auto v:{r.x,r.y,r.width,r.height}) e.real(v); }
    else if(roi.is_area()) { auto parts=roi.polygon_parts(); e.u32(parts.size()); for(const auto& part:parts) { e.ring(part.shell); e.u32(part.holes.size()); for(const auto& hole:part.holes) e.ring(hole); } }
    else e.ring(roi.paths().at(0));
    return std::move(e.bytes);
}
struct Decoder {
    const std::uint8_t* p; std::size_t remaining,total=0;
    std::uint8_t byte() { if(!remaining) throw std::invalid_argument("truncated ROI blob"); --remaining; return *p++; }
    std::uint32_t u32() { std::uint32_t v=0; for(int i=0;i<4;++i) v|=std::uint32_t(byte())<<(8*i); return v; }
    double real() { std::uint64_t bits=0; for(int i=0;i<8;++i) bits|=std::uint64_t(byte())<<(8*i); double v; std::memcpy(&v,&bits,8); return v; }
    Ring ring() { auto n=u32(); if(n>1000000 || n>remaining/16 || total>1000000-n) throw std::invalid_argument("invalid persisted vertex count"); total+=n; Ring r; r.reserve(n); for(std::uint32_t i=0;i<n;++i) { auto x=real(),y=real(); r.emplace_back(x,y); } return r; }
};
Roi decode(Statement& s,int column) {
    auto size=sqlite3_column_bytes(s.stmt,column);
    if(s.type(column)!=SQLITE_BLOB || size<5 || size>32*1024*1024) throw std::invalid_argument("invalid ROI blob");
    Decoder d{static_cast<const std::uint8_t*>(sqlite3_column_blob(s.stmt,column)),static_cast<std::size_t>(size)};
    if(d.byte()!='Q' || d.byte()!='R' || d.byte()!='O' || d.byte()!=1) throw std::invalid_argument("unsupported ROI encoding");
    auto kind=static_cast<RoiKind>(d.byte());
    auto roi=[&]()->Roi {
        if(kind==RoiKind::Rectangle || kind==RoiKind::Ellipse) { auto x=d.real(),y=d.real(),w=d.real(),h=d.real(); return kind==RoiKind::Rectangle?Roi::rectangle({x,y,w,h}):Roi::ellipse({x,y,w,h}); }
        if(kind==RoiKind::Polyline) return Roi::polyline(d.ring());
        if(kind==RoiKind::Points) return Roi::points(d.ring());
        if(kind!=RoiKind::Polygon && kind!=RoiKind::MultiPolygon) throw std::invalid_argument("unknown persisted ROI kind");
        auto count=d.u32(); if(!count || count>1000000 || count>d.remaining/8 || (kind==RoiKind::Polygon && count!=1) || (kind==RoiKind::MultiPolygon && count<2)) throw std::invalid_argument("invalid persisted polygon count");
        std::vector<PolygonPart> parts;
        for(std::uint32_t i=0;i<count;++i) { auto shell=d.ring(); auto n=d.u32(); if(n>d.remaining/4) throw std::invalid_argument("invalid hole count"); std::vector<Ring> holes; for(std::uint32_t j=0;j<n;++j) holes.push_back(d.ring()); parts.emplace_back(std::move(shell),std::move(holes)); }
        return Roi::multipolygon(std::move(parts));
    }();
    if(d.remaining) throw std::invalid_argument("trailing ROI blob bytes"); return roi;
}
}
void Project::save(const std::string& path) {
    save_impl(path,"");
}
void Project::save_impl(const std::string& path,const std::string& link_directory,const std::atomic<bool>* cancelled,const RecoveryInfo* recovery) {
    auto destination=absolute(path);
    std::map<std::uint64_t,std::uint64_t> revisions;
    for(const auto& [id,e]:entries_) revisions.emplace(id,e.document->revision());
    Database db(destination,true);
    db.cancellation(cancelled);
    db.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
    Transaction transaction(db,true);
    const bool fresh=scalar(db,"PRAGMA application_id")==0 && scalar(db,"PRAGMA user_version")==0 &&
        scalar(db,"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'")==0;
    std::uint64_t next_generation=1;
    if(fresh) {
        db.exec("PRAGMA application_id=1364219992; PRAGMA user_version=1;"
            "CREATE TABLE project(identity TEXT NOT NULL,next_image TEXT NOT NULL,active TEXT NOT NULL,generation INTEGER NOT NULL) STRICT;"
            "CREATE TABLE images(id TEXT PRIMARY KEY NOT NULL,name TEXT NOT NULL,reader TEXT NOT NULL,source TEXT NOT NULL,embedded INTEGER NOT NULL,width INTEGER NOT NULL,height INTEGER NOT NULL,vendor TEXT NOT NULL,server_x REAL,server_y REAL,cal_x REAL,cal_y REAL,next_object TEXT NOT NULL,revision TEXT NOT NULL,rgb BLOB) STRICT;"
            "CREATE TABLE levels(image_id TEXT NOT NULL REFERENCES images(id),ordinal INTEGER NOT NULL,width INTEGER NOT NULL,height INTEGER NOT NULL,downsample REAL NOT NULL,PRIMARY KEY(image_id,ordinal)) STRICT;"
            "CREATE TABLE classes(image_id TEXT NOT NULL REFERENCES images(id),name TEXT NOT NULL,color INTEGER NOT NULL,PRIMARY KEY(image_id,name)) STRICT;"
            "CREATE TABLE objects(image_id TEXT NOT NULL REFERENCES images(id),id TEXT NOT NULL,name TEXT NOT NULL,parent TEXT NOT NULL,class_name TEXT NOT NULL,revision TEXT NOT NULL,kind INTEGER NOT NULL,geometry BLOB NOT NULL,nucleus BLOB,PRIMARY KEY(image_id,id)) STRICT;"
            "CREATE INDEX objects_image ON objects(image_id);");
        create_views(db);
    } else {
        if(recovery_table(db)) throw std::invalid_argument("destination is a recovery snapshot; choose a project path");
        auto version=schema(db); Statement existing(db,"SELECT identity,generation FROM project");
        if(!existing.row()) throw std::invalid_argument("project metadata is missing");
        auto identity=existing.string(0); auto generation=existing.integer(1);
        if(identity!=identity_) throw std::invalid_argument("destination contains a different project; choose a new path");
        if(generation<1 || static_cast<std::uint64_t>(generation)!=generation_) throw std::runtime_error("project changed on disk; save to a new path or reopen before saving");
        if(generation==std::numeric_limits<std::int64_t>::max()) throw std::overflow_error("project save generation exhausted");
        next_generation=static_cast<std::uint64_t>(generation)+1;
        if(existing.row()) throw std::invalid_argument("duplicate project metadata");
        // Schema changes share the same transaction as canonical rows. A failed
        // save rolls back both the migration and data, retaining the v1 project.
        if(version==1) create_views(db);
        if(version<3) create_analysis(db);
        if(version<4) create_tissue(db);
    }
    if(fresh) {create_analysis(db);create_tissue(db);}
    db.exec("DELETE FROM tissue_records; DELETE FROM analysis_records; DELETE FROM views; DELETE FROM objects; DELETE FROM classes; DELETE FROM levels; DELETE FROM images; DELETE FROM project;");
    Statement meta(db,"INSERT INTO project VALUES(?,?,?,?)");
    meta.string(1,identity_); meta.string(2,std::to_string(next_id_)); meta.string(3,std::to_string(active_image_)); meta.integer(4,static_cast<std::int64_t>(next_generation)); meta.done();
    Statement image(db,"INSERT INTO images VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    Statement level(db,"INSERT INTO levels VALUES(?,?,?,?,?)");
    Statement classification(db,"INSERT INTO classes VALUES(?,?,?)");
    Statement object(db,"INSERT INTO objects VALUES(?,?,?,?,?,?,?,?,?)");
    Statement view(db,"INSERT INTO views VALUES(?,?,?,?,?,?,?,?)");
    Statement tissue(db,"INSERT INTO tissue_records VALUES(?,?,?,?,?)");
    Statement analysis(db,"INSERT INTO analysis_records VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    auto directory=link_directory.empty()?fs::u8path(destination).parent_path():fs::u8path(link_directory);
    for(const auto& [id,e]:entries_) {
        if(cancelled && cancelled->load()) throw std::runtime_error("recovery cancelled");
        const auto& doc=*e.document; const auto& m=doc.metadata(); validate_metadata(m);
        auto key=std::to_string(id),source=e.info.source_path;
        if(!e.info.embedded) { auto relative=fs::u8path(source).lexically_relative(directory); if(!relative.empty()) source=relative.generic_u8string(); }
        image.string(1,key); image.string(2,e.info.name); image.string(3,e.info.reader); image.string(4,source); image.integer(5,e.info.embedded?1:0);
        image.integer(6,m.width); image.integer(7,m.height); image.string(8,m.vendor); image.optional(9,m.pixel_width_um); image.optional(10,m.pixel_height_um);
        image.optional(11,doc.pixel_width_um_); image.optional(12,doc.pixel_height_um_); image.string(13,std::to_string(doc.next_id_)); image.string(14,std::to_string(doc.revision_));
        if(e.info.embedded) image.blob(15,doc.image().pixels());
        image.done();
        auto saved_view=views_.find(id);
        if(saved_view!=views_.end()) {
            const auto& v=saved_view->second; validate_view(v,m);
            view.string(1,key); view.real(2,v.center.x); view.real(3,v.center.y); view.real(4,v.zoom);
            view.integer(5,static_cast<int>(v.display.mode)); view.real(6,v.display.black); view.real(7,v.display.white); view.real(8,v.display.gamma); view.done();
        }
        for(std::size_t i=0;i<m.levels.size();++i) {
            level.string(1,key); level.integer(2,static_cast<std::int64_t>(i)); level.integer(3,m.levels[i].width); level.integer(4,m.levels[i].height); level.real(5,m.levels[i].downsample); level.done();
        }
        for(const auto& [name,c]:doc.classes_) { classification.string(1,key); classification.string(2,name); classification.integer(3,c.color); classification.done(); }
        for(const auto& [object_id,a]:doc.objects_) {
            if(cancelled && cancelled->load()) throw std::runtime_error("recovery cancelled");
            object.string(1,key); object.string(2,std::to_string(object_id)); object.string(3,a.name); object.string(4,std::to_string(a.parent_id));
            object.string(5,a.class_name); object.string(6,std::to_string(a.revision)); object.integer(7,static_cast<int>(a.kind));
            auto geometry=encode(a.geometry); object.blob(8,geometry);
            auto nucleus=a.nucleus?encode(*a.nucleus):std::vector<std::uint8_t>{}; if(a.nucleus) object.blob(9,nucleus);
            object.done();
        }
    }
    for(const auto& [image_id,e]:entries_) {
      auto write_analysis=[&](const AnalysisRecord& record) {
        if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
        const auto& r=record.result;
        analysis.string(1,std::to_string(image_id));analysis.string(2,std::to_string(record.id));analysis.string(3,std::to_string(r.object_id));
        analysis.string(4,std::to_string(r.object_revision));analysis.string(5,std::to_string(r.document_revision));analysis.string(6,r.algorithm);analysis.string(7,r.input_sha256);
        analysis.integer(8,static_cast<int>(r.kind));analysis.integer(9,r.nucleus?1:0);analysis.integer(10,static_cast<std::int64_t>(record.tile_size));
        analysis.string(11,std::to_string(r.samples));analysis.string(12,std::to_string(r.tiles));analysis.real(13,r.elapsed_ms);
        for(int c=0;c<3;++c) {analysis.real(14+c,r.means[c]);analysis.real(17+c,r.white[c]);}
        for(int i=0;i<3;++i) for(int c=0;c<3;++c) analysis.real(20+i*3+c,r.stains[i][c]);
        const auto geometry=encode(record.roi);analysis.blob(29,geometry);analysis.done();
      };
      for(const auto& [record_id,stored]:e.document->analysis_) write_analysis(*stored.value);
      for(const auto& block:e.document->analysis_snapshot_) for(const auto& record:block->records) if(record) write_analysis(*record);
      auto write_tissue=[&](const TissueRecord& record) {
        if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
        tissue.string(1,std::to_string(image_id));tissue.string(2,std::to_string(record.id));tissue.string(3,record.producer);
        const auto geometry=encode(record.result->roi());const auto mask=TissueArchiveCodec::encode(*record.result,cancelled);
        tissue.blob(4,geometry);tissue.blob(5,mask);tissue.done();
      };
      for(const auto& [record_id,stored]:e.document->tissue_) write_tissue(*stored.value);
      for(const auto& block:e.document->tissue_snapshot_) for(const auto& record:block->records) if(record) write_tissue(*record);
    }
    // Canonical rows and recovery provenance must commit together.
    if(recovery) {
        db.exec("PRAGMA application_id=1364218456; CREATE TABLE recovery(identity TEXT NOT NULL,session TEXT NOT NULL,base_path TEXT NOT NULL,base_generation TEXT NOT NULL,created_ms INTEGER NOT NULL,image_count INTEGER NOT NULL,object_count INTEGER NOT NULL) STRICT;");
        Statement row(db,"INSERT INTO recovery VALUES(?,?,?,?,?,?,?)");
        row.string(1,recovery->identity); row.string(2,recovery->session); row.string(3,recovery->base_path); row.string(4,std::to_string(recovery->base_generation));
        row.integer(5,recovery->created_unix_ms); row.integer(6,static_cast<std::int64_t>(recovery->images)); row.integer(7,static_cast<std::int64_t>(recovery->objects)); row.done();
    }
    if(cancelled && cancelled->load()) throw std::runtime_error("project save cancelled");
    transaction.commit();
    path_.swap(destination); generation_=next_generation; saved_revision_=revision_; saved_revisions_.swap(revisions);
}
std::shared_ptr<Project> Project::open(const std::string& path) {
    return open_impl(path,false);
}
std::shared_ptr<Project> Project::recover(const std::string& path,const std::string& base) { return open_impl(path,true,base); }
std::shared_ptr<Project> Project::open_impl(const std::string& path,bool recovery,const std::string& base_path,LoadProgress* progress) {
    auto stopped=[&] {if(progress && progress->cancelled.load()) throw std::runtime_error("project opening cancelled");};
    stopped();
    auto source=absolute(path); Database db(source,false,progress?&progress->cancelled:nullptr);
    db.cancellation(progress?&progress->cancelled:nullptr); Transaction transaction(db,false);
    if(recovery_table(db)!=recovery) throw std::invalid_argument(recovery?"not a recovery snapshot":"use Project.recover for recovery snapshots");
    auto version=schema(db,recovery);
    std::optional<RecoveryInfo> recovery_info;
    if(recovery) recovery_info=recovery_header(db,source);
    std::string associated_base;
    if(!base_path.empty()) {
        if(!recovery_info || recovery_info->base_path.empty() || !recovery_info->base_generation)
            throw std::invalid_argument("only a saved-project recovery can be associated with a primary location");
        associated_base=absolute(base_path);
        const auto base=read_save_identity(associated_base);
        if(base.first!=recovery_info->identity) throw std::invalid_argument("recovery association requires the same primary project identity");
        // Keep the captured generation even if the relocated primary advanced.
        // A later save must still reject that stale baseline, rather than rebase.
    }
    stopped();
    if(progress) {
        progress->total_images.store(static_cast<std::size_t>(scalar(db,"SELECT count(*) FROM images")));
        progress->total_objects.store(static_cast<std::size_t>(scalar(db,"SELECT count(*) FROM objects")));
        progress->phase.store(2);
    }
    auto project=std::make_shared<Project>();
    Statement meta(db,"SELECT identity,next_image,active,generation FROM project");
    if(!meta.row()) throw std::invalid_argument("project metadata is missing");
    project->identity_=meta.string(0); project->next_id_=number(meta.string(1)); project->active_image_=number(meta.string(2));
    auto generation=meta.integer(3);
    if(project->identity_.size()!=32 || project->identity_.find_first_not_of("0123456789abcdef")!=std::string::npos || !project->next_id_ || generation<1 || meta.row())
        throw std::invalid_argument("invalid project metadata");
    project->generation_=static_cast<std::uint64_t>(generation);
    Statement images(db,"SELECT id,name,reader,source,embedded,width,height,vendor,server_x,server_y,cal_x,cal_y,next_object,revision,rgb FROM images");
    Statement levels(db,"SELECT ordinal,width,height,downsample FROM levels WHERE image_id=? ORDER BY ordinal");
    Statement classes(db,"SELECT name,color FROM classes WHERE image_id=? ORDER BY name");
    Statement objects(db,"SELECT id,name,parent,class_name,revision,kind,geometry,nucleus FROM objects WHERE image_id=?");
    while(images.row()) {
        stopped(); if(progress) progress->phase.store(2);
        auto key=images.string(0); ProjectImage info;
        info.id=number(key); info.name=images.string(1); info.reader=images.string(2); auto file=images.string(3);
        auto embedded=images.integer(4); if(embedded!=0 && embedded!=1) throw std::invalid_argument("invalid embedded-image flag");
        info.embedded=embedded==1; info.width=images.integer(5); info.height=images.integer(6); text(info.name);
        if(!info.id || info.id>=project->next_id_) throw std::invalid_argument("invalid project image ID");
        ImageMetadata metadata; metadata.width=info.width; metadata.height=info.height; metadata.vendor=images.string(7);
        metadata.pixel_width_um=images.optional(8); metadata.pixel_height_um=images.optional(9);
        levels.string(1,key); std::int64_t ordinal=0;
        while(levels.row()) { if(levels.integer(0)!=ordinal++ || ordinal>65536) throw std::invalid_argument("invalid pyramid level order"); metadata.levels.push_back({levels.integer(1),levels.integer(2),levels.real(3)}); }
        levels.check(sqlite3_reset(levels.stmt)); levels.check(sqlite3_clear_bindings(levels.stmt)); validate_metadata(metadata);
        std::shared_ptr<ImageServer> server;
        if(info.embedded) {
            auto bytes=sqlite3_column_bytes(images.stmt,14);
            if(info.reader!="embedded" || !file.empty() || images.type(14)!=SQLITE_BLOB || metadata.levels.size()!=1 || info.width>std::numeric_limits<int>::max() || info.height>std::numeric_limits<int>::max() ||
                info.width>256*1024*1024/3 || info.height>(256*1024*1024)/(3*info.width) || bytes!=info.width*info.height*3)
                throw std::invalid_argument("invalid embedded image payload");
            auto* p=static_cast<const std::uint8_t*>(sqlite3_column_blob(images.stmt,14));
            server=dense_image_server(Image(static_cast<int>(info.width),static_cast<int>(info.height),{p,p+bytes}));
            if(metadata.vendor!=server->metadata().vendor || metadata.pixel_width_um!=server->metadata().pixel_width_um || metadata.pixel_height_um!=server->metadata().pixel_height_um)
                throw std::invalid_argument("embedded source metadata does not match dense pixels");
        } else {
            if(file.empty() || images.type(14)!=SQLITE_NULL || reader_name(file,info.reader)!=info.reader) throw std::invalid_argument("invalid linked image descriptor");
            text(file,32768); auto location=fs::u8path(file); if(location.is_relative()) location=fs::u8path(source).parent_path()/location;
            info.source_path=absolute(location.u8string()); server=std::make_shared<LinkedServer>(metadata,info.source_path,info.reader);
        }
        std::vector<ObjectClass> definitions;
        classes.string(1,key);
        while(classes.row()) { auto color=classes.integer(1); if(color<0 || color>0xffffff) throw std::invalid_argument("invalid persisted class color"); definitions.push_back({classes.string(0),static_cast<std::uint32_t>(color)}); }
        classes.check(sqlite3_reset(classes.stmt)); classes.check(sqlite3_clear_bindings(classes.stmt));
        std::vector<Annotation> annotations;
        objects.string(1,key);
        while(objects.row()) {
            stopped();
            auto kind=objects.integer(5); if(kind<0 || kind>2) throw std::invalid_argument("unknown persisted object kind");
            auto geometry=decode(objects,6); std::optional<Roi> nucleus; if(objects.type(7)!=SQLITE_NULL) nucleus=decode(objects,7);
            annotations.push_back({number(objects.string(0)),objects.string(1),std::move(geometry),number(objects.string(2)),objects.string(3),number(objects.string(4)),static_cast<ObjectKind>(kind),std::move(nucleus)});
            if(progress) progress->objects.fetch_add(1,std::memory_order_relaxed);
        }
        objects.check(sqlite3_reset(objects.stmt)); objects.check(sqlite3_clear_bindings(objects.stmt));
        stopped(); if(progress) progress->phase.store(3);
        auto document=Document::restore(std::move(server),annotations,definitions,images.optional(10),images.optional(11),number(images.string(12)),number(images.string(13)));
        project->saved_revisions_.emplace(info.id,document->revision());
        if(!project->entries_.emplace(info.id,Entry{std::move(info),std::move(document),true}).second) throw std::invalid_argument("duplicate project image ID");
        stopped(); if(progress) progress->images.fetch_add(1,std::memory_order_relaxed);
    }
    stopped(); if(progress) progress->phase.store(4);
    if(project->active_image_ && !project->entries_.count(project->active_image_)) throw std::invalid_argument("project active image does not exist");
    if(version>=2) {
        Statement views(db,"SELECT image_id,center_x,center_y,zoom,mode,black,white,gamma FROM views");
        while(views.row()) {
            auto id=number(views.string(0)); auto mode=views.integer(4);
            if(!project->entries_.count(id)) throw std::invalid_argument("orphaned project view");
            if(mode<0 || mode>4) throw std::invalid_argument("invalid persisted display mode");
            ProjectView v{{views.real(1),views.real(2)},views.real(3),{static_cast<DisplayMode>(mode),views.real(5),views.real(6),views.real(7)}};
            validate_view(v,project->entry(id).document->metadata());
            if(!project->views_.emplace(id,v).second) throw std::invalid_argument("duplicate persisted view");
        }
    }
    if(version>=3) {
        Statement rows(db,"SELECT image_id,id,object_id,object_revision,document_revision,algorithm,input_sha256,kind,nucleus,tile_size,samples,tiles,elapsed_ms,mean_r,mean_g,mean_b,white_r,white_g,white_b,s00,s01,s02,s10,s11,s12,s20,s21,s22,geometry FROM analysis_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();auto image_id=number(rows.string(0));auto image=project->entries_.find(image_id);
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned analysis record");
            AnalysisRecord record;auto& r=record.result;
            record.id=number(rows.string(1));r.object_id=number(rows.string(2));r.object_revision=number(rows.string(3));r.document_revision=number(rows.string(4));
            r.algorithm=rows.string(5);r.input_sha256=rows.string(6);
            auto kind=rows.integer(7),nucleus=rows.integer(8),tile=rows.integer(9);
            if(kind<0 || kind>1 || nucleus<0 || nucleus>1 || tile<16 || tile>1024) throw std::invalid_argument("invalid persisted analysis parameters");
            r.kind=static_cast<AnalysisKind>(kind);r.nucleus=nucleus!=0;record.tile_size=static_cast<std::size_t>(tile);
            r.samples=number(rows.string(10));r.tiles=number(rows.string(11));r.elapsed_ms=rows.real(12);
            for(int c=0;c<3;++c) {r.means[c]=rows.real(13+c);r.white[c]=rows.real(16+c);}
            for(int i=0;i<3;++i) for(int c=0;c<3;++c) r.stains[i][c]=rows.real(19+i*3+c);
            record.roi=decode(rows,28);image->second.document->restore_analysis(std::move(record));
        }
    }
    if(version>=4) {
        Statement rows(db,"SELECT image_id,id,producer,geometry,mask FROM tissue_records ORDER BY image_id,id");
        while(rows.row()) {
            stopped();const auto image=project->entries_.find(number(rows.string(0)));
            if(image==project->entries_.end()) throw std::invalid_argument("orphaned tissue record");
            auto record=std::make_shared<TissueRecord>();record->id=number(rows.string(1));record->producer=rows.string(2);
            auto roi=decode(rows,3);const auto size=sqlite3_column_bytes(rows.stmt,4);
            if(rows.type(4)!=SQLITE_BLOB || size<0) throw std::invalid_argument("invalid tissue mask blob");
            record->result=TissueArchiveCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(rows.stmt,4)),static_cast<std::size_t>(size),std::move(roi),image->second.document->metadata(),progress?&progress->cancelled:nullptr);
            image->second.document->restore_tissue(std::move(record));
        }
    }
    if(scalar(db,"SELECT count(*) FROM objects WHERE image_id NOT IN (SELECT id FROM images)") ||
       scalar(db,"SELECT count(*) FROM classes WHERE image_id NOT IN (SELECT id FROM images)") ||
       scalar(db,"SELECT count(*) FROM levels WHERE image_id NOT IN (SELECT id FROM images)")) throw std::invalid_argument("orphaned project data");
    stopped(); transaction.commit();
    if(recovery_info) {
        project->path_=associated_base.empty()?recovery_info->base_path:associated_base; project->generation_=recovery_info->base_generation;
        project->recovery_source_=source;
        project->revision_=1; project->saved_revision_=0;
    } else project->path_.swap(source);
    stopped(); if(progress) progress->phase.store(5);
    return project;
}
void Project::save_recovery(const std::string& path,const std::string& link_directory,const RecoveryInfo& info,const std::atomic<bool>* cancelled) {
    save_impl(path,link_directory,cancelled,&info);
    Database db(path,false); db.cancellation(cancelled);
    Statement check(db,"PRAGMA quick_check"); if(!check.row() || check.string(0)!="ok") throw std::runtime_error("recovery snapshot integrity check failed");
    recovery_header(db,path);
}
std::optional<RecoveryInfo> Project::inspect_recovery_work(const std::string& path,const std::atomic<bool>* cancelled) {
    // Opening may recover a hot journal before classifying the committed state.
    Database db(path,false); db.cancellation(cancelled); Transaction transaction(db,false);
    {
        Statement mode(db,"PRAGMA journal_mode");
        if(!mode.row() || mode.string(0)!="delete") throw std::invalid_argument("unexpected recovery work journal mode");
    }
    if(scalar(db,"PRAGMA application_id")==0 && scalar(db,"PRAGMA user_version")==0 &&
       scalar(db,"SELECT count(*) FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%'")==0) {
        transaction.commit(); return std::nullopt;
    }
    auto info=recovery_header(db,path);
    Statement check(db,"PRAGMA quick_check");
    if(!check.row() || check.string(0)!="ok") throw std::runtime_error("abandoned recovery integrity check failed");
    transaction.commit(); return info;
}
RecoveryInfo Project::inspect_recovery_header(const std::string& path,const std::atomic<bool>* cancelled) {
    auto source=absolute(path); Database db(source,false); db.cancellation(cancelled); Transaction transaction(db,false);
    auto info=recovery_header(db,source); transaction.commit();
    return info;
}
std::pair<std::string,std::uint64_t> Project::read_save_identity(const std::string& path) {
    Database base(absolute(path),false); Transaction transaction(base,false); schema(base);
    if(recovery_table(base)) throw std::invalid_argument("recovery cannot be a primary project");
    Statement meta(base,"SELECT identity,generation FROM project");
    if(!meta.row()) throw std::invalid_argument("primary project metadata is missing");
    auto identity=meta.string(0); const auto generation=meta.integer(1);
    if(identity.size()!=32 || identity.find_first_not_of("0123456789abcdef")!=std::string::npos || generation<1 || meta.row())
        throw std::invalid_argument("invalid primary project identity or generation");
    transaction.commit(); return {std::move(identity),static_cast<std::uint64_t>(generation)};
}
RecoveryInfo Project::inspect_recovery(const std::string& path) {
    auto info=inspect_recovery_header(path);
    if(info.base_path.empty()) info.base_status="unsaved";
    else {
        try {
            if(!fs::exists(fs::u8path(info.base_path))) info.base_status="missing";
            else {
                const auto base=read_save_identity(info.base_path);
                info.base_status=base.first==info.identity && base.second==info.base_generation?"matching":"changed";
            }
        } catch(const std::exception&) { info.base_status="unreadable"; }
    }
    return info;
}
#endif
}
