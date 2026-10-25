#pragma once
#include <filesystem>

namespace qpx::detail {
// Physical access only: callers retain their logical paths for metadata/UI.
inline std::filesystem::path access_path(const std::filesystem::path& path,bool extended=false) {
#ifdef _WIN32
    auto value=std::filesystem::absolute(path).lexically_normal().native();
    if((extended || value.size()>240) && value.rfind(L"\\\\?\\",0)!=0) {
        if(value.rfind(L"\\\\",0)==0) value=L"\\\\?\\UNC\\"+value.substr(2);
        else value=L"\\\\?\\"+value;
    }
    return std::filesystem::path(value);
#else
    (void)extended;return path;
#endif
}
}
