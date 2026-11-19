// SPDX-License-Identifier: AGPL-3.0-only
#include "n5_source.hpp"
#include "n5_store.hpp"
#ifdef QUPATHXX_HAS_BDV_N5
#include "../bdv_n5/bdv_xml.hpp"
#endif

#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <string_view>

namespace qpx::detail {
namespace {
using Json = nlohmann::json;
using Shape = std::array<std::int64_t, 5>;
using Strides = std::array<std::size_t, 5>;
using XYZ = std::array<std::int64_t, 3>;
constexpr std::size_t mib = 1024 * 1024;
constexpr std::size_t metadata_limit = 64 * 1024;
constexpr std::size_t scratch_bytes = mib;
constexpr std::string_view axes_order = "xyzct";
constexpr std::size_t x_axis = 0, y_axis = 1, z_axis = 2, c_axis = 3, t_axis = 4;

enum class Compression { Raw, Gzip, Zlib };

void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}

struct Dataset {
    Shape shape{1, 1, 1, 1, 1};
    Shape block{1, 1, 1, 1, 1};
    std::vector<std::size_t> axes;
    std::shared_ptr<const RawImageMetadata> raw;
    std::string prefix;
    Compression compression = Compression::Raw;
    std::size_t block_bytes = 0;
    std::size_t encoded_limit = 0;
};

Json metadata_json(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::set<std::string>> keys;
    std::size_t events = 0;
    auto result = Json::parse(bytes.begin(), bytes.end(),
        [&](int depth, Json::parse_event_t event, Json& value) {
            require(depth <= 16 && ++events <= 4096, "N5 metadata exceeds structural limits");
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            if (event == Json::parse_event_t::key)
                require(keys.back().insert(value.get<std::string>()).second, "Duplicate N5 metadata key");
            return true;
        });
    require(result.is_object(), "N5 attributes must be a JSON object");
    return result;
}

std::int64_t positive_integer(const Json& value, std::int64_t limit) {
    require(value.is_number_integer(), "N5 dimensions and block sizes must be integers");
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        require(number <= static_cast<std::uint64_t>(limit), "N5 dimensions or block sizes exceed limits");
        require(number != 0, "N5 dimensions and block sizes must be positive");
        return static_cast<std::int64_t>(number);
    }
    const auto number = value.get<std::int64_t>();
    require(number > 0 && number <= limit, "N5 dimensions or block sizes exceed limits");
    return number;
}

Compression compression_mode(const Json& compression) {
    require(compression.is_object(), "N5 compression must describe its codec");
    const auto& type = compression.at("type");
    if (type == "raw") {
        require(compression.size() == 1, "Unsupported N5 raw compression parameters");
        return Compression::Raw;
    }
    require(type == "gzip", "Unsupported N5 compression");
    for (auto item = compression.begin(); item != compression.end(); ++item) {
        const auto& key = item.key();
        require(key == "type" || key == "level" || key == "useZlib", "Unsupported N5 gzip compression parameters");
    }
    if (compression.contains("level")) {
        const auto& level = compression.at("level");
        require(level.is_number_integer(), "Invalid N5 gzip compression level");
        if (level.is_number_unsigned())
            require(level.get<std::uint64_t>() <= 9, "Invalid N5 gzip compression level");
        else {
            const auto number = level.get<std::int64_t>();
            require(number >= -1 && number <= 9, "Invalid N5 gzip compression level");
        }
    }
    if (!compression.contains("useZlib")) return Compression::Gzip;
    const auto& zlib = compression.at("useZlib");
    require(zlib.is_boolean(), "N5 useZlib must be a boolean");
    return zlib.get<bool>() ? Compression::Zlib : Compression::Gzip;
}

void validate_coordinate_frame(const Json& attributes, std::size_t rank, bool bdv_scales = false) {
    require(bdv_scales || !attributes.contains("downsamplingFactors"), "Unsupported N5 downsampling metadata");
    for (const char* field : {"transform", "coordinateTransformations", "affine",
                              "pixelResolution", "multiscales", "scales", "unitsPerPixel"})
        require(!attributes.contains(field), "Unsupported N5 transform or alternate calibration");
    for (const char* field : {"offset", "origin", "translation"}) {
        if (!attributes.contains(field)) continue;
        const auto& offset = attributes.at(field);
        require(offset.is_array() && offset.size() == rank, "Invalid N5 coordinate origin");
        for (const auto& element : offset)
            require(element.is_number() && element.get<double>() == 0, "Unsupported nonzero N5 coordinate origin");
    }
}

double calibration_factor(std::size_t axis, std::string_view unit) {
    if (axis == t_axis) {
        if (unit == "s") return 1;
        if (unit == "ms") return 0.001;
        throw std::invalid_argument("Unsupported N5 time unit");
    }
    if (unit == "nm") return 0.001;
    if (unit == "um" || unit == u8"µm") return 1;
    if (unit == "mm") return 1000;
    if (unit == "m") return 1000000;
    throw std::invalid_argument("Unsupported N5 spatial unit");
}

void calibrate(const Json& attributes, const std::vector<std::size_t>& axes, RawImageMetadata& raw) {
    if (!attributes.contains("units")) {
        require(!attributes.contains("resolution"), "N5 resolution requires explicit units");
        return;
    }
    const auto& units = attributes.at("units");
    require(units.is_array() && units.size() == axes.size(), "N5 units must match the declared axes");
    auto resolution = attributes.find("resolution");
    if (resolution != attributes.end())
        require(resolution->is_array() && resolution->size() == units.size(), "N5 resolution must match the declared axes");
    const std::array<std::optional<double>*, 5> result{
        &raw.pixel_width_um, &raw.pixel_height_um, &raw.z_spacing_um, nullptr, &raw.time_interval_seconds};
    for (std::size_t slot = 0; slot < units.size(); ++slot) {
        const auto axis = axes[slot];
        require(units[slot].is_string(), "N5 units must be strings");
        const auto& unit = units[slot].get_ref<const std::string&>();
        double multiplier = 1;
        if (resolution != attributes.end()) {
            const auto& value = (*resolution)[slot];
            require(value.is_number(), "Invalid N5 resolution");
            multiplier = value.get<double>();
            require(std::isfinite(multiplier) && multiplier > 0, "N5 resolution must be positive and finite");
        }
        if (unit.empty() || axis == c_axis) {
            require(unit.empty() && multiplier == 1, "Unsupported N5 resolution for a unitless axis");
            continue;
        }
        const auto value = multiplier * calibration_factor(axis, unit);
        require(std::isfinite(value) && value > 0, "N5 calibration exceeds representable limits");
        *result[axis] = value;
    }
}

Dataset parse_dataset(const std::vector<std::uint8_t>& bytes, std::size_t chunk_budget,
    const XYZ* bdv_factors = nullptr) {
    const auto attributes = metadata_json(bytes);
    require(!attributes.contains("n5") || attributes.at("n5") == "2.0.0", "Unsupported explicit N5 version");
    require(attributes.at("dataType") == "uint16", "This N5 profile requires UInt16 samples");
    const auto bdv_axes = Json::array({"x", "y", "z"});
    const auto& axes = bdv_factors && !attributes.contains("axes") ? bdv_axes : attributes.at("axes");
    if (bdv_factors) {
        require(axes == bdv_axes, "BDV N5 cell axes must be XYZ");
        require(!attributes.contains("units") && !attributes.contains("resolution"),
            "BDV N5 cell calibration must come from the registered project");
        if (attributes.contains("downsamplingFactors")) {
            const auto& factors = attributes.at("downsamplingFactors");
            require(factors.is_array() && factors.size() == bdv_factors->size(),
                "Invalid BDV N5 cell downsampling factors");
            for (std::size_t axis = 0; axis < bdv_factors->size(); ++axis)
                require(positive_integer(factors[axis], (*bdv_factors)[axis]) == (*bdv_factors)[axis],
                    "BDV N5 cell downsampling differs from its setup");
        }
    }
    const auto& shape = attributes.at("dimensions");
    const auto& block = attributes.at("blockSize");
    require(axes.is_array() && axes.size() >= 2 && axes.size() <= axes_order.size()
        && axes[0] == "x" && axes[1] == "y", "N5 requires explicit x,y[,z][,c][,t] axes");
    require(shape.is_array() && block.is_array() && shape.size() == axes.size()
        && block.size() == axes.size(), "N5 dimensions and block sizes must match the declared axes");
    validate_coordinate_frame(attributes, axes.size(), bdv_factors != nullptr);
    for (const char* field : {"fillValue", "fill_value"}) {
        if (!attributes.contains(field)) continue;
        const auto& value = attributes.at(field);
        require(value.is_number() && value.get<double>() == 0, "Unsupported nonzero N5 fill value");
    }

    Dataset output;
    RawImageMetadata raw;
    output.block_bytes = sizeof(std::uint16_t);
    for (std::size_t slot = 0; slot < axes.size(); ++slot) {
        require(axes[slot].is_string(), "N5 axes must be strings");
        const auto& label = axes[slot].get_ref<const std::string&>();
        const auto axis = label.size() == 1 ? axes_order.find(label[0]) : std::string_view::npos;
        require(axis != std::string_view::npos && (slot == 0 || axis > output.axes.back()),
            "N5 requires explicit x,y[,z][,c][,t] axes");
        output.axes.push_back(axis);
        constexpr std::int64_t max_plane = 65536;
        output.shape[axis] = positive_integer(shape[slot],
            axis <= y_axis ? (std::numeric_limits<std::int64_t>::max)() : max_plane);
        output.block[axis] = positive_integer(block[slot], (std::numeric_limits<std::uint32_t>::max)());
        require(static_cast<std::uint64_t>(output.block[axis]) <= chunk_budget / output.block_bytes,
            "N5 block exceeds decoded byte limit");
        output.block_bytes *= static_cast<std::size_t>(output.block[axis]);
        raw.dimension_order.push_back(static_cast<char>(label[0] - 'a' + 'A'));
    }
    output.compression = compression_mode(attributes.at("compression"));
    const auto header_bytes = 4 + 4 * output.axes.size();
    output.encoded_limit = output.compression == Compression::Raw ? header_bytes + output.block_bytes
        : header_bytes + 2 * output.block_bytes + metadata_limit;
    raw.width = output.shape[x_axis];
    raw.height = output.shape[y_axis];
    raw.z_planes = static_cast<std::uint32_t>(output.shape[z_axis]);
    raw.channels = static_cast<std::uint32_t>(output.shape[c_axis]);
    raw.timepoints = static_cast<std::uint32_t>(output.shape[t_axis]);
    raw.sample_type = SampleType::UInt16;
    raw.levels.push_back({raw.width, raw.height, 1});
    raw.channel_names.reserve(raw.channels);
    for (std::uint32_t c = 0; c < raw.channels; ++c)
        raw.channel_names.push_back("Channel " + std::to_string(c + 1));
    calibrate(attributes, output.axes, raw);
    output.raw = std::make_shared<const RawImageMetadata>(std::move(raw));
    return output;
}

std::uint16_t be16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>((std::uint32_t(bytes[0]) << 8) | bytes[1]);
}

std::uint32_t be32(const std::uint8_t* bytes) {
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16)
        | (std::uint32_t(bytes[2]) << 8) | bytes[3];
}

std::vector<std::uint8_t> inflate_payload(const std::uint8_t* encoded, std::size_t size,
    std::size_t expected, Compression compression) {
    std::vector<std::uint8_t> result(expected);
    z_stream stream{};
    if (inflateInit2(&stream, compression == Compression::Gzip ? 31 : 15) != Z_OK)
        throw std::runtime_error("Cannot initialize the N5 block decoder");
    struct End {
        z_stream* stream;
        ~End() { inflateEnd(stream); }
    } end{&stream};
    stream.next_in = const_cast<std::uint8_t*>(encoded);
    stream.avail_in = static_cast<uInt>(size);
    stream.next_out = result.data();
    stream.avail_out = static_cast<uInt>(result.size());
    const auto status = inflate(&stream, Z_FINISH);
    require(status == Z_STREAM_END && stream.total_out == expected && stream.total_in == size,
        "Truncated, corrupt or trailing compressed N5 block data");
    return result;
}

struct Block {
    std::vector<std::uint8_t> bytes;
    std::size_t payload = 0;
    Strides stride{};
};

Block read_block(const N5Store& store, const Dataset& dataset, const Shape& at,
    const std::atomic<bool>* cancelled) {
    auto key = dataset.prefix;
    for (auto axis : dataset.axes) {
        if (!key.empty()) key += '/';
        key += std::to_string(at[axis]);
    }
    auto bytes = store.read(key, dataset.encoded_limit, true, cancelled);
    if (bytes.empty()) return {};
    const auto header = 4 + 4 * dataset.axes.size();
    require(bytes.size() >= header, "Truncated N5 block header");
    require(be16(bytes.data()) == 0 && be16(bytes.data() + 2) == dataset.axes.size(),
        "N5 block requires mode 0 and the dataset rank");
    Shape shape{1, 1, 1, 1, 1};
    for (std::size_t slot = 0; slot < dataset.axes.size(); ++slot) {
        const auto axis = dataset.axes[slot];
        const auto found = be32(bytes.data() + 4 + 4 * slot);
        const auto edge = std::min(dataset.block[axis], dataset.shape[axis] - at[axis] * dataset.block[axis]);
        require(found == dataset.block[axis] || found == edge, "N5 block shape differs from its grid position");
        shape[axis] = found;
    }
    Strides stride{};
    std::size_t expected = sizeof(std::uint16_t);
    for (std::size_t axis = 0; axis < shape.size(); ++axis) {
        stride[axis] = expected;
        expected *= static_cast<std::size_t>(shape[axis]);
    }
    if (dataset.compression == Compression::Raw) {
        require(bytes.size() - header == expected, "Invalid raw N5 block length");
        return {std::move(bytes), header, stride};
    }
    return {inflate_payload(bytes.data() + header, bytes.size() - header, expected, dataset.compression),
        0, stride};
}

std::size_t work_limit(std::size_t bytes) {
    if (bytes < 8 * mib || bytes > 1024 * mib)
        throw WorkflowCaptureLimit("N5 reader requires an 8 MiB to 1 GiB work limit");
    return bytes;
}

struct DatasetSet {
    std::vector<Dataset> volumes;
    std::vector<std::size_t> view_offsets;
    std::shared_ptr<const RawImageMetadata> raw;
    std::size_t retained = 0;
    bool separate_views = false;

    const Dataset& volume(const RawRegionRequest& request, std::uint32_t channel) const {
        if (!separate_views) return volumes.front();
        const auto view = std::size_t(channel) * raw->timepoints + request.t;
        const auto offset = view_offsets[view];
        require(offset != SIZE_MAX, "BigDataViewer view is declared missing for the requested channel and timepoint");
        return volumes[offset + std::size_t(request.region.level)];
    }
};

DatasetSet standalone_dataset(const N5Store& store, std::size_t work,
    const std::atomic<bool>* cancelled) {
    DatasetSet result;
    result.volumes.push_back(parse_dataset(store.read("attributes.json", metadata_limit, false, cancelled),
        std::min(64 * mib, work / 8)));
    result.raw = result.volumes.front().raw;
    result.retained = result.volumes.capacity() * sizeof(Dataset) +
        result.volumes.front().axes.capacity() * sizeof(std::size_t);
    return result;
}

#ifdef QUPATHXX_HAS_BDV_N5
XYZ scale_factors(const Json& values) {
    require(values.is_array() && values.size() == 3, "BDV N5 setup requires XYZ scale factors");
    return {positive_integer(values[0], INT32_MAX), positive_integer(values[1], INT32_MAX),
        positive_integer(values[2], INT32_MAX)};
}

void verify_timepoint(const N5Store& store, const std::string& prefix, const BdvXmlProject& project,
    std::size_t levels, const std::atomic<bool>* cancelled) {
    auto bytes = store.read(prefix + "/attributes.json", metadata_limit, true, cancelled);
    if (bytes.empty()) return;
    const auto attributes = metadata_json(bytes);
    validate_coordinate_frame(attributes, 3);
    require(!attributes.contains("units"), "BDV N5 timepoint units must come from the registered project");
    if (attributes.contains("multiScale")) {
        const auto& multi = attributes.at("multiScale");
        require(multi.is_boolean() && multi.get<bool>() == (levels > 1), "BDV N5 timepoint scale flag disagrees with its setup");
    }
    if (attributes.contains("resolution")) {
        const auto& resolution = attributes.at("resolution");
        require(resolution.is_array() && resolution.size() == 3, "Invalid BDV N5 timepoint resolution");
        for (std::size_t axis = 0; axis < 3; ++axis)
            require(resolution[axis].is_number() && resolution[axis].get<double>() == project.voxel_size[axis],
                "BDV N5 timepoint resolution disagrees with its registered project");
    }
}

std::vector<XYZ> setup_factors(const N5Store& store, const std::string& setup,
    const std::atomic<bool>* cancelled) {
    const auto setup_attributes = metadata_json(store.read(setup + "/attributes.json", metadata_limit, false, cancelled));
    require(setup_attributes.at("dataType") == "uint16", "BDV N5 setup requires UInt16");
    validate_coordinate_frame(setup_attributes, 3, true);
    require(!setup_attributes.contains("resolution") && !setup_attributes.contains("units"),
        "BDV N5 setup calibration must come from the registered project");
    const auto& scales = setup_attributes.at("downsamplingFactors");
    require(scales.is_array() && !scales.empty() && scales.size() <= 32, "Invalid BDV N5 scale count");
    std::vector<XYZ> factors;
    for (const auto& scale : scales) {
        const auto xyz = scale_factors(scale);
        require(xyz[0] == xyz[1] && xyz[2] == 1 &&
            (factors.empty() ? xyz[0] == 1 : xyz[0] > factors.back()[0]),
            "BDV N5 pyramid requires an increasing equal XY scale and unchanged Z");
        factors.push_back(xyz);
    }
    return factors;
}

DatasetSet bdv_dataset(const N5Store& store, std::size_t work, const std::atomic<bool>* cancelled) {
    const auto project = parse_bdv_xml(store.read("dataset.xml", metadata_limit, false, cancelled), BdvStorage::N5);
    const auto root = metadata_json(store.read(project.data_path + "/attributes.json", metadata_limit, false, cancelled));
    require(root.at("n5") == "2.0.0", "BDV profile requires N5 2.0.0");
    validate_coordinate_frame(root, 3);
    require(!root.contains("resolution") && !root.contains("units"),
        "BDV N5 root calibration must come from the registered project");

    constexpr std::size_t volume_limit = 4096;
    require(!project.setups.empty() && project.setups.size() <= volume_limit &&
        project.time_count > 0 && project.time_count <= volume_limit / project.setups.size(),
        "BDV view and level count exceeds limits");
    require(project.first_time <= UINT32_MAX - (project.time_count - 1), "BDV timepoint range overflows");
    RawImageMetadata raw;
    raw.width = project.dimensions[0];
    raw.height = project.dimensions[1];
    require(project.dimensions[2] > 0 && project.dimensions[2] <= 65536, "BDV N5 Z axis exceeds limits");
    raw.z_planes = static_cast<std::uint32_t>(project.dimensions[2]);
    raw.timepoints = project.time_count;
    raw.channels = static_cast<std::uint32_t>(project.setups.size());
    raw.sample_type = SampleType::UInt16;
    raw.dimension_order = raw.channels == 1 ? "XYZT" : "XYZCT";
    for (const auto& setup : project.setups)
        raw.channel_names.push_back(setup.name.empty() ?
            "Channel " + std::to_string(raw.channel_names.size() + 1) : setup.name);
    raw.pixel_width_um = project.voxel_um[0];
    raw.pixel_height_um = project.voxel_um[1];
    raw.z_spacing_um = project.voxel_um[2];
    DatasetSet result;
    result.separate_views = true;
    result.view_offsets.resize(project.missing_views.size(), SIZE_MAX);
    workflow_capture_charge(result.retained, result.view_offsets.capacity() * sizeof(std::size_t), work);
    std::vector<XYZ> common_factors;
    for (std::size_t channel = 0; channel < project.setups.size(); ++channel) {
        if (!project.setup_has_views(channel))
            continue;
        const auto& view = project.setups[channel];
        const auto setup = project.data_path + "/setup" + std::to_string(view.id);
        const auto factors = setup_factors(store, setup, cancelled);
        const auto first_setup = common_factors.empty();
        if (first_setup) {
            const auto views = project.missing_views.size();
            require(factors.size() <= volume_limit / views, "BDV view and level count exceeds limits");
            const auto available_views = std::count(project.missing_views.begin(), project.missing_views.end(), false);
            const auto count = available_views * factors.size();
            workflow_capture_charge(result.retained, count * sizeof(Dataset), work);
            result.volumes.reserve(count);
            common_factors = factors;
        } else {
            require(factors == common_factors, "BDV N5 pyramid scales differ across setups");
        }
        for (std::uint32_t time = 0; time < project.time_count; ++time) {
            const auto view_index = channel * project.time_count + time;
            if (project.missing_views[view_index])
                continue;
            result.view_offsets[view_index] = result.volumes.size();
            const auto first_view = raw.levels.empty();
            const auto prefix = setup + "/timepoint" + std::to_string(project.first_time + time);
            verify_timepoint(store, prefix, project, factors.size(), cancelled);
            for (std::size_t level = 0; level < factors.size(); ++level) {
                workflow_checkpoint(cancelled);
                const auto path = prefix + "/s" + std::to_string(level);
                auto dataset = parse_dataset(store.read(path + "/attributes.json", metadata_limit, false, cancelled),
                    std::min(64 * mib, work / 8), &factors[level]);
                dataset.prefix = path;
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const auto full = project.dimensions[axis];
                    const auto factor = factors[level][axis];
                    const auto lower = full / factor;
                    const auto upper = lower + (full % factor != 0 ? 1 : 0);
                    require(dataset.shape[axis] == lower || dataset.shape[axis] == upper,
                        "BDV N5 cell dimensions disagree with its registered pyramid");
                }
                if (first_view) {
                    raw.levels.push_back({dataset.shape[x_axis], dataset.shape[y_axis],
                        static_cast<double>(factors[level][x_axis])});
                } else {
                    const auto& expected = raw.levels[level];
                    require(dataset.shape[x_axis] == expected.width && dataset.shape[y_axis] == expected.height,
                        "BDV N5 pyramid shape differs across views");
                }
                dataset.raw.reset();
                workflow_capture_charge(result.retained, dataset.prefix.capacity() +
                    dataset.axes.capacity() * sizeof(std::size_t), work);
                result.volumes.push_back(std::move(dataset));
            }
        }
    }
    result.raw = std::make_shared<const RawImageMetadata>(std::move(raw));
    return result;
}
#endif

std::int64_t level_origin(std::int64_t coordinate, std::int64_t scale) {
    return coordinate / scale - (coordinate % scale < 0 ? 1 : 0);
}

class N5Image final : public ImageServer {
    const std::size_t work_;
    const N5Store store_;
    const DatasetSet datasets_;
    const std::shared_ptr<const RawImageMetadata> raw_;
    const ImageMetadata image_;
    std::atomic<bool> closed_{false};

    void available(const std::atomic<bool>* cancelled) const {
        workflow_checkpoint(cancelled);
        if (closed()) throw std::runtime_error("N5 image server is closed");
    }

    void charge(std::size_t bytes, std::size_t output, const Dataset* decoding = nullptr) const {
        workflow_capture_charge(bytes, output, work_);
        workflow_capture_charge(bytes, store_.payload_bytes(), work_);
        workflow_capture_charge(bytes, sizeof(*this) + raw_->payload_bytes() + datasets_.retained + scratch_bytes, work_);
        if (decoding) {
            workflow_capture_charge(bytes, decoding->encoded_limit, work_);
            if (decoding->compression != Compression::Raw)
                workflow_capture_charge(bytes, decoding->block_bytes, work_);
        } else {
            workflow_capture_charge(bytes, store_.inventory_budget(), work_);
        }
    }

    void copy_volume(const Dataset& dataset, const RegionRequest& r, const RawRegionRequest& request,
        std::size_t output_channel, RawPixelBuffer& output, const std::atomic<bool>* cancelled) const {
        const auto x0 = std::max<std::int64_t>(0, r.x);
        const auto y0 = std::max<std::int64_t>(0, r.y);
        const auto x1 = std::min(dataset.shape[x_axis], r.x + r.width);
        const auto y1 = std::min(dataset.shape[y_axis], r.y + r.height);
        if (x0 >= x1 || y0 >= y1) return;
        const auto width = static_cast<std::size_t>(r.width);
        auto& values = std::get<std::vector<std::uint16_t>>(output.writable_storage());

        const auto& block = dataset.block;
        const std::int64_t first_channel = datasets_.separate_views ? 0 : request.first_channel;
        const auto end_channel = first_channel + (datasets_.separate_views ? 1u : request.channels);
        Shape at{};
        at[z_axis] = request.z / block[z_axis];
        const auto dataset_time = datasets_.separate_views ? 0u : request.t;
        at[t_axis] = dataset_time / block[t_axis];
        for (auto c = first_channel / block[c_axis]; c <= (end_channel - 1) / block[c_axis]; ++c) {
            at[c_axis] = c;
            const auto base_c = c * block[c_axis];
            for (auto by = y0 / block[y_axis]; by <= (y1 - 1) / block[y_axis]; ++by) {
                at[y_axis] = by;
                const auto base_y = by * block[y_axis];
                const auto last_y = base_y + std::min(block[y_axis], y1 - base_y);
                for (auto bx = x0 / block[x_axis]; bx <= (x1 - 1) / block[x_axis]; ++bx) {
                    workflow_checkpoint(cancelled);
                    at[x_axis] = bx;
                    const auto decoded = read_block(store_, dataset, at, cancelled);
                    if (decoded.bytes.empty()) continue;
                    const auto base_x = bx * block[x_axis];
                    const auto last_x = base_x + std::min(block[x_axis], x1 - base_x);
                    const auto begin_channel = std::max(first_channel, base_c);
                    const auto last_channel = std::min(end_channel, base_c + block[c_axis]);
                    const auto zt = static_cast<std::size_t>(request.z % block[z_axis]) * decoded.stride[z_axis]
                        + static_cast<std::size_t>(dataset_time % block[t_axis]) * decoded.stride[t_axis];
                    const auto* source = decoded.bytes.data() + decoded.payload;
                    for (auto y = std::max(y0, base_y); y < last_y; ++y) {
                        workflow_checkpoint(cancelled);
                        const auto row = zt + static_cast<std::size_t>(y - base_y) * decoded.stride[y_axis];
                        for (auto x = std::max(x0, base_x); x < last_x; ++x) {
                            const auto input = row + static_cast<std::size_t>(x - base_x) * decoded.stride[x_axis];
                            const auto pixel = static_cast<std::size_t>(y - r.y) * width + static_cast<std::size_t>(x - r.x);
                            const auto output_sample = pixel * request.channels + output_channel;
                            for (auto channel = begin_channel; channel < last_channel; ++channel) {
                                const auto offset = input + static_cast<std::size_t>(channel - base_c) * decoded.stride[c_axis];
                                values[output_sample + static_cast<std::size_t>(channel - first_channel)] = be16(source + offset);
                            }
                        }
                    }
                }
            }
        }
    }

    RawPixelBuffer sample(const RawRegionRequest& request, std::size_t other,
        const std::atomic<bool>* cancelled) const {
        available(cancelled);
        request.validate(*raw_);
        const auto& level = raw_->levels[std::size_t(request.region.level)];
        const auto scale = static_cast<std::int64_t>(level.downsample);
        auto r = request.region;
        r.x = level_origin(r.x, scale);
        r.y = level_origin(r.y, scale);
        const auto width = static_cast<std::size_t>(r.width);
        const auto pixels = width * static_cast<std::size_t>(r.height);
        const auto output_bytes = pixels * (2 * std::size_t(request.channels) + 1);
        const auto volumes = datasets_.separate_views ? request.channels : 1u;
        for (std::uint32_t i = 0; i < volumes; ++i)
            charge(other, output_bytes, &datasets_.volume(request, request.first_channel + i));
        RawPixelBuffer output(r.width, r.height, request.channels, SampleType::UInt16);
        const auto x0 = std::max<std::int64_t>(0, r.x);
        const auto y0 = std::max<std::int64_t>(0, r.y);
        const auto x1 = std::min(level.width, r.x + r.width);
        const auto y1 = std::min(level.height, r.y + r.height);
        if (x0 >= x1 || y0 >= y1) return output;

        // All selected volumes were checked against the same level bounds.
        // A valid pixel therefore has one common mask, even if a block is sparse.
        auto& valid = output.writable_valid();
        for (auto y = y0; y < y1; ++y) {
            workflow_checkpoint(cancelled);
            const auto begin = static_cast<std::size_t>(y - r.y) * width + static_cast<std::size_t>(x0 - r.x);
            std::fill_n(valid.data() + begin, static_cast<std::size_t>(x1 - x0), std::uint8_t{1});
        }
        for (std::uint32_t i = 0; i < volumes; ++i)
            copy_volume(datasets_.volume(request, request.first_channel + i), r, request, i, output, cancelled);
        available(cancelled);
        return output;
    }

public:
    using Loader = DatasetSet (*)(const N5Store&, std::size_t, const std::atomic<bool>*);
    N5Image(const std::string& directory, std::size_t bytes, const std::atomic<bool>* cancelled,
        Loader load, const char* provider, const char* primary, const char* vendor)
        : work_(work_limit(bytes)), store_(directory, std::min(64 * mib, work_ / 4), cancelled, provider, primary),
          datasets_(load(store_, work_, cancelled)), raw_(datasets_.raw),
          image_{raw_->width, raw_->height, raw_->levels, raw_->pixel_width_um,
              raw_->pixel_height_um, vendor} {
        require_raw_current(cancelled);
    }

    const ImageMetadata& metadata() const override { return image_; }
    std::string reader_id() const override { return store_.reader_source()->reader; }
    std::string source_locator() const override { return store_.directory(); }
    std::shared_ptr<const ReaderSource> reader_source() const override { return store_.reader_source(); }
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled) const override {
        available(cancelled);
        return raw_;
    }
    void require_raw_current(const std::atomic<bool>* cancelled) const override {
        available(cancelled);
        charge(0, 0);
        store_.require_current(cancelled);
    }
    RawPixelBuffer read_raw(const RawRegionRequest& request, const std::atomic<bool>* cancelled) const override {
        require_raw_current(cancelled);
        auto result = sample(request, 0, cancelled);
        charge(result.payload_bytes(), 0);
        store_.require_current(cancelled);
        available(cancelled);
        return result;
    }
    PixelBuffer read_region(const RegionRequest& request) const override {
        validate_region(image_, request);
        require_raw_current(nullptr);
        const auto rgba_bytes = region_byte_count(request.width, request.height);
        charge(0, rgba_bytes);
        PixelBuffer result(request.width, request.height);
        auto& rgba = result.writable_bytes();
        RawRegionRequest raw;
        raw.channels = std::min(3u, raw_->channels);
        const auto scale = static_cast<std::int64_t>(raw_->levels[std::size_t(request.level)].downsample);
        constexpr std::int64_t tile = 256;
        for (std::int64_t y = 0; y < request.height; y += tile) {
            for (std::int64_t x = 0; x < request.width; x += tile) {
                raw.region = {request.x + x * scale, request.y + y * scale, request.level,
                    std::min(tile, request.width - x),
                    std::min(tile, request.height - y)};
                const auto slice = sample(raw, rgba_bytes, nullptr);
                const auto& values = std::get<std::vector<std::uint16_t>>(slice.storage());
                for (std::int64_t row = 0; row < slice.height(); ++row) {
                    for (std::int64_t col = 0; col < slice.width(); ++col) {
                        const auto p = static_cast<std::size_t>(row * slice.width() + col);
                        if (!slice.valid()[p]) continue;
                        const auto target = static_cast<std::size_t>((y + row) * request.width + x + col) * 4;
                        for (std::uint32_t channel = 0; channel < 3; ++channel) {
                            const auto source_channel = raw.channels == 1 ? 0 : channel;
                            if (source_channel >= raw.channels) continue;
                            const auto value = values[p * raw.channels + source_channel];
                            rgba[target + channel] = static_cast<std::uint8_t>((std::uint32_t(value) * 255 + 32767) / 65535);
                        }
                        rgba[target + 3] = 255;
                    }
                }
            }
        }
        store_.require_current(nullptr);
        available(nullptr);
        return result;
    }
    void close() override { closed_.store(true, std::memory_order_relaxed); }
    bool closed() const override { return closed_.load(std::memory_order_relaxed); }
};
}

std::shared_ptr<ImageServer> open_n5_image(const std::string& directory, std::size_t work_bytes,
    const std::atomic<bool>* cancelled) {
    return std::make_shared<N5Image>(directory, work_bytes, cancelled,
        standalone_dataset, "org.qupathxx.n5", "attributes.json", "N5");
}
#ifdef QUPATHXX_HAS_BDV_N5
std::shared_ptr<ImageServer> open_bdv_n5_image(const std::string& directory, std::size_t work_bytes,
    const std::atomic<bool>* cancelled) {
    return std::make_shared<N5Image>(directory, work_bytes, cancelled,
        bdv_dataset, "org.qupathxx.bdv-n5", "dataset.xml", "BigDataViewer N5");
}
#endif
}
