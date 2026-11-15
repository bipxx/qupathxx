#pragma once
#include "qupathxx/format_import.hpp"
#include "expression_matrix_internal.hpp"
#ifdef QUPATHXX_HAS_PROJECT
namespace qpx::detail::matrix {
constexpr const char* external_profile="external-dense-counts/1";
std::string external_positions_json(const ExternalMatrixReceipt&);
ExternalMatrixReceipt external_receipt_value(const std::string& reader,const std::string& positions);
std::string external_recipe_identity(const ExternalMatrixReceipt&,const CoordinateFrame&);
void store_external_meta(Database&,const ExpressionMatrixMetadata&,const ExternalMatrixReceipt&);
std::optional<ExternalMatrixReceipt> load_external_receipt(Database&);
}
#endif
