#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/format_plugins.hpp"
#include "qupathxx/spatial_counts.hpp"
#include <map>
#include <memory>
#include <optional>

namespace qpx {
struct ExternalMatrixReceipt {
    FormatPluginReceipt reader;
    std::string positions_sha256;
    std::array<std::string,3> position_columns;
};
struct MatrixPluginImportOptions {
    CoordinateFrame frame;
    SpatialPositionOptions positions;
    ExpressionMatrixImportOptions limits;
    std::size_t plugin_work_bytes=64u*1024*1024;
    void validate() const;
};
// Native matrix target IDs/display names use the existing <=256-byte UTF-8
// catalogue contract. Observation names must be empty; they are not discarded.
ExpressionMatrixImportResult import_expression_matrix_plugin(const std::string& provider_id,
    const std::string& input,const std::string& positions,const std::string& store,
    const MatrixPluginImportOptions&,const std::optional<std::string>& options=std::nullopt,
    const std::atomic<bool>* cancelled=nullptr);
// Additional schema4 provenance without enlarging legacy matrix metadata.
// Legacy schemas return nullopt. The verified source remains independently usable
// after the original TSV files or provider module become unavailable.
std::optional<ExternalMatrixReceipt> external_expression_matrix_receipt(
    const ExpressionMatrixSource&,std::size_t work_bytes=64u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);

struct AnnotationPluginImportOptions {
    // Empty selects the derived native image-pixel frame. A supplied ID must
    // match it exactly; no origin, affine or unit conversion is inferred.
    std::string frame_id;
    std::map<std::string,std::string> class_map;
    std::uint64_t file_bytes=32u*1024*1024,max_points=100000;
    std::size_t work_bytes=64u*1024*1024,plugin_work_bytes=16u*1024*1024;
    void validate() const;
};
CoordinateFrame annotation_plugin_target_frame(const Document&,
    const std::atomic<bool>* cancelled=nullptr);
struct AnnotationPluginObjectMapping {std::string source_id;std::uint64_t object_id=0;};
struct AnnotationPluginImportResult {
    FormatPluginReceipt receipt;
    std::string preview_sha256;
    std::vector<AnnotationPluginObjectMapping> objects;
};
class AnnotationPluginPreview {
public:
    AnnotationPluginPreview(AnnotationPluginPreview&&) noexcept;
    AnnotationPluginPreview& operator=(AnnotationPluginPreview&&) noexcept;
    ~AnnotationPluginPreview();
    AnnotationPluginPreview(const AnnotationPluginPreview&)=delete;
    AnnotationPluginPreview& operator=(const AnnotationPluginPreview&)=delete;
    const FormatPluginReceipt& receipt() const;
    const CoordinateFrame& frame() const;
    const std::vector<Annotation>& objects() const;
    const std::string& sha256() const;
    // Retained preview payload. The total work limit additionally covers DLL
    // allowance/pages during capture and native transaction/result scratch.
    std::size_t payload_bytes() const;
    bool consumed() const noexcept;
private:
    struct Data;std::unique_ptr<Data> data_;
    explicit AnnotationPluginPreview(std::unique_ptr<Data>);
    friend struct FormatImportKernel;
};
// Source IDs use the same nonempty <=256-byte UTF-8 identifier contract;
// class labels/native class keys retain their separate <=4096-byte bounds.
AnnotationPluginPreview preview_annotation_plugin(const Document&,const std::string& provider_id,
    const std::string& input,const AnnotationPluginImportOptions& = {},
    const std::optional<std::string>& options=std::nullopt,
    const std::atomic<bool>* cancelled=nullptr);
// Owner-thread transaction. Failed/cancelled/stale applications leave both the
// document and preview unconsumed. One successful append creates one undo entry.
AnnotationPluginImportResult apply_annotation_plugin(Document&,AnnotationPluginPreview&,
    const std::atomic<bool>* cancelled=nullptr);
} // namespace qpx
