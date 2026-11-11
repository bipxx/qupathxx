#pragma once
// Private, loader-independent saved-source contract. Legacy public source/view
// layouts and encodings remain unchanged.
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include <string>
#include <vector>
namespace qpx {class Sha256;}
namespace qpx::detail {
struct ReaderPluginInfo;
struct ReaderPluginDependency {std::string name,sha256;};
struct ReaderPluginImplementation {
    std::string provider_id,version,contract,profile,module_sha256,options;
    std::uint32_t abi=1;
    std::uint64_t capabilities=0;
    std::vector<ReaderPluginDependency> dependencies;
    void validate() const;
    std::size_t payload_bytes() const;
    std::string sha256() const;
};
struct ReaderPluginLink {
    ReaderPluginImplementation implementation;
    ImageMetadata metadata;
    std::shared_ptr<const RawImageMetadata> raw;
    ReaderSource observed;
    void validate() const;
    std::size_t payload_bytes() const;
};
inline constexpr std::size_t reader_plugin_link_limit=2u*1024*1024;
bool valid_reader_plugin_id(const std::string&) noexcept;
std::string normalize_reader_plugin_options(const std::string&);
ReaderPluginImplementation reader_plugin_implementation(const ReaderPluginInfo&,const std::string& options);
std::string reader_plugin_binding(const ReaderPluginImplementation&);
bool is_reader_plugin_binding(const std::string&) noexcept;
// Legacy sources keep their existing replay rules. Plugin records cannot adopt
// another implementation solely because a rerun produced identical pixels.
void require_same_reader_plugin_binding(const ReaderSource* archived,const ReaderSource* current);
std::string encode_reader_plugin_link(const ReaderPluginLink&);
std::shared_ptr<const ReaderPluginLink> decode_reader_plugin_link(const std::string&);
bool same_reader_plugin_metadata(const ReaderPluginLink&,const ReaderPluginLink&);
// No-op for legacy sources; identical insertion in retained RGB and replay hashes.
void hash_reader_plugin_binding(Sha256&,const ImageServer&);
}
