// SPDX-License-Identifier: AGPL-3.0-only
// Public SDK control entry point. Native replay, identity, validation and export
// services own the workflow; this consumer does not parse a recipe or NGFF data.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/reader_plugins.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int wmain(int argc, wchar_t** args) {
    try {
        if (argc != 7)
            throw std::invalid_argument("Usage: MANIFEST PROJECT IMAGE RECORD NEW_CSV NEW_BUNDLE");
        const auto absolute = [](const wchar_t* value) {
            return std::filesystem::absolute(std::filesystem::path(value)).lexically_normal().u8string();
        };
        const auto manifest = absolute(args[1]), project_path = absolute(args[2]);
        const auto image_text = std::filesystem::path(args[3]).u8string();
        std::uint64_t image = 0;
        const auto parsed = std::from_chars(image_text.data(), image_text.data() + image_text.size(), image);
        if (!image || parsed.ec != std::errc{} || parsed.ptr != image_text.data() + image_text.size() ||
            std::to_string(image) != image_text)
            throw std::invalid_argument("Invalid native image ID");
        const auto record = std::filesystem::path(args[4]).u8string();
        const auto csv = absolute(args[5]), bundle = absolute(args[6]);
        qpx::configure_reader_plugins(manifest);
        const auto project = qpx::Project::open_without_journal_recovery(project_path);
        const auto document = project->document(image);
        const auto request = qpx::replay_label_analysis(*document, record);
        if (request.method() != qpx::LabelAnalysisMethod::NgffMask)
            throw std::invalid_argument("Selected record is not a native NGFF label-mask workflow");
        const auto run = qpx::run_label_analysis(request);
        if (!run || run->sha256() != record)
            throw std::runtime_error("Native replay result differs from the exact saved record ID");
        const auto* codes = run->exact_source_labels();
        const auto* description = run->label_source_description();
        if (!codes || !description || codes->size() != run->raster().regions().size())
            throw std::runtime_error("Native replay omitted owned source-code/metadata results");
        // Each output is native CreateNew. The other requested output is an
        // additional protected path, along with both named input files.
        const auto csv_bytes = qpx::save_label_analysis_csv(*run, csv, 256u * 1024 * 1024,
            {manifest, project_path, bundle});
        qpx::save_label_analysis_raster(*run, bundle, {manifest, project_path, csv});
        std::cout << "image " << image << '\n'
                  << "record_sha256 " << run->sha256() << '\n'
                  << "input_sha256 " << run->input_sha256() << '\n'
                  << "raster_sha256 " << run->raster().sha256() << '\n'
                  << "description_sha256 " << description->sha256 << '\n'
                  << "source_count " << codes->size() << '\n';
        for (std::size_t i = 0; i < codes->size(); ++i)
            std::cout << "dense_label " << (i + 1) << " source_code " << run->source_code(i).decimal() << '\n';
        std::cout << "source_metadata_bytes " << description->json.size() << '\n'
                  << description->json << '\n'
                  << "csv_bytes " << csv_bytes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
