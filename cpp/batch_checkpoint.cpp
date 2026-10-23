#include "batch_checkpoint.hpp"
#include "project_schema.hpp"
#include "pixel_stream_record.hpp"
#include <filesystem>
#include <algorithm>
#include "qupathxx/project_batch.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <charconv>
#include <cstring>
#include <limits>
#ifdef QUPATHXX_HAS_PROJECT
#include <sqlite3.h>
#endif
namespace qpx::detail {
std::string batch_json_quote(const std::string& s) {
    bounded_utf8(s,64*1024*1024);std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:s) {if(c=='"' || c=='\\') {out+='\\';out+=char(c);}else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
#ifdef QUPATHXX_HAS_PROJECT
namespace {
void check(int code,sqlite3* db) {if(code!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));}
struct Row {
    sqlite3* db;sqlite3_stmt* value=nullptr;
    Row(sqlite3* d,const std::string& sql):db(d) {check(sqlite3_prepare_v2(db,sql.c_str(),-1,&value,nullptr),db);}
    ~Row() {if(value) sqlite3_finalize(value);}
    bool next() {const auto code=sqlite3_step(value);if(code==SQLITE_ROW) return true;if(code==SQLITE_DONE) return false;throw std::runtime_error(sqlite3_errmsg(db));}
    std::string text(int i,std::size_t limit=32768) const {if(sqlite3_column_type(value,i)!=SQLITE_TEXT) throw std::invalid_argument("invalid batch checkpoint text");const auto n=sqlite3_column_bytes(value,i);if(n<0 || std::size_t(n)>limit) throw std::length_error("batch checkpoint text exceeds limit");std::string s(reinterpret_cast<const char*>(sqlite3_column_text(value,i)),std::size_t(n));bounded_utf8(s,limit);return s;}
    std::uint64_t number(int i) const {const auto s=text(i);std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(r.ec!=std::errc{} || r.ptr!=s.data()+s.size() || std::to_string(n)!=s) throw std::invalid_argument("invalid batch checkpoint integer");return n;}
};
void bytes(Sha256& h,const void* data,std::size_t size,const std::atomic<bool>* c) {const auto* p=static_cast<const char*>(data);for(std::size_t at=0;at<size;) {export_checkpoint(c);const auto count=std::min<std::size_t>(65536,size-at);h.update(p+at,count);at+=count;}}
void text(Sha256& h,const std::string& s,const std::atomic<bool>* c) {h.integer(s.size());bytes(h,s.data(),s.size(),c);}
std::string data_digest(sqlite3* db,const std::atomic<bool>* c) {
    Sha256 hash;const std::string tag="qpx.batch-data/1";hash.update(tag.data(),tag.size());
    std::vector<std::pair<std::string,std::string>> tables={{"project","identity"},{"images","id"},{"levels","image_id,ordinal"},{"classes","image_id,name"},{"objects","image_id,id"},{"views","image_id"},{"analysis_records","image_id,id"},{"tissue_records","image_id,id"},{"cell_records","image_id,id"},{"cell_feature_links","image_id,object_id"},{"analysis_provenance","image_id,record_id"}};
    Row schema(db,"PRAGMA user_version");if(!schema.next()) throw std::invalid_argument("missing batch project schema");
    if(sqlite3_column_int(schema.value,0)>=8) tables.emplace_back("classifier_records","image_id,id");
    if(sqlite3_column_int(schema.value,0)>=9) tables.emplace_back("pixel_classifier_records","image_id,id");
    if(sqlite3_column_int(schema.value,0)>=10) tables.emplace_back("pixel_stream_records","image_id,id");
    if(sqlite3_column_int(schema.value,0)>=11) {tables.emplace_back("tma_grids","image_id");tables.emplace_back("tma_cores","image_id,object_id");}
    if(sqlite3_column_int(schema.value,0)>=12) tables.emplace_back("tiff_views","image_id");
    if(sqlite3_column_int(schema.value,0)>=13) tables.emplace_back("raw_analysis_records","image_id,id");
    for(const auto& spec:tables) {
        text(hash,spec.first,c);Row row(db,"SELECT * FROM "+spec.first+" ORDER BY "+spec.second);
        while(row.next()) {export_checkpoint(c);const auto columns=sqlite3_column_count(row.value);hash.integer(columns);
            for(int i=0;i<columns;++i) {const auto type=sqlite3_column_type(row.value,i);hash.integer(type);
                if(type==SQLITE_INTEGER) hash.integer(static_cast<std::uint64_t>(sqlite3_column_int64(row.value,i)));
                else if(type==SQLITE_FLOAT) {auto number=sqlite3_column_double(row.value,i);std::uint64_t bits=0;static_assert(sizeof(bits)==sizeof(number));if(number!=0) std::memcpy(&bits,&number,8);hash.integer(bits);}
                else if(type==SQLITE_TEXT || type==SQLITE_BLOB) {const auto n=sqlite3_column_bytes(row.value,i);if(n<0) throw std::invalid_argument("invalid checkpoint value size");hash.integer(n);bytes(hash,type==SQLITE_TEXT?static_cast<const void*>(sqlite3_column_text(row.value,i)):sqlite3_column_blob(row.value,i),std::size_t(n),c);}
                else if(type!=SQLITE_NULL) throw std::invalid_argument("invalid checkpoint value type");
            }
        }
    }
    export_checkpoint(c);return hash.finish();
}
std::string seal(const BatchCheckpoint& m,const void* csv,std::size_t count,const std::atomic<bool>* c) {
    Sha256 h;const std::string tag="qpx.batch-checkpoint/1";h.update(tag.data(),tag.size());
    for(const auto* s:{&m.parameters_sha256,&m.input_sha256,&m.pixel_sha256,&m.source_identity,&m.image_id,&m.image_name,&m.parameters_json,&m.data_sha256,&m.source_locator,&m.source_file_identity}) text(h,*s,c);
    for(auto n:{m.source_generation,m.source_revision,m.objects,m.generated_bytes}) h.integer(n);h.integer(count);bytes(h,csv,count,c);return h.finish();
}
}
void write_batch_checkpoint(sqlite3* db,const BatchCheckpoint& input,const std::atomic<bool>* c) {
    auto m=input;m.data_sha256=data_digest(db,c);const auto signature=seal(m,m.csv.data(),m.csv.size(),c);
    check(sqlite3_exec(db,"DROP TABLE IF EXISTS qpx_batch_checkpoint; CREATE TABLE qpx_batch_checkpoint(version INTEGER NOT NULL,parameters_sha TEXT NOT NULL,input_sha TEXT NOT NULL,pixel_sha TEXT NOT NULL,source_identity TEXT NOT NULL,image_id TEXT NOT NULL,image_name TEXT NOT NULL,parameters TEXT NOT NULL,data_sha TEXT NOT NULL,source_locator TEXT NOT NULL,source_file_identity TEXT NOT NULL,source_generation TEXT NOT NULL,source_revision TEXT NOT NULL,objects TEXT NOT NULL,generated_bytes TEXT NOT NULL,csv BLOB NOT NULL,seal_sha TEXT NOT NULL) STRICT;",nullptr,nullptr,nullptr),db);
    Row row(db,"INSERT INTO qpx_batch_checkpoint VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");int i=1;
    auto bind=[&](const std::string& s){check(sqlite3_bind_text64(row.value,i++,s.data(),s.size(),SQLITE_TRANSIENT,SQLITE_UTF8),db);};
    for(const auto* s:{&m.parameters_sha256,&m.input_sha256,&m.pixel_sha256,&m.source_identity,&m.image_id,&m.image_name,&m.parameters_json,&m.data_sha256,&m.source_locator,&m.source_file_identity}) bind(*s);
    for(auto n:{m.source_generation,m.source_revision,m.objects,m.generated_bytes}) bind(std::to_string(n));
    check(sqlite3_bind_blob64(row.value,i++,m.csv.data(),m.csv.size(),SQLITE_STATIC),db);bind(signature);if(row.next()) throw std::logic_error("unexpected checkpoint insert row");
}
BatchCheckpoint read_batch_checkpoint(const std::string& path,const std::atomic<bool>* c,bool include_csv,std::size_t csv_limit) {
    if(!csv_limit || csv_limit>256*1024*1024) throw std::invalid_argument("invalid checkpoint CSV limit");export_checkpoint(c);
    sqlite3* raw=nullptr;const auto code=sqlite3_open_v2(path.c_str(),&raw,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr);std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,&sqlite3_close);if(code!=SQLITE_OK) throw std::runtime_error(raw?sqlite3_errmsg(raw):"cannot open batch checkpoint");
    sqlite3_limit(raw,SQLITE_LIMIT_LENGTH,272*1024*1024);sqlite3_busy_timeout(raw,1000);if(c) sqlite3_progress_handler(raw,1000,[](void* p){return static_cast<const std::atomic<bool>*>(p)->load()?1:0;},const_cast<std::atomic<bool>*>(c));
    {Row app(raw,"PRAGMA application_id");Row version(raw,"PRAGMA user_version");if(!app.next() || sqlite3_column_int(app.value,0)!=1364219992 || !version.next() || (sqlite3_column_int(version.value,0)<first_batch_project_schema_version || sqlite3_column_int(version.value,0)>project_schema_version)) throw std::invalid_argument("not a supported batch project");}
    check(sqlite3_exec(raw,"BEGIN",nullptr,nullptr,nullptr),raw);Row marker(raw,"SELECT version,parameters_sha,input_sha,pixel_sha,source_identity,image_id,image_name,parameters,data_sha,source_locator,source_file_identity,source_generation,source_revision,objects,generated_bytes,csv,seal_sha FROM qpx_batch_checkpoint");if(!marker.next() || sqlite3_column_type(marker.value,0)!=SQLITE_INTEGER || sqlite3_column_int(marker.value,0)!=1) throw std::invalid_argument("not a supported batch checkpoint");
    BatchCheckpoint m;m.parameters_sha256=marker.text(1);m.input_sha256=marker.text(2);m.pixel_sha256=marker.text(3);m.source_identity=marker.text(4);m.image_id=marker.text(5);m.image_name=marker.text(6,4096);m.parameters_json=marker.text(7,512*1024);m.data_sha256=marker.text(8);m.source_locator=marker.text(9);m.source_file_identity=marker.text(10);
    m.source_generation=marker.number(11);m.source_revision=marker.number(12);m.objects=marker.number(13);m.generated_bytes=marker.number(14);
    const auto n=sqlite3_column_bytes(marker.value,15);if(sqlite3_column_type(marker.value,15)!=SQLITE_BLOB || n<0 || std::size_t(n)>csv_limit) throw std::length_error("checkpoint CSV exceeds byte limit");const auto* csv=sqlite3_column_blob(marker.value,15);
    if(seal(m,csv,std::size_t(n),c)!=marker.text(16,64)) throw std::invalid_argument("batch checkpoint metadata checksum mismatch");if(include_csv) m.csv.assign(static_cast<const char*>(csv),std::size_t(n));
    if(marker.next()) throw std::invalid_argument("duplicate batch checkpoint metadata");if(data_digest(raw,c)!=m.data_sha256) throw std::invalid_argument("batch checkpoint data checksum mismatch");export_checkpoint(c);check(sqlite3_exec(raw,"COMMIT",nullptr,nullptr,nullptr),raw);return m;
}
std::vector<std::shared_ptr<const PixelStreamRecord>> batch_pixel_records(const std::string& path,std::uint64_t image_id,const std::set<std::uint64_t>& excluded,std::size_t budget,const std::atomic<bool>* c) {
    export_checkpoint(c);sqlite3* raw=nullptr;const auto code=sqlite3_open_v2(path.c_str(),&raw,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr);std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,&sqlite3_close);
    if(code!=SQLITE_OK) throw std::runtime_error(raw?sqlite3_errmsg(raw):"cannot open pixel checkpoint");sqlite3_limit(raw,SQLITE_LIMIT_LENGTH,128*1024);sqlite3_busy_timeout(raw,1000);
    if(c) sqlite3_progress_handler(raw,1000,[](void* p){return static_cast<const std::atomic<bool>*>(p)->load()?1:0;},const_cast<std::atomic<bool>*>(c));
    Row row(raw,"SELECT id,payload FROM pixel_stream_records WHERE image_id=?");const auto image=std::to_string(image_id);check(sqlite3_bind_text(row.value,1,image.c_str(),-1,SQLITE_TRANSIENT),raw);
    std::vector<std::shared_ptr<const PixelStreamRecord>> result;std::size_t used=0,count=0;const auto directory=std::filesystem::u8path(path).parent_path().u8string();
    while(row.next()) {export_checkpoint(c);if(++count>10000) throw std::length_error("pixel checkpoint catalog exceeds limit");const auto id=row.number(0);if(excluded.count(id)) continue;
        const auto n=sqlite3_column_bytes(row.value,1);if(sqlite3_column_type(row.value,1)!=SQLITE_BLOB || n<=0 || n>128*1024) throw std::invalid_argument("invalid pixel checkpoint descriptor");
        const auto charge=4ULL*std::size_t(n)+4096;if(charge>budget-used) throw std::length_error("pixel checkpoint descriptors exceed materialization budget");used+=charge;
        auto record=PixelStreamRecordCodec::decode(static_cast<const std::uint8_t*>(sqlite3_column_blob(row.value,1)),std::size_t(n),directory,c);
        if(record->id!=id) throw std::invalid_argument("pixel checkpoint descriptor id mismatch");result.push_back(std::move(record));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a->id<b->id;});return result;
}

#else
void write_batch_checkpoint(sqlite3*,const BatchCheckpoint&,const std::atomic<bool>*) {throw std::runtime_error("batch checkpoints require project storage");}
BatchCheckpoint read_batch_checkpoint(const std::string&,const std::atomic<bool>*,bool,std::size_t) {throw std::runtime_error("batch checkpoints require project storage");}
std::vector<std::shared_ptr<const PixelStreamRecord>> batch_pixel_records(const std::string&,std::uint64_t,const std::set<std::uint64_t>&,std::size_t,const std::atomic<bool>*) {throw std::runtime_error("batch checkpoints require project storage");}
#endif
}
namespace qpx {
std::string batch_checkpoint_csv(const std::string& path) {return detail::read_batch_checkpoint(detail::export_path(path),nullptr,true).csv;}
void save_batch_checkpoint_csv(const std::string& path,const std::string& output) {
    const auto source=detail::export_path(path),destination=detail::export_path(output);detail::ReaderFileGuard guard(source);const auto m=detail::read_batch_checkpoint(source,nullptr,true);
    auto protect=[&] {detail::guard_file_output(source,guard.version()->identity,destination);if(!m.source_locator.empty()) detail::guard_file_output(m.source_locator,m.source_file_identity,destination);};
    protect();detail::AtomicExportFile file(destination,256*1024*1024);file.write(m.csv.data(),m.csv.size());guard.verify();protect();file.commit();
}
}
