#include "qupathxx/label_mask.hpp"
#include "qupathxx/label_codes.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "label_analysis_internal.hpp"
#include "reader_plugin.hpp"
#include "reader_registry.hpp"
#include "reader_file_guard.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <filesystem>
namespace qpx {
namespace {
bool digest(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
std::shared_ptr<const detail::AdmittedReaderPlugin> png_provider() {
    for (const auto& plugin : detail::reader_plugin_catalog().readers)
        if (plugin->info.provider_id == "org.qupathxx.png") {
            if (plugin->info.contract != "png-static-native-samples/1" ||
                !(plugin->info.capabilities & QPX_READER_CAP_RAW) ||
                !plugin->info.default_options.empty())
                throw std::invalid_argument(
                    "PNG labels require the native-samples/1 reader profile without options");
            return plugin;
        }
    throw std::invalid_argument("PNG labels require configured org.qupathxx.png; see the native "
                                "reader catalog diagnostics");
}
} // namespace
void ExternalLabelMaskOptions::validate() const {
    detail::bounded_utf8(path, 2048);
    detail::bounded_utf8(producer, 256);
    detail::bounded_utf8(producer_version, 256);
    if (path.empty() || path.find('\0') != std::string::npos ||
        !std::filesystem::u8path(path).is_absolute() || producer.empty() ||
        producer_version.empty() || !digest(model_sha256) || !digest(input_sha256) ||
        !max_regions || max_regions > 1000000 ||
        (format != ExternalLabelMaskFormat::Tiff && format != ExternalLabelMaskFormat::Png &&
         format != ExternalLabelMaskFormat::PngIndexed) ||
        (format == ExternalLabelMaskFormat::Tiff && !reader_sha256.empty()) ||
        (!reader_sha256.empty() && !digest(reader_sha256)))
        throw std::invalid_argument(
            "External masks require an absolute path, explicit TIFF/PNG profile, "
            "producer/version, exact model/input SHA-256 and region limit");
}
void detail::capture_label_mask_reader(ExternalLabelMaskOptions& options) {
    options.validate();
    if (options.format != ExternalLabelMaskFormat::Tiff) {
        const auto plugin = png_provider();
        const auto identity = reader_plugin_implementation(plugin->info, "").sha256();
        if (!options.reader_sha256.empty() && options.reader_sha256 != identity)
            throw WorkflowInputChanged(
                "PNG label decoder differs from the captured implementation");
        options.reader_sha256 = identity;
    }
}
ImportedLabelMask import_label_mask(const ExternalLabelMaskOptions& options, std::int64_t width,
                                    std::int64_t height, std::size_t work,
                                    const std::atomic<bool>* cancelled) {
    options.validate();
    detail::label_work_limit(work);
    workflow_checkpoint(cancelled);
    const auto n = detail::label_pixels(width, height);
    std::size_t base = 65536;
    detail::label_charge(base, n, 16, work);
    detail::label_charge(base, options.max_regions, 4, work);
    detail::ReaderFileGuard file(options.path);
    if (file.version()->bytes > 256u * 1024 * 1024)
        throw WorkflowCaptureLimit("External mask file exceeds256MiB");
    const bool png = options.format != ExternalLabelMaskFormat::Tiff;
    const bool indexed = options.format == ExternalLabelMaskFormat::PngIndexed;
    // The composed workflow reserves the complete plugin allowance and bounded
    // host metadata/link storage, alongside raw/label/mapping buffers above.
    if (png)
        detail::label_charge(base, 4u * 1024 * 1024, 1, work);
    const auto decode_bytes = std::min<std::size_t>(64u * 1024 * 1024, work - base);
    if (decode_bytes < 65536)
        throw WorkflowCaptureLimit("No work remains for external mask decoding");
    auto captured = options;
    detail::capture_label_mask_reader(captured);
    std::shared_ptr<ImageServer> server;
    if (png) {
        server = detail::open_bounded_plugin_image_server(options.path, png_provider(),
                                                          decode_bytes, cancelled);
    } else {
        detail::ReaderOpenOptions opening;
        opening.tiff.cache_bytes = 0;
        opening.tiff.decode_bytes = decode_bytes;
        server = detail::open_reader(options.path, detail::select_reader(options.path, "tiff"),
                                     opening, cancelled);
    }
    const auto metadata = server->raw_metadata(cancelled);
    if (!metadata || metadata->width != width || metadata->height != height ||
        metadata->channels != 1 || metadata->z_planes != 1 || metadata->timepoints != 1 ||
        metadata->series_count != 1 ||
        (metadata->sample_type != SampleType::UInt8 &&
         metadata->sample_type != SampleType::UInt16 &&
         metadata->sample_type != SampleType::UInt32) ||
        (png && (metadata->sample_type == SampleType::UInt32 ||
                 (indexed && metadata->sample_type != SampleType::UInt8) ||
                 metadata->channel_names != std::vector<std::string>{indexed ? "Palette index" : "Gray"})))
        throw std::invalid_argument(
            "External mask must match the selected unsigned TIFF, grayscale PNG or indexed PNG profile "
            "and have one plane with exact processing-crop dimensions");
    RawRegionRequest region;
    region.region = {0, 0, 0, width, height};
    auto raw = server->read_raw(region, cancelled);
    detail::require_raw_lattice(raw);
    if (raw.width() != width || raw.height() != height || raw.channels() != 1 ||
        raw.sample_type() != metadata->sample_type)
        throw WorkflowInputChanged("External mask decoder returned another lattice");
    const auto source = server->reader_source();
    if (png) {
        const auto link = detail::capture_native_reader_plugin_link(*server);
        if (!source || !link || link->implementation.sha256() != captured.reader_sha256 ||
            source->reader != detail::reader_plugin_binding(link->implementation))
            throw WorkflowInputChanged("PNG label reader returned another implementation identity");
        // Full-raster decoder storage is gone before sorting/normalizing labels
        // or constructing the retained raster. Owned raw codes remain exact.
        server->close();
        server.reset();
    }
    auto normalized = normalize_label_codes(raw, {options.max_regions, work}, cancelled);
    auto source_labels = std::move(normalized.source_codes).release_uint32();
    file.verify();
    FileFingerprintOptions hashing;
    hashing.byte_limit = 256u * 1024 * 1024;
    hashing.buffer_bytes = 65536;
    const auto fingerprint = fingerprint_file(options.path, hashing, cancelled);
    file.verify();
    if (!source || !source->primary ||
        !detail::same_reader_files(*source, *make_reader_source(source->reader, source->locator,
                                                                source->scope, file.version())))
        throw WorkflowInputChanged("External mask source changed during decoding");
    const auto extra = raw.payload_bytes() + source_labels.capacity() * 4 + 65536;
    if (extra >= work)
        throw WorkflowCaptureLimit("External mask retention exceeds work");
    return {std::move(normalized.raster),
            std::move(source_labels), fingerprint->sha256, source};
}
ImportedLabelMask import_label_mask_tiff(const ExternalLabelMaskOptions& options,
                                         std::int64_t width, std::int64_t height, std::size_t work,
                                         const std::atomic<bool>* cancelled) {
    if (options.format != ExternalLabelMaskFormat::Tiff)
        throw std::invalid_argument("The TIFF mask entry point requires the TIFF profile");
    return import_label_mask(options, width, height, work, cancelled);
}
} // namespace qpx
