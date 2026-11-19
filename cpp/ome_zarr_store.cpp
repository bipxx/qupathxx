#include "ome_zarr_internal.hpp"
#include "directory_inventory.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <fstream>
#include <limits>
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
}
std::map<std::string,FileVersion> ZarrStore::observe(const std::atomic<bool>* cancelled)const {
    if(layout()==NgffLayout::v05)
        return observe_directory_inventory(root,{options.inventory_bytes,options.max_files,64,4096},cancelled);
    return observe_directory_inventory(root,{options.inventory_bytes,options.max_files,
        directory_inventory_legacy_unbounded,directory_inventory_legacy_unbounded},cancelled);
}
#ifdef QUPATHXX_HAS_NGFF05
ZarrStore::ZarrStore(const std::string& path,OmeZarrOptions opts,const std::atomic<bool>* c):ZarrStore(path,opts,c,NgffLayout::v04){}
ZarrStore::ZarrStore(const std::string& path,OmeZarrOptions opts,const std::atomic<bool>* c,NgffLayout profile):root(fs::absolute(fs::u8path(path)).lexically_normal()),options(opts),layout_(profile){
#else
ZarrStore::ZarrStore(const std::string& path,OmeZarrOptions opts,const std::atomic<bool>* c):root(fs::absolute(fs::u8path(path)).lexically_normal()),options(opts){
#endif
    options.validate();
    if(layout()==NgffLayout::v05){
        require_directory_path_bytes(root,4096);
        require_directory_real_ancestors(root,c);
    }
    for(auto p=root;!p.empty();p=p.parent_path()){zarr_require(!link(p),"store ancestors cannot be links/reparse points");if(p==p.parent_path())break;}
    files_=observe(c);payload_=directory_inventory_payload(files_);id=directory_inventory_sha256(root,files_);
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
    return read_bytes(key,0,std::nullopt,limit,missing_allowed);
}
#ifdef QUPATHXX_HAS_NGFF05
std::vector<std::uint8_t> ZarrStore::read_range(const std::string& key,std::uint64_t offset,std::size_t bytes,std::size_t limit)const {
    return read_bytes(key,offset,bytes,limit,false);
}
#endif
std::vector<std::uint8_t> ZarrStore::read_bytes(const std::string& key,std::uint64_t offset,
    std::optional<std::size_t> length,std::size_t limit,bool missing_allowed)const {
    zarr_require(!link(root),"store root became a link");
    const auto relative=fs::u8path(key);zarr_require(!relative.empty() && !relative.has_root_path() && key.find('\0')==std::string::npos && key.find(':')==std::string::npos && key.find('\\')==std::string::npos,"invalid store key");for(const auto& p:relative)zarr_require(p!=".." && p!=".","escaping store key");
    const auto path=root/relative;for(auto p=path.parent_path();p!=root;p=p.parent_path()){if(fs::exists(p))zarr_require(!link(p),"store path became a link");}
    const auto found=files_.find(key);
    if(found==files_.end()){zarr_require(missing_allowed,"required store metadata absent: "+key);if(fs::exists(fs::symlink_status(path)))throw WorkflowInputChanged("Previously absent OME-Zarr chunk appeared");return {};}
    zarr_require(!link(path),"store file became a link");ReaderFileGuard guard(path.u8string());same(found->second,*guard.version());
    const auto size=found->second.bytes;
    const auto count=length?std::uint64_t(*length):size;
    zarr_require(offset<=size && count<=size-offset,"encoded range is outside file: "+key);
    zarr_require(count<=limit,"encoded file exceeds limit: "+key);
    zarr_require(offset<=std::uint64_t((std::numeric_limits<std::streamoff>::max)()),"encoded range offset is too large: "+key);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count));
    std::ifstream stream(path,std::ios::binary);
    if(offset)stream.seekg(static_cast<std::streamoff>(offset));
    if(!stream || (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()))))
        throw std::runtime_error("Cannot read OME-Zarr file: "+key);
    guard.verify();return bytes;
}
void ZarrStore::require_current(const std::atomic<bool>* c)const {
    const auto now=observe(c);if(now.size()!=files_.size())throw WorkflowInputChanged("OME-Zarr directory membership changed");
    auto a=files_.begin(),b=now.begin();for(;a!=files_.end();++a,++b){if(a->first!=b->first)throw WorkflowInputChanged("OME-Zarr directory membership changed");same(a->second,b->second);}
}
}
