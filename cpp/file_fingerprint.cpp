#include "qupathxx/file_fingerprint.hpp"
#include "sha256.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <sys/stat.h>
#endif
namespace qpx {
namespace {
using Clock=std::chrono::steady_clock;
void checkpoint(const std::atomic<bool>* cancelled) {if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("file fingerprint cancelled");}
std::uint64_t unix_ms() {const auto n=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();if(n<0) throw std::runtime_error("invalid file fingerprint clock");return static_cast<std::uint64_t>(n);}
std::string normalized(const std::string& path) {
    detail::bounded_utf8(path,32768);if(path.empty()) throw std::invalid_argument("empty file fingerprint path");
    auto value=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();detail::bounded_utf8(value,32768);
#ifdef _WIN32
    if(value.rfind("\\\\.\\",0)==0 || value.rfind("\\\\?\\GLOBALROOT\\",0)==0) throw std::invalid_argument("file fingerprint requires an ordinary file path");
#endif
    return value;
}
struct Stamp {
    std::uint64_t bytes=0;std::string identity,modified,changed;
    bool operator==(const Stamp& other) const {return bytes==other.bytes && identity==other.identity && modified==other.modified && changed==other.changed;}
};
class Input {
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int fd_=-1;
#endif
public:
    explicit Input(const std::string& path,bool metadata=false) {
#ifdef _WIN32
        const auto native=std::filesystem::u8path(path);
        const auto attributes=GetFileAttributesW(native.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("file fingerprint input is missing or not a regular file");
        handle_=CreateFileW(native.c_str(),metadata?FILE_READ_ATTRIBUTES:(GENERIC_READ|FILE_READ_ATTRIBUTES),
                            metadata?(FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE):FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
        if(handle_==INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open file fingerprint input (possibly busy or inaccessible)");
        if(GetFileType(handle_)!=FILE_TYPE_DISK) {CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;throw std::invalid_argument("file fingerprint requires a regular disk file");}
#else
        (void)metadata;fd_=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK);
        if(fd_<0) throw std::runtime_error("cannot open file fingerprint input");
        struct stat status{};if(::fstat(fd_,&status) || !S_ISREG(status.st_mode)) {::close(fd_);fd_=-1;throw std::invalid_argument("file fingerprint requires a regular file");}
#endif
    }
    ~Input() {
#ifdef _WIN32
        if(handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if(fd_>=0) ::close(fd_);
#endif
    }
    Input(const Input&)=delete;
    Stamp stamp() const {
        Stamp s;
#ifdef _WIN32
        BY_HANDLE_FILE_INFORMATION info{};FILE_BASIC_INFO basic{};FILE_ID_INFO id{};
        if(!GetFileInformationByHandle(handle_,&info) || !GetFileInformationByHandleEx(handle_,FileBasicInfo,&basic,sizeof(basic)) || !GetFileInformationByHandleEx(handle_,FileIdInfo,&id,sizeof(id)) || (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("cannot inspect file fingerprint input");
        s.bytes=(std::uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
        std::uint64_t first=0,last=0;for(int i=0;i<8;++i) {first|=std::uint64_t(id.FileId.Identifier[i])<<(8*i);last|=std::uint64_t(id.FileId.Identifier[i+8])<<(8*i);}
        if((first==0 && last==0) || (first==std::numeric_limits<std::uint64_t>::max() && last==std::numeric_limits<std::uint64_t>::max())) throw std::runtime_error("filesystem does not provide a usable file identity");
        s.identity=std::to_string(id.VolumeSerialNumber)+":"+std::to_string(first)+":"+std::to_string(last);
        s.modified=std::to_string(basic.LastWriteTime.QuadPart);s.changed=std::to_string(basic.ChangeTime.QuadPart);
#else
        struct stat info{};if(::fstat(fd_,&info) || !S_ISREG(info.st_mode) || info.st_size<0) throw std::runtime_error("cannot inspect file fingerprint input");
        s.bytes=static_cast<std::uint64_t>(info.st_size);s.identity=std::to_string(info.st_dev)+":"+std::to_string(info.st_ino);
#ifdef __APPLE__
        s.modified=std::to_string(info.st_mtimespec.tv_sec)+":"+std::to_string(info.st_mtimespec.tv_nsec);s.changed=std::to_string(info.st_ctimespec.tv_sec)+":"+std::to_string(info.st_ctimespec.tv_nsec);
#else
        s.modified=std::to_string(info.st_mtim.tv_sec)+":"+std::to_string(info.st_mtim.tv_nsec);s.changed=std::to_string(info.st_ctim.tv_sec)+":"+std::to_string(info.st_ctim.tv_nsec);
#endif
#endif
        if(s.bytes>std::uint64_t(std::numeric_limits<std::int64_t>::max())) throw std::length_error("file fingerprint input exceeds supported size");return s;
    }
    std::size_t read(char* data,std::size_t count) {
#ifdef _WIN32
        DWORD got=0;if(!ReadFile(handle_,data,static_cast<DWORD>(count),&got,nullptr)) throw std::runtime_error("file fingerprint read failed");return got;
#else
        ssize_t got;do {got=::read(fd_,data,count);} while(got<0 && errno==EINTR);
        if(got<0) throw std::runtime_error("file fingerprint read failed");return static_cast<std::size_t>(got);
#endif
    }
};
void output_guard(const FileFingerprint& value,const std::string& path) {
    if(value.path==path) throw std::invalid_argument("fingerprint output must not replace its input");
    std::error_code error;
    if(std::filesystem::equivalent(std::filesystem::u8path(value.path),std::filesystem::u8path(path),error)) throw std::invalid_argument("fingerprint output aliases its input");
    error.clear();const auto exists=std::filesystem::exists(std::filesystem::u8path(path),error);
    if(error) throw std::runtime_error("cannot inspect fingerprint output");
    if(exists) {
        std::string identity;try {identity=Input(path,true).stamp().identity;} catch(const std::exception& e) {throw std::runtime_error(std::string("cannot inspect fingerprint output: ")+e.what());}
        if(identity==value.identity) throw std::invalid_argument("fingerprint output aliases its observed input");
    }
}
std::shared_ptr<const FileFingerprint> run(const std::string& path,FileFingerprintOptions options,const std::atomic<bool>* cancelled,
                                         std::atomic<std::uint64_t>* progress=nullptr,std::atomic<std::uint64_t>* total=nullptr,const std::string& destination={}) {
    checkpoint(cancelled);const auto start=Clock::now();auto value=std::make_shared<FileFingerprint>();value->path=path;value->started_unix_ms=unix_ms();
    Input input(path);const auto before=input.stamp();if(total) total->store(before.bytes,std::memory_order_relaxed);
    if(before.bytes>options.byte_limit) throw std::length_error("file fingerprint exceeds byte limit");
    value->bytes=before.bytes;value->identity=before.identity;value->modified_token=before.modified;value->change_token=before.changed;
    if(!destination.empty()) output_guard(*value,destination);
#ifdef _WIN32
    value->identity_scheme="windows.volume-file-id128/1";value->stamp_scheme="windows.filetime-100ns/1";
#else
    value->identity_scheme="posix.device-inode/1";value->stamp_scheme="posix.seconds-nanoseconds/1";
#endif
    std::vector<char> buffer(static_cast<std::size_t>(std::min<std::uint64_t>(options.buffer_bytes,std::max<std::uint64_t>(before.bytes,1))));Sha256 hash;
    std::uint64_t done=0;
    while(done<before.bytes) {
        checkpoint(cancelled);const auto want=static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(),before.bytes-done));const auto got=input.read(buffer.data(),want);
        if(!got) throw std::runtime_error("file fingerprint input was truncated");hash.update(buffer.data(),got);done+=got;if(progress) progress->store(done,std::memory_order_release);
    }
    checkpoint(cancelled);char extra;if(input.read(&extra,1)) throw std::runtime_error("file fingerprint input grew");
    if(!(before==input.stamp()) || !(before==Input(path,true).stamp())) throw std::runtime_error("file fingerprint input changed while hashing");
    checkpoint(cancelled);value->sha256=hash.finish();value->finished_unix_ms=unix_ms();value->elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();value->validate();return value;
}
std::string quoted(const std::string& value) {
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {if(c=='\"' || c=='\\') {out+='\\';out+=char(c);}else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'\"';
}
void save(const FileFingerprint& value,const std::string& path,const std::atomic<bool>* cancelled) {
    value.validate();output_guard(value,path);const auto json=file_fingerprint_json(value);detail::AtomicExportFile file(path,1024*1024,cancelled);
    file.write(json.data(),json.size());checkpoint(cancelled);output_guard(value,path);file.commit();
}
}
void FileFingerprintOptions::validate() const {
    if(byte_limit>std::uint64_t(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("file fingerprint byte limit exceeds signed 64-bit size");
    if(buffer_bytes<65536 || buffer_bytes>16*1024*1024) throw std::invalid_argument("file fingerprint buffer must be 64 KiB..16 MiB");
}
void FileFingerprint::validate() const {
    detail::bounded_utf8(path,32768);if(path.empty() || !std::filesystem::u8path(path).is_absolute() || bytes>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || sha256.size()!=64 || sha256.find_first_not_of("0123456789abcdef")!=std::string::npos || !std::isfinite(elapsed_ms) || elapsed_ms<0 || started_unix_ms>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || finished_unix_ms>std::uint64_t(std::numeric_limits<std::int64_t>::max())) throw std::invalid_argument("invalid file fingerprint");
    for(const auto* s:{&identity,&modified_token,&change_token}) {detail::bounded_utf8(*s,128);if(s->empty() || s->find_first_not_of("0123456789:-")!=std::string::npos) throw std::invalid_argument("invalid file fingerprint identity/stamp");}
    if(!((identity_scheme=="windows.volume-file-id128/1" && stamp_scheme=="windows.filetime-100ns/1") || (identity_scheme=="posix.device-inode/1" && stamp_scheme=="posix.seconds-nanoseconds/1"))) throw std::invalid_argument("invalid file fingerprint scheme");
}
std::shared_ptr<const FileFingerprint> fingerprint_file(const std::string& path,FileFingerprintOptions options,const std::atomic<bool>* cancelled) {options.validate();return run(normalized(path),options,cancelled);}
std::string file_fingerprint_json(const FileFingerprint& value) {
    value.validate();char real[128];auto formatted=std::to_chars(real,real+128,value.elapsed_ms,std::chars_format::general,std::numeric_limits<double>::max_digits10);if(formatted.ec!=std::errc{}) throw std::runtime_error("fingerprint duration formatting failed");
    return "{\"schema\":\"qpx.file-fingerprint/1\",\"path\":"+quoted(value.path)+",\"bytes\":"+std::to_string(value.bytes)+",\"sha256\":"+quoted(value.sha256)+",\"identity_scheme\":"+quoted(value.identity_scheme)+",\"identity\":"+quoted(value.identity)+",\"stamp_scheme\":"+quoted(value.stamp_scheme)+",\"modified_token\":"+quoted(value.modified_token)+",\"change_token\":"+quoted(value.change_token)+",\"started_unix_ms\":"+std::to_string(value.started_unix_ms)+",\"finished_unix_ms\":"+std::to_string(value.finished_unix_ms)+",\"elapsed_ms\":"+std::string(real,formatted.ptr)+"}\n";
}
void save_file_fingerprint(const FileFingerprint& value,const std::string& path) {save(value,normalized(path),nullptr);}
struct FileFingerprintJob::Impl {
    struct Completion {std::shared_ptr<const FileFingerprint> value;std::string error;bool cancelled=false;double ms=0;};
    std::future<Completion> future;std::shared_ptr<std::atomic<bool>> cancelled;std::atomic<std::uint64_t> bytes{0},total{0};
    FileFingerprintStats stats;std::shared_ptr<const FileFingerprint> value;
};
FileFingerprintJob::FileFingerprintJob():impl_(std::make_unique<Impl>()) {}
FileFingerprintJob::~FileFingerprintJob() {cancel();try {wait();} catch(...) {}}
bool FileFingerprintJob::busy() const {return impl_->future.valid();}
void FileFingerprintJob::cancel() {if(impl_->cancelled) impl_->cancelled->store(true);}
std::shared_ptr<const FileFingerprint> FileFingerprintJob::result() const {return impl_->value;}
bool FileFingerprintJob::request(const std::string& path,const std::string& output_path,FileFingerprintOptions options) {
    poll();if(busy()) return false;options.validate();const auto source=normalized(path),destination=output_path.empty()?std::string{}:normalized(output_path);
    if(source==destination) throw std::invalid_argument("fingerprint output must not replace its input");
    if(impl_->stats.completed==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("fingerprint completion counter exhausted");
    auto cancelled=std::make_shared<std::atomic<bool>>(false);impl_->bytes.store(0);impl_->total.store(0);
    auto future=std::async(std::launch::async,[source,destination,options,cancelled,state=impl_.get()] {
        Impl::Completion result;const auto start=Clock::now();
        try {auto value=run(source,options,cancelled.get(),&state->bytes,&state->total,destination);if(destination.empty()) checkpoint(cancelled.get());else save(*value,destination,cancelled.get());result.value=std::move(value);}
        catch(const std::exception& e) {result.error=e.what();result.cancelled=cancelled->load();}
        result.ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();return result;
    });
    const auto completed=impl_->stats.completed;impl_->stats={};impl_->stats.completed=completed;impl_->stats.state="running";impl_->stats.path=source;impl_->stats.output_path=destination;
    impl_->value.reset();impl_->cancelled=std::move(cancelled);impl_->future=std::move(future);return true;
}
FileFingerprintStats FileFingerprintJob::poll() {
    impl_->stats.bytes=impl_->bytes.load(std::memory_order_acquire);impl_->stats.total_bytes=impl_->total.load(std::memory_order_relaxed);
    if(busy() && impl_->cancelled->load()) impl_->stats.state="cancelling";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto value=impl_->future.get();impl_->stats.elapsed_ms=value.ms;impl_->stats.error=std::move(value.error);
        if(value.value) {impl_->value=std::move(value.value);impl_->stats.state="succeeded";++impl_->stats.completed;}
        else impl_->stats.state=value.cancelled?"cancelled":"failed";
        impl_->stats.bytes=impl_->bytes.load();impl_->stats.total_bytes=impl_->total.load();
    }
    return impl_->stats;
}
FileFingerprintStats FileFingerprintJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
