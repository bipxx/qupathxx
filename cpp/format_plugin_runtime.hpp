#pragma once
#include "qupathxx/format_plugins.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace qpx::detail {
struct PluginAxisRow {std::uint64_t index=0;std::string id,name;};
struct PluginMatrixMetadata {std::uint64_t features=0,observations=0;};
struct PluginPoint {
    double x=0,y=0;
    std::int32_t c=-1,z=-1,t=-1;
    std::string source_id,class_label;
};
struct PluginAnnotationMetadata {std::uint64_t points=0;};

class MatrixPluginSession {
public:
    static std::unique_ptr<MatrixPluginSession> open(const std::string& provider_id,
        const std::string& path,const std::optional<std::string>& options,
        std::uint64_t work_bytes,const std::atomic<bool>* cancelled=nullptr);
    ~MatrixPluginSession();
    MatrixPluginSession(const MatrixPluginSession&)=delete;
    MatrixPluginSession& operator=(const MatrixPluginSession&)=delete;
    const PluginMatrixMetadata& metadata() const noexcept;
    const FormatPluginReceipt& receipt() const noexcept;
    std::uint64_t retained_work_bytes() const noexcept;
    std::vector<PluginAxisRow> features(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<PluginAxisRow> observations(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    std::vector<std::uint64_t> values(std::uint64_t first,std::uint64_t count,
        const std::atomic<bool>* cancelled=nullptr);
    void verify_source() const;
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
    void verify_source() const;
private:
    struct Impl;explicit AnnotationPluginSession(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace qpx::detail
