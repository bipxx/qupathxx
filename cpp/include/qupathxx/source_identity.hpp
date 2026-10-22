#pragma once
#include "qupathxx/file_fingerprint.hpp"
namespace qpx {
struct FileVersion {
    std::string path,identity_scheme,identity,stamp_scheme,modified_token,change_token;
    std::uint64_t bytes=0,observed_unix_ms=0;
    void validate() const;
    std::size_t payload_bytes() const;
};
struct ReaderSource {
    std::string generation,reader,locator,scope;
    std::shared_ptr<const FileVersion> primary;
    void validate() const;
    std::size_t payload_bytes() const;
};
// Generation is process-unique and never derived from a pathname or file hash.
std::shared_ptr<const ReaderSource> make_reader_source(std::string reader,std::string locator,std::string scope,std::shared_ptr<const FileVersion> primary={});
struct SourceFileAssociation {
    ReaderSource source;
    FileFingerprint fingerprint;
    std::string status;
};
SourceFileAssociation associate_source_file(const ReaderSource&,const FileFingerprint&);
std::string reader_source_json(const ReaderSource&);
std::string source_file_association_json(const SourceFileAssociation&);
void save_source_file_association(const SourceFileAssociation&,const std::string& path);
}
