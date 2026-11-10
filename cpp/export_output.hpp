#pragma once
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/export_policy.hpp"
#include "qupathxx/workflow.hpp"
namespace qpx::detail {
inline void validate_export_policy(ExportWritePolicy policy) {
    if(policy!=ExportWritePolicy::CreateNew && policy!=ExportWritePolicy::ReplaceExisting) throw std::invalid_argument("invalid export write policy");
}
inline void guard_export_output(const std::string& path,const std::vector<std::string>& protected_paths) {
    const auto destination=export_path(path);const auto output=std::filesystem::weakly_canonical(std::filesystem::u8path(destination));
    for(const auto& source:protected_paths) if(!source.empty()) {
        const auto input=std::filesystem::weakly_canonical(std::filesystem::u8path(export_path(source)));
        bool same=input==output;
#ifdef _WIN32
        const auto a=input.native(),b=output.native();same=CompareStringOrdinal(a.c_str(),int(a.size()),b.c_str(),int(b.size()),TRUE)==CSTR_EQUAL;
#endif
        if(same)throw std::invalid_argument("Export destination is protected: "+source);
        try {guard_file_output(export_path(source),"",destination);}
        catch(const std::invalid_argument& e) {throw std::invalid_argument("Export destination is protected: "+source+" ("+e.what()+")");}
    }
}
inline void require_new_export(const std::string& path) {
    if(std::filesystem::exists(std::filesystem::u8path(path))) throw WorkflowOutputExists("A file already exists at "+path+"; nothing was replaced. Choose a new destination or review replacement.");
}
inline void commit_export_output(AtomicExportFile& file,const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths={}) {
    validate_export_policy(policy);guard_export_output(path,protected_paths);
    try {file.commit(policy==ExportWritePolicy::ReplaceExisting);}
    catch(const std::exception&) {if(policy==ExportWritePolicy::CreateNew) require_new_export(path);throw;}
}
}
