#pragma once
#include "utf8_text.hpp"
#include <string>
namespace qpx::detail {
inline void validate_file_identity(const std::string& identity_scheme,const std::string& identity,const std::string& stamp_scheme,const std::string& modified,const std::string& changed) {
    for(const auto* s:{&identity,&modified,&changed}) {bounded_utf8(*s,128);if(s->empty() || s->find_first_not_of("0123456789:-")!=std::string::npos) throw std::invalid_argument("invalid file fingerprint identity/stamp");}
    if(!((identity_scheme=="windows.volume-file-id128/1" && stamp_scheme=="windows.filetime-100ns/1") || (identity_scheme=="posix.device-inode/1" && stamp_scheme=="posix.seconds-nanoseconds/1"))) throw std::invalid_argument("invalid file fingerprint scheme");
}
}
