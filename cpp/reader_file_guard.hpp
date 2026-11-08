#pragma once
#include "qupathxx/source_identity.hpp"
namespace qpx::detail {
// Regular-file handle retained across decoder initialization, never across a
// reader's lifetime. Verification checks both that handle and a fresh path.
class ReaderFileGuard {
public:
    explicit ReaderFileGuard(const std::string& path);
    ~ReaderFileGuard();
    ReaderFileGuard(const ReaderFileGuard&)=delete;
    std::shared_ptr<const FileVersion> version() const;
    void verify() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
void guard_file_output(const std::string& observed_path,const std::string& identity,const std::string& absolute_path);
// Compare observed file sets, not content hashes or process-unique generations.
bool same_reader_files(const ReaderSource&,const ReaderSource&);
void save_source_association(const SourceFileAssociation&,const std::string&,const std::atomic<bool>*);
}
