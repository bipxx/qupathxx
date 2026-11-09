#include "ome_zarr_internal.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace qpx::detail {
namespace fs=std::filesystem;
void zarr_require(bool okay,const std::string& message){if(!okay)throw std::invalid_argument("OME-Zarr: "+message);}
namespace {
bool link(const fs::path& p){
#ifdef _WIN32
    const auto a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Cannot inspect OME-Zarr path");
    if(a&FILE_ATTRIBUTE_REPARSE_POINT)return true;
#endif
    return fs::is_symlink(fs::symlink_status(p));
}
void same(const FileVersion& a,const FileVersion& b){if(a.path!=b.path || a.identity!=b.identity || a.identity_scheme!=b.identity_scheme || a.bytes!=b.bytes || a.modified_token!=b.modified_token || a.change_token!=b.change_token || a.stamp_scheme!=b.stamp_scheme)throw WorkflowInputChanged("OME-Zarr store file changed");}
void text(Sha256& hash,const std::string& value){hash.integer(value.size());hash.update(value.data(),value.size());}
}
std::map<std::string,FileVersion> ZarrStore::observe(const std::atomic<bool>* cancelled)const {
    std::map<std::string,FileVersion> files;std::size_t used=0,nodes=0;
    zarr_require(fs::is_directory(root) && !link(root),"local store must be a real directory");
    for(const auto& entry:fs::recursive_directory_iterator(root)){
        workflow_checkpoint(cancelled);zarr_require(++nodes<=options.max_files*4,"store directory-entry limit exceeded");zarr_require(!link(entry.path()),"links/reparse points are unsupported");
        if(entry.is_directory())continue;zarr_require(entry.is_regular_file(),"unsupported store object");
        zarr_require(files.size()<options.max_files,"store file-count limit exceeded");
        ReaderFileGuard guard(entry.path().u8string());guard.verify();auto v=*guard.version();const auto name=entry.path().lexically_relative(root).generic_u8string();
        workflow_capture_charge(used,v.payload_bytes()+name.capacity()+128,options.inventory_bytes);files.emplace(name,std::move(v));
    }return files;
}
ZarrStore::ZarrStore(const std::string& path,OmeZarrOptions opts,const std::atomic<bool>* c):root(fs::absolute(fs::u8path(path)).lexically_normal()),options(opts){
    options.validate();
    for(auto p=root;!p.empty();p=p.parent_path()){zarr_require(!link(p),"store ancestors cannot be links/reparse points");if(p==p.parent_path())break;}
    files_=observe(c);Sha256 hash;text(hash,"qpx.ome-zarr-observed-store/1");text(hash,root.u8string());
    for(const auto& item:files_){const auto& v=item.second;payload_+=v.payload_bytes()+item.first.capacity()+128;text(hash,item.first);text(hash,v.identity_scheme);text(hash,v.identity);hash.integer(v.bytes);text(hash,v.stamp_scheme);text(hash,v.modified_token);text(hash,v.change_token);}id=hash.finish();
}
bool ZarrStore::contains(const std::string& key)const{return files_.find(key)!=files_.end();}
void ZarrStore::verify(const std::string& key)const {
    zarr_require(!link(root),"store root became a link");
    const auto found=files_.find(key);const auto path=root/fs::u8path(key);
    for(auto p=path.parent_path();p!=root;p=p.parent_path())if(fs::exists(fs::symlink_status(p)))zarr_require(!link(p),"store path became a link");
    if(found==files_.end()){if(fs::exists(fs::symlink_status(path)))throw WorkflowInputChanged("OME-Zarr missing chunk appeared");return;}
    for(auto p=path;p!=root;p=p.parent_path())zarr_require(!link(p),"store path became a link");
    ReaderFileGuard guard(path.u8string());same(found->second,*guard.version());guard.verify();
}
std::vector<std::uint8_t> ZarrStore::read(const std::string& key,std::size_t limit,bool missing_allowed)const {
    zarr_require(!link(root),"store root became a link");
    const auto relative=fs::u8path(key);zarr_require(!relative.empty() && !relative.has_root_path() && key.find('\0')==std::string::npos && key.find(':')==std::string::npos && key.find('\\')==std::string::npos,"invalid store key");for(const auto& p:relative)zarr_require(p!=".." && p!=".","escaping store key");
    const auto path=root/relative;for(auto p=path.parent_path();p!=root;p=p.parent_path()){if(fs::exists(p))zarr_require(!link(p),"store path became a link");}
    const auto found=files_.find(key);
    if(found==files_.end()){zarr_require(missing_allowed,"required store metadata absent: "+key);if(fs::exists(fs::symlink_status(path)))throw WorkflowInputChanged("Previously absent OME-Zarr chunk appeared");return {};}
    zarr_require(!link(path),"store file became a link");ReaderFileGuard guard(path.u8string());same(found->second,*guard.version());zarr_require(found->second.bytes<=limit,"encoded file exceeds limit: "+key);
    std::vector<std::uint8_t> bytes(std::size_t(found->second.bytes));std::ifstream stream(path,std::ios::binary);if(!stream || (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))))throw std::runtime_error("Cannot read OME-Zarr file: "+key);
    guard.verify();return bytes;
}
void ZarrStore::require_current(const std::atomic<bool>* c)const {
    const auto now=observe(c);if(now.size()!=files_.size())throw WorkflowInputChanged("OME-Zarr directory membership changed");
    auto a=files_.begin(),b=now.begin();for(;a!=files_.end();++a,++b){if(a->first!=b->first)throw WorkflowInputChanged("OME-Zarr directory membership changed");same(a->second,b->second);}
}
}
