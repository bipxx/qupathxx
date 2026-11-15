#pragma once
#include "qupathxx/format_plugin.h"
#include "qupathxx/format_plugins.hpp"
#include <memory>
#include <mutex>
namespace qpx::detail {
class NativePluginModule;
struct AdmittedFormatPlugin {
    FormatPluginInfo info;
    qpx_matrix_format_api_v1 matrix{};
    qpx_annotation_format_api_v1 annotation{};
    std::shared_ptr<NativePluginModule> module;
    mutable std::mutex calls;
};
struct FormatPluginCatalog {
    std::vector<std::shared_ptr<AdmittedFormatPlugin>> providers;
    std::vector<std::string> diagnostics;
};
const FormatPluginCatalog& format_plugin_catalog();
std::string format_plugin_profile();
std::string format_plugin_implementation_identity(const FormatPluginInfo&);
}
