#include "matrix_format_registry.hpp"
#include <array>
#include <stdexcept>
#include <utility>

namespace qpx::detail {
namespace {
using OpenFactory=std::shared_ptr<const ExpressionMatrixSource>(*)(const MatrixFormatOpenRequest&,const std::atomic<bool>*);
using ImportFactory=ExpressionMatrixImportResult(*)(const MatrixFormatImportRequest&,const std::atomic<bool>*);
std::shared_ptr<const ExpressionMatrixSource> qmx(const MatrixFormatOpenRequest& request,const std::atomic<bool>* cancelled) {
    return open_expression_matrix(request.path,request.expected_file_sha256,request.work_bytes,request.file_bytes,cancelled);
}
ExpressionMatrixImportResult matrix_market(const MatrixFormatImportRequest& request,const std::atomic<bool>* cancelled) {
    const auto* selected=std::get_if<MatrixMarketImportRequest>(&request);
    if(!selected)throw std::invalid_argument("Matrix Market format requires a Matrix Market import request");
    return import_expression_matrix(selected->matrix,selected->features,selected->observations,selected->store,selected->options,cancelled);
}
ExpressionMatrixImportResult h5ad(const MatrixFormatImportRequest& request,const std::atomic<bool>* cancelled) {
    const auto* selected=std::get_if<H5adMatrixImportRequest>(&request);
    if(!selected)throw std::invalid_argument("H5AD format requires an H5AD import request");
    return import_h5ad_matrix(selected->input,selected->store,selected->options,selected->limits,cancelled);
}
struct Builtin {
    MatrixFormat format;const char* id;const char* name;
    OpenFactory open;ImportFactory importer;
};
const std::array<Builtin,3>& builtins() {
    static const std::array<Builtin,3> entries{{
        {MatrixFormat::Qmx,"qmx","Indexed expression sidecar (QMX)",qmx,nullptr},
        {MatrixFormat::MatrixMarket,"matrix-market","Matrix Market with explicit QPX/10x catalogues",nullptr,matrix_market},
        {MatrixFormat::H5ad,"h5ad","AnnData H5AD with explicit domain/frame",nullptr,h5ad}
    }};return entries;
}
const Builtin& builtin(MatrixFormat format) {
    for(const auto& entry:builtins())if(entry.format==format)return entry;
    throw std::invalid_argument("Unknown matrix format");
}
}
MatrixFormatFeatures matrix_format_features() {return {has_expression_matrix_storage(),has_h5ad()};}
bool known_matrix_format(MatrixFormat format) noexcept {
    for(const auto& entry:builtins())if(entry.format==format)return true;return false;
}
std::vector<MatrixFormatDescriptor> matrix_format_catalog(MatrixFormatFeatures features) {
    std::vector<MatrixFormatDescriptor> result;result.reserve(builtins().size());
    for(const auto& entry:builtins()) {
        const auto available=features.storage && (entry.format!=MatrixFormat::H5ad || features.h5ad);
        std::string reason;
        if(!features.storage)reason=entry.format==MatrixFormat::H5ad?"Indexed H5AD import requires native project/SQLite support":"Expression matrix storage requires native project/SQLite support";
        else if(!available)reason="H5AD requires QUPATHXX_BUILD_H5AD";
        result.push_back({entry.format,entry.id,entry.name,available,std::move(reason),bool(entry.open),bool(entry.importer)});
    }
    return result;
}
MatrixFormatSelection select_matrix_format(MatrixFormat format) {(void)builtin(format);return {format};}
std::shared_ptr<const ExpressionMatrixSource> open_matrix_format(MatrixFormatSelection selection,
    const MatrixFormatOpenRequest& request,const std::atomic<bool>* cancelled) {
    const auto& entry=builtin(selection.format);if(!entry.open)throw std::invalid_argument("Matrix format does not support opening an indexed sidecar");
    return entry.open(request,cancelled);
}
ExpressionMatrixImportResult import_matrix_format(MatrixFormatSelection selection,
    const MatrixFormatImportRequest& request,const std::atomic<bool>* cancelled) {
    const auto& entry=builtin(selection.format);if(!entry.importer)throw std::invalid_argument("Matrix format does not support importing a source");
    return entry.importer(request,cancelled);
}
}
