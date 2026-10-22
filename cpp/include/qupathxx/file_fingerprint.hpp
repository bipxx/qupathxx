#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
namespace qpx {
struct ReaderSource;
struct SourceFileAssociation;
struct FileFingerprintOptions {
    std::uint64_t byte_limit=std::uint64_t(1)<<40;
    std::size_t buffer_bytes=1024*1024;
    void validate() const;
};
struct FileFingerprint {
    std::string path,sha256,identity_scheme,identity,stamp_scheme,modified_token,change_token;
    std::uint64_t bytes=0,started_unix_ms=0,finished_unix_ms=0;
    double elapsed_ms=0;
    void validate() const;
};
std::shared_ptr<const FileFingerprint> fingerprint_file(const std::string& path,FileFingerprintOptions={},const std::atomic<bool>* cancelled=nullptr);
std::string file_fingerprint_json(const FileFingerprint&);
void save_file_fingerprint(const FileFingerprint&,const std::string& path);
struct FileFingerprintStats {
    std::string state="idle",path,output_path,error;
    std::uint64_t bytes=0,total_bytes=0,completed=0;
    double elapsed_ms=0;
};
class FileFingerprintJob {
public:
    FileFingerprintJob();~FileFingerprintJob();FileFingerprintJob(const FileFingerprintJob&)=delete;
    bool request(const std::string& path,const std::string& output_path="",FileFingerprintOptions={});
    FileFingerprintStats poll();FileFingerprintStats wait();void cancel();bool busy() const;
    std::shared_ptr<const FileFingerprint> result() const;
    bool request_source(const ReaderSource&,const std::string& output_path="",FileFingerprintOptions={});
    std::shared_ptr<const SourceFileAssociation> association() const;
private:
    bool request_impl(const std::string&,const std::string&,FileFingerprintOptions,std::shared_ptr<const ReaderSource>);
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
