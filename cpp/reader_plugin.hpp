#pragma once
// Private host API. Installed plugins use only the C header, never this bridge.
#include "qupathxx/reader_plugin.h"
#include "qupathxx/image_server.hpp"
#include "reader_plugin_link.hpp"
#include <string>
#include <vector>
namespace qpx { class DirectoryStoreSnapshot; }
namespace qpx::detail {
struct ReaderPluginInfo {
    std::string provider_id,display_name,version,contract,profile,module_sha256;
    std::uint64_t capabilities=0;
    std::vector<ReaderPluginDependency> dependencies;
    std::string default_options;
};
struct AdmittedReaderPlugin {
    ReaderPluginInfo info;
    qpx_reader_api_v1 api{};
    qpx_reader_files_api_v1 files{};
    std::shared_ptr<const void> keepalive;
    // Append-only: retained native registry objects use the existing prefix.
    qpx_reader_store_api_v1 store{};
    qpx_reader_label_api_v1 label{};
};
struct ReaderPluginCatalog {
    std::vector<std::shared_ptr<const AdmittedReaderPlugin>> readers;
    std::vector<std::string> diagnostics;
};
// Configure once before the first catalog/open. Explicit absolute manifest;
// relative module paths resolve under its directory, never CWD/PATH. Catalog
// freezes on first query; no reload/unload, diagnostics cannot disable built-ins.
void configure_reader_plugins(const std::string& manifest_path);
const ReaderPluginCatalog& reader_plugin_catalog();
const ReaderPluginCatalog* cached_reader_plugin_catalog() noexcept;
std::string reader_plugin_profile();
std::shared_ptr<ImageServer> open_plugin_image_server(const std::string& path,
    std::shared_ptr<const AdmittedReaderPlugin>,const std::atomic<bool>* cancelled=nullptr,
    const std::string& options="");
// A composed native workflow reserves this entire allowance in addition to its
// own buffers. The original open path keeps its existing 256 MiB allowance.
std::shared_ptr<ImageServer> open_bounded_plugin_image_server(const std::string& path,
    std::shared_ptr<const AdmittedReaderPlugin>,std::size_t work_bytes,
    const std::atomic<bool>* cancelled=nullptr,const std::string& options="");
bool is_reader_plugin_server(const ImageServer&) noexcept;
std::shared_ptr<const ReaderPluginLink> capture_native_reader_plugin_link(const ImageServer&);
std::shared_ptr<const DirectoryStoreSnapshot> capture_native_reader_plugin_store(const ImageServer&);
// Owned capture, made once after successful open/metadata/source validation.
// The bridge leaves JSON/schema interpretation to shared native label services.
std::shared_ptr<const std::string> capture_native_reader_plugin_label_description(const ImageServer&);
enum class ReaderPluginOpenPolicy {Exact,Review};
std::shared_ptr<ImageServer> reopen_reader_plugin(const std::string& path,const ReaderPluginLink&,
    ReaderPluginOpenPolicy,const std::atomic<bool>* cancelled=nullptr);
bool reader_plugin_cached_available(const ReaderPluginLink&) noexcept;
}
