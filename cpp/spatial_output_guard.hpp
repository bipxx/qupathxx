#pragma once
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
namespace qpx::detail {
inline void protect_spatial_path(const std::string& input,const std::string& identity,const std::string& output) {
    const auto a=std::filesystem::weakly_canonical(std::filesystem::u8path(input));
    const auto b=std::filesystem::weakly_canonical(std::filesystem::u8path(output));
#ifdef _WIN32
    const auto wa=a.wstring(),wb=b.wstring();
    if(CompareStringOrdinal(wa.data(),int(wa.size()),wb.data(),int(wb.size()),TRUE)==CSTR_EQUAL)
#else
    if(a==b)
#endif
        throw std::invalid_argument("spatial output aliases a protected input path");
    guard_file_output(input,identity,output);
}
}
