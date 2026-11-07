#pragma once
#include "atomic_export.hpp"
#include "qupathxx/workflow.hpp"
#include "workflow_file_version.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include <filesystem>
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
        FileFingerprintOptions options;options.byte_limit=limit_;return fingerprint_file(path_,options,c)->sha256;
    }
public:
    WorkflowInput(const std::string& path,std::uint64_t limit,bool project=false,const std::atomic<bool>* c=nullptr):limit_(limit),project_(project) {
        export_checkpoint(c);path_=std::filesystem::canonical(std::filesystem::u8path(export_path(path))).u8string();sidecars();
        version_=workflow_file_version(path_);
        if(version_->bytes>limit_) throw std::invalid_argument("Workflow input exceeds byte limit");
        sha_=digest(c);verify(false,c);
    }
    void verify(bool rehash=false,const std::atomic<bool>* c=nullptr) const {
        export_checkpoint(c);try {sidecars();const auto now=workflow_file_version(path_);
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
    const std::string& path() const {return path_;}
    const std::string& sha256() const {return sha_;}
};
}
