#include "expression_matrix_internal.hpp"
#include "reader_file_guard.hpp"
#include "expression_matrix_writer.hpp"
#include "matrix_format_registry.hpp"
#include "project_database_output.hpp"
#include "stored_zip_read.hpp"
#include <fstream>
#include <charconv>
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
bool detail::matrix_market_gzip_available() noexcept {
#if defined(QUPATHXX_HAS_PROJECT) && defined(QUPATHXX_HAS_MATRIX_MARKET_GZIP)
    return true;
#else
    return false;
#endif
}
#ifdef QUPATHXX_HAS_PROJECT
namespace {
using namespace detail::matrix;
namespace fs=std::filesystem;
namespace zip=detail::stored_zip;

struct MatrixArchive {
    detail::ReaderFileGuard guard;
    zip::Input input;
    std::array<zip::ExchangeMember,3> members;
    MatrixArchive(const std::string& path,std::uint64_t limit,const std::atomic<bool>* cancelled)
        :guard(detail::export_path(path)),input(fs::u8path(guard.version()->path),guard.version()->bytes,
            cancelled,"Matrix Market ZIP") {
        const auto maximum=std::min<std::uint64_t>(limit,1024ull*1024*1024);
        if(guard.version()->bytes>maximum)
            throw WorkflowCaptureLimit("Matrix Market ZIP exceeds byte limit");
        constexpr std::array<std::string_view,3> names{"matrix.mtx","features.tsv","barcodes.tsv"};
        members=input.exchange_members(names);
        std::uint64_t expanded=0;
        for(const auto& entry:members) {
            expanded+=entry.member.size;
            if(expanded>maximum)throw WorkflowCaptureLimit("Expanded Matrix Market ZIP exceeds byte limit");
        }
        guard.verify();
    }
};
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
struct CompressedInput {
    z_stream stream{};
    std::array<unsigned char,65536> buffer;
    std::uint64_t decoded=0;
    explicit CompressedInput(bool zip) {
        if(inflateInit2(&stream,zip?-MAX_WBITS:MAX_WBITS+16)!=Z_OK)
            throw std::runtime_error("Cannot initialize Matrix Market decompressor");
    }
    ~CompressedInput(){inflateEnd(&stream);}
};
#endif
class Lines {
    std::optional<detail::ReaderFileGuard> guard_;std::ifstream input_;Sha256 hash_;
    MatrixArchive* archive_=nullptr;
    zip::ExchangeMember member_{};
    zip::Checksum crc_;
    std::array<char,65536> buffer_;std::size_t at_=0,end_=0;
    std::uint64_t bytes_=0,limit_;bool eof_=false;const std::atomic<bool>* cancel_;
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
    std::unique_ptr<CompressedInput> compressed_;
#endif
    std::size_t read_raw(char* data,std::size_t size) {
        workflow_checkpoint(cancel_);
        if(archive_) {
            const auto remaining=member_.encoded_size-bytes_;
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(size,remaining));
            if(count){archive_->input.seek(member_.data_offset+bytes_);archive_->input.read(data,count);bytes_+=count;}
            return count;
        }
        if(input_.eof())return 0;
        input_.read(data,size);
        const auto read=std::size_t(input_.gcount());
        if(input_.bad() || (input_.fail() && !input_.eof()))throw WorkflowInputChanged("Cannot read expression matrix input");
        if(read>guard_->version()->bytes-bytes_)throw WorkflowInputChanged("Expression matrix input grew");
        bytes_+=read;
        if(read)hash_.update(data,read);
        return read;
    }
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
    std::size_t inflate_chunk() {
        auto& stream=compressed_->stream;
        auto* source=reinterpret_cast<char*>(compressed_->buffer.data());
        stream.next_out=reinterpret_cast<unsigned char*>(buffer_.data());
        stream.avail_out=static_cast<uInt>(buffer_.size());
        while(stream.avail_out==buffer_.size()) {
            workflow_checkpoint(cancel_);
            if(!stream.avail_in) {
                stream.next_in=compressed_->buffer.data();
                stream.avail_in=static_cast<uInt>(read_raw(source,compressed_->buffer.size()));
            }
            const auto available=stream.avail_in;
            const auto status=inflate(&stream,Z_NO_FLUSH);
            if(status==Z_STREAM_END) {
                if(stream.avail_in || read_raw(source,1))
                    throw std::invalid_argument(archive_?"Trailing bytes in Matrix Market ZIP Deflate member":
                        "Matrix Market gzip input must contain exactly one member");
                eof_=true;
                break;
            }
            if(status!=Z_OK || (stream.avail_in==available && stream.avail_out==buffer_.size()))
                throw std::invalid_argument(archive_?"Invalid or truncated Matrix Market ZIP Deflate member":
                    "Invalid or truncated Matrix Market gzip input");
        }
        const auto decoded=buffer_.size()-stream.avail_out;
        if(decoded>limit_-compressed_->decoded)throw WorkflowCaptureLimit(archive_?
            "Expanded Matrix Market ZIP member exceeds declared size":"Expanded Matrix Market gzip input exceeds byte limit");
        compressed_->decoded+=decoded;
        return decoded;
    }
#endif
    bool fill() {
        if(eof_)return false;
        at_=0;
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
        if(compressed_){
            end_=inflate_chunk();
            if(archive_)crc_.write(buffer_.data(),end_);
            return end_!=0;
        }
#endif
        if(archive_) {
            end_=read_raw(buffer_.data(),buffer_.size());
            crc_.write(buffer_.data(),end_);
            eof_=bytes_==member_.encoded_size;
            return end_!=0;
        }
        end_=read_raw(buffer_.data(),buffer_.size());
        eof_=input_.eof();
        return end_!=0;
    }
public:
    Lines(const std::string& path,std::uint64_t limit,const std::atomic<bool>* c)
        :guard_(std::in_place,detail::export_path(path)),input_(fs::u8path(guard_->version()->path),std::ios::binary),
            crc_{c},limit_(limit),cancel_(c) {
        if(!input_)throw std::runtime_error("Cannot open expression input");
        if(guard_->version()->bytes>limit)throw WorkflowCaptureLimit("Expression input file exceeds byte limit");
        guard_->verify();
        const auto suffix=fs::u8path(path).extension().u8string();
        if(suffix.size()==3 && suffix[0]=='.' && (suffix[1]=='g' || suffix[1]=='G') && (suffix[2]=='z' || suffix[2]=='Z')) {
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
            compressed_=std::make_unique<CompressedInput>(false);
#else
            throw std::invalid_argument("Matrix Market gzip input requires QUPATHXX_BUILD_MATRIX_MARKET_GZIP");
#endif
        }
    }
    Lines(MatrixArchive& archive,std::size_t member,const std::atomic<bool>* c)
        :archive_(&archive),member_(archive.members[member]),crc_{c,0xffffffffu,0,&hash_},
            limit_(member_.member.size),cancel_(c) {
        if(member_.method==8){
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
            compressed_=std::make_unique<CompressedInput>(true);
#else
            throw std::invalid_argument("Matrix Market ZIP Deflate requires QUPATHXX_BUILD_MATRIX_MARKET_GZIP");
#endif
        }
    }
    std::size_t extra_work_bytes()const {
#ifdef QUPATHXX_HAS_MATRIX_MARKET_GZIP
        // Account for both the compressed input buffer and zlib's decoder/window.
        return compressed_?sizeof(CompressedInput)+64u*1024:0;
#else
        return 0;
#endif
    }
    bool next(std::string& line){line.clear();while(true){if(at_==end_ && !fill()){if(line.empty())return false;break;}const auto* begin=buffer_.data()+at_;const auto* stop=static_cast<const char*>(std::memchr(begin,'\n',end_-at_));const auto n=stop?std::size_t(stop-begin):end_-at_;if(n>4096-line.size())throw std::invalid_argument("Expression input line exceeds4096 bytes");line.append(begin,n);at_+=n;if(stop){++at_;break;}}if(!line.empty() && line.back()=='\r')line.pop_back();if(line.find('\0')!=std::string::npos)throw std::invalid_argument("Expression input contains NUL");return true;}
    bool matrix_line(std::string& line,std::string* value_domain=nullptr){
        while(next(line)){
            const auto at=line.find_first_not_of(" \t");
            if(at==std::string::npos)continue;
            if(line[at]!='%')return true;
            if(value_domain && line.compare(at,market_domain_comment.size(),market_domain_comment)==0){
                if(!value_domain->empty())throw std::invalid_argument("Duplicate Matrix Market value-domain comment");
                *value_domain=line.substr(at+market_domain_comment.size());validate_market_domain(*value_domain);
            }
        }
        return false;
    }
    std::string finish() {
        const auto expected=archive_?member_.encoded_size:guard_->version()->bytes;
        if(at_!=end_ || !eof_ || bytes_!=expected)throw WorkflowInputChanged("Expression input not completely consumed");
        if(archive_ && crc_.bytes!=member_.member.size)
            throw std::invalid_argument("Invalid Matrix Market ZIP expanded member size");
        if(archive_ && ~crc_.crc!=member_.member.crc)
            throw std::invalid_argument("Invalid Matrix Market ZIP member CRC");
        verify();return hash_.finish();
    }
    void verify()const {
        try{(archive_?archive_->guard:*guard_).verify();}
        catch(const std::exception& e){throw WorkflowInputChanged(std::string("Expression input changed: ")+e.what());}
    }
    std::uint64_t bytes()const{return archive_?crc_.bytes:bytes_;}
};
void feature_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="feature_id"))throw std::invalid_argument("Expected feature_id TSV header");for(std::uint64_t i=0;i<m.features;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression feature catalogue");std::string id,name,type;if(format==ExpressionCatalogueFormat::Tenx){const auto parts=detail::spatial_triple(line,true);id=detail::spatial_identifier(parts[0]);name=detail::spatial_identifier(parts[1]);type=detail::spatial_identifier(parts[2]);}else{id=detail::spatial_identifier(line);name=id;type="Gene Expression";}writer.feature_row(i,id,name,type);}if(input.next(line))throw std::invalid_argument("Extra expression feature records");}
void observation_rows(Writer& writer,Lines& input,const ExpressionMatrixMetadata& m,ExpressionCatalogueFormat format,const std::atomic<bool>* c){std::string line;if(format==ExpressionCatalogueFormat::Qpx && (!input.next(line) || line!="observation_id"))throw std::invalid_argument("Expected observation_id TSV header");for(std::uint64_t i=0;i<m.observations;++i){workflow_checkpoint(c);if(!input.next(line))throw std::invalid_argument("Truncated expression observation catalogue");writer.observation_row(i,detail::spatial_identifier(line));}if(input.next(line))throw std::invalid_argument("Extra expression observation records");}
void protect_matrix_index_destination(const std::string& destination) {
    if(fs::exists(fs::u8path(destination)))throw WorkflowOutputExists("Expression sidecar output already exists");
    if(detail::has_sqlite_companion(destination))throw WorkflowOutputExists("Expression sidecar destination has a SQLite companion");
}
double market_real_value(std::string_view value) {
    if(!value.empty() && value.front()=='+'){
        value.remove_prefix(1);
        if(!value.empty() && value.front()=='-')throw std::invalid_argument("Invalid finite Matrix Market real value");
    }
    double parsed=0;
    const auto end=value.data()+value.size();
    const auto number=std::from_chars(value.data(),end,parsed,std::chars_format::general);
    if(number.ec!=std::errc{} || number.ptr!=end || !std::isfinite(parsed))
        throw std::invalid_argument("Invalid finite Matrix Market real value");
    return parsed;
}
ExpressionMatrixImportResult index_matrix_lines(Lines& entries,Lines& feature_input,Lines& observation_input,
    const std::string& destination,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled,
    std::chrono::steady_clock::time_point began,bool real) {
    std::string line,label;
    if(!entries.next(line) || line!=(real?"%%MatrixMarket matrix coordinate real general":"%%MatrixMarket matrix coordinate integer general") ||
       !entries.matrix_line(line,real?&label:nullptr))
        throw std::invalid_argument(real?"Expected Matrix Market coordinate real general and dimensions":
            "Expected Matrix Market coordinate integer general and dimensions");
    const auto dims=detail::spatial_triple(line);ExpressionMatrixMetadata m;m.features=detail::spatial_unsigned(dims[0]);m.observations=detail::spatial_unsigned(dims[1]);m.entries=detail::spatial_unsigned(dims[2]);m.input_profile=options.format==ExpressionCatalogueFormat::Tenx?"tenx-tsv":"qpx-tsv";
    if(real){
        m.domain=ExpressionValueDomain::Continuous;
        m.value_domain=label.empty()?"matrix-market-real":label;
        m.input_profile=options.format==ExpressionCatalogueFormat::Tenx?"matrix-market-real-tenx/1":"matrix-market-real-qpx/1";
    }
    const auto decoder_work=entries.extra_work_bytes()+feature_input.extra_work_bytes()+observation_input.extra_work_bytes();
    Writer writer(destination,m,options,decoder_work,cancelled);feature_rows(writer,feature_input,m,options.format,cancelled);observation_rows(writer,observation_input,m,options.format,cancelled);
    const auto feature_hash=feature_input.finish(),observation_hash=observation_input.finish();
    for(std::uint64_t i=0;i<m.entries;++i){
        if((i&1023)==0)workflow_checkpoint(cancelled);
        if(!entries.matrix_line(line))throw std::invalid_argument("Truncated expression matrix entries");
        const auto values=detail::spatial_triple(line);
        const auto row=detail::spatial_unsigned(values[0]),column=detail::spatial_unsigned(values[1]);
        if(!row || !column)throw std::invalid_argument("Expression matrix index outside dimensions");
        if(real)writer.continuous(row-1,column-1,market_real_value(values[2]));
        else writer.count(row-1,column-1,detail::spatial_unsigned(values[2]));
    }
    if(entries.matrix_line(line))throw std::invalid_argument("Extra expression matrix entries");const auto matrix_hash=entries.finish();auto result=writer.finish({matrix_hash,feature_hash,observation_hash},entries.bytes(),[&]{entries.verify();feature_input.verify();observation_input.verify();protect_matrix_index_destination(destination);});result.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();return result;
}
ExpressionMatrixImportResult import_market_files(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled,bool real){
    options.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();
    const auto destination=detail::export_path(store);protect_matrix_index_destination(destination);
    Lines entries(matrix,options.file_bytes,cancelled),feature_input(features,options.file_bytes,cancelled),observation_input(observations,options.file_bytes,cancelled);
    return index_matrix_lines(entries,feature_input,observation_input,destination,options,cancelled,began,real);
}
}
ExpressionMatrixImportResult import_expression_matrix(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled){
    return import_market_files(matrix,features,observations,store,options,cancelled,false);
}
ExpressionMatrixImportResult import_real_expression_matrix(const std::string& matrix,const std::string& features,const std::string& observations,const std::string& store,const ExpressionMatrixImportOptions& options,const std::atomic<bool>* cancelled){
    return import_market_files(matrix,features,observations,store,options,cancelled,true);
}
ExpressionMatrixImportResult detail::import_expression_matrix_archive(const MatrixMarketArchiveImportRequest& request,
    const std::atomic<bool>* cancelled) {
    auto options=request.limits;options.format=ExpressionCatalogueFormat::Tenx;
    options.validate();workflow_checkpoint(cancelled);const auto began=std::chrono::steady_clock::now();
    const auto destination=export_path(request.store);protect_matrix_index_destination(destination);
    MatrixArchive archive(request.archive,options.file_bytes,cancelled);
    Lines entries(archive,0,cancelled),features(archive,1,cancelled),observations(archive,2,cancelled);
    return index_matrix_lines(entries,features,observations,destination,options,cancelled,began,request.real);
}
#else
ExpressionMatrixImportResult import_expression_matrix(const std::string&,const std::string&,const std::string&,const std::string&,const ExpressionMatrixImportOptions&,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
ExpressionMatrixImportResult import_real_expression_matrix(const std::string&,const std::string&,const std::string&,const std::string&,const ExpressionMatrixImportOptions&,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
ExpressionMatrixImportResult detail::import_expression_matrix_archive(const MatrixMarketArchiveImportRequest&,
    const std::atomic<bool>*) {throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
#endif
}
