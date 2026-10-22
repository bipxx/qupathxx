#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include "file_identity_contract.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <random>
namespace qpx {
namespace {
std::string quoted(const std::string& value) {
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {if(c=='\"' || c=='\\') {out+='\\';out+=char(c);}else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'\"';
}
std::string version_json(const FileVersion& v) {
    return "{\"path\":"+quoted(v.path)+",\"bytes\":"+std::to_string(v.bytes)+",\"identity_scheme\":"+quoted(v.identity_scheme)+",\"identity\":"+quoted(v.identity)+",\"stamp_scheme\":"+quoted(v.stamp_scheme)+",\"modified_token\":"+quoted(v.modified_token)+",\"change_token\":"+quoted(v.change_token)+",\"observed_unix_ms\":"+std::to_string(v.observed_unix_ms)+"}";
}
std::string generation() {
    static const std::string prefix=[] {std::random_device random;constexpr char hex[]="0123456789abcdef";std::string value;for(int i=0;i<32;++i) value+=hex[random()&15];return value+":";}();
    static std::atomic<std::uint64_t> counter{0};auto current=counter.load();
    do {if(current==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("reader generation exhausted");} while(!counter.compare_exchange_weak(current,current+1));
#ifdef _WIN32
    const auto process=GetCurrentProcessId();
#else
    const auto process=::getpid();
#endif
    const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
    return prefix+std::to_string(process)+":"+std::to_string(tick)+":"+std::to_string(current+1);
}
std::string status(const ReaderSource& source,const FileFingerprint& fingerprint) {
    if(!source.primary) return "unobserved";
    const auto& v=*source.primary;
    if(v.identity_scheme!=fingerprint.identity_scheme || v.stamp_scheme!=fingerprint.stamp_scheme) return "incompatible_scheme";
    if(v.identity!=fingerprint.identity) return "different_file";
    if(v.bytes!=fingerprint.bytes || v.modified_token!=fingerprint.modified_token || v.change_token!=fingerprint.change_token) return "version_changed";
    return "matching_metadata";
}
}
void FileVersion::validate() const {
    detail::bounded_utf8(path,32768);
    // Historical observations must remain readable across operating systems.
    const bool absolute=!path.empty() && (path[0]=='/' || (path.size()>=2 && path[0]=='\\' && path[1]=='\\') ||
        (path.size()>=3 && ((path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z')) && path[1]==':' && (path[2]=='/' || path[2]=='\\')));
    if(!absolute || bytes>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || observed_unix_ms>std::uint64_t(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("invalid observed file version");
    detail::validate_file_identity(identity_scheme,identity,stamp_scheme,modified_token,change_token);
}
std::size_t FileVersion::payload_bytes() const {return sizeof(*this)+path.capacity()+identity_scheme.capacity()+identity.capacity()+stamp_scheme.capacity()+modified_token.capacity()+change_token.capacity();}
std::size_t ReaderSource::payload_bytes() const {return sizeof(*this)+generation.capacity()+reader.capacity()+locator.capacity()+scope.capacity()+(primary?primary->payload_bytes():0);}
void ReaderSource::validate() const {
    detail::bounded_utf8(generation,128);detail::bounded_utf8(reader,256);detail::bounded_utf8(locator,32768);
    if(generation.empty() || reader.empty() || payload_bytes()>128*1024) throw std::invalid_argument("invalid reader source identity");
    if(scope=="memory") {if(primary || !locator.empty()) throw std::invalid_argument("invalid memory reader source");}
    else if(scope=="unopened") {if(primary || locator.empty()) throw std::invalid_argument("invalid unopened reader source");}
    else if(scope=="single_file" || scope=="primary_file") {if(!primary || locator!=primary->path) throw std::invalid_argument("invalid file reader source");primary->validate();}
    else throw std::invalid_argument("invalid reader dependency scope");
}
std::shared_ptr<const ReaderSource> make_reader_source(std::string reader,std::string locator,std::string scope,std::shared_ptr<const FileVersion> primary) {
    auto value=std::make_shared<ReaderSource>();value->generation=generation();value->reader=std::move(reader);value->locator=std::move(locator);value->scope=std::move(scope);value->primary=std::move(primary);value->validate();return value;
}
SourceFileAssociation associate_source_file(const ReaderSource& source,const FileFingerprint& fingerprint) {
    source.validate();fingerprint.validate();return {source,fingerprint,status(source,fingerprint)};
}
std::string reader_source_json(const ReaderSource& source) {
    source.validate();return "{\"schema\":\"qpx.reader-source/1\",\"generation\":"+quoted(source.generation)+",\"reader\":"+quoted(source.reader)+",\"locator\":"+quoted(source.locator)+",\"scope\":"+quoted(source.scope)+",\"primary\":"+(source.primary?version_json(*source.primary):"null")+"}";
}
std::string source_file_association_json(const SourceFileAssociation& a) {
    const auto checked=associate_source_file(a.source,a.fingerprint);if(a.status!=checked.status) throw std::invalid_argument("invalid source association status");
    auto fingerprint=file_fingerprint_json(a.fingerprint);if(!fingerprint.empty() && fingerprint.back()=='\n') fingerprint.pop_back();
    return "{\"schema\":\"qpx.source-file-association/1\",\"status\":"+quoted(a.status)+",\"source\":"+reader_source_json(a.source)+",\"fingerprint\":"+fingerprint+"}\n";
}
namespace detail {
void save_source_association(const SourceFileAssociation& value,const std::string& path,const std::atomic<bool>* cancelled) {
    const auto json=source_file_association_json(value);
    auto guard=[&] {
        guard_file_output(value.fingerprint.path,value.fingerprint.identity,path);
        if(!value.source.locator.empty()) {
            const auto& primary=value.source.primary;
            const auto identity=primary && primary->identity_scheme==value.fingerprint.identity_scheme?primary->identity:std::string{};
            guard_file_output(value.source.locator,identity,path);
        }
    };
    guard();AtomicExportFile file(path,2*1024*1024,cancelled);
    file.write(json.data(),json.size());export_checkpoint(cancelled);guard();file.commit();
}
}
void save_source_file_association(const SourceFileAssociation& value,const std::string& path) {detail::save_source_association(value,detail::export_path(path),nullptr);}
}
