#include "qupathxx/omero.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_registry.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "export_output.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <random>
namespace qpx {
void save_omero_preview_project(const OmeroPreview& preview, const std::string& output,
                                OmeroLimits limits, const std::atomic<bool>* cancelled) {
    limits.validate();
    workflow_checkpoint(cancelled);
    detail::require_new_export(output);
    // Temporary decoder input is private to this operation. No remote locator is
    // masqueraded as a file-source identity, and saved projects need no network.
    std::random_device random;
    std::string token;
    for (unsigned i = 0; i < 32; ++i)
        token += "0123456789abcdef"[random() & 15];
    const auto temporary =
        (std::filesystem::temp_directory_path() / ("qpx-omero-" + token + ".jpg")).u8string();
    struct Cleanup {
        std::string path;
        bool created = false;
        ~Cleanup() {
            if (created) {
                std::error_code ignored;
                std::filesystem::remove(std::filesystem::u8path(path), ignored);
            }
        }
    } cleanup{temporary};
    save_omero_preview(preview, temporary, cancelled);
    cleanup.created = true;
    detail::ReaderFileGuard observed(temporary);
    FileFingerprintOptions hashing;
    hashing.byte_limit = preview.jpeg().size();
    hashing.buffer_bytes = 65536;
    if (fingerprint_file(temporary, hashing, cancelled)->sha256 != preview.sha256())
        throw WorkflowInputChanged("OMERO preview decoder input bytes changed");
    observed.verify();
    auto source = detail::open_reader(
        temporary, detail::select_reader(temporary, "org.qupathxx.jpeg"), {}, cancelled);
    const auto& m = source->metadata();
    if (m.width < 1 || m.height < 1 ||
        std::uint64_t(m.width) > limits.max_preview_pixels / std::uint64_t(m.height))
        throw std::length_error("Decoded OMERO preview exceeds pixel limit");
    auto pixels = source->read_region({0, 0, 0, m.width, m.height});
    workflow_checkpoint(cancelled);
    observed.verify();
    if (pixels.width() != m.width || pixels.height() != m.height)
        throw std::runtime_error("Preview decoder changed its lattice");
    std::vector<std::uint8_t> rgb(std::size_t(m.width) * std::size_t(m.height) * 3);
    for (std::size_t i = 0; i < rgb.size() / 3; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        if (pixels.bytes()[4 * i + 3] != 255)
            throw std::invalid_argument("OMERO JPEG preview contains nonopaque pixels");
        std::copy_n(pixels.bytes().data() + 4 * i, 3, rgb.data() + 3 * i);
    }
    auto doc = std::make_shared<Document>(Image(m.width, m.height, std::move(rgb)));
    source->close();
    using V = ImageDataValue;
    doc->set_image_data_properties(std::make_shared<const ImageDataProperties>(
        V::Map{{"qpx.omero-preview/1",
                V{V::Map{{"server", V{preview.server()}},
                         {"image", V{std::to_string(preview.image().id)}},
                         {"pixels", V{std::to_string(preview.image().pixels_id)}},
                         {"metadata_sha256", V{preview.image().metadata_sha256}},
                         {"jpeg_sha256", V{preview.sha256()}},
                         {"representation", V{std::string("uncalibrated-server-thumbnail-rgb8")}},
                         {"source_width", V{std::to_string(preview.image().width)}},
                         {"source_height", V{std::to_string(preview.image().height)}}}}}}));
    Project project;
    project.add_document(doc, "OMERO preview (not raw): " + preview.image().name);
    project.save_new(output, 128u * 1024 * 1024, 256u * 1024 * 1024, cancelled);
}
} // namespace qpx
