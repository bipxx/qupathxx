// SPDX-License-Identifier: AGPL-3.0-only
// Public SDK control entry point: native services own AnnData Zarr interpretation,
// values, joins, QMX identities, association copies and project publication.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::filesystem::path absolute(const wchar_t* value, const char* name) {
    const std::filesystem::path path(value);
    if (!path.is_absolute())
        throw std::invalid_argument(std::string(name) + " requires an absolute path");
    return path.lexically_normal();
}
std::uint64_t image_id(const wchar_t* value) {
    const auto text = std::filesystem::path(value).u8string();
    std::uint64_t image = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), image);
    if (!image || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        std::to_string(image) != text)
        throw std::invalid_argument("Invalid native image ID");
    return image;
}
std::string expected_digest(const wchar_t* value) {
    const auto text = std::filesystem::path(value).u8string();
    if (text.size() != 64 || text.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("Expected identity must be a lowercase SHA-256 digest");
    return text;
}
qpx::ExternalMatrixStoreReceipt native_receipt(const qpx::ExpressionMatrixSource& source) {
    constexpr const char* provider_id = "org.qupathxx.anndata-zarr";
    const auto receipt = qpx::external_expression_matrix_store_receipt(source);
    const auto& metadata = source.metadata();
    if (!receipt || (receipt->matrix.reader.provider.abi != 3 && receipt->matrix.reader.provider.abi != 4) ||
        receipt->matrix.reader.provider.provider_id != provider_id ||
        receipt->matrix.reader.provider.contract != "anndata-zarr2-typed/1" ||
        (metadata.input_profile != "external-dense-store/1" && metadata.input_profile != "external-sparse-store/1") ||
        !metadata.spatial_frame ||
        (metadata.domain != qpx::ExpressionValueDomain::Counts &&
         metadata.domain != qpx::ExpressionValueDomain::Continuous))
        throw std::runtime_error("Native indexed AnnData Zarr receipt, frame or typed domain is absent or unsupported");
    return *receipt;
}
void same_receipt(const qpx::ExternalMatrixStoreReceipt& expected,
                  const qpx::ExternalMatrixStoreReceipt& actual) {
    if (qpx::format_plugin_receipt_json(expected.matrix.reader) !=
            qpx::format_plugin_receipt_json(actual.matrix.reader) ||
        expected.matrix.positions_sha256 != actual.matrix.positions_sha256 ||
        expected.matrix.position_columns != actual.matrix.position_columns ||
        expected.primary_key != actual.primary_key ||
        expected.observed_sha256 != actual.observed_sha256 ||
        expected.content_sha256 != actual.content_sha256 ||
        expected.files != actual.files || expected.bytes != actual.bytes)
        throw qpx::WorkflowInputChanged("Saved owned AnnData Zarr receipt differs from the indexed source");
}
void same_metadata(const qpx::ExpressionMatrixMetadata& expected,
                   const qpx::ExpressionMatrixMetadata& actual) {
    if (expected.features != actual.features || expected.observations != actual.observations ||
        expected.entries != actual.entries || expected.domain != actual.domain ||
        expected.value_domain != actual.value_domain || expected.input_profile != actual.input_profile ||
        expected.source_sha256 != actual.source_sha256 || expected.input_sha256 != actual.input_sha256 ||
        expected.spatial_sha256 != actual.spatial_sha256 || expected.input_matrix != actual.input_matrix ||
        expected.input_spatial != actual.input_spatial || !expected.spatial_frame || !actual.spatial_frame ||
        expected.spatial_frame->sha256() != actual.spatial_frame->sha256())
        throw qpx::WorkflowInputChanged("Saved owned AnnData Zarr metadata differs from the indexed source");
}
void print_result(const qpx::Project& project, std::uint64_t image,
                  const std::string& association_id, const qpx::ExpressionMatrixSource& source,
                  const qpx::ExternalMatrixStoreReceipt& receipt) {
    const auto& metadata = source.metadata();
    std::cout << "image " << image << '\n'
              << "features " << metadata.features << '\n'
              << "observations " << metadata.observations << '\n'
              << "entries " << metadata.entries << '\n'
              << "value_kind " << (metadata.domain == qpx::ExpressionValueDomain::Counts ? "counts" : "continuous") << '\n'
              << "value_domain " << metadata.value_domain << '\n'
              << "input_profile " << metadata.input_profile << '\n'
              << "frame_sha256 " << metadata.spatial_frame->sha256() << '\n'
              << "file_sha256 " << source.file_sha256() << '\n'
              << "source_sha256 " << metadata.source_sha256 << '\n'
              << "receipt_sha256 " << qpx::format_plugin_receipt_identity(receipt.matrix.reader) << '\n'
              << "receipt " << qpx::format_plugin_receipt_json(receipt.matrix.reader) << '\n'
              << "positions_sha256 " << receipt.matrix.positions_sha256 << '\n'
              << "source_scope directory_store" << '\n'
              << "primary_key " << receipt.primary_key << '\n'
              << "store_observed_sha256 " << receipt.observed_sha256 << '\n'
              << "store_content_sha256 " << receipt.content_sha256 << '\n'
              << "store_files " << receipt.files << '\n'
              << "store_bytes " << receipt.bytes << '\n'
              << "association_id " << association_id << '\n'
              << "owned_file_sha256 " << source.file_sha256() << '\n'
              << "owned_source_sha256 " << metadata.source_sha256 << '\n'
              << "disk_reopened 1\n"
              << "project " << project.path() << '\n';
}
int inspect_owned(int argc, wchar_t** args) {
    if (argc != 9)
        throw std::invalid_argument(
            "Usage: --inspect ABS_PROJECT IMAGE_ID ASSOCIATION_ID FILE_SHA256 SOURCE_SHA256 READER_RECEIPT_SHA256 FRAME_SHA256");
    const auto path = absolute(args[2], "PROJECT");
    const auto image = image_id(args[3]);
    const auto association_id = std::filesystem::path(args[4]).u8string();
    const auto file_sha256 = expected_digest(args[5]);
    const auto source_sha256 = expected_digest(args[6]);
    const auto receipt_sha256 = expected_digest(args[7]);
    const auto frame_sha256 = expected_digest(args[8]);
    // No provider is configured: the saved owned QMX must be independently readable.
    const auto project = qpx::Project::open_without_journal_recovery(path.u8string());
    const auto association = qpx::load_expression_matrix_association(*project, image, association_id);
    const auto source = qpx::reopen_expression_matrix(*project, image, association_id);
    association.require_source(*source);
    const auto receipt = native_receipt(*source);
    if (!association.owned || association.file_sha256 != file_sha256 ||
        association.source_sha256 != source_sha256 || source->file_sha256() != file_sha256 ||
        source->metadata().source_sha256 != source_sha256 ||
        qpx::format_plugin_receipt_identity(receipt.matrix.reader) != receipt_sha256 ||
        source->metadata().spatial_frame->sha256() != frame_sha256)
        throw qpx::WorkflowInputChanged("Fresh process owned project identities differ from the expected native result");
    source->require_current();
    print_result(*project, image, association_id, *source, receipt);
    return 0;
}
} // namespace

int wmain(int argc, wchar_t** args) {
    std::string indexed_output;
    try {
        if (argc > 1 && std::wstring(args[1]) == L"--inspect")
            return inspect_owned(argc, args);
        if (argc != 10)
            throw std::invalid_argument(
                "Usage: ABS_MANIFEST ABS_PROJECT IMAGE_ID ABS_STORE PRIMARY_KEY ABS_POSITIONS ABS_NEW_QMX OPTIONS_JSON ABS_NEW_PROJECT");
        const auto manifest = absolute(args[1], "MANIFEST");
        const auto project_path = absolute(args[2], "PROJECT");
        const auto directory = absolute(args[4], "STORE");
        const auto primary_key = std::filesystem::path(args[5]).u8string();
        const auto positions = absolute(args[6], "POSITIONS");
        const auto output = absolute(args[7], "NEW_QMX");
        const auto new_project = absolute(args[9], "NEW_PROJECT");
        const auto image = image_id(args[3]);
        const auto provider_options = std::filesystem::path(args[8]).u8string();
        constexpr const char* provider_id = "org.qupathxx.anndata-zarr";
        qpx::configure_format_plugins(manifest.u8string());
        bool typed_provider = false;
        for (const auto& provider : qpx::format_plugin_infos())
            if (provider.provider_id == provider_id)
                typed_provider = provider.domain == qpx::FormatPluginDomain::Matrix &&
                    (provider.abi == 3 || provider.abi == 4);
        if (!typed_provider)
            throw std::invalid_argument("AnnData Zarr requires the admitted directory matrix provider org.qupathxx.anndata-zarr");

        qpx::MatrixStorePluginImportOptions options;
        options.primary_key = primary_key;
        const qpx::MatrixStoreProjectTarget target{project_path.u8string(), new_project.u8string(), image};
        const auto imported = qpx::index_expression_matrix_store_for_project(provider_id, directory.u8string(),
            positions.u8string(), output.u8string(), target, options, provider_options);
        indexed_output = imported.path;
        const auto source = qpx::open_expression_matrix(imported.path, imported.file_sha256,
            64u * 1024 * 1024, options.matrix.limits.store_bytes);
        const auto receipt = native_receipt(*source);
        const auto& metadata = source->metadata();
        same_metadata(imported.metadata, metadata);
        const auto attached = qpx::attach_indexed_expression_matrix(target.input_project, image,
            imported.path, imported.file_sha256, target.new_project);
        const auto& association = attached.association;
        const auto& association_id = association.source_sha256;
        const auto reopened = qpx::Project::open_without_journal_recovery(attached.project_path);
        const auto retained = qpx::reopen_expression_matrix(*reopened, image, association_id);
        association.require_source(*retained);
        association.require_source(*source);
        if (!association.owned || retained->file_sha256() != source->file_sha256())
            throw std::runtime_error("Native project association is not owned");
        same_metadata(metadata, retained->metadata());
        const auto retained_receipt = native_receipt(*retained);
        same_receipt(receipt, retained_receipt);
        source->require_current();
        retained->require_current();
        print_result(*reopened, image, association_id, *retained, retained_receipt);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (!indexed_output.empty())
            std::cerr << "The completed indexed matrix remains at " << indexed_output
                      << "; project publication may not have completed.\n";
        return 2;
    }
}
