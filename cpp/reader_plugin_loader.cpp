// Initial supported loader profile is Windows x64/MSVC Release. No global DLL
// search-path changes: each verified absolute load uses package + System32 only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include "reader_plugin.hpp"
#include "sha256.hpp"
#include "ascii_case.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#if !defined(_M_X64) || !defined(_MSC_VER) || !defined(_DLL) || defined(_DEBUG)
#error "Reader plugin pilot requires Windows x64 MSVC Release /MD"
#endif

namespace qpx::detail {
namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
[[noreturn]] void refuse(const std::string& why){throw std::invalid_argument("Reader plugin: "+why);}
std::string lower(std::string value){ascii_lower_in_place(value);return value;}
bool digest(const std::string& value){return value.size()==64 && value.find_first_not_of("0123456789abcdef")==std::string::npos;}
std::string text(const Json& value,const char* key,std::size_t maximum=256) {
    auto result=value.at(key).get<std::string>();bounded_utf8(result,maximum);if(result.empty())refuse(std::string("empty ")+key);return result;
}
bool provider_id(const std::string& id) {
    // Namespaced IDs cannot collide with a built-in or reserved reader role.
    if(id.size()>128 || id.find('.')==std::string::npos)return false;
    bool start=true;
    for(char c:id){if(c=='.'){if(start)return false;start=true;}else{if(start && !(c>='a' && c<='z'))return false;if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-'))return false;start=false;}}
    return !start;
}
std::string path_key(const fs::path& path){return lower(path.lexically_normal().generic_u8string());}
bool inside(const fs::path& path,const fs::path& root) {
    auto child=path_key(path),parent=path_key(root);if(parent.back()!='/')parent+='/';return child.size()>parent.size() && child.compare(0,parent.size(),parent)==0;
}
fs::path package_path(const fs::path& root,const std::string& relative) {
    bounded_utf8(relative,32768);const auto path=fs::u8path(relative);
    if(relative.empty() || path.is_absolute() || path.has_root_name() || relative.find(':')!=std::string::npos)refuse("module/dependency path must be relative to its package");
    for(const auto& part:path)if(part==".." || part==".")refuse("module/dependency path escapes its package");
    const auto resolved=fs::canonical(root/path);if(!inside(resolved,root))refuse("module/dependency path escapes its package");return resolved;
}
fs::path module_path(HMODULE module) {
    std::wstring path(32768,L'\0');const auto n=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(!n || n>=path.size())refuse("cannot identify a resolved module");path.resize(n);return fs::canonical(fs::path(path));
}
fs::path system_directory() {
    std::wstring path(32768,L'\0');const auto n=GetSystemDirectoryW(path.data(),UINT(path.size()));
    if(!n || n>=path.size())refuse("cannot locate System32");path.resize(n);return fs::canonical(fs::path(path));
}
bool api_set(const std::string& name){return name.rfind("api-ms-win-",0)==0 || name.rfind("ext-ms-",0)==0;}
struct File {
    fs::path path;std::string sha;std::vector<unsigned char> bytes;HANDLE handle=INVALID_HANDLE_VALUE;
    explicit File(fs::path p,const std::string& expected,std::size_t remaining):path(std::move(p)) {
        // Deny mutation/replacement from hash verification through all loading.
        handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)refuse("cannot lock module/dependency file");
        try {
            LARGE_INTEGER size{};if(!GetFileSizeEx(handle,&size) || size.QuadPart<=0 || std::uint64_t(size.QuadPart)>remaining)refuse("module/dependency package exceeds 256 MiB");
            bytes.resize(std::size_t(size.QuadPart));std::size_t offset=0;Sha256 hash;
            while(offset<bytes.size()){DWORD got=0;const auto count=DWORD(std::min<std::size_t>(bytes.size()-offset,1024*1024));if(!ReadFile(handle,bytes.data()+offset,count,&got,nullptr) || !got)refuse("cannot read locked module/dependency");hash.update(bytes.data()+offset,got);offset+=got;}
            sha=hash.finish();if(!digest(expected) || sha!=expected)refuse("module/dependency SHA-256 mismatch");
        }catch(...){CloseHandle(handle);handle=INVALID_HANDLE_VALUE;throw;}
    }
    ~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    template<class T> T get(std::size_t offset) const {if(offset>bytes.size() || sizeof(T)>bytes.size()-offset)refuse("truncated PE image");T result;std::memcpy(&result,bytes.data()+offset,sizeof(T));return result;}
};
struct Import {std::string name;std::uint32_t iat=0;};
struct Pe {
    const File& file;IMAGE_NT_HEADERS64 nt{};std::vector<IMAGE_SECTION_HEADER> sections;std::vector<Import> imports;
    explicit Pe(const File& f):file(f) {
        const auto dos=file.get<IMAGE_DOS_HEADER>(0);if(dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0)refuse("invalid PE header");
        nt=file.get<IMAGE_NT_HEADERS64>(std::size_t(dos.e_lfanew));
        if(nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || !(nt.FileHeader.Characteristics&IMAGE_FILE_DLL) || nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections>96 || nt.OptionalHeader.NumberOfRvaAndSizes<IMAGE_NUMBEROF_DIRECTORY_ENTRIES)refuse("module is not an admitted x64 DLL");
        const auto section_start=std::size_t(dos.e_lfanew)+sizeof(IMAGE_NT_HEADERS64);
        for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i)sections.push_back(file.get<IMAGE_SECTION_HEADER>(section_start+i*sizeof(IMAGE_SECTION_HEADER)));
        if(nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].Size)refuse("delay-load dependencies are outside the v1 pilot profile");
        const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!directory.Size)return;
        if(directory.Size>1024*sizeof(IMAGE_IMPORT_DESCRIPTOR))refuse("too many PE imports");
        bool ended=false;
        for(std::size_t i=0;i+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size;i+=sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
            const auto item=file.get<IMAGE_IMPORT_DESCRIPTOR>(offset(std::uint64_t(directory.VirtualAddress)+i,sizeof(IMAGE_IMPORT_DESCRIPTOR)));
            if(!item.Name && !item.FirstThunk && !item.OriginalFirstThunk){ended=true;break;}
            if(!item.Name || !item.FirstThunk)refuse("invalid PE import");
            auto name=lower(cstring(item.Name));if(name.size()>255 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos || fs::u8path(name).extension()!=".dll")refuse("invalid dependency basename");
            imports.push_back({std::move(name),item.FirstThunk});
        }
        if(!ended)refuse("unterminated PE import table");
    }
    std::size_t offset(std::uint64_t rva,std::size_t bytes) const {
        if(rva<nt.OptionalHeader.SizeOfHeaders && rva<=file.bytes.size() && bytes<=file.bytes.size()-std::size_t(rva))return std::size_t(rva);
        for(const auto& section:sections)if(rva>=section.VirtualAddress && rva-section.VirtualAddress<section.SizeOfRawData) {
            const auto delta=rva-section.VirtualAddress;
            const auto result=std::uint64_t(section.PointerToRawData)+delta;
            if(bytes>section.SizeOfRawData-delta || result>file.bytes.size() || bytes>file.bytes.size()-std::size_t(result))refuse("PE RVA exceeds file");return std::size_t(result);
        }
        refuse("PE RVA is outside image sections");
    }
    std::string cstring(std::uint32_t rva) const {
        std::string result;for(unsigned i=0;i<256;++i){const auto value=file.bytes[offset(std::uint64_t(rva)+i,1)];if(!value)return result;result+=char(value);}refuse("unterminated PE dependency name");
    }
};
struct Package {
    std::map<std::string,std::shared_ptr<File>> files;std::map<std::string,std::shared_ptr<Pe>> images;
    std::map<std::string,HMODULE> loaded;fs::path system=system_directory();
    std::set<std::string> visiting;
    std::size_t bytes=0;
    bool is_system(const fs::path& path)const{return inside(path,system);}
    fs::path expected(const std::string& name)const {
        const auto found=files.find(name);if(found!=files.end())return found->second->path;
        if(api_set(name))return {};
        const auto path=system/fs::u8path(name);if(fs::is_regular_file(path))return fs::canonical(path);
        refuse("undeclared non-system dependency: "+name);
    }
    void preloaded(const std::string& name,const fs::path& wanted)const {
        if(wanted.empty())return;
        const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());if(snapshot==INVALID_HANDLE_VALUE)refuse("cannot inspect resolved dependencies");
        MODULEENTRY32W item{};item.dwSize=sizeof(item);bool conflict=false;
        if(Module32FirstW(snapshot,&item))do{if(lower(fs::path(item.szModule).u8string())==name && path_key(module_path(item.hModule))!=path_key(wanted))conflict=true;}while(Module32NextW(snapshot,&item));
        CloseHandle(snapshot);if(conflict)refuse("unexpected already-loaded dependency: "+name);
    }
    void preflight()const {
        for(const auto& [name,image]:images)for(const auto& dependency:image->imports)preloaded(dependency.name,expected(dependency.name));
    }
    HMODULE load(const std::string& name) {
        const auto ready=loaded.find(name);if(ready!=loaded.end())return ready->second;
        if(!visiting.insert(name).second)refuse("cyclic package dependencies are outside the v1 pilot profile");
        const auto image=images.at(name);
        for(const auto& import:image->imports)if(images.count(import.name))load(import.name);
        const auto path=files.at(name)->path;
        const auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module)refuse("module/dependency load failed (Win32 "+std::to_string(GetLastError())+")");
        // Deliberately never FreeLibrary, including after admission failure.
        loaded.emplace(name,module);visiting.erase(name);
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(module),&pinned))refuse("cannot pin module through process exit");
        if(path_key(module_path(module))!=path_key(path))refuse("loaded module path differs from its admitted file");
        resolved(*image,module);return module;
    }
    void resolved(const Pe& image,HMODULE module)const {
        const auto base=reinterpret_cast<const unsigned char*>(module);
        for(const auto& import:image.imports) {
            const auto wanted=expected(import.name);bool terminated=false;
            for(std::uint64_t i=0;i<65536;++i) {
                const auto rva=std::uint64_t(import.iat)+i*sizeof(IMAGE_THUNK_DATA64);
                if(rva>image.nt.OptionalHeader.SizeOfImage || sizeof(IMAGE_THUNK_DATA64)>image.nt.OptionalHeader.SizeOfImage-rva)refuse("resolved PE import exceeds mapped image");
                IMAGE_THUNK_DATA64 thunk;std::memcpy(&thunk,base+rva,sizeof(thunk));if(!thunk.u1.Function){terminated=true;break;}
                HMODULE dependency=nullptr;
                if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(thunk.u1.Function),&dependency))refuse("cannot identify actual resolved dependency");
                const auto actual=module_path(dependency);
                // Forwarded system functions may resolve into another System32 DLL.
                if(files.count(import.name)){if(path_key(actual)!=path_key(wanted))refuse("actual dependency differs from admitted package file: "+import.name);}
                else if(!is_system(actual))refuse("actual dependency is outside System32: "+import.name);
            }
            if(!terminated)refuse("unterminated resolved import table");
        }
    }
    template<class Function> void callback(Function function)const {
        if(!function)refuse("missing ABI callback");HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(function),&module))refuse("ABI callback has no admitted module");
        const auto actual=path_key(module_path(module));for(const auto& [name,file]:files)if(path_key(file->path)==actual)return;
        refuse("ABI callback points outside admitted package");
    }
};
std::string returned(const qpx_reader_text_v1& value,char* buffer,std::size_t capacity,qpx_reader_status_v1 status=QPX_READER_OK) {
    if(value.data!=buffer || value.capacity!=capacity)refuse("invalid ABI text buffer");
    if(value.size>capacity){if(status==QPX_READER_BUFFER_TOO_SMALL)return "required error text exceeds host limit";refuse("invalid ABI text buffer");}
    std::string result(buffer,std::size_t(value.size));bounded_utf8(result,capacity);return result;
}
std::shared_ptr<const AdmittedReaderPlugin> admit(const Json& descriptor,const fs::path& root) {
    auto result=std::make_shared<AdmittedReaderPlugin>();auto& info=result->info;
    info.provider_id=text(descriptor,"provider_id",128);if(!valid_reader_plugin_id(info.provider_id))refuse("reserved or invalid provider ID");
    info.display_name=text(descriptor,"display_name");info.version=text(descriptor,"version");info.contract=text(descriptor,"contract");info.profile=text(descriptor,"profile");
    if(descriptor.at("abi")!=QPX_READER_ABI_V1 || info.profile!=reader_plugin_profile())refuse("ABI/compiler/CRT profile mismatch");
    const auto& caps=descriptor.at("capabilities");if(!caps.at("display_rgba").get<bool>())refuse("pilot provider must support RGBA display");
    info.capabilities=QPX_READER_CAP_RGBA|(caps.at("raw_samples").get<bool>()?QPX_READER_CAP_RAW:0)|
        (caps.value("source_files",false)?QPX_READER_CAP_SOURCE_FILES:0);
    if(descriptor.contains("options"))info.default_options=normalize_reader_plugin_options(descriptor.at("options").dump());
    auto package=std::make_shared<Package>();
    const auto add=[&](const Json& entry) {
        const auto path=package_path(root,text(entry,"path",32768));const auto name=lower(path.filename().u8string());
        if(package->files.count(name))refuse("duplicate module/dependency basename");
        auto file=std::make_shared<File>(path,text(entry,"sha256",64),256u*1024*1024-package->bytes);
        package->bytes+=file->bytes.size();auto image=std::make_shared<Pe>(*file);
        package->files.emplace(name,file);package->images.emplace(name,image);return name;
    };
    const auto module_name=add(descriptor.at("module"));info.module_sha256=package->files.at(module_name)->sha;
    const auto& dependencies=descriptor.at("dependencies");if(!dependencies.is_array() || dependencies.size()>63)refuse("dependency count exceeds limit");
    for(const auto& dependency:dependencies){const auto name=add(dependency);info.dependencies.push_back({name,package->files.at(name)->sha});}
    std::sort(info.dependencies.begin(),info.dependencies.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    package->preflight();const auto module=package->load(module_name);
    for(const auto& [name,file]:package->files)if(!package->loaded.count(name))refuse("declared dependency is not in the module import graph");
    const auto entry=reinterpret_cast<qpx_reader_entry_fn_v1>(GetProcAddress(module,"qpx_reader_entry_v1"));if(!entry)refuse("missing versioned ABI entry");package->callback(entry);
    std::array<char,4096> error_buffer{};qpx_reader_text_v1 error{error_buffer.data(),error_buffer.size(),0};result->api.struct_size=sizeof(result->api);
    qpx_reader_status_v1 status;
    try {status=entry(QPX_READER_ABI_V1,&result->api,&error);}catch(...){refuse("entry threw across C ABI");}
    const auto message=returned(error,error_buffer.data(),error_buffer.size(),status);
    if(status!=QPX_READER_OK)refuse("ABI entry refused: "+message);
    auto& api=result->api;if(api.struct_size!=sizeof(api) || api.abi_version!=QPX_READER_ABI_V1)refuse("returned ABI table mismatch");
    package->callback(api.describe);package->callback(api.open);package->callback(api.destroy);package->callback(api.metadata);package->callback(api.level);package->callback(api.channel_name);package->callback(api.read_rgba);package->callback(api.read_raw);
    std::array<std::array<char,256>,4> buffers{};qpx_reader_info_v1 actual{};actual.struct_size=sizeof(actual);
    qpx_reader_text_v1* fields[]={&actual.provider_id,&actual.display_name,&actual.version,&actual.contract};
    for(unsigned i=0;i<4;++i)*fields[i]={buffers[i].data(),buffers[i].size(),0};error.size=0;
    qpx_reader_status_v1 described;
    try {described=api.describe(&actual,&error);}catch(...){refuse("describe threw across C ABI");}
    const auto description_error=returned(error,error_buffer.data(),error_buffer.size(),described);if(described!=QPX_READER_OK)refuse("descriptor query refused: "+description_error);
    std::array<std::string,4> values;for(unsigned i=0;i<4;++i)values[i]=returned(*fields[i],buffers[i].data(),buffers[i].size());
    if(actual.struct_size!=sizeof(actual) || actual.abi_version!=QPX_READER_ABI_V1 || actual.capabilities!=info.capabilities || values[0]!=info.provider_id || values[1]!=info.display_name || values[2]!=info.version || values[3]!=info.contract)refuse("descriptor/ABI provider disagreement");
    if(info.capabilities&QPX_READER_CAP_SOURCE_FILES) {
        const auto files_entry=reinterpret_cast<qpx_reader_files_entry_fn_v1>(GetProcAddress(module,"qpx_reader_files_entry_v1"));
        if(!files_entry)refuse("missing file-set ABI entry");package->callback(files_entry);
        result->files.struct_size=sizeof(result->files);error.size=0;
        qpx_reader_status_v1 files_status;
        try {files_status=files_entry(1,&result->files,&error);}catch(...){refuse("file-set entry threw across C ABI");}
        const auto files_error=returned(error,error_buffer.data(),error_buffer.size(),files_status);
        if(files_status!=QPX_READER_OK)refuse("file-set entry refused: "+files_error);
        if(result->files.struct_size!=sizeof(result->files) || result->files.abi_version!=1)refuse("file-set ABI table mismatch");
        package->callback(result->files.count);package->callback(result->files.path);
    }
    // Only the native library reference is retained. File locks end after all
    // verification/load callbacks; Windows keeps mapped images immutable/loaded.
    package->images.clear();package->files.clear();
    result->keepalive=std::shared_ptr<const void>(package,module);return result;
}
}
ReaderPluginCatalog load_reader_plugin_catalog(const std::string& path) {
    ReaderPluginCatalog result;
    try {
        const auto config=fs::canonical(fs::u8path(path));const auto root=config.parent_path();const auto size=fs::file_size(config);if(!size || size>1024*1024)refuse("manifest size exceeds limit");
        std::ifstream input(config,std::ios::binary);std::string contents(1024*1024+1,'\0');
        input.read(contents.data(),std::streamsize(contents.size()));const auto read=input.gcount();
        if(input.bad() || read<=0 || read>1024*1024)refuse("manifest read exceeds limits or failed");
        contents.resize(std::size_t(read));bounded_utf8(contents,1024*1024);
        const auto document=Json::parse(contents,[](int depth,Json::parse_event_t,Json&){if(depth>64)refuse("manifest nesting exceeds limit");return true;});if(document.at("schema")!="qpx.reader-plugins/1")refuse("unsupported manifest schema");
        const auto& entries=document.at("plugins");if(!entries.is_array() || entries.size()>32)refuse("manifest provider count exceeds limit");
        std::set<std::string> ids;
        for(const auto& entry:entries) {
            try {
                const auto id=text(entry,"provider_id",128);if(!ids.insert(id).second)refuse("duplicate provider ID");
                result.readers.push_back(admit(entry,root));
            }catch(const std::exception& error){result.diagnostics.push_back(error.what());}
        }
    }catch(const std::exception& error){result.diagnostics.push_back(error.what());}
    return result;
}
}
