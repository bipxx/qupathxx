#include "json_string.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "export_output.hpp"
#include "utf8_text.hpp"
#include "file_identity_contract.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <random>
#include <algorithm>
#include <set>
#include <filesystem>
namespace qpx {
namespace {

std::string version_json(const FileVersion& v) {
    return "{\"path\":"+detail::json_string(v.path)+",\"bytes\":"+std::to_string(v.bytes)+",\"identity_scheme\":"+detail::json_string(v.identity_scheme)+",\"identity\":"+detail::json_string(v.identity)+",\"stamp_scheme\":"+detail::json_string(v.stamp_scheme)+",\"modified_token\":"+detail::json_string(v.modified_token)+",\"change_token\":"+detail::json_string(v.change_token)+",\"observed_unix_ms\":"+std::to_string(v.observed_unix_ms)+"}";
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
    return source.scope=="compound_files" || source.scope=="directory_store"?"matching_primary_metadata":"matching_metadata";
}
}
void FileVersion::validate() const {
    detail::bounded_utf8(path,32768);
    // Historical observations must remain readable across operating systems.
    const bool absolute=detail::absolute_source_path(path);
    if(!absolute || bytes>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || observed_unix_ms>std::uint64_t(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("invalid observed file version");
    detail::validate_file_identity(identity_scheme,identity,stamp_scheme,modified_token,change_token);
}
std::size_t FileVersion::payload_bytes() const {return sizeof(*this)+path.capacity()+identity_scheme.capacity()+identity.capacity()+stamp_scheme.capacity()+modified_token.capacity()+change_token.capacity();}
std::size_t ReaderSource::payload_bytes() const {
    std::size_t bytes=sizeof(*this)+generation.capacity()+reader.capacity()+locator.capacity()+scope.capacity()+directory_sha256.capacity()+(primary?primary->payload_bytes():0);
    bytes+=companions.capacity()*sizeof(FileVersion);
    for(const auto& file:companions) bytes+=file.payload_bytes()-sizeof(FileVersion);
    return bytes;
}
void ReaderSource::validate() const {
    detail::bounded_utf8(generation,128);detail::bounded_utf8(reader,256);detail::bounded_utf8(locator,32768);
    if(generation.empty() || reader.empty() || companions.size()>63 || payload_bytes()>128*1024) throw std::invalid_argument("invalid reader source identity");
    if(scope!="compound_files" && !companions.empty()) throw std::invalid_argument("companions require compound file scope");
    if(scope!="directory_store" && !directory_sha256.empty())throw std::invalid_argument("Directory inventory requires directory scope");
    if(scope=="memory") {if(primary || !locator.empty()) throw std::invalid_argument("invalid memory reader source");}
    else if(scope=="unopened") {if(primary || locator.empty()) throw std::invalid_argument("invalid unopened reader source");}
    else if(scope=="single_file" || scope=="primary_file") {if(!primary || locator!=primary->path) throw std::invalid_argument("invalid file reader source");primary->validate();}
    else if(scope=="compound_files") {
        if(!primary || locator!=primary->path || companions.empty()) throw std::invalid_argument("invalid compound reader source");
        primary->validate();std::set<std::pair<std::string,std::string>> identities{{primary->identity_scheme,primary->identity}};
        std::string previous;
        for(const auto& file:companions) {
            file.validate();
            if(file.path==primary->path || (!previous.empty() && file.path<=previous) || !identities.emplace(file.identity_scheme,file.identity).second)
                throw std::invalid_argument("duplicate or unordered compound reader dependency");
            previous=file.path;
        }
    }
    else if(scope=="directory_store") {
        if(!primary || !detail::absolute_source_path(locator) || directory_sha256.size()!=64 || !std::all_of(directory_sha256.begin(),directory_sha256.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))throw std::invalid_argument("Invalid directory source identity");
        primary->validate();
        const auto portable=[](std::string p){std::replace(p.begin(),p.end(),'\\','/');return std::filesystem::u8path(p).lexically_normal().generic_u8string();};
        auto prefix=portable(locator);if(prefix.back()!='/')prefix+='/';const auto file=portable(primary->path);
        if(file.size()<=prefix.size() || file.compare(0,prefix.size(),prefix))throw std::invalid_argument("Directory metadata must be inside its store");
    }
    else throw std::invalid_argument("invalid reader dependency scope");
}
std::shared_ptr<const ReaderSource> make_reader_source(std::string reader,std::string locator,std::string scope,std::shared_ptr<const FileVersion> primary) {
    auto value=std::make_shared<ReaderSource>();value->generation=generation();value->reader=std::move(reader);value->locator=std::move(locator);value->scope=std::move(scope);value->primary=std::move(primary);value->validate();return value;
}
std::shared_ptr<const ReaderSource> make_compound_reader_source(std::string reader,std::string locator,std::shared_ptr<const FileVersion> primary,std::vector<FileVersion> companions) {
    if(companions.empty() || companions.size()>63) throw std::invalid_argument("compound reader requires 1..63 companions");
    std::sort(companions.begin(),companions.end(),[](const auto& a,const auto& b){return a.path<b.path;});
    auto value=std::make_shared<ReaderSource>();value->generation=generation();value->reader=std::move(reader);value->locator=std::move(locator);value->scope="compound_files";
    value->primary=std::move(primary);value->companions=std::move(companions);value->validate();return value;
}
SourceFileAssociation associate_source_file(const ReaderSource& source,const FileFingerprint& fingerprint) {
    source.validate();fingerprint.validate();return {source,fingerprint,status(source,fingerprint)};
}
std::shared_ptr<const ReaderSource> make_directory_reader_source(std::string reader,std::string locator,std::shared_ptr<const FileVersion> primary,std::string inventory) {
    auto value=std::make_shared<ReaderSource>();value->generation=generation();value->reader=std::move(reader);value->locator=std::move(locator);value->scope="directory_store";value->primary=std::move(primary);value->directory_sha256=std::move(inventory);value->validate();return value;
}
std::string reader_source_json(const ReaderSource& source) {
    source.validate();std::string out="{\"schema\":\"qpx.reader-source/"+std::string(source.scope=="directory_store"?"3":source.companions.empty()?"1":"2")+"\",\"generation\":"+detail::json_string(source.generation)+",\"reader\":"+detail::json_string(source.reader)+",\"locator\":"+detail::json_string(source.locator)+",\"scope\":"+detail::json_string(source.scope)+",\"primary\":"+(source.primary?version_json(*source.primary):"null");
    if(!source.companions.empty()) {out+=",\"companions\":[";bool comma=false;for(const auto& file:source.companions) {if(comma) out+=',';comma=true;out+=version_json(file);}out+=']';}
    if(source.scope=="directory_store")out+=",\"directory_sha256\":"+detail::json_string(source.directory_sha256);
    return out+"}";
}
std::string source_file_association_json(const SourceFileAssociation& a) {
    const auto checked=associate_source_file(a.source,a.fingerprint);if(a.status!=checked.status) throw std::invalid_argument("invalid source association status");
    auto fingerprint=file_fingerprint_json(a.fingerprint);if(!fingerprint.empty() && fingerprint.back()=='\n') fingerprint.pop_back();
    return "{\"schema\":\"qpx.source-file-association/"+std::string(a.source.scope=="directory_store"?"3":a.source.companions.empty()?"1":"2")+"\",\"status\":"+detail::json_string(a.status)+",\"source\":"+reader_source_json(a.source)+",\"fingerprint\":"+fingerprint+"}\n";
}
namespace detail {
bool same_reader_files(const ReaderSource& a,const ReaderSource& b) {
    auto same=[](const FileVersion& x,const FileVersion& y) {
        return x.identity_scheme==y.identity_scheme && x.identity==y.identity && x.bytes==y.bytes &&
            x.stamp_scheme==y.stamp_scheme && x.modified_token==y.modified_token && x.change_token==y.change_token;
    };
    if(a.locator!=b.locator || a.scope!=b.scope || a.directory_sha256!=b.directory_sha256 || !a.primary || !b.primary || !same(*a.primary,*b.primary) || a.companions.size()!=b.companions.size()) return false;
    for(std::size_t i=0;i<a.companions.size();++i) if(a.companions[i].path!=b.companions[i].path || !same(a.companions[i],b.companions[i])) return false;
    return true;
}
void save_source_association(const SourceFileAssociation& value,const std::string& path,const std::atomic<bool>* cancelled,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {
    validate_export_policy(policy);guard_export_output(path,protected_paths);
    const auto json=source_file_association_json(value);
    auto guard=[&] {
        guard_file_output(value.fingerprint.path,value.fingerprint.identity,path);
        if(value.source.scope=="directory_store") {guard_file_output(value.source.locator,"",path);}else if(!value.source.locator.empty()) {
            const auto& primary=value.source.primary;
            const auto identity=primary && primary->identity_scheme==value.fingerprint.identity_scheme?primary->identity:std::string{};
            guard_file_output(value.source.locator,identity,path);
        }
        for(const auto& file:value.source.companions) guard_file_output(file.path,file.identity_scheme==value.fingerprint.identity_scheme?file.identity:std::string{},path);
    };
    guard();AtomicExportFile file(path,2*1024*1024,cancelled);
    file.write(json.data(),json.size());export_checkpoint(cancelled);guard();commit_export_output(file,path,policy,protected_paths);
}
}
void save_source_file_association(const SourceFileAssociation& value,const std::string& path) {detail::save_source_association(value,detail::export_path(path),nullptr);}
}
