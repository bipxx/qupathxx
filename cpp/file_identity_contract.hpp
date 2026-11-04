#pragma once
#include "utf8_text.hpp"
#include <string>
namespace qpx::detail {
inline bool absolute_source_path(const std::string& path) {
    return !path.empty() && (path[0]=='/' || (path.size()>=2 && path[0]=='\\' && path[1]=='\\') ||
        (path.size()>=3 && ((path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z')) && path[1]==':' && (path[2]=='/' || path[2]=='\\')));
}
inline void validate_file_identity(const std::string& identity_scheme,const std::string& identity,const std::string& stamp_scheme,const std::string& modified,const std::string& changed) {
    for(const auto* s:{&identity,&modified,&changed}) {bounded_utf8(*s,128);if(s->empty() || s->find_first_not_of("0123456789:-")!=std::string::npos) throw std::invalid_argument("invalid file fingerprint identity/stamp");}
    if(!((identity_scheme=="windows.volume-file-id128/1" && stamp_scheme=="windows.filetime-100ns/1") || (identity_scheme=="posix.device-inode/1" && stamp_scheme=="posix.seconds-nanoseconds/1"))) throw std::invalid_argument("invalid file fingerprint scheme");
}
}
