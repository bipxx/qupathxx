#include "expression_matrix_internal.hpp"
#include "reader_file_guard.hpp"
#include "expression_matrix_writer.hpp"
#include "project_database_output.hpp"
#include <fstream>
#include <cstring>
#include <chrono>
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
#include <zlib.h>
#endif
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
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
struct GzipInput {
    z_stream stream{};
    std::array<unsigned char,65536> buffer;
    std::uint64_t decoded=0;
    explicit GzipInput() {
        if(inflateInit2(&stream,MAX_WBITS+16)!=Z_OK)
            throw std::runtime_error("Cannot initialize Matrix Market gzip decoder");
    }
    ~GzipInput(){inflateEnd(&stream);}
};
#endif
class Lines {
    detail::ReaderFileGuard guard_;std::ifstream input_;Sha256 hash_;
    std::array<char,65536> buffer_;std::size_t at_=0,end_=0;
    std::uint64_t bytes_=0,limit_;bool eof_=false;const std::atomic<bool>* cancel_;
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
    std::unique_ptr<GzipInput> gzip_;
#endif
    std::size_t read_raw(char* data,std::size_t size) {
        workflow_checkpoint(cancel_);
        if(input_.eof())return 0;
        input_.read(data,size);
        const auto read=std::size_t(input_.gcount());
        if(input_.bad() || (input_.fail() && !input_.eof()))throw WorkflowInputChanged("Cannot read expression matrix input");
        if(read>guard_.version()->bytes-bytes_)throw WorkflowInputChanged("Expression matrix input grew");
        bytes_+=read;
        if(read)hash_.update(data,read);
        return read;
    }
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
    std::size_t inflate_chunk() {
        auto& stream=gzip_->stream;
        auto* source=reinterpret_cast<char*>(gzip_->buffer.data());
        stream.next_out=reinterpret_cast<unsigned char*>(buffer_.data());
        stream.avail_out=static_cast<uInt>(buffer_.size());
        while(stream.avail_out==buffer_.size()) {
            workflow_checkpoint(cancel_);
            if(!stream.avail_in) {
                stream.next_in=gzip_->buffer.data();
                stream.avail_in=static_cast<uInt>(read_raw(source,gzip_->buffer.size()));
            }
            const auto available=stream.avail_in;
            const auto status=inflate(&stream,Z_NO_FLUSH);
            if(status==Z_STREAM_END) {
                if(stream.avail_in || read_raw(source,1))
                    throw std::invalid_argument("Matrix Market gzip input must contain exactly one member");
                eof_=true;
                break;
            }
            if(status!=Z_OK || (stream.avail_in==available && stream.avail_out==buffer_.size()))
                throw std::invalid_argument("Invalid or truncated Matrix Market gzip input");
        }
        const auto decoded=buffer_.size()-stream.avail_out;
        if(decoded>limit_-gzip_->decoded)throw WorkflowCaptureLimit("Expanded Matrix Market gzip input exceeds byte limit");
        gzip_->decoded+=decoded;
        return decoded;
    }
#endif
    bool fill() {
        if(eof_)return false;
        at_=0;
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
        if(gzip_) {
            end_=inflate_chunk();
            return end_!=0;
        }
#endif
        end_=read_raw(buffer_.data(),buffer_.size());
        eof_=input_.eof();
        return end_!=0;
    }
public:
    Lines(const std::string& path,std::uint64_t limit,const std::atomic<bool>* c)
        :guard_(detail::export_path(path)),input_(fs::u8path(guard_.version()->path),std::ios::binary),limit_(limit),cancel_(c) {
        if(!input_)throw std::runtime_error("Cannot open expression input");
        if(guard_.version()->bytes>limit)throw WorkflowCaptureLimit("Expression input file exceeds byte limit");
        guard_.verify();
        const auto suffix=fs::u8path(path).extension().u8string();
        if(suffix.size()==3 && suffix[0]=='.' && (suffix[1]=='g' || suffix[1]=='G') && (suffix[2]=='z' || suffix[2]=='Z')) {
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
            gzip_=std::make_unique<GzipInput>();
#else
            throw std::invalid_argument("Matrix Market gzip input requires QUPATHXX_BUILD_MATRIX_MARKET_GZIP");
#endif
        }
    }
    std::size_t extra_work_bytes()const {
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
        // Account for both the compressed input buffer and zlib's decoder/window.
        return gzip_?sizeof(GzipInput)+64u*1024:0;
#else
        return 0;
#endif
    }
    bool next(std::string& line){line.clear();while(true){if(at_==end_ && !fill()){if(line.empty())return false;break;}const auto* begin=buffer_.data()+at_;const auto* stop=static_cast<const char*>(std::memchr(begin,'\n',end_-at_));const auto n=stop?std::size_t(stop-begin):end_-at_;if(n>4096-line.size())throw std::invalid_argument("Expression input line exceeds4096 bytes");line.append(begin,n);at_+=n;if(stop){++at_;break;}}if(!line.empty() && line.back()=='\r')line.pop_back();if(line.find('\0')!=std::string::npos)throw std::invalid_argument("Expression input contains NUL");return true;}
    bool matrix_line(std::string& line){while(next(line)){const auto at=line.find_first_not_of(" \t");if(at!=std::string::npos && line[at]!='%')return true;}return false;}
    std::string finish(){if(at_!=end_ || !eof_ || bytes_!=guard_.version()->bytes)throw WorkflowInputChanged("Expression input not completely consumed");verify();return hash_.finish();}
    void verify()const{try{guard_.verify();}catch(const std::exception& e){throw WorkflowInputChanged(std::string("Expression input changed: ")+e.what());}}
    std::uint64_t bytes()const{return bytes_;}
};
void feature_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="feature_id"))throw std::invalid_argument("Expected feature_id TSV header");for(std::uint64_t i=0;i<m.features;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression feature catalogue");std::string id,name,type;if(format==ExpressionCatalogueFormat::Tenx){const auto parts=detail::spatial_triple(line,true);id=detail::spatial_identifier(parts[0]);name=detail::spatial_identifier(parts[1]);type=detail::spatial_identifier(parts[2]);}else{id=detail::spatial_identifier(line);name=id;type="Gene Expression";}writer.feature_row(i,id,name,type);}if(input.next(line))throw std::invalid_argument("Extra expression feature records");}
void observation_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="observation_id"))throw std::invalid_argument("Expected observation_id TSV header");for(std::uint64_t i=0;i<m.observations;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression observation catalogue");writer.observation_row(i,detail::spatial_identifier(line));}if(input.next(line))throw std::invalid_argument("Extra expression observation records");}
}
ExpressionMatrixImportResult import_expression_matrix(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled){
    options.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto destination=detail::export_path(store);if(fs::exists(fs::u8path(destination)))throw WorkflowOutputExists("Expression sidecar output already exists");
    const auto protect_destination=[&] {
        if(detail::has_sqlite_companion(destination))
            throw WorkflowOutputExists("Expression sidecar destination has a SQLite companion");
    };
    protect_destination();
    Lines entries(matrix,options.file_bytes,cancelled),feature_input(features,options.file_bytes,cancelled),observation_input(observations,options.file_bytes,cancelled);std::string line;
    if(!entries.next(line) || line!="%%MatrixMarket matrix coordinate integer general" || !entries.matrix_line(line))throw std::invalid_argument("Expected Matrix Market coordinate integer general and dimensions");
    const auto dims=detail::spatial_triple(line);ExpressionMatrixMetadata m;m.features=detail::spatial_unsigned(dims[0]);m.observations=detail::spatial_unsigned(dims[1]);m.entries=detail::spatial_unsigned(dims[2]);m.input_profile=options.format==ExpressionCatalogueFormat::Tenx?"tenx-tsv":"qpx-tsv";
    const auto decoder_work=entries.extra_work_bytes()+feature_input.extra_work_bytes()+observation_input.extra_work_bytes();
    Writer writer(destination,m,options,decoder_work,cancelled);feature_rows(writer,feature_input,m,options.format,cancelled);observation_rows(writer,observation_input,m,options.format,cancelled);
    const auto feature_hash=feature_input.finish(),observation_hash=observation_input.finish();
    for(std::uint64_t i=0;i<m.entries;++i){if((i&1023)==0)workflow_checkpoint(cancelled);if(!entries.matrix_line(line))throw std::invalid_argument("Truncated expression matrix entries");const auto values=detail::spatial_triple(line);const auto row=detail::spatial_unsigned(values[0]),column=detail::spatial_unsigned(values[1]),value=detail::spatial_unsigned(values[2]);if(!row || !column)throw std::invalid_argument("Expression matrix index outside dimensions");writer.count(row-1,column-1,value);}
    if(entries.matrix_line(line))throw std::invalid_argument("Extra expression matrix entries");const auto matrix_hash=entries.finish();auto result=writer.finish({matrix_hash,feature_hash,observation_hash},entries.bytes(),[&]{entries.verify();feature_input.verify();observation_input.verify();protect_destination();});result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
}
#else
ExpressionMatrixImportResult import_expression_matrix(const std::string&,const std::string&,const std::string&,const std::string&,const ExpressionMatrixImportOptions&,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
#endif
}
