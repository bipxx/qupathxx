// SPDX-License-Identifier: AGPL-3.0-only
// Public-only source-matched native SDK consumer. Class labels/keys below are
// explicit example values; all interpretation/publication stays in native APIs.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "qupathxx/format_import.hpp"
#include "qupathxx/project.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <windows.h>

int wmain(int argc, wchar_t **args) {
  try {
    if (argc != 6)
      throw std::invalid_argument(
          "Usage: MANIFEST INPUT_PROJECT IMAGE_ID SHAPES.tsv NEW_PROJECT");
    const auto text = [](const wchar_t *value) {
      return std::filesystem::path(value).u8string();
    };
    const auto id_text = text(args[3]);
    std::uint64_t id = 0;
    const auto parsed =
        std::from_chars(id_text.data(), id_text.data() + id_text.size(), id);
    if (!id || parsed.ec != std::errc{} ||
        parsed.ptr != id_text.data() + id_text.size())
      throw std::invalid_argument("Invalid native image ID");
    qpx::configure_format_plugins(text(args[1]));
    auto project = qpx::Project::open_without_journal_recovery(text(args[2]));
    auto document = project->document(id);
    qpx::AnnotationPluginImportOptions options;
    options.class_map = {{"T", "Native Tumor"}, {"S", "Native Stroma"}};
    auto preview = qpx::preview_annotation_plugin(
        *document, "org.qupathxx.geometry-tsv", text(args[4]), options);
    const auto result = qpx::apply_annotation_plugin(*document, preview);
    project->save_new(text(args[5]));
    std::cout << result.preview_sha256 << '\n'
              << qpx::format_plugin_receipt_json(result.receipt) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 2;
  }
}
