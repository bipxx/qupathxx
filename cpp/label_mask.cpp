#include "qupathxx/label_mask.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "label_analysis_internal.hpp"
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
} // namespace
void ExternalLabelMaskOptions::validate() const {
    detail::bounded_utf8(path, 2048);
    detail::bounded_utf8(producer, 256);
    detail::bounded_utf8(producer_version, 256);
    if (path.empty() || path.find('\0') != std::string::npos ||
        !std::filesystem::u8path(path).is_absolute() || producer.empty() ||
        producer_version.empty() || !digest(model_sha256) || !digest(input_sha256) ||
        !max_regions || max_regions > 1000000)
        throw std::invalid_argument("External masks require an absolute TIFF path, "
                                    "producer/version, exact model/input SHA-256 and region limit");
}
ImportedLabelMask import_label_mask_tiff(const ExternalLabelMaskOptions& options,
                                         std::int64_t width, std::int64_t height, std::size_t work,
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
    detail::ReaderOpenOptions opening;
    opening.tiff.cache_bytes = 0;
    opening.tiff.decode_bytes = std::min<std::size_t>(64u * 1024 * 1024, work - base);
    if (opening.tiff.decode_bytes < 65536)
        throw WorkflowCaptureLimit("No work remains for external mask decoding");
    auto server = detail::open_reader(options.path, detail::select_reader(options.path, "tiff"),
                                      opening, cancelled);
    const auto metadata = server->raw_metadata(cancelled);
    if (!metadata || metadata->width != width || metadata->height != height ||
        metadata->channels != 1 || metadata->z_planes != 1 || metadata->timepoints != 1 ||
        metadata->series_count != 1 ||
        (metadata->sample_type != SampleType::UInt8 &&
         metadata->sample_type != SampleType::UInt16 &&
         metadata->sample_type != SampleType::UInt32))
        throw std::invalid_argument("External mask must be one unsigned UInt8/16/32 TIFF plane "
                                    "with exact processing-crop dimensions");
    RawRegionRequest region;
    region.region = {0, 0, 0, width, height};
    auto raw = server->read_raw(region, cancelled);
    detail::require_raw_lattice(raw);
    if (raw.width() != width || raw.height() != height || raw.channels() != 1 ||
        raw.sample_type() != metadata->sample_type)
        throw WorkflowInputChanged("External mask decoder returned another lattice");
    std::vector<std::uint32_t> labels(n);
    std::visit(
        [&](const auto& values) {
            for (std::size_t i = 0; i < n; ++i) {
                if (!(i & 4095))
                    workflow_checkpoint(cancelled);
                if (!raw.valid()[i])
                    throw std::invalid_argument("External mask contains missing pixels");
                labels[i] = std::uint32_t(values[i]);
            }
        },
        raw.storage());
    auto source_labels = labels;
    std::uint64_t comparisons = 0;
    std::sort(source_labels.begin(), source_labels.end(), [&](auto a, auto b) {
        if (!(comparisons++ & 4095))
            workflow_checkpoint(cancelled);
        return a < b;
    });
    source_labels.erase(std::unique(source_labels.begin(), source_labels.end()),
                        source_labels.end());
    if (!source_labels.empty() && !source_labels.front())
        source_labels.erase(source_labels.begin());
    if (source_labels.size() > options.max_regions)
        throw WorkflowCaptureLimit("External mask exceeds the region limit");
    // Keep just the admitted mapping rather than a retained full-raster capacity.
    std::vector<std::uint32_t>(source_labels).swap(source_labels);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(i & 4095))
            workflow_checkpoint(cancelled);
        if (labels[i])
            labels[i] = std::uint32_t(
                std::lower_bound(source_labels.begin(), source_labels.end(), labels[i]) -
                source_labels.begin() + 1);
    }
    file.verify();
    FileFingerprintOptions hashing;
    hashing.byte_limit = 256u * 1024 * 1024;
    hashing.buffer_bytes = 65536;
    const auto fingerprint = fingerprint_file(options.path, hashing, cancelled);
    file.verify();
    const auto source = server->reader_source();
    if (!source || !source->primary ||
        !detail::same_reader_files(*source, *make_reader_source(source->reader, source->locator,
                                                                source->scope, file.version())))
        throw WorkflowInputChanged("External mask source changed during decoding");
    const auto extra = raw.payload_bytes() + source_labels.capacity() * 4 + 65536;
    if (extra >= work)
        throw WorkflowCaptureLimit("External mask retention exceeds work");
    return {LabelRaster(width, height, std::move(labels), {options.max_regions, work - extra},
                        cancelled),
            std::move(source_labels), fingerprint->sha256, source};
}
} // namespace qpx
