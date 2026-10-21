#include "qupathxx/project_recovery.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <random>
#include <stdexcept>
#include <thread>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <locale>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace qpx {
namespace {
namespace fs=std::filesystem;
std::string token() {
    std::random_device random; std::string result; constexpr char hex[]="0123456789abcdef";
    for(int i=0;i<32;++i) result.push_back(hex[random()&15]); return result;
}
fs::path location(const std::string& value) {
    if(value.empty() || value.size()>32768 || value.find('\0')!=std::string::npos) throw std::invalid_argument("invalid recovery path");
    return fs::absolute(fs::u8path(value)).lexically_normal();
}
bool same_path(const fs::path& a,const fs::path& b) {
    std::error_code error; auto equivalent=fs::equivalent(a,b,error); if(!error) return equivalent;
    auto left=fs::weakly_canonical(a,error); if(error) left=a.lexically_normal();
    error.clear(); auto right=fs::weakly_canonical(b,error); if(error) right=b.lexically_normal();
#ifdef _WIN32
    return CompareStringOrdinal(left.c_str(),-1,right.c_str(),-1,TRUE)==CSTR_EQUAL;
#else
    return left==right;
#endif
}
bool name_prefix(const fs::path& path,const std::string& prefix) {
#ifdef _WIN32
    auto name=path.filename().native(),start=fs::u8path(prefix).native();
    return name.size()>=start.size() && (start.empty() || CompareStringOrdinal(name.c_str(),static_cast<int>(start.size()),start.c_str(),static_cast<int>(start.size()),TRUE)==CSTR_EQUAL);
#else
    return path.filename().u8string().find(prefix)==0;
#endif
}
bool recovery_extension(const fs::path& path) {
    auto ext=path.extension().u8string();
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return ext==".qpxrecover";
}
fs::path cache_directory() {
#ifdef _WIN32
    wchar_t* root=nullptr; std::size_t length=0; _wdupenv_s(&root,&length,L"LOCALAPPDATA");
    std::unique_ptr<wchar_t,decltype(&std::free)> owned(root,&std::free);
    return (root && *root?fs::path(root):fs::temp_directory_path())/"qupathxx"/"recovery";
#else
    auto* root=std::getenv("XDG_STATE_HOME");
    if(root && *root) return fs::u8path(root)/"qupathxx"/"recovery";
    auto* home=std::getenv("HOME");
    return (home && *home?fs::u8path(home)/".local"/"state":fs::temp_directory_path())/"qupathxx"/"recovery";
#endif
}
class FileLock {
    int fd_=-1;
public:
    explicit FileLock(const fs::path& path) {
        auto lock=path; lock+=fs::u8path(".lock");
#ifdef _WIN32
        if(_wsopen_s(&fd_,lock.c_str(),_O_CREAT|_O_RDWR|_O_BINARY|_O_NOINHERIT,_SH_DENYRW,_S_IREAD|_S_IWRITE)!=0) fd_=-1;
#else
        fd_=::open(lock.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);
        if(fd_>=0 && ::flock(fd_,LOCK_EX|LOCK_NB)!=0) { ::close(fd_); fd_=-1; }
#endif
        if(fd_<0) throw std::runtime_error("recovery file is locked or its directory is unavailable");
    }
    ~FileLock() {
#ifdef _WIN32
        if(fd_>=0) _close(fd_);
#else
        if(fd_>=0) ::close(fd_);
#endif
    }
};
bool publish(const fs::path& source,const fs::path& destination,const std::atomic<bool>& cancelled) {
#ifdef _WIN32
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);
    for(;;) {
        if(cancelled.load()) return false;
        if(MoveFileExW(source.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) break;
        auto error=GetLastError();
        if((error!=ERROR_SHARING_VIOLATION && error!=ERROR_LOCK_VIOLATION) || std::chrono::steady_clock::now()>=deadline)
            throw std::runtime_error("recovery atomic replacement failed");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
#else
    if(cancelled.load()) return false;
    if(::rename(source.c_str(),destination.c_str())!=0) throw std::runtime_error("recovery atomic replacement failed");
    auto directory=::open(destination.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(directory<0) throw std::runtime_error("recovery directory flush failed after replacement");
    auto code=::fsync(directory); ::close(directory);
    if(code!=0) throw std::runtime_error("recovery directory flush failed after replacement");
#endif
    return true;
}
struct WorkDirectory {
    fs::path path,file;
    explicit WorkDirectory(const fs::path& destination) {
        path=destination; path+=fs::u8path(".work-"+token());
        if(!fs::create_directory(path)) throw std::runtime_error("cannot create owned recovery work directory");
        file=path/"snapshot.qpxproj";
    }
    ~WorkDirectory() {
        // Remove only known files in our exclusively created directory. Never
        // recursively traverse a directory that could contain someone else's data.
        std::error_code error;
        for(const auto& suffix:{"","-journal","-wal","-shm"}) { auto name=file; name+=fs::u8path(suffix); fs::remove(name,error); }
        fs::remove(path,error);
    }
};
void check_owner(const std::string& path,const std::string& identity,const std::string& session,const std::string& base) {
    auto info=Project::inspect_recovery(path);
    if(info.identity!=identity || info.session!=session || (info.base_path.empty()!=base.empty()) ||
       (!base.empty() && !same_path(location(info.base_path),location(base))))
        throw std::invalid_argument("recovery destination belongs to another project, base path or session");
}
void erase_owned(const std::string& path,const std::string& identity,const std::string& session,const std::string& base) {
    if(path.empty() || !fs::exists(location(path))) return;
    FileLock lock(location(path)); check_owner(path,identity,session,base);
    if(!fs::remove(location(path))) throw std::runtime_error("recovery removal failed");
}
std::string fingerprint(const fs::path& path) {
    // Streaming FNV-1a plus byte count is a consistency stamp for cooperating
    // writers, not a cryptographic/tamper-detection or authorization boundary.
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("cannot read recovery checkpoint for review");
    std::array<char,65536> buffer{};
    std::uint64_t hash=14695981039346656037ull,bytes=0;
    while(input.read(buffer.data(),buffer.size()) || input.gcount()) {
        const auto count=input.gcount(); bytes+=static_cast<std::uint64_t>(count);
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]); hash*=1099511628211ull;}
    }
    if(!input.eof()) throw std::runtime_error("recovery checkpoint review read failed");
    std::ostringstream result; result.imbue(std::locale::classic());
    result<<std::hex<<std::setfill('0')<<std::setw(16)<<hash<<':'<<std::dec<<bytes;
    return result.str();
}
}
struct ProjectAutosave::Impl {
    struct Stamp {
        std::string instance,identity,base;
        std::uint64_t revision=0,generation=0;
        std::map<std::uint64_t,std::uint64_t> documents;
        bool operator==(const Stamp& b) const { return instance==b.instance && identity==b.identity && base==b.base && revision==b.revision && generation==b.generation && documents==b.documents; }
    };
    struct Result { bool published=false,cancelled=false,cleared=false; double write_ms=0; std::string error; std::uintmax_t file_bytes=0; fs::file_time_type modified{}; };
    std::size_t budget;
    std::string session=token(),identity,base;
    AutosaveStats stats;
    std::future<Result> future;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::optional<Stamp> last,inflight;
    bool clear_pending=false;
    std::uintmax_t file_bytes=0;
    fs::file_time_type modified{};
    explicit Impl(std::size_t value):budget(value) {}
};
ProjectAutosave::ProjectAutosave(std::size_t budget):impl_(std::make_unique<Impl>(budget)) {
    if(!budget || budget>4ull*1024*1024*1024) throw std::invalid_argument("autosave snapshot budget must be between 1 byte and 4 GiB");
}
ProjectAutosave::~ProjectAutosave() { cancel(); try { wait(); } catch(...) {} }
bool ProjectAutosave::busy() const { return impl_->future.valid(); }
const AutosaveStats& ProjectAutosave::stats() const { return impl_->stats; }
const std::string& ProjectAutosave::session() const { return impl_->session; }
std::string ProjectAutosave::default_path(const Project& project) const {
    auto name=".recovery-"+impl_->session+".qpxrecover";
    return project.path().empty()?(cache_directory()/(project.identity()+name)).u8string():project.path()+name;
}
bool ProjectAutosave::request(const Project& project,const std::string& requested) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    poll(); if(busy() || !project.dirty()) return false;
    auto destination=location(requested.empty()?default_path(project):requested);
    if(!recovery_extension(destination)) throw std::invalid_argument("recovery destination must use .qpxrecover");
    if(!project.path_.empty() && same_path(destination,location(project.path_))) throw std::invalid_argument("recovery cannot overwrite the primary project");
    for(const auto& [id,e]:project.entries_) if(!e.info.embedded && same_path(destination,location(e.info.source_path))) throw std::invalid_argument("recovery cannot overwrite an image source");
    auto path=destination.u8string();
    Impl::Stamp stamp{project.instance_identity_,project.identity_,project.path_,project.revision_,project.generation_,{}};
    for(const auto& [id,e]:project.entries_) stamp.documents.emplace(id,e.document->revision());
    if(impl_->last && *impl_->last==stamp && impl_->stats.path==path && fs::exists(destination) &&
       fs::file_size(destination)==impl_->file_bytes && fs::last_write_time(destination)==impl_->modified) return false;
    auto start=std::chrono::steady_clock::now(); std::size_t bytes=0;
    auto snapshot=project.snapshot_for_save(impl_->budget,bytes);
    RecoveryInfo info; info.path=path; info.identity=project.identity_; info.session=impl_->session;
    info.base_path=project.path_; info.base_generation=project.generation_; info.images=project.entries_.size();
    for(const auto& [id,e]:project.entries_) info.objects+=e.document->object_count();
    info.created_unix_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto cancelled=std::make_shared<std::atomic<bool>>(false);
    auto capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    const bool create_cache=requested.empty() && project.path_.empty();
    auto future=std::async(std::launch::async,[snapshot=std::move(snapshot),info,destination,cancelled,create_cache]() mutable {
        Impl::Result result; auto start=std::chrono::steady_clock::now();
        try {
            if(create_cache) fs::create_directories(destination.parent_path());
            FileLock lock(destination);
            if(fs::exists(destination)) check_owner(info.path,info.identity,info.session,info.base_path);
            WorkDirectory work(destination);
            if(!cancelled->load()) snapshot->save_recovery(work.file.u8string(),destination.parent_path().u8string(),info,cancelled.get());
            if(cancelled->load()) result.cancelled=true;
            else {
                result.published=publish(work.file,destination,*cancelled);
                result.cancelled=!result.published;
                if(result.published) { result.file_bytes=fs::file_size(destination); result.modified=fs::last_write_time(destination); }
            }
        } catch(const std::exception& e) { if(cancelled->load()) result.cancelled=true; else result.error=e.what(); }
        snapshot.reset(); // Large canonical maps are released on the worker, not by GUI future.get().
        result.write_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); return result;
    });
    impl_->future=std::move(future); impl_->cancelled=std::move(cancelled); impl_->inflight=std::move(stamp);
    impl_->identity=project.identity_; impl_->base=project.path_;
    impl_->stats.state="writing"; impl_->stats.path=path; impl_->stats.error.clear(); impl_->stats.snapshot_bytes=bytes; impl_->stats.capture_ms=capture_ms;
    return true;
}
void ProjectAutosave::cancel() { if(impl_->cancelled) impl_->cancelled->store(true); }
void ProjectAutosave::clear() {
    cancel(); impl_->clear_pending=true; impl_->last.reset();
    if(!busy()) poll();
}
AutosaveStats ProjectAutosave::poll() {
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=impl_->future.get(); impl_->stats.write_ms=result.write_ms;
            if(!result.error.empty()) { impl_->stats.state="error"; impl_->stats.error=std::move(result.error); }
            else if(result.published) {
                ++impl_->stats.completed; impl_->last=std::move(impl_->inflight); impl_->stats.state="ready";
                impl_->file_bytes=result.file_bytes; impl_->modified=result.modified;
            }
            else if(result.cleared) { impl_->stats.state="idle"; impl_->stats.error.clear(); }
            else impl_->stats.state="cancelled";
        } catch(const std::exception& e) { impl_->stats.state="error"; impl_->stats.error=e.what(); }
        impl_->inflight.reset();
    }
    if(impl_->clear_pending && !busy()) {
        impl_->clear_pending=false;
        if(impl_->stats.path.empty()) { impl_->stats.state="idle"; impl_->stats.error.clear(); }
        else {
            auto path=impl_->stats.path,identity=impl_->identity,session=impl_->session,base=impl_->base;
            impl_->future=std::async(std::launch::async,[path,identity,session,base] {
                Impl::Result result;
                try { erase_owned(path,identity,session,base); result.cleared=true; }
                catch(const std::exception& e) { result.error=e.what(); }
                return result;
            });
            impl_->stats.state="clearing";
        }
    }
    return impl_->stats;
}
AutosaveStats ProjectAutosave::wait() {
    do { if(busy()) impl_->future.wait(); poll(); } while(busy() || impl_->clear_pending);
    return impl_->stats;
}
void Project::discard_recovery(const std::string& path,const std::string& identity) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    auto target=location(path); FileLock lock(target); auto info=inspect_recovery(target.u8string());
    if(info.identity!=identity) throw std::invalid_argument("recovery project identity does not match");
    if(!fs::remove(target)) throw std::runtime_error("recovery removal failed");
}
RecoveryInfo Project::review_recovery(const std::string& path) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    const auto target=location(path); FileLock lock(target);
    auto info=inspect_recovery(target.u8string()); info.fingerprint=fingerprint(target);
    return info;
}
void Project::discard_recovery_checked(const RecoveryInfo& reviewed) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    if(reviewed.fingerprint.empty()) throw std::invalid_argument("review the recovery checkpoint before deleting it");
    const auto target=location(reviewed.path); FileLock lock(target);
    const auto current=inspect_recovery(target.u8string());
    if(current.identity!=reviewed.identity || current.session!=reviewed.session || fingerprint(target)!=reviewed.fingerprint)
        throw std::runtime_error("recovery checkpoint changed since review; review it again before deleting");
    if(!fs::remove(target)) throw std::runtime_error("recovery removal failed");
}
std::vector<RecoveryInfo> Project::find_recoveries(const std::string& base,std::size_t limit) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    if(!limit || limit>1024) throw std::invalid_argument("recovery discovery limit must be between 1 and 1024");
    auto directory=base.empty()?cache_directory():location(base).parent_path();
    std::string prefix=base.empty()?"":location(base).filename().u8string()+".recovery-";
    std::vector<RecoveryInfo> result;
    auto better=[](const auto& a,const auto& b){return a.created_unix_ms!=b.created_unix_ms?a.created_unix_ms>b.created_unix_ms:a.path<b.path;};
    if(!fs::exists(directory)) return result;
    for(const auto& item:fs::directory_iterator(directory)) {
        if(!recovery_extension(item.path()) || !name_prefix(item.path(),prefix) || !item.is_regular_file()) continue;
        RecoveryInfo info;
        try {
            info=inspect_recovery(item.path().u8string());
            if(info.base_path.empty()!=base.empty() || (!base.empty() && !same_path(location(info.base_path),location(base)))) continue;
        } catch(const std::exception& e) { info.path=item.path().u8string(); info.error=e.what(); }
        result.push_back(std::move(info)); std::push_heap(result.begin(),result.end(),better);
        if(result.size()>limit) { std::pop_heap(result.begin(),result.end(),better); result.pop_back(); }
    }
    std::sort(result.begin(),result.end(),better);
    return result;
}
}
