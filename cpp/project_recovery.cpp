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
#include <cmath>
#include <set>
#include <condition_variable>
#include <deque>
#include <mutex>
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
    if(a==b) return true;
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
    explicit FileLock(const fs::path& path,bool create=true) {
        auto lock=path; lock+=fs::u8path(".lock");
#ifdef _WIN32
        if(_wsopen_s(&fd_,lock.c_str(),(create?_O_CREAT:0)|_O_RDWR|_O_BINARY|_O_NOINHERIT,_SH_DENYRW,_S_IREAD|_S_IWRITE)!=0) fd_=-1;
#else
        fd_=::open(lock.c_str(),(create?O_CREAT:0)|O_RDWR|O_CLOEXEC,0600);
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
fs::path session_path(const fs::path& destination) { auto result=destination; result+=fs::u8path(".session"); return result; }
std::string lease_marker(const RecoveryInfo& info) {return "qupathxx recovery lease v1\n"+info.identity+'\n'+info.session+'\n';}
std::string unmanaged_marker(const RecoveryInfo& info) {return "qupathxx recovery unmanaged v1\n"+info.identity+'\n'+info.session+'\n';}
fs::path lease_key(const RecoveryInfo& info) {return cache_directory()/"sessions"/(info.identity+"-"+info.session);}
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
    fs::path path,file,marker;
    explicit WorkDirectory(const fs::path& destination,const RecoveryInfo& info) {
        const auto id=token();
        path=destination; path+=fs::u8path(".work-"+id);
        if(!fs::create_directory(path)) throw std::runtime_error("cannot create owned recovery work directory");
        file=path/"snapshot.qpxproj";
        marker=path/"owner";
        std::ofstream output(marker,std::ios::binary);
        output<<"qupathxx recovery work v2\n"<<id<<'\n'<<info.identity<<'\n'<<info.session<<'\n'<<info.base_path; output.close();
        if(!output) {
            std::error_code error; fs::remove(marker,error); fs::remove(path,error);
            throw std::runtime_error("cannot mark owned recovery work directory");
        }
    }
    ~WorkDirectory() {
        // Remove only known files in our exclusively created directory. Never
        // recursively traverse a directory that could contain someone else's data.
        std::error_code error;
        bool removed=true;
        for(const auto& suffix:{"","-journal","-wal","-shm"}) {
            auto name=file; name+=fs::u8path(suffix); fs::remove(name,error); if(error) removed=false;
        }
        // Keep provenance when a sharing violation prevents immediate teardown.
        if(!removed) return;
        fs::remove(marker,error);
        fs::remove(path,error);
    }
};
bool plain_entry(const fs::path& path,bool directory) {
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DEVICE))) return false;
    return static_cast<bool>(attributes&FILE_ATTRIBUTE_DIRECTORY)==directory;
#else
    const auto status=fs::symlink_status(path);
    return directory?fs::is_directory(status):fs::is_regular_file(status);
#endif
}
bool same_name(const fs::path& a,const fs::path& b) {
#ifdef _WIN32
    return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;
#else
    return a==b;
#endif
}
struct TimedSection {
    double* total;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    explicit TimedSection(double& value):total(&value) {}
    void stop() {if(total) {*total+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); total=nullptr;}}
    ~TimedSection() {stop();}
};
// Filesystem-only prefetch. The coordinator bounds pending records separately;
// the queue also has a hard cap for exception paths. No model/Python callbacks.
class MarkerReaders {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::packaged_task<bool()>> queue_;
    std::vector<std::thread> workers_;
    bool stopping_=false;
    void stop() {
        {std::lock_guard<std::mutex> lock(mutex_); stopping_=true;}
        ready_.notify_all();
        for(auto& worker:workers_) if(worker.joinable()) worker.join();
    }
public:
    static constexpr std::size_t capacity=4;
    MarkerReaders() {
        try {
            workers_.reserve(capacity);
            for(std::size_t i=0;i<capacity;++i) workers_.emplace_back([this] {
                for(;;) {
                    std::packaged_task<bool()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        ready_.wait(lock,[this]{return stopping_ || !queue_.empty();});
                        if(queue_.empty()) return;
                        task=std::move(queue_.front()); queue_.pop_front();
                    }
                    task();
                }
            });
        } catch(...) {stop(); throw;}
    }
    ~MarkerReaders() {stop();}
    template<class Work> std::future<bool> submit(Work&& work) {
        std::packaged_task<bool()> task(std::forward<Work>(work));
        auto future=task.get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopping_ || queue_.size()>=capacity) throw std::runtime_error("recovery marker prefetch queue is full");
            queue_.push_back(std::move(task));
        }
        ready_.notify_one(); return future;
    }
};
std::string read_session_marker(const fs::path& path) {
    std::ifstream input(path,std::ios::binary); std::array<char,256> data{};
    input.read(data.data(),data.size());
    if(!input.eof()) throw std::runtime_error("invalid recovery session marker");
    return std::string(data.data(),static_cast<std::size_t>(input.gcount()));
}
void write_session_marker(const fs::path& destination,const RecoveryInfo& info) {
    const auto marker=session_path(destination);
    std::error_code error; const auto status=fs::symlink_status(marker,error);
    if(error && error!=std::errc::no_such_file_or_directory) throw fs::filesystem_error("cannot inspect recovery session marker",marker,error);
    if(fs::exists(status)) {
        if(!plain_entry(marker,false)) throw std::runtime_error("recovery session marker must be a regular file");
        const auto content=read_session_marker(marker);
        if(content==lease_marker(info)) return;
        const auto hex=[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');};
        const auto known=[&](const std::string& prefix) {
            return content.size()==prefix.size()+66 && content.compare(0,prefix.size(),prefix)==0 &&
                content[prefix.size()+32]=='\n' && content.back()=='\n' &&
                std::all_of(content.begin()+prefix.size(),content.begin()+prefix.size()+32,hex) &&
                std::all_of(content.begin()+prefix.size()+33,content.end()-1,hex);
        };
        if(!known("qupathxx recovery lease v1\n") && !known("qupathxx recovery unmanaged v1\n"))
            throw std::runtime_error("unrecognized recovery session marker is preserved");
    }
    std::ofstream output(marker,std::ios::binary); output<<lease_marker(info); output.close();
    if(!output) throw std::runtime_error("cannot write recovery session marker");
}
void unmanage_session_marker(const fs::path& destination,const RecoveryInfo& info) {
    const auto marker=session_path(destination);
    const auto expected=lease_marker(info);
    if(!fs::exists(marker) || !plain_entry(marker,false) || fs::file_size(marker)!=expected.size() || read_session_marker(marker)!=expected) return;
    // A controller can return to a previous project after losing its lease.
    // Its old matching tag must not make the new unleased publication eligible.
    std::ofstream output(marker,std::ios::binary); output<<unmanaged_marker(info); output.close();
    if(!output) throw std::runtime_error("cannot mark recovery publication as unmanaged");
}
bool work_inventory(const fs::path& path) {
    if(!plain_entry(path,true)) return false;
    std::size_t count=0;
    for(const auto& child:fs::directory_iterator(path)) {
        const auto leaf=child.path().filename().u8string();
        if(++count>5 || !plain_entry(child.path(),false) ||
           (leaf!="owner" && leaf!="snapshot.qpxproj" && leaf!="snapshot.qpxproj-journal" &&
            leaf!="snapshot.qpxproj-wal" && leaf!="snapshot.qpxproj-shm")) return false;
    }
    return plain_entry(path/"owner",false);
}
bool valid_token(const std::string& value) {
    return value.size()==32 && value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
std::optional<RecoveryInfo> work_owner(const fs::path& path,const std::string& id,bool legacy) {
    const auto marker=path/"owner";
    if(fs::file_size(marker)>32900) return std::nullopt;
    std::ifstream input(marker,std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(input)),{});
    if(input.bad()) return std::nullopt;
    if(legacy && content=="qupathxx recovery work v1\n"+id+"\n") return RecoveryInfo{};
    const std::string prefix="qupathxx recovery work v2\n"+id+"\n";
    if(content.compare(0,prefix.size(),prefix)!=0 || content.size()<prefix.size()+66) return std::nullopt;
    RecoveryInfo info;
    info.identity=content.substr(prefix.size(),32); info.session=content.substr(prefix.size()+33,32);
    info.base_path=content.substr(prefix.size()+66);
    if(content[prefix.size()+32]!='\n' || content[prefix.size()+65]!='\n' ||
       !valid_token(info.identity) || !valid_token(info.session) || info.base_path.size()>32768 ||
       info.base_path.find('\0')!=std::string::npos ||
       (!info.base_path.empty() && !fs::u8path(info.base_path).is_absolute())) return std::nullopt;
    return info;
}
bool same_scope(const RecoveryInfo& a,const RecoveryInfo& b) {
    return a.identity==b.identity && a.base_path.empty()==b.base_path.empty() &&
        (a.base_path.empty() || same_path(location(a.base_path),location(b.base_path)));
}
void rescue_work(const fs::path& source,const fs::path& destination) {
    // No replacement: a preexisting rescue (including a link) always wins.
#ifdef _WIN32
    if(!MoveFileExW(source.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot publish rescued recovery without replacing an existing file");
#else
    if(::link(source.c_str(),destination.c_str())!=0)
        throw std::runtime_error("cannot publish rescued recovery without replacing an existing file");
    auto directory=::open(destination.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(directory<0) throw std::runtime_error("rescued recovery directory flush failed");
    auto code=::fsync(directory); ::close(directory);
    if(code!=0) throw std::runtime_error("rescued recovery directory flush failed");
    fs::remove(source);
#endif
}
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
    struct Result { bool published=false,cancelled=false,cleared=false; double write_ms=0; std::string error,work_cleanup_error; std::size_t work_directories_cleaned=0; std::uintmax_t file_bytes=0; fs::file_time_type modified{}; std::shared_ptr<FileLock> lease; RecoveryRetentionStats retention; };
    std::size_t budget;
    std::string session=token(),identity,base;
    std::string lease_identity;
    std::shared_ptr<FileLock> lease;
    RecoveryRetention retention;
    AutosaveStats stats;
    std::future<Result> future;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::shared_ptr<Project::RetentionProgress> progress;
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
RecoveryRetention ProjectAutosave::retention() const { return impl_->retention; }
void ProjectAutosave::set_retention(RecoveryRetention policy) {policy.validate(); impl_->retention=policy;}
void RecoveryRetention::validate() const {
    if(!keep_latest || keep_latest>1024 || !max_deletions || max_deletions>128 ||
       !std::isfinite(max_age_days) || max_age_days<1 || max_age_days>36500)
        throw std::invalid_argument("recovery retention requires 1..1024 newest checkpoints, 1..128 deletions and 1..36500 age days");
}
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
    auto progress=std::make_shared<Project::RetentionProgress>();
    auto capture_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    const bool create_cache=requested.empty() && project.path_.empty();
    auto lease=impl_->lease_identity==info.identity?impl_->lease:nullptr;
    const auto retention=impl_->retention;
    auto future=std::async(std::launch::async,[snapshot=std::move(snapshot),info,destination,cancelled,progress,create_cache,lease=std::move(lease),retention]() mutable {
        Impl::Result result; auto start=std::chrono::steady_clock::now();
        try {
            if(create_cache) fs::create_directories(destination.parent_path());
            if(!lease) {
                try {
                    const auto key=lease_key(info); fs::create_directories(key.parent_path());
                    auto lock_path=key; lock_path+=fs::u8path(".lock");
                    if(fs::exists(lock_path) && !plain_entry(lock_path,false)) throw std::runtime_error("invalid recovery session lease");
                    lease=std::make_shared<FileLock>(key);
                } catch(const std::exception& e) {result.retention.error=e.what();}
            }
            {
                FileLock lock(destination);
                if(fs::exists(destination)) check_owner(info.path,info.identity,info.session,info.base_path);
                if(!lease) unmanage_session_marker(destination,info);
                {
                    WorkDirectory work(destination,info);
                    if(!cancelled->load()) snapshot->save_recovery(work.file.u8string(),destination.parent_path().u8string(),info,cancelled.get());
                    if(cancelled->load()) result.cancelled=true;
                    else {
                        result.published=publish(work.file,destination,*cancelled);
                        result.cancelled=!result.published;
                        if(result.published) {
                            progress->published.store(true);
                            result.lease=lease;
                            result.file_bytes=fs::file_size(destination); result.modified=fs::last_write_time(destination);
                            if(lease) {
                                try {write_session_marker(destination,info);} catch(const std::exception& e) {result.retention.error=e.what();}
                            }
                        }
                    }
                }
                // Publication has priority over debris collection. Bound deletion
                // per autosave; explicit maintenance can sweep the full destination.
                if(result.published && !cancelled->load()) {
                    try { result.work_directories_cleaned=Project::cleanup_recovery_work_impl(destination.u8string(),32,cancelled.get()); }
                    catch(const std::exception& e) {result.work_cleanup_error=e.what();}
                }
            }
            if(result.published && result.work_directories_cleaned<32 && !cancelled->load()) {
                try {result.work_directories_cleaned+=Project::cleanup_recovery_work_impl(info.path,
                    32-result.work_directories_cleaned,cancelled.get(),&info);}
                catch(const std::exception& e) {result.work_cleanup_error=e.what();}
            }
            // Retention needs only the published header and native lease. Drop
            // the potentially large private canonical snapshot before scanning.
            snapshot.reset();
            if(result.published && retention.enabled && result.retention.error.empty() && !cancelled->load()) {
                try {result.retention=Project::prune_recoveries_impl(info.path,retention,cancelled.get(),progress.get());}
                catch(const std::exception& e) {
                    result.retention.cancelled=cancelled->load();
                    if(!result.retention.cancelled) result.retention.error=e.what();
                    progress->phase.store(result.retention.cancelled?4:0);
                }
            }
            else if(result.published && retention.enabled && cancelled->load()) {result.retention.cancelled=true; progress->phase.store(4);}
        } catch(const std::exception& e) { if(cancelled->load()) result.cancelled=true; else result.error=e.what(); }
        snapshot.reset(); // Large canonical maps are released on the worker, not by GUI future.get().
        result.write_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); return result;
    });
    impl_->future=std::move(future); impl_->cancelled=std::move(cancelled); impl_->inflight=std::move(stamp);
    impl_->progress=std::move(progress);
    impl_->stats.checkpoint_published=false; impl_->stats.cancel_requested=false; impl_->stats.retention_cancelled=false;
    impl_->stats.maintenance_phase="idle"; impl_->stats.retention_inspected=impl_->stats.retention_removed=0;
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
    if(impl_->progress) {
        impl_->stats.checkpoint_published=impl_->progress->published.load();
        impl_->stats.cancel_requested=impl_->cancelled && impl_->cancelled->load();
        impl_->stats.retention_inspected=impl_->progress->inspected.load();
        impl_->stats.retention_removed=impl_->progress->removed.load();
        const auto phase=impl_->progress->phase.load();
        impl_->stats.maintenance_phase=phase==1?"scanning":phase==2?"collecting":phase==3?"complete":phase==4?"cancelled":"idle";
    }
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=impl_->future.get(); impl_->stats.write_ms=result.write_ms;
            impl_->stats.work_directories_cleaned+=result.work_directories_cleaned;
            impl_->stats.work_cleanup_error=std::move(result.work_cleanup_error);
            if(result.lease) {impl_->lease=std::move(result.lease); impl_->lease_identity=impl_->identity;}
            impl_->stats.checkpoints_pruned+=result.retention.removed;
            impl_->stats.retention_ms=result.retention.elapsed_ms;
            impl_->stats.retention_error=std::move(result.retention.error);
            impl_->stats.retention_cancelled=result.retention.cancelled;
            impl_->stats.retention_metadata_ms=result.retention.metadata_ms;
            impl_->stats.retention_ownership_ms=result.retention.ownership_ms;
            impl_->stats.retention_removal_ms=result.retention.removal_ms;
            impl_->stats.retention_prefetch_peak=result.retention.marker_prefetch_peak;
            if(impl_->progress) {
                impl_->stats.checkpoint_published=impl_->progress->published.load();
                impl_->stats.retention_inspected=impl_->progress->inspected.load();
                impl_->stats.retention_removed=impl_->progress->removed.load();
                impl_->stats.maintenance_phase=result.retention.cancelled?"cancelled":impl_->progress->phase.load()==3?"complete":"idle";
            }
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
            impl_->progress.reset(); impl_->stats.checkpoint_published=false; impl_->stats.cancel_requested=false;
            impl_->stats.maintenance_phase="idle";
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
std::size_t Project::cleanup_recovery_work(const std::string& path) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    const auto target=location(path);
    if(!recovery_extension(target)) throw std::invalid_argument("recovery destination must use .qpxrecover");
    FileLock lock(target);
    return cleanup_recovery_work_impl(target.u8string(),0,nullptr);
}
std::size_t Project::cleanup_abandoned_recovery_work(const std::string& current_path,std::size_t limit) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    if(!limit || limit>1024) throw std::invalid_argument("recovery work cleanup limit must be between 1 and 1024");
    const auto anchor=location(current_path);
    if(!recovery_extension(anchor) || !plain_entry(anchor,false)) throw std::invalid_argument("recovery anchor must be a regular .qpxrecover checkpoint");
    RecoveryInfo scope;
    {FileLock lock(anchor); scope=inspect_recovery_header(anchor.u8string());}
    return cleanup_recovery_work_impl(anchor.u8string(),limit,nullptr,&scope);
}
std::size_t Project::cleanup_recovery_work_impl(const std::string& path,std::size_t limit,
        const std::atomic<bool>* cancelled,const RecoveryInfo* scope) {
    const auto destination=location(path);
    const auto prefix=destination.filename().u8string()+".work-";
    std::size_t cleaned=0;
    for(const auto& item:fs::directory_iterator(destination.parent_path())) {
        if((limit && cleaned>=limit) || (cancelled && cancelled->load())) break;
        const auto name=item.path().filename().u8string();
        const auto split=name.rfind(".work-");
        if(split==std::string::npos || name.size()!=split+6+32 || !valid_token(name.substr(split+6))) continue;
        const auto target=destination.parent_path()/fs::u8path(name.substr(0,split));
        if(!recovery_extension(target)) continue;
        if(!scope && (name.size()!=prefix.size()+32 || !name_prefix(item.path(),prefix))) continue;
        if(!work_inventory(item.path())) continue;
        const auto id=name.substr(split+6);
        auto owner=work_owner(item.path(),id,!scope);
        if(!owner || (scope && !same_scope(*owner,*scope))) continue;
        std::unique_ptr<FileLock> lock;
        if(scope) {
            auto lock_path=target; lock_path+=fs::u8path(".lock");
            if(!plain_entry(lock_path,false)) continue;
            try {lock=std::make_unique<FileLock>(target,false);} catch(const std::exception&) {continue;}
            // Recheck all provenance after acquiring this writer's exact lock.
            if(!work_inventory(item.path())) continue;
            owner=work_owner(item.path(),id,false);
            if(!owner || !same_scope(*owner,*scope)) continue;
        }
        const auto snapshot=item.path()/"snapshot.qpxproj";
        if(!owner->identity.empty() && fs::exists(snapshot)) {
            std::optional<RecoveryInfo> complete;
            try {complete=inspect_recovery_work(snapshot.u8string(),cancelled);}
            catch(const std::exception&) {continue;} // Unknown/corrupt data stays private.
            if(cancelled && cancelled->load()) break;
            if(complete) {
                if(!same_scope(*complete,*owner) || complete->session!=owner->session) continue;
                const auto rescue=destination.parent_path()/fs::u8path(target.filename().u8string()+".rescued-"+id+".qpxrecover");
                rescue_work(snapshot,rescue);
            }
        }
        if(cancelled && cancelled->load()) break;
        for(const auto& suffix:{"","-journal","-wal","-shm"}) {
            auto file=snapshot; file+=fs::u8path(suffix); fs::remove(file);
        }
        fs::remove(item.path()/"owner");
        if(fs::remove(item.path())) ++cleaned;
    }
    return cleaned;
}
RecoveryRetentionStats Project::prune_recoveries(const std::string& current_path,const RecoveryRetention& policy) {
    return prune_recoveries_impl(current_path,policy,nullptr,nullptr);
}
RecoveryRetentionStats Project::prune_recoveries_impl(const std::string& current_path,const RecoveryRetention& policy,
        const std::atomic<bool>* cancelled,RetentionProgress* progress) {
    if(!has_project()) throw std::runtime_error("project recovery requires QUPATHXX_BUILD_PROJECT=ON");
    policy.validate(); RecoveryRetentionStats stats;
    if(!policy.enabled) return stats;
    const auto start=std::chrono::steady_clock::now();
    const auto stopped=[&]{return cancelled && cancelled->load();};
    const auto finish=[&] {
        stats.cancelled=stopped();
        stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(progress) progress->phase.store(stats.cancelled?4:3);
        return stats;
    };
    if(stopped()) return finish();
    if(progress) progress->phase.store(1);
    const auto target=location(current_path);
    if(!recovery_extension(target) || !plain_entry(target,false)) throw std::invalid_argument("retention requires a regular recovery checkpoint");
    RecoveryInfo current;
    {FileLock lock(target); current=inspect_recovery_header(target.u8string(),cancelled);}
    const auto same_scope=[&](const RecoveryInfo& info) {
        return info.identity==current.identity && (info.base_path.empty()==current.base_path.empty()) &&
            (info.base_path.empty() || info.base_path==current.base_path || same_path(location(info.base_path),location(current.base_path)));
    };
    const auto newer=[](const RecoveryInfo& a,const RecoveryInfo& b) {
        return a.created_unix_ms!=b.created_unix_ms?a.created_unix_ms>b.created_unix_ms:a.path<b.path;
    };
    const auto age_ms=static_cast<std::int64_t>(policy.max_age_days*86400000.);
    const auto now_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto file_cutoff=fs::file_time_type::clock::now()-std::chrono::duration_cast<fs::file_time_type::duration>(std::chrono::duration<double>(policy.max_age_days*86400.));
    const bool anchor_has_aliases=fs::hard_link_count(target)>1;
    struct Candidate {RecoveryInfo info; fs::file_time_type modified; std::shared_ptr<FileLock> lease;};
    std::vector<RecoveryInfo> newest;
    std::vector<Candidate> candidates;
    const auto older=[&](const Candidate& a,const Candidate& b){return newer(b.info,a.info);};
    struct Pending {RecoveryInfo info; fs::file_time_type modified; std::future<bool> marker;};
    // Declared before pending: joining workers remains safe on exceptional unwind.
    std::unique_ptr<MarkerReaders> readers;
    std::deque<Pending> pending;
    const auto consume=[&] {
        auto item=std::move(pending.front()); pending.pop_front();
        try {
            if(!item.marker.get() || stopped()) return;
            auto& info=item.info;
            auto global=lease_key(info); auto lock_path=global; lock_path+=fs::u8path(".lock");
            if(!plain_entry(lock_path,false)) return;
            const auto held=std::find_if(candidates.begin(),candidates.end(),[&](const Candidate& c){return c.info.session==info.session;});
            auto lease=held!=candidates.end()?held->lease:std::make_shared<FileLock>(global,false);
            candidates.push_back({std::move(info),item.modified,std::move(lease)});
            std::push_heap(candidates.begin(),candidates.end(),older);
            if(candidates.size()>policy.max_deletions) {std::pop_heap(candidates.begin(),candidates.end(),older); candidates.pop_back();}
        } catch(const std::exception&) { /* Unknown, active or unreadable files stay protected. */ }
    };
    for(const auto& item:fs::directory_iterator(target.parent_path())) {
        if(stopped()) break;
        if(!recovery_extension(item.path()) || !plain_entry(item.path(),false)) continue;
        ++stats.inspected; if(progress) progress->inspected.store(stats.inspected);
        TimedSection metadata(stats.metadata_ms);
        try {
            auto info=inspect_recovery_header(item.path().u8string(),cancelled);
            if(!same_scope(info)) continue;
            newest.push_back(info); std::push_heap(newest.begin(),newest.end(),newer);
            if(newest.size()>policy.keep_latest) {std::pop_heap(newest.begin(),newest.end(),newer); newest.pop_back();}
            if(same_name(item.path(),target) || (anchor_has_aliases && same_path(item.path(),target)) || info.created_unix_ms>now_ms-age_ms) continue;
            const auto modified=fs::last_write_time(item.path()); if(modified>file_cutoff) continue;
            metadata.stop(); TimedSection ownership(stats.ownership_ms);
            if(pending.size()>=MarkerReaders::capacity) consume();
            if(stopped()) break;
            if(!readers) readers=std::make_unique<MarkerReaders>();
            const auto sidecar=session_path(item.path());
            auto marker=readers->submit([sidecar,expected=lease_marker(info),cancelled] {
                if(cancelled && cancelled->load()) return false;
                try {return plain_entry(sidecar,false) && read_session_marker(sidecar)==expected;}
                catch(const std::exception&) {return false;}
            });
            pending.push_back({std::move(info),modified,std::move(marker)});
            stats.marker_prefetch_peak=std::max(stats.marker_prefetch_peak,pending.size());
        } catch(const std::exception&) { /* Unknown, active or unreadable files are preserved. */ }
    }
    {TimedSection ownership(stats.ownership_ms); while(!pending.empty()) consume();}
    // A partial scan has no trustworthy newest set: cancellation must not
    // turn a partially ranked catalog into permission to remove checkpoints.
    if(stopped()) return finish();
    if(progress) progress->phase.store(2);
    std::set<std::string> retained;
    for(const auto& info:newest) retained.insert(info.path);
    std::sort(candidates.begin(),candidates.end(),older);
    {
        TimedSection removal(stats.removal_ms);
        for(const auto& candidate:candidates) {
            if(stopped()) break;
            if(retained.count(candidate.info.path)) continue;
            try {
                const auto path=location(candidate.info.path); FileLock lock(path);
                if(!plain_entry(path,false)) continue;
                const auto info=inspect_recovery_header(candidate.info.path,cancelled);
                if(!same_scope(info) || info.session!=candidate.info.session || info.created_unix_ms!=candidate.info.created_unix_ms ||
                   info.base_generation!=candidate.info.base_generation || fs::last_write_time(path)!=candidate.modified) continue;
                if(!plain_entry(session_path(path),false) || read_session_marker(session_path(path))!=lease_marker(info)) continue;
                if(stopped()) break;
                if(!fs::remove(path)) throw std::runtime_error("recovery retention removal failed");
                ++stats.removed; if(progress) progress->removed.store(stats.removed);
            } catch(const std::exception& e) {if(!stopped()) {++stats.failed; if(stats.error.empty()) stats.error=e.what();}}
        }
    }
    return finish();
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
    std::optional<std::pair<std::string,std::uint64_t>> primary;
    std::string unavailable_status;
    if(!base.empty()) {
        try {primary=read_save_identity(base);}
        catch(const std::exception&) {
            std::error_code error; const auto exists=fs::exists(location(base),error);
            unavailable_status=error || exists?"unreadable":"missing";
        }
    }
    std::vector<RecoveryInfo> result;
    auto better=[](const auto& a,const auto& b){return a.created_unix_ms!=b.created_unix_ms?a.created_unix_ms>b.created_unix_ms:a.path<b.path;};
    if(!fs::exists(directory)) return result;
    for(const auto& item:fs::directory_iterator(directory)) {
        if(!recovery_extension(item.path()) || !item.is_regular_file()) continue;
        RecoveryInfo info;
        try {
            info=inspect_recovery_header(item.path().u8string());
            if(base.empty()) {
                if(!info.base_path.empty()) continue;
                info.base_status="unsaved";
            } else {
                if(info.base_path.empty()) continue;
                const bool recorded_here=same_path(location(info.base_path),location(base));
                const bool matching_identity=primary && primary->first==info.identity;
                if(!recorded_here && !matching_identity) continue;
                if(matching_identity) {
                    info.associated_base_path=location(base).u8string();
                    info.base_status=primary->second!=info.base_generation?"changed":(recorded_here?"matching":"relocated");
                } else info.base_status=primary?"changed":unavailable_status;
            }
        } catch(const std::exception& e) {
            // An invalid file has no trustworthy identity. Retain the existing
            // filename-prefix error visibility, without claiming association.
            if(!name_prefix(item.path(),prefix)) continue;
            info.path=item.path().u8string(); info.error=e.what();
        }
        result.push_back(std::move(info)); std::push_heap(result.begin(),result.end(),better);
        if(result.size()>limit) { std::pop_heap(result.begin(),result.end(),better); result.pop_back(); }
    }
    std::sort(result.begin(),result.end(),better);
    return result;
}
}
