#include "expression_matrix_internal.hpp"
#include "reader_file_guard.hpp"
#include "expression_matrix_writer.hpp"
#include <fstream>
#include <cstring>
#include <chrono>
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
void feature_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="feature_id"))throw std::invalid_argument("Expected feature_id TSV header");for(std::uint64_t i=0;i<m.features;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression feature catalogue");std::string id,name,type;if(format==ExpressionCatalogueFormat::Tenx){const auto parts=detail::spatial_triple(line,true);id=detail::spatial_identifier(parts[0]);name=detail::spatial_identifier(parts[1]);type=detail::spatial_identifier(parts[2]);}else{id=detail::spatial_identifier(line);name=id;type="Gene Expression";}writer.feature_row(i,id,name,type);}if(input.next(line))throw std::invalid_argument("Extra expression feature records");}
void observation_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="observation_id"))throw std::invalid_argument("Expected observation_id TSV header");for(std::uint64_t i=0;i<m.observations;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression observation catalogue");writer.observation_row(i,detail::spatial_identifier(line));}if(input.next(line))throw std::invalid_argument("Extra expression observation records");}
}
ExpressionMatrixImportResult import_expression_matrix(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled){
    options.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();const auto destination=detail::export_path(store);if(fs::exists(fs::u8path(destination)))throw WorkflowOutputExists("Expression sidecar output already exists");
    Lines entries(matrix,options.file_bytes,cancelled),feature_input(features,options.file_bytes,cancelled),observation_input(observations,options.file_bytes,cancelled);std::string line;
    if(!entries.next(line) || line!="%%MatrixMarket matrix coordinate integer general" || !entries.matrix_line(line))throw std::invalid_argument("Expected Matrix Market coordinate integer general and dimensions");
    const auto dims=detail::spatial_triple(line);ExpressionMatrixMetadata m;m.features=detail::spatial_unsigned(dims[0]);m.observations=detail::spatial_unsigned(dims[1]);m.entries=detail::spatial_unsigned(dims[2]);m.input_profile=options.format==ExpressionCatalogueFormat::Tenx?"tenx-tsv":"qpx-tsv";
    Writer writer(destination,m,options,0,cancelled);feature_rows(writer,feature_input,m,options.format,cancelled);observation_rows(writer,observation_input,m,options.format,cancelled);
    const auto feature_hash=feature_input.finish(),observation_hash=observation_input.finish();
    for(std::uint64_t i=0;i<m.entries;++i){if((i&1023)==0)workflow_checkpoint(cancelled);if(!entries.matrix_line(line))throw std::invalid_argument("Truncated expression matrix entries");const auto values=detail::spatial_triple(line);const auto row=detail::spatial_unsigned(values[0]),column=detail::spatial_unsigned(values[1]),value=detail::spatial_unsigned(values[2]);if(!row || !column)throw std::invalid_argument("Expression matrix index outside dimensions");writer.count(row-1,column-1,value);}
    if(entries.matrix_line(line))throw std::invalid_argument("Extra expression matrix entries");const auto matrix_hash=entries.finish();auto result=writer.finish({matrix_hash,feature_hash,observation_hash},entries.bytes(),[&]{entries.verify();feature_input.verify();observation_input.verify();});result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
}
#else
ExpressionMatrixImportResult import_expression_matrix(const std::string&,const std::string&,const std::string&,const std::string&,const ExpressionMatrixImportOptions&,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
#endif
}
