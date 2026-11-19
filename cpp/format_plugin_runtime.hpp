#pragma once
#include "qupathxx/format_plugins.hpp"
#include "qupathxx/format_plugin.h"
#include "qupathxx/expression_dataset.hpp"
#include "qupathxx/geometry.hpp"
#include "qupathxx/directory_store.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <map>
#include <string>
#include <vector>

namespace qpx::detail {
struct PluginAxisRow {std::uint64_t index=0;std::string id,name;};
struct PluginMatrixMetadata {
    std::uint64_t features=0,observations=0;
    ExpressionValueDomain domain=ExpressionValueDomain::Counts;
    std::string value_domain="raw-counts";
    std::optional<std::uint64_t> stored_entries;
};
struct PluginMatrixStoreSource {
    std::string primary_key,observed_sha256,content_sha256;
    std::uint64_t files=0,bytes=0;
};
struct PluginPoint {
    double x=0,y=0;
    std::int32_t c=-1,z=-1,t=-1;
    std::string source_id,class_label;
};
struct PluginAnnotationMetadata {
    std::uint64_t points=0,geometry_bytes=0,source_bytes=0,properties=0,geometry_encodings=1;
    std::uint64_t canvas_width=0,canvas_height=0;
};
struct PluginAnnotationObject {
    Roi geometry;
    // ABI1 coordinates stay exact for the existing row identity protocol.
    double x=0,y=0;
    std::int32_t c=-1,z=-1,t=-1;
    std::string source_id,class_label,geometry_sha256;
    std::uint64_t geometry_bytes=0;
    std::string display_name,canonical_geometry_sha256;
    std::map<std::string,std::string> source_properties;
    std::optional<std::uint32_t> color_rgb;
    std::uint32_t geometry_encoding=1,plane_kind=0;
    std::uint64_t linear_position=0,source_bytes=0;
    bool locked=false;
    // Nonzero only for ABI5's full, origin-zero pixel canvas (QBM1).
    std::uint32_t canvas_width=0,canvas_height=0;
};

class MatrixPluginSession {
public:
    static std::unique_ptr<MatrixPluginSession> open(const std::string& provider_id,
        const std::string& path,const std::optional<std::string>& options,
        std::uint64_t work_bytes,const std::atomic<bool>* cancelled=nullptr);
    // ABI2 streams an independently bounded file. ABI1 keeps file<=work.
    static std::unique_ptr<MatrixPluginSession> open_with_file_limit(const std::string& provider_id,
        const std::string& path,const std::optional<std::string>& options,
        std::uint64_t work_bytes,std::uint64_t file_bytes,
        const std::atomic<bool>* cancelled=nullptr);
    static std::unique_ptr<MatrixPluginSession> open_store(const std::string& provider_id,
        const std::string& directory,const std::string& primary_key,
        const std::optional<std::string>& options,std::uint64_t plugin_work_bytes,
        const DirectoryStoreLimits&,const DirectoryStoreFingerprintLimits&,
        std::uint64_t host_work_bytes,const std::atomic<bool>* cancelled=nullptr);
    ~MatrixPluginSession();
    MatrixPluginSession(const MatrixPluginSession&)=delete;
    MatrixPluginSession& operator=(const MatrixPluginSession&)=delete;
    const PluginMatrixMetadata& metadata() const noexcept;
    const FormatPluginReceipt& receipt() const noexcept;
    const std::optional<PluginMatrixStoreSource>& store_source() const noexcept;
    std::uint64_t retained_work_bytes() const noexcept;
    std::vector<PluginAxisRow> features(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<PluginAxisRow> observations(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<std::uint64_t> values(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<double> continuous_values(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<qpx_matrix_sparse_entry_v1> sparse_values(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    void verify_source() const;
    void verify_source(const std::atomic<bool>* cancelled) const;
private:
    struct Impl;explicit MatrixPluginSession(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
class AnnotationPluginSession {
public:
    static std::unique_ptr<AnnotationPluginSession> open(const std::string& provider_id,
        const std::string& path,const std::optional<std::string>& options,
        std::uint64_t work_bytes,const std::atomic<bool>* cancelled=nullptr);
    ~AnnotationPluginSession();
    AnnotationPluginSession(const AnnotationPluginSession&)=delete;
    AnnotationPluginSession& operator=(const AnnotationPluginSession&)=delete;
    const PluginAnnotationMetadata& metadata() const noexcept;
    const FormatPluginReceipt& receipt() const noexcept;
    std::uint64_t retained_work_bytes() const noexcept;
    std::vector<PluginPoint> points(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<PluginAnnotationObject> objects(std::uint64_t first,std::uint64_t count,
        std::size_t available_work,const std::atomic<bool>* cancelled=nullptr);
    void verify_source() const;
private:
    struct Impl;explicit AnnotationPluginSession(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace qpx::detail
