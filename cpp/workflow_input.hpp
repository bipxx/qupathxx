#pragma once
#include "atomic_export.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_file_version.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
namespace qpx::detail {
class WorkflowInput {
    std::string path_,sha_;
    std::uint64_t limit_;
    bool project_;
    std::shared_ptr<const FileVersion> version_;
    void sidecars() const {
        if(project_) for(const auto* suffix:{"-journal","-wal","-shm"})
            if(std::filesystem::exists(std::filesystem::u8path(path_+suffix)))
                throw std::invalid_argument("Close/recover the project; SQLite sidecar present");
    }
    std::string digest(const std::atomic<bool>* c) const {
        FileFingerprintOptions options;options.byte_limit=limit_;
        try{return fingerprint_file(path_,options,c)->sha256;}catch(...){workflow_checkpoint(c);throw;}
    }
public:
    WorkflowInput(const std::string& path,std::uint64_t limit,bool project=false,const std::atomic<bool>* c=nullptr):limit_(limit),project_(project) {
        workflow_checkpoint(c);path_=std::filesystem::canonical(std::filesystem::u8path(export_path(path))).u8string();sidecars();
        version_=workflow_file_version(path_);
        if(version_->bytes>limit_) throw std::invalid_argument("Workflow input exceeds byte limit");
        sha_=digest(c);verify(false,c);
    }
    void verify(bool rehash=false,const std::atomic<bool>* c=nullptr) const {
        workflow_checkpoint(c);try {sidecars();const auto now=workflow_file_version(path_);
        if(now->identity_scheme!=version_->identity_scheme || now->identity!=version_->identity || now->bytes!=version_->bytes ||
            now->stamp_scheme!=version_->stamp_scheme || now->modified_token!=version_->modified_token || now->change_token!=version_->change_token)
            throw std::runtime_error("Workflow input version changed");
        if(!rehash)return;
        if(digest(c)!=sha_) throw std::runtime_error("Workflow input content changed");
        const auto after=workflow_file_version(path_);
        if(after->identity!=now->identity || after->bytes!=now->bytes || after->modified_token!=now->modified_token || after->change_token!=now->change_token)
            throw std::runtime_error("Workflow input changed during verification");
        }catch(const std::exception& e) {
            if(c && c->load())throw;
            throw WorkflowInputChanged(std::string("Workflow input changed or unavailable: ")+path_+": "+e.what());
        }
    }
    std::uint64_t bytes() const {return version_->bytes;}
    const std::string& path() const {return path_;}
    const std::string& sha256() const {return sha_;}
};
inline std::string read_workflow_input(const WorkflowInput& source,const std::atomic<bool>* c=nullptr) {
    source.verify(false,c);
    if(source.bytes()>std::numeric_limits<std::size_t>::max())throw std::length_error("Workflow input exceeds address space");
    std::ifstream file(std::filesystem::u8path(source.path()),std::ios::binary);
    if(!file)throw std::runtime_error("Cannot open workflow input");
    std::string data(static_cast<std::size_t>(source.bytes()),'\0');
    for(std::size_t at=0;at<data.size();) {
        workflow_checkpoint(c);const auto count=std::min<std::size_t>(65536,data.size()-at);
        file.read(data.data()+at,count);
        if(static_cast<std::size_t>(file.gcount())!=count){source.verify(false,c);throw std::runtime_error("Cannot read workflow input snapshot");}
        at+=count;
    }
    char extra;file.read(&extra,1);source.verify(false,c);
    if(file.gcount() || file.bad())throw std::runtime_error("Workflow input differs from snapshot");
    return data;
}

}
