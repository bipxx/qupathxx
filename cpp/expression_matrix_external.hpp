#pragma once
#include "qupathxx/format_import.hpp"
#include "expression_matrix_internal.hpp"
#ifdef QUPATHXX_HAS_PROJECT
namespace qpx::detail::matrix {
constexpr const char* external_profile="external-dense-counts/1";
constexpr const char* typed_external_profile="external-dense-typed/1";
constexpr const char* sparse_file_external_profile="external-sparse-file/1";
constexpr const char* store_external_profile="external-dense-store/1";
constexpr const char* sparse_store_external_profile="external-sparse-store/1";
// Separate names preserve the exact frozen inline external()/source_hash().
bool typed_external(const ExpressionMatrixMetadata&) noexcept;
std::string typed_external_source_hash(const ExpressionMatrixMetadata&);
std::string typed_external_recipe_identity(const ExternalMatrixReceipt&,const CoordinateFrame&,
    ExpressionValueDomain,const std::string& value_domain);
bool store_external(const ExpressionMatrixMetadata&) noexcept;
std::string store_external_source_hash(const ExpressionMatrixMetadata&);
std::string store_external_recipe_identity(const ExternalMatrixStoreReceipt&,const CoordinateFrame&,
    ExpressionValueDomain,const std::string& value_domain);
std::string external_store_json(const ExternalMatrixStoreReceipt&);
ExternalMatrixStoreReceipt external_store_receipt_value(const std::string& reader,
    const std::string& positions,const std::string& store);
void write_external_store_metadata(Database&,const ExpressionMatrixMetadata&,const ExternalMatrixStoreReceipt&);
std::optional<ExternalMatrixStoreReceipt> load_external_store_receipt(Database&);
std::string external_positions_json(const ExternalMatrixReceipt&);
ExternalMatrixReceipt external_receipt_value(const std::string& reader,const std::string& positions);
std::string external_recipe_identity(const ExternalMatrixReceipt&,const CoordinateFrame&);
void store_external_meta(Database&,const ExpressionMatrixMetadata&,const ExternalMatrixReceipt&);
std::optional<ExternalMatrixReceipt> load_external_receipt(Database&);
}
#endif
