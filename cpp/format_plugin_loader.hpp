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
    qpx_annotation_format_api_v2 geometry{};
    qpx_annotation_format_api_v3 metadata{};
    std::shared_ptr<NativePluginModule> module;
    mutable std::mutex calls;
    // Append negotiated matrix ABI2 after the frozen ABI1/annotation prefix.
    qpx_matrix_format_api_v2 matrix_typed{};
    // ABI3 independently selects a guarded local directory source.
    qpx_matrix_format_api_v3 matrix_store{};
    qpx_matrix_format_api_v4 matrix_sparse{};
    qpx_matrix_format_api_v5 matrix_sparse_file{};
    qpx_annotation_format_api_v6 annotation_canvas{};
};
inline bool matrix_directory_abi(std::uint32_t abi) noexcept {
    return abi==QPX_MATRIX_ABI_V3 || abi==QPX_MATRIX_ABI_V4;
}
struct FormatPluginCatalog {
    std::vector<std::shared_ptr<AdmittedFormatPlugin>> providers;
    std::vector<std::string> diagnostics;
};
const FormatPluginCatalog& format_plugin_catalog();
std::string format_plugin_profile();
std::string format_plugin_implementation_identity(const FormatPluginInfo&);
// ABI2 interpretation options reject duplicate decoded object keys before the
// existing canonicalizer can erase them. Legacy options retain their contract.
std::string normalize_typed_matrix_options(const std::string&);
}
