#pragma once
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "spatial_count_text.hpp"
#include "workflow_file_version.hpp"
#include "atomic_export.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <cmath>
#include <cstring>
#ifdef QUPATHXX_HAS_PROJECT
#include <sqlite3.h>
namespace qpx::detail::matrix {
constexpr std::size_t workspace=1024u*1024;
inline void work_limit(std::size_t n){if(n<4u*1024*1024 || n>1024ull*1024*1024)throw std::invalid_argument("Expression matrix work must be4..1024MiB");}
inline void text(Sha256& h,const std::string& s){h.integer(s.size());h.update(s.data(),s.size());}
inline void digest(const std::string& s){if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("Invalid expression matrix digest");}
inline std::array<unsigned char,8> encoded(std::uint64_t value){std::array<unsigned char,8> b;for(unsigned i=0;i<8;++i)b[i]=static_cast<unsigned char>(value>>(i*8));return b;}
inline std::uint64_t decoded(const void* p){const auto* b=static_cast<const unsigned char*>(p);std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[i])<<(i*8);return v;}
inline bool legacy(const ExpressionMatrixMetadata& m){return m.input_profile=="qpx-tsv" || m.input_profile=="tenx-tsv";}
inline std::uint64_t real_bits(double value){if(!std::isfinite(value))throw std::invalid_argument("Expression continuous/coordinate value must be finite");std::uint64_t bits;std::memcpy(&bits,&value,8);return bits;}
inline double real_value(std::uint64_t bits){double value;std::memcpy(&value,&bits,8);if(!std::isfinite(value))throw std::invalid_argument("Expression continuous/coordinate value must be finite");return value;}
inline std::string coordinate_hash(std::uint64_t index,const std::string& id,Point p,const std::string& frame_sha){Sha256 h;text(h,"qpx.expression-coordinate/1");text(h,frame_sha);h.integer(index);text(h,id);h.integer(real_bits(p.x));h.integer(real_bits(p.y));return h.finish();}
inline std::string source_hash(const ExpressionMatrixMetadata& m){Sha256 h;text(h,legacy(m)?"qpx.expression-matrix/1":m.input_matrix.empty()?"qpx.expression-matrix/2":"qpx.expression-matrix/3");text(h,m.input_profile);text(h,m.value_domain);h.integer(m.features);h.integer(m.observations);h.integer(m.entries);for(const auto& s:m.input_sha256)text(h,s);if(!legacy(m)){h.integer(std::uint64_t(m.domain));text(h,m.spatial_frame?m.spatial_frame->sha256():"");text(h,m.spatial_sha256);if(!m.input_matrix.empty()){text(h,m.input_matrix);text(h,m.input_spatial);}}return h.finish();}
inline void bind_file_identity(ExpressionMatrixMetadata& m,const std::string& file){Sha256 h;text(h,"qpx.expression-source-file/1");text(h,m.source_sha256);text(h,file);m.source_sha256=h.finish();}
class PairHash {
    Sha256& hash_;std::array<unsigned char,65536> buffer_;std::size_t used_=0;
public:
    explicit PairHash(Sha256& hash):hash_(hash){}
    PairHash(const PairHash&)=delete;
    void flush(){if(used_){hash_.update(buffer_.data(),used_);used_=0;}}
    void add(std::uint64_t observation,std::uint64_t value){if(used_==buffer_.size())flush();const auto a=encoded(observation),b=encoded(value);std::copy(a.begin(),a.end(),buffer_.begin()+used_);std::copy(b.begin(),b.end(),buffer_.begin()+used_+8);used_+=16;}
};
class GeneHash {
    Sha256 hash_;PairHash pairs_{hash_};
public:
    GeneHash(const ExpressionFeature& f,const std::string& observations){text(hash_,"qpx.expression-gene/1");hash_.integer(f.index);text(hash_,f.id);text(hash_,f.name);text(hash_,f.type);text(hash_,observations);}
    void add(std::uint64_t observation,std::uint64_t value){pairs_.add(observation,value);}
    std::string finish(){pairs_.flush();return hash_.finish();}
};
inline void no_sidecars(const std::string& path){for(const char* suffix:{"-wal","-shm","-journal"})if(std::filesystem::exists(std::filesystem::u8path(path+suffix)))throw WorkflowInputChanged("Expression matrix has SQLite sidecars; close/recover the writer first");}
inline void same_version(const FileVersion& a,const FileVersion& b){if(a.identity_scheme!=b.identity_scheme || a.identity!=b.identity || a.bytes!=b.bytes || a.stamp_scheme!=b.stamp_scheme || a.modified_token!=b.modified_token || a.change_token!=b.change_token)throw WorkflowInputChanged("Expression matrix file changed");}
class Database {
    sqlite3* db_=nullptr;std::size_t limit_,cache_,peak_=0;
    const std::atomic<bool>* cancelled_;
    static int progress(void* p){const auto* c=static_cast<const std::atomic<bool>*>(p);return c && c->load(std::memory_order_relaxed);}
public:
    Database(const std::string& path,bool write,std::size_t work,const std::atomic<bool>* c):limit_(work),cache_(std::min<std::size_t>(128u*1024*1024,work/2)),cancelled_(c){
        work_limit(work);workflow_checkpoint(c);const auto flags=(write?SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:SQLITE_OPEN_READONLY)|SQLITE_OPEN_NOMUTEX;
        const auto rc=sqlite3_open_v2(path.c_str(),&db_,flags,nullptr);if(rc!=SQLITE_OK){const auto message=db_?sqlite3_errmsg(db_):"Cannot open SQLite expression matrix";std::string error(message);if(db_)sqlite3_close(db_);db_=nullptr;throw std::runtime_error(error);}
        try{sqlite3_extended_result_codes(db_,1);sqlite3_db_config(db_,SQLITE_DBCONFIG_LOOKASIDE,nullptr,0,0);if(!write)sqlite3_db_config(db_,SQLITE_DBCONFIG_DEFENSIVE,1,nullptr);sqlite3_limit(db_,SQLITE_LIMIT_LENGTH,1024*1024);sqlite3_limit(db_,SQLITE_LIMIT_SQL_LENGTH,65536);sqlite3_progress_handler(db_,4096,progress,const_cast<std::atomic<bool>*>(c));exec("PRAGMA trusted_schema=OFF; PRAGMA temp_store=FILE; PRAGMA cache_size=-"+std::to_string(cache_/1024));if(!write)exec("PRAGMA query_only=ON;");admit(0);}catch(...){sqlite3_close(db_);db_=nullptr;throw;}
    }
    ~Database(){if(db_)sqlite3_close_v2(db_);}
    Database(const Database&)=delete;
    sqlite3* get()const{return db_;}
    void check(int rc){if(rc==SQLITE_OK || rc==SQLITE_ROW || rc==SQLITE_DONE)return;workflow_checkpoint(cancelled_);if((rc&255)==SQLITE_CONSTRAINT)throw std::invalid_argument(std::string("Expression matrix duplicate/constraint: ")+sqlite3_errmsg(db_));throw std::runtime_error(std::string("Expression SQLite: ")+sqlite3_errmsg(db_));}
    void exec(const std::string& sql){check(sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,nullptr));}
    void admit(std::size_t buffers){int cache=0,unused=0,schema=0,statements=0;sqlite3_db_status(db_,SQLITE_DBSTATUS_CACHE_USED,&cache,&unused,0);sqlite3_db_status(db_,SQLITE_DBSTATUS_SCHEMA_USED,&schema,&unused,0);sqlite3_db_status(db_,SQLITE_DBSTATUS_STMT_USED,&statements,&unused,0);std::size_t bytes=workspace;workflow_capture_charge(bytes,std::max(cache_,std::size_t(cache)),limit_);workflow_capture_charge(bytes,std::size_t(schema)+std::size_t(statements),limit_);workflow_capture_charge(bytes,buffers,limit_);peak_=std::max(peak_,bytes);}
    std::size_t peak()const{return peak_;}
};
class Statement {
    Database& db_;sqlite3_stmt* s_=nullptr;
public:
    Statement(Database& db,const char* sql):db_(db){db_.check(sqlite3_prepare_v2(db.get(),sql,-1,&s_,nullptr));}
    ~Statement(){if(s_)sqlite3_finalize(s_);}
    Statement(const Statement&)=delete;
    void number(int i,std::uint64_t n){if(n>std::uint64_t(INT64_MAX))throw std::invalid_argument("Expression index exceeds signed SQLite integer range");db_.check(sqlite3_bind_int64(s_,i,static_cast<sqlite3_int64>(n)));}
    void text(int i,const std::string& s){db_.check(sqlite3_bind_text(s_,i,s.data(),int(s.size()),SQLITE_TRANSIENT));}
    void value(int i,const std::array<unsigned char,8>& b){db_.check(sqlite3_bind_blob(s_,i,b.data(),8,SQLITE_STATIC));}
    bool step(){const auto code=sqlite3_step(s_);db_.check(code);return code==SQLITE_ROW;}
    void reset(){db_.check(sqlite3_reset(s_));}
    std::uint64_t number(int c)const{if(sqlite3_column_type(s_,c)!=SQLITE_INTEGER || sqlite3_column_int64(s_,c)<0)throw std::invalid_argument("Expression matrix index is not an unsigned integer");return std::uint64_t(sqlite3_column_int64(s_,c));}
    std::string text(int c,std::size_t limit=256)const{if(sqlite3_column_type(s_,c)!=SQLITE_TEXT)throw std::invalid_argument("Expression matrix text type differs");const auto bytes=sqlite3_column_bytes(s_,c);if(bytes<0 || std::size_t(bytes)>limit)throw std::invalid_argument("Expression matrix text exceeds limit");std::string s(reinterpret_cast<const char*>(sqlite3_column_text(s_,c)),std::size_t(bytes));bounded_utf8(s,limit);return s;}
    std::uint64_t value(int c)const{if(sqlite3_column_type(s_,c)!=SQLITE_BLOB || sqlite3_column_bytes(s_,c)!=8)throw std::invalid_argument("Expression numeric value must contain exactly eight stored bytes");return decoded(sqlite3_column_blob(s_,c));}
};
inline ExpressionFeature feature(Statement& s,bool complete=true){ExpressionFeature f;f.index=s.number(0);f.id=spatial_identifier(s.text(1));f.name=spatial_identifier(s.text(2));f.type=spatial_identifier(s.text(3));f.sha256=s.text(4,64);if(complete)digest(f.sha256);return f;}
inline void range(std::uint64_t first,std::size_t n,std::uint64_t size){if(n>10000 || first>size || n>size-first)throw std::out_of_range("Expression catalogue page exceeds range/10000 records");}
void store_meta(Database&,const ExpressionMatrixMetadata&);
ExpressionMatrixMetadata load_meta(Database&);
}
#endif
