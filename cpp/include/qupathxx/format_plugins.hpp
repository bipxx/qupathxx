#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace qpx {
enum class FormatPluginDomain : std::uint32_t { Matrix=1, Annotation=2 };
struct FormatPluginDependency {std::string name,sha256;};
struct FormatPluginInfo {
    FormatPluginDomain domain=FormatPluginDomain::Matrix;
    std::string provider_id,format_id,display_name,version,contract,profile;
    std::string implementation_sha256,module_sha256,default_options;
    std::vector<FormatPluginDependency> dependencies;
    std::uint32_t abi=1;
};
struct FormatPluginReceipt {
    FormatPluginInfo provider;
    std::string options,source_sha256;
};
// Configure once before the first family catalog/provider query. No hot reload.
void configure_format_plugins(const std::string& manifest_path);
std::vector<FormatPluginInfo> format_plugin_infos();
std::vector<std::string> format_plugin_diagnostics();
std::string format_plugin_receipt_json(const FormatPluginReceipt&);
FormatPluginReceipt parse_format_plugin_receipt(const std::string&);
std::string format_plugin_receipt_identity(const FormatPluginReceipt&);
} // namespace qpx
