#include "expression_matrix_internal.hpp"
#include "reader_file_guard.hpp"
#include <fstream>
#include <cstring>
#include <chrono>
#include <random>
namespace qpx {
void ExpressionMatrixImportOptions::validate()const{if((format!=ExpressionCatalogueFormat::Qpx && format!=ExpressionCatalogueFormat::Tenx) || !file_bytes || file_bytes>16ull*1024*1024*1024 || store_bytes<4096 || store_bytes>64ull*1024*1024*1024 || work_bytes<4u*1024*1024 || work_bytes>1024ull*1024*1024 || !max_features || max_features>1000000 || !max_observations || max_observations>1000000 || max_entries>1000000000)throw std::invalid_argument("Invalid expression matrix import limits/profile");}
bool has_expression_matrix_storage(){
#ifdef QUPATHXX_HAS_PROJECT
return true;
#else
return false;
#endif
}
#ifdef QUPATHXX_HAS_PROJECT
namespace {
using namespace detail::matrix;
namespace fs=std::filesystem;
class Lines {
    detail::ReaderFileGuard guard_;std::ifstream input_;Sha256 hash_;
    std::array<char,65536> buffer_;std::size_t at_=0,end_=0;
    std::uint64_t bytes_=0;bool eof_=false;const std::atomic<bool>* cancel_;
    bool fill(){if(eof_)return false;workflow_checkpoint(cancel_);input_.read(buffer_.data(),buffer_.size());end_=std::size_t(input_.gcount());at_=0;if(input_.bad() || (input_.fail() && !input_.eof()))throw WorkflowInputChanged("Cannot read expression matrix input");eof_=input_.eof();bytes_+=end_;if(bytes_>guard_.version()->bytes)throw WorkflowInputChanged("Expression matrix input grew");if(end_)hash_.update(buffer_.data(),end_);return end_!=0;}
public:
    Lines(const std::string& path,std::uint64_t limit,const std::atomic<bool>* c):guard_(detail::export_path(path)),input_(fs::u8path(guard_.version()->path),std::ios::binary),cancel_(c){if(!input_)throw std::runtime_error("Cannot open expression input");if(guard_.version()->bytes>limit)throw WorkflowCaptureLimit("Expression input file exceeds byte limit");guard_.verify();}
    bool next(std::string& line){line.clear();while(true){if(at_==end_ && !fill()){if(line.empty())return false;break;}const auto* begin=buffer_.data()+at_;const auto* stop=static_cast<const char*>(std::memchr(begin,'\n',end_-at_));const auto n=stop?std::size_t(stop-begin):end_-at_;if(n>4096-line.size())throw std::invalid_argument("Expression input line exceeds4096 bytes");line.append(begin,n);at_+=n;if(stop){++at_;break;}}if(!line.empty() && line.back()=='\r')line.pop_back();if(line.find('\0')!=std::string::npos)throw std::invalid_argument("Expression input contains NUL");return true;}
    bool matrix_line(std::string& line){while(next(line)){const auto at=line.find_first_not_of(" \t");if(at!=std::string::npos && line[at]!='%')return true;}return false;}
    std::string finish(){if(at_!=end_ || !eof_ || bytes_!=guard_.version()->bytes)throw WorkflowInputChanged("Expression input not completely consumed");verify();return hash_.finish();}
    void verify()const{try{guard_.verify();}catch(const std::exception& e){throw WorkflowInputChanged(std::string("Expression input changed: ")+e.what());}}
    std::uint64_t bytes()const{return bytes_;}
};
struct Stage {
    fs::path directory,file;
    explicit Stage(const fs::path& parent){std::random_device random;constexpr char hex[]="0123456789abcdef";for(int i=0;i<16;++i){std::string name=".qpx-expression-";for(int j=0;j<32;++j)name+=hex[random()&15];auto candidate=parent/fs::u8path(name);std::error_code error;if(fs::create_directory(candidate,error)){directory=std::move(candidate);file=directory/"matrix.sqlite";return;}if(error && error!=std::errc::file_exists)throw fs::filesystem_error("Cannot create expression staging directory",candidate,error);}throw std::runtime_error("Cannot allocate expression staging directory");}
    ~Stage(){std::error_code error;for(const char* suffix:{"","-journal","-wal","-shm"}){auto path=file;path+=suffix;fs::remove(path,error);}fs::remove(directory,error);}
};
void feature_rows(Database& db,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){Statement insert(db,"INSERT INTO features VALUES(?,?,?,?, '')");std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="feature_id"))throw std::invalid_argument("Expected feature_id TSV header");for(std::uint64_t i=0;i<m.features;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression feature catalogue");std::string id,name,type;if(format==ExpressionCatalogueFormat::Tenx){const auto parts=detail::spatial_triple(line,true);id=detail::spatial_identifier(parts[0]);name=detail::spatial_identifier(parts[1]);type=detail::spatial_identifier(parts[2]);}else{id=detail::spatial_identifier(line);name=id;type="Gene Expression";}insert.number(1,i);insert.text(2,id);insert.text(3,name);insert.text(4,type);insert.step();insert.reset();if((i&4095)==0)db.admit(0);}if(input.next(line))throw std::invalid_argument("Extra expression feature records");}
void observation_rows(Database& db,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){Statement insert(db,"INSERT INTO observations VALUES(?,?)");std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="observation_id"))throw std::invalid_argument("Expected observation_id TSV header");for(std::uint64_t i=0;i<m.observations;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression observation catalogue");const auto id=detail::spatial_identifier(line);insert.number(1,i);insert.text(2,id);insert.step();insert.reset();if((i&4095)==0)db.admit(0);}if(input.next(line))throw std::invalid_argument("Extra expression observation records");}
void gene_hashes(Database& db,const ExpressionMatrixMetadata& m,const std::atomic<bool>* c){Statement features(db,"SELECT idx,id,name,type,sha256 FROM features ORDER BY idx"),counts(db,"SELECT observation,value FROM counts WHERE feature=? ORDER BY observation"),update(db,"UPDATE features SET sha256=? WHERE idx=?");std::uint64_t entries=0,expected=0;while(features.step()){workflow_checkpoint(c);const auto f=feature(features,false);if(f.index!=expected++)throw std::invalid_argument("Expression feature axis is not contiguous");GeneHash hash(f,m.input_sha256[2]);counts.number(1,f.index);while(counts.step()){if((entries&4095)==0){workflow_checkpoint(c);db.admit(0);}hash.add(counts.number(0),counts.value(1));++entries;}counts.reset();update.text(1,hash.finish());update.number(2,f.index);update.step();update.reset();}if(expected!=m.features || entries!=m.entries)throw std::runtime_error("Expression index entry/feature count differs");}
}
ExpressionMatrixImportResult import_expression_matrix(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled){
    options.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto destination=detail::export_path(store);if(fs::exists(fs::u8path(destination)))throw WorkflowOutputExists("Expression sidecar output already exists");
    Lines entries(matrix,options.file_bytes,cancelled),feature_input(features,options.file_bytes,cancelled),observation_input(observations,options.file_bytes,cancelled);std::string line;
    if(!entries.next(line) || line!="%%MatrixMarket matrix coordinate integer general" || !entries.matrix_line(line))throw std::invalid_argument("Expected Matrix Market coordinate integer general and dimensions");
    const auto dims=detail::spatial_triple(line);ExpressionMatrixImportResult result;auto& m=result.metadata;m.features=detail::spatial_unsigned(dims[0]);m.observations=detail::spatial_unsigned(dims[1]);m.entries=detail::spatial_unsigned(dims[2]);m.input_profile=options.format==ExpressionCatalogueFormat::Tenx?"tenx-tsv":"qpx-tsv";
    if(!m.features || m.features>options.max_features || !m.observations || m.observations>options.max_observations || m.entries>options.max_entries)throw WorkflowCaptureLimit("Expression dimensions/entries exceed import limits");
    Stage stage(fs::u8path(destination).parent_path());{
        Database db(stage.file.u8string(),true,options.work_bytes,cancelled);
        // Private scratch only: any failure discards it. AtomicExportFile flushes
        // the complete copied database before its new-only public commit.
        db.exec("PRAGMA page_size=4096; PRAGMA max_page_count="+std::to_string(options.store_bytes/4096)+"; PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;");
        db.exec("CREATE TABLE metadata(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT, WITHOUT ROWID; CREATE TABLE features(idx INTEGER PRIMARY KEY,id TEXT UNIQUE NOT NULL,name TEXT NOT NULL,type TEXT NOT NULL,sha256 TEXT NOT NULL) STRICT; CREATE TABLE observations(idx INTEGER PRIMARY KEY,id TEXT UNIQUE NOT NULL) STRICT; CREATE TABLE counts(feature INTEGER NOT NULL,observation INTEGER NOT NULL,value BLOB NOT NULL CHECK(length(value)=8),PRIMARY KEY(feature,observation)) STRICT, WITHOUT ROWID; PRAGMA application_id=1364219973; PRAGMA user_version=1; BEGIN;");
        feature_rows(db,feature_input,m,options.format,cancelled);observation_rows(db,observation_input,m,options.format,cancelled);m.input_sha256[1]=feature_input.finish();m.input_sha256[2]=observation_input.finish();
        {Statement insert(db,"INSERT INTO counts VALUES(?,?,?)");for(std::uint64_t i=0;i<m.entries;++i){if((i&1023)==0)workflow_checkpoint(cancelled);if(!entries.matrix_line(line))throw std::invalid_argument("Truncated expression matrix entries");const auto values=detail::spatial_triple(line);const auto row=detail::spatial_unsigned(values[0]),column=detail::spatial_unsigned(values[1]),value=detail::spatial_unsigned(values[2]);if(!row || row>m.features || !column || column>m.observations)throw std::invalid_argument("Expression matrix index outside dimensions");const auto bytes=encoded(value);insert.number(1,row-1);insert.number(2,column-1);insert.value(3,bytes);insert.step();insert.reset();if((i&4095)==0)db.admit(0);}}
        if(entries.matrix_line(line))throw std::invalid_argument("Extra expression matrix entries");m.input_sha256[0]=entries.finish();result.matrix_bytes=entries.bytes();m.source_sha256=source_hash(m);gene_hashes(db,m,cancelled);store_meta(db,m);db.exec("COMMIT;");db.admit(0);result.peak_work_bytes=db.peak();
    }
    entries.verify();feature_input.verify();observation_input.verify();result.file_bytes=fs::file_size(stage.file);if(result.file_bytes>options.store_bytes)throw WorkflowCaptureLimit("Expression sidecar exceeds store byte limit");
    std::ifstream input(stage.file,std::ios::binary);if(!input)throw std::runtime_error("Cannot read completed expression sidecar");detail::AtomicExportFile output(destination,std::size_t(options.store_bytes),cancelled);std::array<char,65536> bytes;Sha256 hash;
    while(input){workflow_checkpoint(cancelled);input.read(bytes.data(),bytes.size());const auto n=std::size_t(input.gcount());output.write(bytes.data(),n);hash.update(bytes.data(),n);}if(!input.eof() || input.bad() || output.bytes()!=result.file_bytes)throw std::runtime_error("Expression staged copy differs");
    entries.verify();feature_input.verify();observation_input.verify();workflow_checkpoint(cancelled);output.commit(false);result.path=destination;result.file_sha256=hash.finish();bind_file_identity(m,result.file_sha256);result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
}
#else
ExpressionMatrixImportResult import_expression_matrix(const std::string&,const std::string&,const std::string&,const std::string&,const ExpressionMatrixImportOptions&,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
#endif
}
