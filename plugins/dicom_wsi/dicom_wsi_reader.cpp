// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "../image-common/rgba8_reader.hpp"

extern "C" {
#include <dicom/dicom.h>
}
#include <openslide.h>
#include <windows.h>

#include <locale>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

namespace {
using namespace qpx_image_plugin;
namespace fs = std::filesystem;
constexpr auto abi = QPX_READER_ABI_V1;
constexpr const char* provider = "org.qupathxx.dicom-wsi";
constexpr const char* wsi_class = "1.2.840.10008.5.1.4.1.1.77.1.6";
constexpr std::uint64_t metadata_budget = 8u * 1024 * 1024;
constexpr std::uint64_t cache_budget = 8u * 1024 * 1024;

struct Error {
    DcmError* value = nullptr;
    ~Error() { dcm_error_clear(&value); }
};

std::string tag(const DcmDataSet* dataset, const char* keyword, unsigned index = 0) {
    Error error;
    const auto* element = dcm_dataset_get(&error.value, dataset, dcm_dict_tag_from_keyword(keyword));
    const char* value = nullptr;
    if (!element || !dcm_element_get_value_string(&error.value, element, index, &value) || !value) return {};
    return value;
}

struct Instance {
    std::string series;
    std::string sop;
    std::string flavor;
};

struct Input {
    const fs::path& path;
    qpx_reader_cancel_v1 cancel;
    std::uint64_t remaining = metadata_budget;
    bool io_failed = false;
};

struct FileIo : DcmIO {
    FILE* stream;
    Input* input;
};

DcmIO* open_io(DcmError**, void* context) noexcept {
    auto& input = *static_cast<Input*>(context);
    FILE* file = nullptr;
    if (_wfopen_s(&file, input.path.c_str(), L"rb") || !file) return nullptr;
    auto* io = new (std::nothrow) FileIo{{nullptr}, file, &input};
    if (!io) std::fclose(file);
    return io;
}

void close_io(DcmIO* value) noexcept {
    auto* io = static_cast<FileIo*>(value);
    std::fclose(io->stream);
    delete io;
}

std::int64_t read_io(DcmError**, DcmIO* value, char* output, std::int64_t length) noexcept {
    auto& io = *static_cast<FileIo*>(value);
    auto& input = *io.input;
    if (input.cancel.is_cancelled && input.cancel.is_cancelled(input.cancel.context)) return -1;
    if (length < 0 || std::uint64_t(length) > input.remaining) {
        input.io_failed = true;
        return -1;
    }
    const auto count = std::fread(output, 1, std::size_t(length), io.stream);
    input.remaining -= count;
    if (!std::ferror(io.stream)) return std::int64_t(count);
    input.io_failed = true;
    return -1;
}

std::int64_t seek_io(DcmError**, DcmIO* value, std::int64_t offset, int mode) noexcept {
    auto& io = *static_cast<FileIo*>(value);
    if (!_fseeki64(io.stream, offset, mode)) {
        const auto position = _ftelli64(io.stream);
        if (position >= 0) return position;
    }
    io.input->io_failed = true;
    return -1;
}

std::optional<Instance> probe(const fs::path& path, qpx_reader_cancel_v1 cancel) {
    checkpoint(cancel);
    Error error;
    using File = std::unique_ptr<DcmFilehandle, decltype(&dcm_filehandle_destroy)>;
    Input input{path, cancel};
    static const DcmIOMethods methods{open_io, close_io, read_io, seek_io};
    auto* io = dcm_io_create(&error.value, &methods, &input);
    valid(io, "Cannot open DICOM source metadata");
    File file(dcm_filehandle_create(&error.value, io), dcm_filehandle_destroy);
    if (!file) {
        dcm_io_close(io);
        fail(QPX_READER_INTERNAL_ERROR, "Cannot allocate DICOM metadata handle");
    }
    const auto check_io = [&] {
        checkpoint(cancel);
        valid(!input.io_failed, "DICOM metadata IO failed or exceeded the read allowance");
    };
    const auto* meta = dcm_filehandle_get_file_meta(&error.value, file.get());
    check_io();
    if (!meta || tag(meta, "MediaStorageSOPClassUID") != wsi_class) return {};
    // Required WSI geometry follows the instance tags in the accepted syntax.
    // Stop before geometry and pixel/frame metadata.
    static const std::uint32_t stop_tags[] = {0x00280010, 0x00280011, 0x7fe00010, 0};
    using Metadata = std::unique_ptr<DcmDataSet, decltype(&dcm_dataset_destroy)>;
    Metadata dataset(dcm_filehandle_read_metadata(&error.value, file.get(), stop_tags), dcm_dataset_destroy);
    check_io();
    valid(bool(dataset), "Cannot read WSI instance metadata");
    Instance instance{tag(dataset.get(), "SeriesInstanceUID"), tag(dataset.get(), "SOPInstanceUID"),
                      tag(dataset.get(), "ImageType", 2)};
    valid(!instance.series.empty(), "WSI instance has no Series Instance UID");
    return instance;
}

bool retained(const Instance& instance) {
    return instance.flavor == "VOLUME" || instance.flavor == "LABEL" ||
           instance.flavor == "OVERVIEW" || instance.flavor == "THUMBNAIL";
}

DWORD attributes(const fs::path& path) {
    const auto result = GetFileAttributesW(path.c_str());
    valid(result != INVALID_FILE_ATTRIBUTES, "Cannot inspect DICOM source path");
    require(!(result & FILE_ATTRIBUTE_REPARSE_POINT), "DICOM source directory contains a reparse point");
    return result;
}

fs::path primary_path(const qpx_reader_open_v1* request) {
    require(request && request->struct_size >= sizeof(*request) && !request->reserved &&
            request->path.data && request->path.size && request->path.size <= 32768 && !request->options.size,
            "Invalid DICOM WSI open request");
    require(request->work_bytes > metadata_budget + cache_budget && request->work_bytes <= work_limit,
            "DICOM WSI requires more than 16 MiB and at most 256 MiB of working space");
    std::string path(request->path.data, std::size_t(request->path.size));
    require(path.find('\0') == std::string::npos, "DICOM path contains NUL");
    auto native = fs::u8path(path).lexically_normal();
    require(native.is_absolute(), "DICOM path must be absolute");
    return native;
}

std::vector<fs::path> companions(const qpx_reader_open_v1* request) {
    const auto primary = primary_path(request);
    require(!(attributes(primary) & FILE_ATTRIBUTE_DIRECTORY), "DICOM source is a directory");
    const auto selected = probe(primary, request->cancel);
    require(selected && !selected->sop.empty() && retained(*selected), "Primary is not a selected WSI instance");
    std::set<std::string> seen{selected->sop};
    std::vector<fs::path> members;
    unsigned visited = 0;
    for (const auto& candidate : fs::directory_iterator(primary.parent_path())) {
        checkpoint(request->cancel);
        require(++visited <= 4096, "DICOM sibling scan exceeds 4096 entries");
        const auto& path = candidate.path();
        if (path == primary || (attributes(path) & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const auto instance = probe(path, request->cancel);
        if (!instance || instance->series != selected->series) continue;
        valid(!instance->sop.empty() && seen.insert(instance->sop).second,
              "DICOM series has a missing or duplicate SOP Instance UID");
        valid(!instance->flavor.empty(), "DICOM series has an instance with no image flavor");
        if (!retained(*instance)) continue;
        require(members.size() < 63, "DICOM series exceeds 63 companions");
        members.push_back(path);
    }
    std::sort(members.begin(), members.end());
    return members;
}

qpx_reader_status_v1 QPX_READER_CALL source_count(const qpx_reader_open_v1* request, std::uint32_t* out,
                                                 qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out, "Null DICOM companion count");
        *out = static_cast<std::uint32_t>(companions(request).size());
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_READER_CALL source_path(const qpx_reader_open_v1* request, std::uint32_t index,
                                                qpx_reader_text_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        const auto paths = companions(request);
        require(out && index < paths.size(), "Invalid DICOM companion index");
        return text(*out, paths[index].u8string().c_str()) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
}

struct Image {
    std::unique_ptr<openslide_t, decltype(&openslide_close)> slide{nullptr, openslide_close};
    std::vector<qpx_reader_level_v1> levels;
    std::optional<double> x_um, y_um;
    std::uint64_t region_bytes = 0;
};

class SlideError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template<class Action>
qpx_reader_status_v1 slide_boundary(qpx_reader_text_v1* error, Action&& action) noexcept {
    return boundary(error, [&] {
        try {
            return action();
        } catch (const SlideError& failure_message) {
            return failure(error, QPX_READER_IO_ERROR, failure_message.what());
        }
    });
}

void check_slide(openslide_t* slide) {
    if (const auto* error = openslide_get_error(slide)) throw SlideError(error);
}

std::optional<double> calibration(openslide_t* slide, const char* key) {
    const auto* value = openslide_get_property_value(slide, key);
    check_slide(slide);
    if (!value) return {};
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    double result = 0;
    if (!(input >> result) || !std::isfinite(result) || result <= 0) return {};
    input >> std::ws;
    return input.eof() ? std::optional<double>(result) : std::nullopt;
}

qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* out, qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        require(out && out->struct_size >= sizeof(*out), "Short DICOM descriptor buffer");
        out->abi_version = abi;
        out->capabilities = QPX_READER_CAP_RGBA | QPX_READER_CAP_SOURCE_FILES;
        bool complete = text(out->provider_id, provider);
        complete = text(out->display_name, "DICOM WSI") && complete;
        complete = text(out->version, "1.0.0") && complete;
        complete = text(out->contract, "dicom-wsi-openslide/1") && complete;
        return complete ? QPX_READER_OK : failure(error, QPX_READER_BUFFER_TOO_SMALL, "Short DICOM descriptor text buffer");
    });
}

qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request, void** out, qpx_reader_text_v1* error) noexcept {
    return slide_boundary(error, [&] {
        require(out, "Null DICOM handle output");
        *out = nullptr;
        const auto path = primary_path(request);
        checkpoint(request->cancel);
        auto image = std::make_unique<Image>();
        image->slide.reset(openslide_open(path.u8string().c_str()));
        valid(bool(image->slide), "OpenSlide could not open the DICOM source");
        auto* slide = image->slide.get();
        check_slide(slide);
        const auto* vendor = openslide_get_property_value(slide, OPENSLIDE_PROPERTY_NAME_VENDOR);
        check_slide(slide);
        valid(vendor && std::string_view(vendor) == "dicom", "OpenSlide did not select the DICOM WSI decoder");
        const auto count = openslide_get_level_count(slide);
        check_slide(slide);
        valid(count > 0 && count <= 65536, "Invalid DICOM pyramid level count");
        image->levels.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i) {
            checkpoint(request->cancel);
            qpx_reader_level_v1 level{sizeof(level), 0, 0, 0, 0};
            openslide_get_level_dimensions(slide, i, &level.width, &level.height);
            level.downsample = openslide_get_level_downsample(slide, i);
            check_slide(slide);
            valid(level.width > 0 && level.height > 0 && std::isfinite(level.downsample) &&
                  level.downsample >= 1 && (i || level.downsample == 1), "Invalid DICOM pyramid level");
            image->levels.push_back(level);
        }
        image->x_um = calibration(slide, OPENSLIDE_PROPERTY_NAME_MPP_X);
        image->y_um = calibration(slide, OPENSLIDE_PROPERTY_NAME_MPP_Y);
        auto* cache = openslide_cache_create(std::size_t(cache_budget));
        openslide_set_cache(slide, cache);
        openslide_cache_release(cache);
        check_slide(slide);
        checkpoint(request->cancel);
        image->region_bytes = request->work_bytes - metadata_budget - cache_budget;
        *out = image.release();
        return QPX_READER_OK;
    });
}

void QPX_READER_CALL destroy(void* value) noexcept { delete static_cast<Image*>(value); }

const Image& image(void* value) {
    require(value, "Null DICOM image handle");
    return *static_cast<const Image*>(value);
}

qpx_reader_status_v1 QPX_READER_CALL metadata(void* value, qpx_reader_metadata_v1* out, qpx_reader_cancel_v1 cancel,
                                              qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        checkpoint(cancel);
        require(out && out->struct_size >= sizeof(*out), "Short DICOM metadata buffer");
        const auto& source = image(value);
        const auto vendor = out->vendor, order = out->dimension_order;
        *out = {};
        out->struct_size = sizeof(*out);
        out->vendor = vendor;
        out->dimension_order = order;
        out->width = source.levels.front().width;
        out->height = source.levels.front().height;
        out->level_count = static_cast<std::uint32_t>(source.levels.size());
        if (source.x_um) {
            out->calibration_flags |= QPX_READER_CALIBRATION_X;
            out->pixel_width_um = *source.x_um;
        }
        if (source.y_um) {
            out->calibration_flags |= QPX_READER_CALIBRATION_Y;
            out->pixel_height_um = *source.y_um;
        }
        const bool wrote_vendor = text(out->vendor, "dicom");
        return text(out->dimension_order, "") && wrote_vendor ? QPX_READER_OK :
            failure(error, QPX_READER_BUFFER_TOO_SMALL, "Short DICOM metadata text buffer");
    });
}

qpx_reader_status_v1 QPX_READER_CALL level(void* value, std::uint32_t index, qpx_reader_level_v1* out,
                                           qpx_reader_text_v1* error) noexcept {
    return boundary(error, [&] {
        const auto& source = image(value);
        require(out && out->struct_size >= sizeof(*out) && index < source.levels.size(), "Invalid DICOM level request");
        *out = source.levels[index];
        return QPX_READER_OK;
    });
}

qpx_reader_status_v1 QPX_READER_CALL channel_name(void*, std::uint32_t, qpx_reader_text_v1*,
                                                  qpx_reader_text_v1* error) noexcept {
    return failure(error, QPX_READER_UNSUPPORTED, "Display-only DICOM WSI has no raw channels");
}

qpx_reader_status_v1 QPX_READER_CALL raw(void*, const qpx_reader_region_v1*, qpx_reader_pixels_v1*,
                                         qpx_reader_cancel_v1, qpx_reader_text_v1* error) noexcept {
    return failure(error, QPX_READER_UNSUPPORTED, "Display-only DICOM WSI has no raw samples");
}

qpx_reader_status_v1 QPX_READER_CALL rgba(void* value, const qpx_reader_region_v1* request, qpx_reader_pixels_v1* out,
                                          qpx_reader_text_v1* error) noexcept {
    return slide_boundary(error, [&] {
        const auto& source = image(value);
        require(request && request->struct_size >= sizeof(*request) && request->level >= 0 &&
                std::size_t(request->level) < source.levels.size() && !request->first_channel &&
                !request->channels && !request->z && !request->t, "Invalid DICOM region request");
        const auto& grid = source.levels[std::size_t(request->level)];
        const auto width = request->width, height = request->height;
        require(width > 0 && height > 0 && std::uint64_t(width) <= source.region_bytes / 4 &&
                std::uint64_t(height) <= source.region_bytes / 4 / std::uint64_t(width), "DICOM region exceeds working space");
        const auto count = std::uint64_t(width) * std::uint64_t(height);
        require(out && out->struct_size >= sizeof(*out) && !out->reserved && out->data && out->capacity >= count * 4 &&
                out->sample_type == QPX_READER_UINT8 && out->width == width && out->height == height && out->channels == 4 &&
                out->row_stride == std::uint64_t(width) * 4 && out->pixel_stride == 4 && out->channel_stride == 1 &&
                !out->validity && !out->validity_capacity, "Invalid DICOM pixel buffer layout");
        const double extent_x = std::ceil(double(width) * grid.downsample);
        const double extent_y = std::ceil(double(height) * grid.downsample);
        const double bound = double(std::numeric_limits<std::int64_t>::max());
        require(extent_x < bound && extent_y < bound &&
                request->x <= INT64_MAX - static_cast<std::int64_t>(extent_x) &&
                request->y <= INT64_MAX - static_cast<std::int64_t>(extent_y), "DICOM region coordinates overflow");
        out->written = out->validity_written = 0;
        std::vector<std::uint32_t> argb(static_cast<std::size_t>(count));
        auto* slide = source.slide.get();
        openslide_read_region(slide, argb.data(), request->x, request->y, request->level, width, height);
        check_slide(slide);
        const double x0 = std::floor(double(request->x) / grid.downsample);
        const double y0 = std::floor(double(request->y) / grid.downsample);
        for (std::int64_t row = 0; row < height; ++row) {
            for (std::int64_t col = 0; col < width; ++col) {
                const auto index = std::size_t(row * width + col);
                const bool inside = x0 + double(col) >= 0 && x0 + double(col) < double(grid.width) &&
                                    y0 + double(row) >= 0 && y0 + double(row) < double(grid.height);
                const auto pixel = inside ? argb[index] : 0;
                const auto alpha = pixel >> 24;
                for (unsigned c = 0; c < 3; ++c) {
                    const auto sample = (pixel >> (16 - 8 * c)) & 255u;
                    out->data[index * 4 + c] = alpha ? std::uint8_t(std::min(255u, (sample * 255u + alpha / 2) / alpha)) : 0;
                }
                out->data[index * 4 + 3] = std::uint8_t(alpha);
            }
        }
        out->written = count * 4;
        return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
        std::uint32_t requested, qpx_reader_api_v1* out, qpx_reader_text_v1* error) {
    return boundary(error, [&] {
        require(out && out->struct_size >= sizeof(*out) && requested == abi, "Invalid DICOM WSI ABI entry");
        *out = {sizeof(*out), abi, describe, open, destroy, metadata, level, channel_name, rgba, raw};
        return QPX_READER_OK;
    });
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_files_entry_v1(
        std::uint32_t requested, qpx_reader_files_api_v1* out, qpx_reader_text_v1* error) {
    return boundary(error, [&] {
        require(out && out->struct_size >= sizeof(*out) && requested == abi, "Invalid DICOM WSI source ABI entry");
        *out = {sizeof(*out), abi, source_count, source_path};
        return QPX_READER_OK;
    });
}
