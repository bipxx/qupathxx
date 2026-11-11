#pragma once
// Private typed matrix-family dispatch. Existing native readers/importers own
// validation, domains, hashing, budgets and publication.
#include "qupathxx/h5ad.hpp"
#include <variant>

namespace qpx::detail {
enum class MatrixFormat { Qmx, MatrixMarket, H5ad };
struct MatrixFormatFeatures {bool storage=false,h5ad=false;};
struct MatrixFormatDescriptor {
    MatrixFormat format=MatrixFormat::Qmx;
    std::string provider_id,display_name;
    bool available=false;
    std::string unavailable_reason;
    bool can_open=false,can_import=false;
};
struct MatrixFormatSelection {MatrixFormat format=MatrixFormat::Qmx;};
struct MatrixFormatOpenRequest {
    std::string path,expected_file_sha256;
    std::size_t work_bytes=64u*1024*1024;
    std::uint64_t file_bytes=2ull*1024*1024*1024;
};
struct MatrixMarketImportRequest {
    std::string matrix,features,observations,store;
    ExpressionMatrixImportOptions options;
};
struct H5adMatrixImportRequest {
    std::string input,store;
    H5adOptions options;
    ExpressionMatrixImportOptions limits;
};
using MatrixFormatImportRequest=std::variant<MatrixMarketImportRequest,H5adMatrixImportRequest>;
MatrixFormatFeatures matrix_format_features();
bool known_matrix_format(MatrixFormat) noexcept;
std::vector<MatrixFormatDescriptor> matrix_format_catalog(MatrixFormatFeatures=matrix_format_features());
// Selection is explicit and pure: it never opens a source or infers a domain.
MatrixFormatSelection select_matrix_format(MatrixFormat);
std::shared_ptr<const ExpressionMatrixSource> open_matrix_format(MatrixFormatSelection,
    const MatrixFormatOpenRequest&,const std::atomic<bool>* cancelled=nullptr);
ExpressionMatrixImportResult import_matrix_format(MatrixFormatSelection,
    const MatrixFormatImportRequest&,const std::atomic<bool>* cancelled=nullptr);
}
