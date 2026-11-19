#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/expression_matrix.hpp"
#include "qupathxx/format_plugins.hpp"
#include "qupathxx/directory_store.hpp"
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
// Separate types preserve every existing receipt/options/result layout.
struct ExternalMatrixStoreReceipt {
    ExternalMatrixReceipt matrix;
    std::string primary_key,observed_sha256,content_sha256;
    std::uint64_t files=0,bytes=0;
};
struct MatrixStorePluginImportOptions {
    MatrixPluginImportOptions matrix;
    DirectoryStoreLimits inventory;
    DirectoryStoreFingerprintLimits content;
    std::string primary_key;
    void validate() const;
};
// Native matrix target IDs/display names use the existing <=256-byte UTF-8
// catalogue contract. Observation names must be empty; they are not discarded.
ExpressionMatrixImportResult import_expression_matrix_plugin(const std::string& provider_id,
    const std::string& input,const std::string& positions,const std::string& store,
    const MatrixPluginImportOptions&,const std::optional<std::string>& options=std::nullopt,
    const std::atomic<bool>* cancelled=nullptr);
// Use the saved image's reader and frame from a read-only project template.
// The frame plane comes from options.frame.plane; the saved image supplies the other frame facts.
// Index a new QMX, which can subsequently be attached without the matrix input.
ExpressionMatrixImportResult index_expression_matrix_file_for_project(const std::string& provider_id,
    const std::string& input,const std::string& positions,const std::string& new_qmx,
    const std::string& input_project,std::uint64_t image,MatrixPluginImportOptions options={},
    const std::optional<std::string>& plugin_options=std::nullopt,const std::atomic<bool>* cancelled=nullptr);
// ABI3 only: one explicitly selected local store, complete native positions and
// protected new-only QMX. Inventory observations and content hashes are separate.
ExpressionMatrixImportResult import_expression_matrix_store_plugin(const std::string& provider_id,
    const std::string& directory,const std::string& positions,const std::string& store,
    const MatrixStorePluginImportOptions&,const std::optional<std::string>& options=std::nullopt,
    const std::atomic<bool>* cancelled=nullptr);
struct MatrixStoreProjectTarget {
    std::string input_project, new_project;
    std::uint64_t image=0;
};
// Index against the saved image, independently of any future Save As target.
// Use matrix.frame.plane for the observation plane in both project-store overloads.
ExpressionMatrixImportResult index_expression_matrix_store_for_project(const std::string& provider_id,
    const std::string& directory,const std::string& positions,const std::string& new_qmx,
    const std::string& input_project,std::uint64_t image,MatrixStorePluginImportOptions options,
    const std::optional<std::string>& plugin_options=std::nullopt,const std::atomic<bool>* cancelled=nullptr);
// Derive the image frame from a read-only template and guard both destinations
// before indexing. The result can be attached separately or retried offline.
ExpressionMatrixImportResult index_expression_matrix_store_for_project(const std::string& provider_id,
    const std::string& directory, const std::string& positions, const std::string& new_qmx,
    const MatrixStoreProjectTarget&, MatrixStorePluginImportOptions,
    const std::optional<std::string>& options=std::nullopt, const std::atomic<bool>* cancelled=nullptr);
// Preflight both new outputs and the managed QMX folders used by project Save As.
void preflight_matrix_store_project_outputs(const std::string& directory,const std::string& positions,
    const std::string& input_project,const std::string& new_qmx,const std::string& new_project);
// Additional schema4 provenance without enlarging legacy matrix metadata.
// Legacy schemas return nullopt. The verified source remains independently usable
// after the original TSV files or provider module become unavailable.
std::optional<ExternalMatrixReceipt> external_expression_matrix_receipt(
    const ExpressionMatrixSource&,std::size_t work_bytes=64u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);
std::optional<ExternalMatrixStoreReceipt> external_expression_matrix_store_receipt(
    const ExpressionMatrixSource&,std::size_t work_bytes=64u*1024*1024,
    const std::atomic<bool>* cancelled=nullptr);

enum class AnnotationLinearAxis : std::uint32_t { Unspecified=0,Channel=1,Z=2,Time=3 };
struct AnnotationPluginImportOptions {
    // Empty selects the derived native image-pixel frame. A supplied ID must
    // match it exactly; no origin, affine or unit conversion is inferred.
    std::string frame_id;
    std::map<std::string,std::string> class_map;
    std::uint64_t file_bytes=32u*1024*1024,max_points=100000;
    // max_points retains its source-compatible name and limits object rows for
    // both ABIs. ABI2 also bounds the exact total QRO geometry transport bytes.
    std::uint64_t max_geometry_bytes=32u*1024*1024;
    std::size_t work_bytes=64u*1024*1024,plugin_work_bytes=16u*1024*1024;
    // ABI3 source facts and plane interpretation. Missing Z/T can resolve only
    // to a unique target plane or these explicit choices. Linear positions have
    // no implicit axis. ABI1/2 cannot consume these interpretation choices.
    std::uint64_t max_source_bytes=32u*1024*1024;
    std::optional<std::uint32_t> unpositioned_z,unpositioned_time;
    AnnotationLinearAxis legacy_position_axis=AnnotationLinearAxis::Unspecified;
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
// Import stored-coordinate ROIs from the exact XML associated with the selected
// native OME-TIFF series. The optional OME ROI provider must be available.
AnnotationPluginPreview preview_ome_tiff_annotations(const Document&,const std::string& xml,
    const AnnotationPluginImportOptions& = {},const std::atomic<bool>* cancelled=nullptr);
// Owner-thread transaction. Failed/cancelled/stale applications leave both the
// document and preview unconsumed. One successful append creates one undo entry.
AnnotationPluginImportResult apply_annotation_plugin(Document&,AnnotationPluginPreview&,
    const std::atomic<bool>* cancelled=nullptr);
} // namespace qpx
