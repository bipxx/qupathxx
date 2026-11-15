#include "format_plugin_loader.hpp"
#include "native_plugin_package.hpp"
#include "reader_plugin_link.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>

namespace qpx::detail {
namespace {
using Json=nlohmann::json;
[[noreturn]] void refuse(const std::string& message){throw std::invalid_argument("Format plugin: "+message);}
bool sha(const std::string& value){return value.size()==64 && value.find_first_not_of("0123456789abcdef")==std::string::npos;}
void clean(const std::string& value,std::size_t limit,bool empty=false) {
    bounded_utf8(value,limit);if(!empty && value.empty())refuse("empty text field");
    for(unsigned char c:value)if(c<32 || c==127)refuse("text contains a control character");
}
std::string field(const Json& value,const char* name,std::size_t limit=256) {
    auto result=value.at(name).get<std::string>();clean(result,limit);return result;
}
std::string hash(const std::string& prefix,const std::string& value) {
    Sha256 digest;digest.integer(prefix.size());digest.update(prefix.data(),prefix.size());
    digest.integer(value.size());digest.update(value.data(),value.size());return digest.finish();
}
Json implementation_json(const FormatPluginInfo& info) {
    if(info.domain!=FormatPluginDomain::Matrix && info.domain!=FormatPluginDomain::Annotation)refuse("invalid domain");
    if(info.abi!=1 || !valid_reader_plugin_id(info.provider_id))refuse("invalid provider identity");
    clean(info.format_id,64);clean(info.display_name,256);clean(info.version,256);clean(info.contract,256);clean(info.profile,256);
    if(info.format_id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=std::string::npos)refuse("invalid format ID");
    if(!sha(info.module_sha256))refuse("invalid module digest");
    if(info.dependencies.size()>63)refuse("too many dependencies");
    Json dependencies=Json::array();std::string previous;
    for(const auto& item:info.dependencies) {
        clean(item.name,255);
        if(item.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos ||
            item.name.size()<5 || item.name.substr(item.name.size()-4)!=".dll" || !sha(item.sha256) ||
            (!previous.empty() && previous>=item.name))refuse("invalid or unordered dependency identity");
        dependencies.push_back({{"name",item.name},{"sha256",item.sha256}});previous=item.name;
    }
    return Json{{"abi",info.abi},{"domain",static_cast<std::uint32_t>(info.domain)},
        {"provider_id",info.provider_id},{"format_id",info.format_id},{"display_name",info.display_name},
        {"version",info.version},{"contract",info.contract},{"profile",info.profile},
        {"module_sha256",info.module_sha256},{"dependencies",std::move(dependencies)}};
}
struct State {std::mutex mutex;bool configured=false,frozen=false;std::string path;FormatPluginCatalog catalog;};
State& state(){static State value;return value;}
#ifdef QUPATHXX_HAS_FORMAT_PLUGINS
std::string returned(const qpx_reader_text_v1& value,char* data,std::size_t capacity,qpx_reader_status_v1 status=QPX_READER_OK) {
    if(value.data!=data || value.capacity!=capacity || value.size>capacity)refuse("invalid ABI text buffer");
    std::string result(data,static_cast<std::size_t>(value.size));bounded_utf8(result,capacity);
    if(status!=QPX_READER_OK)refuse("ABI operation refused: "+result);return result;
}
std::shared_ptr<AdmittedFormatPlugin> admit(const Json& descriptor,const std::string& root) {
    auto result=std::make_shared<AdmittedFormatPlugin>();auto& info=result->info;
    info.provider_id=field(descriptor,"provider_id",128);info.format_id=field(descriptor,"format_id",64);
    info.display_name=field(descriptor,"display_name");info.version=field(descriptor,"version");
    info.contract=field(descriptor,"contract");info.profile=field(descriptor,"profile");
    const auto domain=field(descriptor,"domain",16);
    if(domain=="matrix")info.domain=FormatPluginDomain::Matrix;
    else if(domain=="annotation")info.domain=FormatPluginDomain::Annotation;else refuse("unsupported domain");
    if(descriptor.at("abi")!=1 || info.profile!=format_plugin_profile())refuse("ABI/compiler/CRT profile mismatch");
    const std::set<std::string> reserved=info.domain==FormatPluginDomain::Matrix?
        std::set<std::string>{"qmx","matrix-market","h5ad"}:std::set<std::string>{"geojson","measurements-csv"};
    if(reserved.count(info.format_id))refuse("format ID collides with a built-in");
    if(descriptor.contains("options"))info.default_options=normalize_reader_plugin_options(descriptor.at("options").dump());
    const auto file=[](const Json& item){return NativePluginFile{field(item,"path",32768),field(item,"sha256",64)};};
    std::vector<NativePluginFile> dependencies;
    const auto& list=descriptor.at("dependencies");if(!list.is_array() || list.size()>63)refuse("invalid dependency count");
    for(const auto& item:list)dependencies.push_back(file(item));
    result->module=NativePluginModule::admit(root,file(descriptor.at("module")),dependencies);
    info.module_sha256=result->module->module_sha256();
    for(const auto& item:result->module->dependencies())info.dependencies.push_back({item.name,item.sha256});
    info.implementation_sha256=format_plugin_implementation_identity(info);
    std::array<char,4096> errors{};qpx_reader_text_v1 error{errors.data(),errors.size(),0};
    qpx_reader_status_v1 status;
    if(info.domain==FormatPluginDomain::Matrix) {
        auto entry=reinterpret_cast<qpx_matrix_format_entry_fn_v1>(result->module->symbol("qpx_matrix_format_entry_v1"));
        auto& api=result->matrix;api.struct_size=sizeof(api);
        try{status=entry(1,&api,&error);}catch(...){refuse("matrix entry threw across C ABI");}
        returned(error,errors.data(),errors.size(),status);
        if(api.struct_size!=sizeof(api) || api.abi_version!=1)refuse("matrix API table mismatch");
        result->module->callback(api.describe);result->module->callback(api.open);result->module->callback(api.destroy);
        result->module->callback(api.metadata);result->module->callback(api.axis);result->module->callback(api.values);
    } else {
        auto entry=reinterpret_cast<qpx_annotation_format_entry_fn_v1>(result->module->symbol("qpx_annotation_format_entry_v1"));
        auto& api=result->annotation;api.struct_size=sizeof(api);
        try{status=entry(1,&api,&error);}catch(...){refuse("annotation entry threw across C ABI");}
        returned(error,errors.data(),errors.size(),status);
        if(api.struct_size!=sizeof(api) || api.abi_version!=1)refuse("annotation API table mismatch");
        result->module->callback(api.describe);result->module->callback(api.open);result->module->callback(api.destroy);
        result->module->callback(api.metadata);result->module->callback(api.points);
    }
    std::array<std::array<char,256>,6> buffers{};qpx_format_info_v1 actual{};actual.struct_size=sizeof(actual);
    qpx_reader_text_v1* fields[]={&actual.provider_id,&actual.format_id,&actual.display_name,&actual.version,&actual.contract,&actual.profile};
    for(unsigned i=0;i<6;++i)*fields[i]={buffers[i].data(),buffers[i].size(),0};error.size=0;
    try{status=info.domain==FormatPluginDomain::Matrix?result->matrix.describe(&actual,&error):result->annotation.describe(&actual,&error);}
    catch(...){refuse("describe threw across C ABI");}
    returned(error,errors.data(),errors.size(),status);
    std::array<std::string,6> values;
    for(unsigned i=0;i<6;++i)values[i]=returned(*fields[i],buffers[i].data(),buffers[i].size());
    if(actual.struct_size!=sizeof(actual) || actual.abi_version!=1 || actual.reserved ||
        actual.domain!=static_cast<std::uint32_t>(info.domain) || values!=std::array<std::string,6>{info.provider_id,info.format_id,info.display_name,info.version,info.contract,info.profile})
        refuse("descriptor/ABI disagreement");
    result->module->finish_admission();return result;
}
FormatPluginCatalog load(const std::string& path) {
    FormatPluginCatalog result;
    try {
        const auto config=std::filesystem::canonical(std::filesystem::u8path(path));
        const auto bytes=std::filesystem::file_size(config);if(!bytes || bytes>1024*1024)refuse("manifest size exceeds limit");
        std::ifstream stream(config,std::ios::binary);std::string contents(static_cast<std::size_t>(bytes)+1,'\0');
        stream.read(contents.data(),static_cast<std::streamsize>(contents.size()));
        if(stream.bad() || stream.gcount()!=static_cast<std::streamsize>(bytes))refuse("manifest read changed or failed");
        contents.resize(static_cast<std::size_t>(bytes));bounded_utf8(contents,1024*1024);
        const auto json=Json::parse(contents,[](int depth,Json::parse_event_t,Json&){if(depth>64)refuse("manifest nesting exceeds limit");return true;});
        if(json.at("schema")!="qpx.format-plugins/1")refuse("unsupported manifest schema");
        const auto& entries=json.at("plugins");if(!entries.is_array() || entries.size()>32)refuse("provider count exceeds limit");
        std::set<std::string> ids,formats;
        for(const auto& item:entries)try {
            const auto id=field(item,"provider_id",128),key=field(item,"domain",16)+":"+field(item,"format_id",64);
            if(!ids.insert(id).second || !formats.insert(key).second)refuse("duplicate provider/format ID");
            result.providers.push_back(admit(item,config.parent_path().u8string()));
        }catch(const std::exception& e){std::string message=e.what();const std::string prefix="Reader plugin:";
            if(message.rfind(prefix,0)==0)message.replace(0,prefix.size(),"Format plugin:");result.diagnostics.push_back(std::move(message));}
    }catch(const std::exception& e){result.diagnostics.push_back(e.what());}
    return result;
}
#endif
}
std::string format_plugin_profile(){
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
    return "windows-x64-msvc"+std::to_string(_MSC_VER)+"-release-md";
#else
    return "unsupported";
#endif
}
std::string format_plugin_implementation_identity(const FormatPluginInfo& info){return hash("qpx.format-plugin-implementation/1",implementation_json(info).dump());}
const FormatPluginCatalog& format_plugin_catalog() {
    auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);
    if(!value.frozen) {
        value.frozen=true;
        if(!value.path.empty()) {
#ifdef QUPATHXX_HAS_FORMAT_PLUGINS
            value.catalog=load(value.path);
#else
            value.catalog.diagnostics.push_back("Format plugins require QUPATHXX_BUILD_FORMAT_PLUGINS=ON on Windows x64 MSVC Release /MD");
#endif
        }
    }
    return value.catalog;
}
} // namespace qpx::detail
namespace qpx {
void configure_format_plugins(const std::string& path) {
    detail::bounded_utf8(path,32768);
    if(path.empty() || !std::filesystem::u8path(path).is_absolute())throw std::invalid_argument("Format plugin manifest must be an explicit absolute path");
    auto& value=detail::state();std::lock_guard<std::mutex> lock(value.mutex);
    if(value.configured || value.frozen)throw std::logic_error("Format plugin catalog is already configured or frozen");
    value.path=path;value.configured=true;
}
std::vector<FormatPluginInfo> format_plugin_infos(){std::vector<FormatPluginInfo> result;for(const auto& item:detail::format_plugin_catalog().providers)result.push_back(item->info);return result;}
std::vector<std::string> format_plugin_diagnostics(){return detail::format_plugin_catalog().diagnostics;}
std::string format_plugin_receipt_json(const FormatPluginReceipt& receipt) {
    if(!detail::sha(receipt.source_sha256) || receipt.options.size()>4096 ||
        detail::normalize_reader_plugin_options(receipt.options)!=receipt.options)throw std::invalid_argument("Invalid format plugin receipt source/options");
    const auto implementation=detail::implementation_json(receipt.provider);
    if(detail::format_plugin_implementation_identity(receipt.provider)!=receipt.provider.implementation_sha256)throw std::invalid_argument("Format plugin implementation digest disagrees");
    const auto result=nlohmann::json{{"schema","qpx.format-plugin-receipt/1"},{"provider",implementation},
        {"implementation_sha256",receipt.provider.implementation_sha256},{"options",receipt.options},{"source_sha256",receipt.source_sha256}}.dump();
    if(result.size()>32768)throw std::invalid_argument("Format plugin receipt exceeds limit");return result;
}
FormatPluginReceipt parse_format_plugin_receipt(const std::string& value) {
    detail::bounded_utf8(value,32768);const auto json=nlohmann::json::parse(value);
    if(!json.is_object() || json.size()!=5 || json.at("schema")!="qpx.format-plugin-receipt/1")throw std::invalid_argument("Invalid format plugin receipt");
    const auto& info=json.at("provider");if(!info.is_object() || info.size()!=10)throw std::invalid_argument("Invalid format provider receipt");
    FormatPluginReceipt result;auto& target=result.provider;
    target.domain=static_cast<FormatPluginDomain>(info.at("domain").get<std::uint32_t>());target.abi=info.at("abi").get<std::uint32_t>();
    target.provider_id=info.at("provider_id").get<std::string>();target.format_id=info.at("format_id").get<std::string>();
    target.display_name=info.at("display_name").get<std::string>();target.version=info.at("version").get<std::string>();target.contract=info.at("contract").get<std::string>();
    target.profile=info.at("profile").get<std::string>();target.module_sha256=info.at("module_sha256").get<std::string>();
    const auto& dependencies=info.at("dependencies");if(!dependencies.is_array() || dependencies.size()>63)throw std::invalid_argument("Invalid receipt dependencies");
    for(const auto& item:dependencies){if(!item.is_object() || item.size()!=2)throw std::invalid_argument("Invalid dependency receipt");target.dependencies.push_back({item.at("name").get<std::string>(),item.at("sha256").get<std::string>()});}
    target.implementation_sha256=json.at("implementation_sha256").get<std::string>();result.options=json.at("options").get<std::string>();result.source_sha256=json.at("source_sha256").get<std::string>();
    if(format_plugin_receipt_json(result)!=value)throw std::invalid_argument("Noncanonical format plugin receipt");return result;
}
std::string format_plugin_receipt_identity(const FormatPluginReceipt& receipt){return detail::hash("qpx.format-plugin-receipt/1",format_plugin_receipt_json(receipt));}
}
