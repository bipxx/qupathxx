#include "reader_plugin.hpp"
#include "reader_registry.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "utf8_text.hpp"
#include <filesystem>
#include <mutex>
#include <set>
#include <stdexcept>
namespace qpx::detail {
#ifdef QUPATHXX_HAS_READER_PLUGINS
ReaderPluginCatalog load_reader_plugin_catalog(const std::vector<std::string>&);
#endif
namespace {
struct CatalogState {std::mutex mutex;bool frozen=false;std::vector<std::string> manifests;ReaderPluginCatalog catalog;};
CatalogState& state(){static CatalogState value;return value;}
}
std::string reader_plugin_profile() {
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
    return "windows-x64-msvc"+std::to_string(_MSC_VER)+"-release-md";
#else
    return "unsupported";
#endif
}
void configure_reader_plugin_manifests(const std::vector<std::string>& manifests) {
    if(manifests.empty() || manifests.size()>32)
        throw std::invalid_argument("Provide between 1 and 32 reader plugin manifests");
    std::set<std::filesystem::path> unique;
    for(const auto& manifest:manifests) {
        bounded_utf8(manifest,32768);
        const auto path=std::filesystem::u8path(manifest);
        if(!path.is_absolute())
            throw std::invalid_argument("Reader plugin manifest must be an explicit absolute path");
        if(!unique.insert(path.lexically_normal()).second)
            throw std::invalid_argument("Duplicate reader plugin manifest path");
    }
    auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);
    if(value.frozen || !value.manifests.empty())throw std::logic_error("Reader plugin catalog is already configured or frozen");
    value.manifests=manifests;
}
void configure_reader_plugins(const std::string& manifest) {
    configure_reader_plugin_manifests({manifest});
}
const ReaderPluginCatalog& reader_plugin_catalog() {
    auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);
    if(!value.frozen) {
        value.frozen=true;
        if(!value.manifests.empty()) {
#ifdef QUPATHXX_HAS_READER_PLUGINS
            value.catalog=load_reader_plugin_catalog(value.manifests);
#else
            value.catalog.diagnostics.push_back("Reader plugins require QUPATHXX_BUILD_READER_PLUGINS=ON");
#endif
        }
    }
    return value.catalog;
}
const ReaderPluginCatalog* cached_reader_plugin_catalog() noexcept {
    try {auto& value=state();std::unique_lock<std::mutex> lock(value.mutex,std::try_to_lock);return lock.owns_lock() && value.frozen?&value.catalog:nullptr;}catch(...){return nullptr;}
}
#ifndef QUPATHXX_HAS_READER_PLUGINS
std::shared_ptr<ImageServer> open_plugin_image_server(const std::string&,std::shared_ptr<const AdmittedReaderPlugin>,const std::atomic<bool>*,const std::string&) {
    throw std::runtime_error("Reader plugins require QUPATHXX_BUILD_READER_PLUGINS=ON");
}
std::shared_ptr<ImageServer> open_bounded_plugin_image_server(const std::string&,std::shared_ptr<const AdmittedReaderPlugin>,std::size_t,const std::atomic<bool>*,const std::string&) {
    throw std::runtime_error("Reader plugins require QUPATHXX_BUILD_READER_PLUGINS=ON");
}
bool is_reader_plugin_server(const ImageServer&) noexcept {return false;}
std::shared_ptr<const ReaderPluginLink> capture_native_reader_plugin_link(const ImageServer&) {return {};}
std::shared_ptr<const DirectoryStoreSnapshot> capture_native_reader_plugin_store(const ImageServer&) {return {};}
std::shared_ptr<const std::string> capture_native_reader_plugin_label_description(const ImageServer&) {return {};}
#endif
}
void qpx::configure_reader_plugins(const std::string& manifest){detail::configure_reader_plugins(manifest);}
void qpx::configure_reader_plugin_manifests(const std::vector<std::string>& manifests){detail::configure_reader_plugin_manifests(manifests);}
std::string qpx::reader_catalog_json(){return detail::reader_catalog_json();}
