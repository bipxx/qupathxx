#include "reader_plugin.hpp"
#include "reader_registry.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "utf8_text.hpp"
#include <filesystem>
#include <mutex>
#include <stdexcept>
namespace qpx::detail {
#ifdef QUPATHXX_HAS_READER_PLUGINS
ReaderPluginCatalog load_reader_plugin_catalog(const std::string&);
#endif
namespace {
struct CatalogState {std::mutex mutex;bool configured=false,frozen=false;std::string manifest;ReaderPluginCatalog catalog;};
CatalogState& state(){static CatalogState value;return value;}
}
std::string reader_plugin_profile() {
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_X64)
    return "windows-x64-msvc"+std::to_string(_MSC_VER)+"-release-md";
#else
    return "unsupported";
#endif
}
void configure_reader_plugins(const std::string& manifest) {
    bounded_utf8(manifest,32768);
    if(manifest.empty() || !std::filesystem::u8path(manifest).is_absolute())
        throw std::invalid_argument("Reader plugin manifest must be an explicit absolute path");
    auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);
    if(value.frozen || value.configured)throw std::logic_error("Reader plugin catalog is already configured or frozen");
    value.manifest=manifest;value.configured=true;
}
const ReaderPluginCatalog& reader_plugin_catalog() {
    auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);
    if(!value.frozen) {
        value.frozen=true;
        if(!value.manifest.empty()) {
#ifdef QUPATHXX_HAS_READER_PLUGINS
            value.catalog=load_reader_plugin_catalog(value.manifest);
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
bool is_reader_plugin_server(const ImageServer&) noexcept {return false;}
std::shared_ptr<const ReaderPluginLink> capture_native_reader_plugin_link(const ImageServer&) {return {};}
#endif
}
void qpx::configure_reader_plugins(const std::string& manifest){detail::configure_reader_plugins(manifest);}
std::string qpx::reader_catalog_json(){return detail::reader_catalog_json();}
