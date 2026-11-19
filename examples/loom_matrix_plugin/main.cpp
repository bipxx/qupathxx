// SPDX-License-Identifier: AGPL-3.0-only
// Public SDK control entry point: native services own Loom interpretation,
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

namespace {
std::filesystem::path absolute(const wchar_t* value, const char* name) {
    const std::filesystem::path path(value);
    if (!path.is_absolute())
        throw std::invalid_argument(std::string(name) + " requires an absolute path");
    return path.lexically_normal();
}
void new_output(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory)
        throw std::filesystem::filesystem_error("Inspect new output", path, error);
    if (status.type() != std::filesystem::file_type::not_found)
        throw qpx::WorkflowOutputExists("Output already exists: " + path.u8string());
}
} // namespace

int wmain(int argc, wchar_t** args) {
    std::string indexed_output;
    try {
        if (argc != 9)
            throw std::invalid_argument(
                "Usage: ABS_MANIFEST ABS_PROJECT IMAGE_ID ABS_LOOM ABS_POSITIONS ABS_NEW_QMX OPTIONS_JSON ABS_NEW_PROJECT");
        const auto manifest = absolute(args[1], "MANIFEST");
        const auto project_path = absolute(args[2], "PROJECT");
        const auto loom = absolute(args[4], "LOOM");
        const auto positions = absolute(args[5], "POSITIONS");
        const auto output = absolute(args[6], "NEW_QMX");
        const auto new_project = absolute(args[8], "NEW_PROJECT");
        const auto image_text = std::filesystem::path(args[3]).u8string();
        std::uint64_t image = 0;
        const auto parsed = std::from_chars(image_text.data(), image_text.data() + image_text.size(), image);
        if (!image || parsed.ec != std::errc{} || parsed.ptr != image_text.data() + image_text.size() ||
            std::to_string(image) != image_text)
            throw std::invalid_argument("Invalid native image ID");
        const auto provider_options = std::filesystem::path(args[7]).u8string();
        // Caller preflight prevents a known two-output conflict before indexing.
        // Native CreateNew/source-identity guards remain the publication authority.
        new_output(output);
        new_output(new_project);
        if (CompareStringOrdinal(output.c_str(), -1, new_project.c_str(), -1, TRUE) == CSTR_EQUAL)
            throw std::invalid_argument("NEW_QMX and NEW_PROJECT must be distinct outputs");

        constexpr const char* provider_id = "org.qupathxx.loom";
        qpx::configure_format_plugins(manifest.u8string());
        bool typed_provider = false;
        for (const auto& provider : qpx::format_plugin_infos())
            if (provider.provider_id == provider_id)
                typed_provider = provider.domain == qpx::FormatPluginDomain::Matrix && provider.abi == 2;
        if (!typed_provider)
            throw std::invalid_argument("Loom requires the admitted matrix ABI2 provider org.qupathxx.loom");

        const auto project = qpx::Project::open_without_journal_recovery(project_path.u8string());
        const auto document = project->document(image);
        qpx::MatrixPluginImportOptions options;
        options.frame = qpx::image_coordinate_frame(*document, "qpx.format-import-cli-image/1");
        const auto imported = qpx::import_expression_matrix_plugin(provider_id, loom.u8string(),
            positions.u8string(), output.u8string(), options, provider_options);
        indexed_output = imported.path;
        const auto source = qpx::open_expression_matrix(imported.path, imported.file_sha256,
            64u * 1024 * 1024, options.limits.store_bytes);
        const auto receipt = qpx::external_expression_matrix_receipt(*source);
        if (!receipt || receipt->reader.provider.abi != 2 || receipt->reader.provider.provider_id != provider_id)
            throw std::runtime_error("Native indexed Loom receipt is absent or has another provider/ABI");
        const auto& metadata = source->metadata();
        if (metadata.domain != qpx::ExpressionValueDomain::Counts &&
            metadata.domain != qpx::ExpressionValueDomain::Continuous)
            throw std::runtime_error("Native indexed Loom value domain is unsupported");

        // Existing owned retention copies beside an already-saved INPUT project.
        // Save As then copies the exact managed QMX beside NEW_PROJECT. The input
        // project file is unchanged; native ownership leaves complete copies on
        // failure/Undo rather than deleting shared research files.
        const auto association_id = qpx::keep_expression_matrix(*project, image, source, true);
        project->save_new(new_project.u8string());
        const auto association = qpx::load_expression_matrix_association(*project, image, association_id);
        const auto retained = qpx::reopen_expression_matrix(*project, image, association_id);
        association.require_source(*retained);
        if (!association.owned)
            throw std::runtime_error("Native project association is not owned");

        std::cout << "image " << image << '\n'
                  << "features " << metadata.features << '\n'
                  << "observations " << metadata.observations << '\n'
                  << "entries " << metadata.entries << '\n'
                  << "value_kind " << (metadata.domain == qpx::ExpressionValueDomain::Counts ? "counts" : "continuous") << '\n'
                  << "value_domain " << metadata.value_domain << '\n'
                  << "input_profile " << metadata.input_profile << '\n'
                  << "frame_sha256 " << options.frame.sha256() << '\n'
                  << "file_sha256 " << source->file_sha256() << '\n'
                  << "source_sha256 " << metadata.source_sha256 << '\n'
                  << "receipt_sha256 " << qpx::format_plugin_receipt_identity(receipt->reader) << '\n'
                  << "receipt " << qpx::format_plugin_receipt_json(receipt->reader) << '\n'
                  << "positions_sha256 " << receipt->positions_sha256 << '\n'
                  << "association_id " << association_id << '\n'
                  << "owned_file_sha256 " << retained->file_sha256() << '\n'
                  << "owned_source_sha256 " << retained->metadata().source_sha256 << '\n'
                  << "project " << project->path() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (!indexed_output.empty())
            std::cerr << "The completed indexed matrix remains at " << indexed_output
                      << "; project publication may not have completed.\n";
        return 2;
    }
}
