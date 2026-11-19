// SPDX-License-Identifier: AGPL-3.0-only
#include "bdv_hdf5_source.hpp"
#include "../bdv_n5/bdv_xml.hpp"
#include "../format-common/hdf5_input.hpp"
#include "directory_inventory.hpp"
#include "qupathxx/directory_store.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>

namespace qpx::detail {
namespace {
namespace hdf5 = qpx_hdf5;
using Triple = std::array<hsize_t, 3>;
constexpr std::size_t reserve_bytes = 8 * hdf5::mib;
constexpr std::uint64_t file_limit = UINT64_C(16) * 1024 * 1024 * 1024;
constexpr std::size_t xml_limit = 64 * 1024;
constexpr auto reader = "org.qupathxx.bdv-hdf5";

void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}

template <class Operation>
auto with_hdf5(const std::string& path, const std::atomic<bool>* cancelled, Operation read) {
    auto flag = cancelled;
    qpx_reader_cancel_v1 cancel{[](void* context) -> std::int32_t {
        const auto* stop = *static_cast<const std::atomic<bool>* const*>(context);
        return stop && stop->load(std::memory_order_relaxed);
    }, &flag};
    try {
        const auto lock = hdf5::library_lock(cancel);
        const auto native_path = std::filesystem::u8path(path);
        require_directory_real_ancestors(native_path, cancelled);
        hdf5::ReadLease lease(native_path, file_limit);
        auto file = hdf5::open_file(path);
        lease.verify_file(file);
        return read(file, lease, cancel);
    } catch (const qpx_binary::Failure& failure) {
        if (failure.status == QPX_READER_CANCELLED)
            throw WorkflowCancelled("BDV HDF5 read cancelled");
        throw std::invalid_argument(failure.message);
    }
}

std::string padded_id(std::uint32_t value, std::size_t width) {
    auto digits = std::to_string(value);
    if (digits.size() < width)
        digits.insert(0, width - digits.size(), '0');
    return digits;
}

std::string setup_path(std::uint32_t setup) {
    return "/s" + padded_id(setup, 2);
}

std::string cell_path(std::uint32_t setup, std::uint32_t time, std::size_t level) {
    return "/t" + padded_id(time, 5) + setup_path(setup) + "/" + std::to_string(level) + "/cells";
}

bool explicit_uint16(hid_t setup) {
    const auto exists = H5Aexists(setup, "dataType");
    hdf5::status(exists, "Cannot inspect BDV HDF5 setup datatype");
    if (!exists)
        return false;
    // Bounded fixed strings keep HDF5 from allocating a variable-length attribute.
    const auto attribute = H5Aopen(setup, "dataType", H5P_DEFAULT);
    hdf5::status(attribute, "Cannot open BDV HDF5 datatype attribute");
    struct AttributeGuard {
        hid_t id;
        ~AttributeGuard() { H5Aclose(id); }
    } guard{attribute};
    const hdf5::Handle type(H5Aget_type(attribute));
    const hdf5::Handle space(H5Aget_space(attribute));
    const auto size = H5Tget_size(type);
    std::array<char, 32> text{};
    require(H5Tget_class(type) == H5T_STRING && H5Tis_variable_str(type) == 0 &&
        H5Sget_simple_extent_type(space) == H5S_SCALAR && size && size < text.size(),
        "BDV HDF5 datatype attribute must be a bounded fixed string");
    hdf5::status(H5Aread(attribute, type, text.data()), "Cannot read BDV HDF5 datatype attribute");
    auto end = text.begin() + size;
    while (end != text.begin() && (end[-1] == '\0' || end[-1] == ' '))
        --end;
    require(std::string_view(text.data(), end - text.begin()) == "uint16", "Unsupported BDV HDF5 setup datatype");
    return true;
}

void require_partition_link(hid_t master, const std::string& key, std::string_view expected_file,
        const std::string& expected_key) {
    hdf5::hard_path(master, key.substr(0, key.find_last_of('/')));
    H5L_info2_t info{};
    hdf5::status(H5Lget_info2(master, key.c_str(), &info, H5P_DEFAULT), "Missing BDV HDF5 partition link");
    std::array<char, 2048> bytes{};
    require(info.type == H5L_TYPE_EXTERNAL && info.u.val_size >= 5 && info.u.val_size <= bytes.size(),
        "BDV HDF5 master cells require a bounded partition link");
    const auto size = info.u.val_size;
    hdf5::status(H5Lget_val(master, key.c_str(), bytes.data(), size, H5P_DEFAULT),
        "Cannot inspect BDV HDF5 partition link");
    require(bytes[size - 1] == '\0' && std::count(bytes.begin() + 1, bytes.begin() + size, '\0') == 2,
        "Malformed BDV HDF5 partition link");
    unsigned flags = 0;
    const char* target_file = nullptr;
    const char* target_key = nullptr;
    hdf5::status(H5Lunpack_elink_val(bytes.data(), size, &flags, &target_file, &target_key),
        "Cannot decode BDV HDF5 partition link");
    auto link_file = std::string(target_file);
    std::replace(link_file.begin(), link_file.end(), '\\', '/');
    auto object = std::string_view(target_key);
    if (!object.empty() && object.front() == '/')
        object.remove_prefix(1);
    require(flags == 0 && link_file == expected_file && object == std::string_view(expected_key).substr(1),
        "BDV HDF5 master link differs from the XML partition map");
}

std::vector<std::array<double, 3>> matrix(hid_t file, const std::string& key, bool floating,
        const hdf5::ReadLease& lease, qpx_reader_cancel_v1 cancel) {
    hdf5::Dataset selected(file, key, hdf5::mib);
    const auto& shape = selected.plan.dimensions;
    require(shape.rank == 2 && shape.extent[0] > 0 && shape.extent[0] <= 32 && shape.extent[1] == 3,
        "BDV HDF5 setup table must have 1 to 32 XYZ rows");
    const hdf5::Handle type(H5Dget_type(selected));
    const auto kind = hdf5::numeric_type(type);
    require(floating == (kind == hdf5::NumericKind::Floating), "Invalid BDV HDF5 setup table dtype");
    if (selected.plan.chunked) {
        hdf5::ChunkWorkspace scratch;
        hdf5::prevalidate(selected, selected.plan, {0, 0}, shape.extent, lease, scratch, cancel);
    }
    std::vector<std::array<double, 3>> result(static_cast<std::size_t>(shape.extent[0]));
    hdf5::status(H5Dread(selected, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, result.data()),
        "Cannot read the BDV HDF5 setup table");
    return result;
}

struct Cells {
    Triple shape{};
    Triple chunk{};
    std::size_t chunk_bytes = 0;
    bool big_endian = false;
    bool deflate = false;
    bool unsigned_type = false;
    std::size_t file = 0;
    std::string key;
};

Cells inspect_cells(hid_t data, const Triple& subdivision, bool unsigned_type) {
    Cells result;
    result.unsigned_type = unsigned_type;
    const hdf5::Handle space(H5Dget_space(data));
    require(H5Sget_simple_extent_ndims(space) == 3 &&
        H5Sget_simple_extent_dims(space, result.shape.data(), nullptr) == 3,
        "BDV HDF5 cells must have ZYX rank three");
    for (const auto extent : result.shape)
        require(extent && extent <= static_cast<hsize_t>(INT64_MAX), "Invalid BDV HDF5 cell dimension");
    const hdf5::Handle type(H5Dget_type(data));
    result.big_endian = H5Tequal(type, unsigned_type ? H5T_STD_U16BE : H5T_STD_I16BE) > 0;
    require(result.big_endian || H5Tequal(type, unsigned_type ? H5T_STD_U16LE : H5T_STD_I16LE) > 0,
        "BDV HDF5 cell storage differs from its declared UInt16 profile");
    const hdf5::Handle layout(H5Dget_create_plist(data));
    require(H5Pget_layout(layout) == H5D_CHUNKED && H5Pget_external_count(layout) == 0,
        "BDV HDF5 cells require local chunked storage");
    require(H5Pget_chunk(layout, 3, result.chunk.data()) == 3 && result.chunk == subdivision,
        "BDV HDF5 chunk dimensions differ from the XYZ subdivision table");
    result.chunk_bytes = sizeof(std::uint16_t);
    for (const auto extent : result.chunk) {
        require(extent && extent <= hdf5::mib / result.chunk_bytes, "BDV HDF5 decoded chunk exceeds 1 MiB");
        result.chunk_bytes *= static_cast<std::size_t>(extent);
    }
    const auto filters = H5Pget_nfilters(layout);
    require(filters == 0 || filters == 1, "Unsupported BDV HDF5 filter pipeline");
    if (filters) {
        unsigned flags = 0;
        unsigned parameters[2]{};
        std::size_t count = 2;
        const auto filter = H5Pget_filter2(layout, 0, &flags, &count, parameters, 0, nullptr, nullptr);
        require(filter == H5Z_FILTER_DEFLATE && count == 1 && parameters[0] <= 9,
            "BDV HDF5 permits only the built-in Deflate filter");
        result.deflate = true;
    }
    H5D_fill_value_t defined = H5D_FILL_VALUE_ERROR;
    std::uint16_t fill = 0;
    require(H5Pfill_value_defined(layout, &defined) >= 0 &&
        (defined == H5D_FILL_VALUE_DEFAULT || defined == H5D_FILL_VALUE_USER_DEFINED) &&
        H5Pget_fill_value(layout, unsigned_type ? H5T_NATIVE_UINT16 : H5T_NATIVE_INT16, &fill) >= 0 && fill == 0,
        "BDV HDF5 cells require a defined zero fill value");
    return result;
}

struct ProjectData {
    std::vector<Cells> cells;
    std::vector<std::size_t> view_offsets;
    std::shared_ptr<const RawImageMetadata> raw;

    const Cells& view(std::uint32_t channel, std::uint32_t time, std::size_t level) const {
        const auto offset = view_offsets[std::size_t(channel) * raw->timepoints + time];
        require(offset != SIZE_MAX, "BigDataViewer view is declared missing for the requested channel and timepoint");
        return cells[offset + level];
    }

    std::size_t payload_bytes() const {
        auto bytes = cells.capacity() * sizeof(Cells) + view_offsets.capacity() * sizeof(std::size_t) +
            raw->payload_bytes();
        for (const auto& view : cells)
            bytes += view.key.capacity();
        return bytes;
    }
};

std::int64_t integer_factor(double value) {
    require(std::isfinite(value) && value > 0 && value <= INT32_MAX && std::trunc(value) == value,
        "BDV HDF5 setup table requires bounded positive integers");
    return static_cast<std::int64_t>(value);
}

ProjectData inspect_project(const BdvXmlProject& project, const std::vector<std::string>& files, hid_t file,
        const hdf5::ReadLease& lease, const std::atomic<bool>* cancelled, qpx_reader_cancel_v1 cancel) {
    ProjectData result;
    require(!project.setups.empty() && project.time_count > 0 &&
        project.setups.size() <= 4096 / project.time_count, "BDV HDF5 view count exceeds limits");
    require(project.dimensions[2] <= 65536, "BDV HDF5 Z axis exceeds limit");

    RawImageMetadata raw;
    raw.width = project.dimensions[0];
    raw.height = project.dimensions[1];
    raw.z_planes = static_cast<std::uint32_t>(project.dimensions[2]);
    raw.timepoints = project.time_count;
    raw.channels = static_cast<std::uint32_t>(project.setups.size());
    raw.sample_type = SampleType::UInt16;
    raw.dimension_order = raw.channels == 1 ? "XYZT" : "XYZCT";
    raw.channel_names.reserve(raw.channels);
    raw.pixel_width_um = project.voxel_um[0];
    raw.pixel_height_um = project.voxel_um[1];
    raw.z_spacing_um = project.voxel_um[2];
    result.view_offsets.resize(project.missing_views.size(), SIZE_MAX);

    const auto master_directory = std::filesystem::u8path(files[0]).parent_path();
    std::vector<std::string> link_names;
    for (std::size_t index = 1; index < files.size(); ++index)
        link_names.push_back(files[index].empty() ? std::string{} :
            std::filesystem::u8path(files[index]).lexically_relative(master_directory).generic_u8string());

    for (std::uint32_t channel = 0; channel < raw.channels; ++channel) {
        const auto& selected_setup = project.setups[channel];
        const auto path = setup_path(selected_setup.id);
        raw.channel_names.push_back(selected_setup.name.empty() ? "Channel " + std::to_string(channel + 1) :
            selected_setup.name);
        if (!project.setup_has_views(channel))
            continue;
        const auto setup = hdf5::group(file, path);
        const auto unsigned_type = explicit_uint16(setup);
        const auto resolutions = matrix(file, path + "/resolutions", true, lease, cancel);
        const auto subdivisions = matrix(file, path + "/subdivisions", false, lease, cancel);
        const auto levels = resolutions.size();
        const auto views = std::size_t(raw.channels) * project.time_count;
        require(levels == subdivisions.size() && views <= 4096 / levels,
            "BDV HDF5 setup tables or view count exceed limits");
        const auto first_setup = raw.levels.empty();
        if (first_setup) {
            const auto available_views = std::count(project.missing_views.begin(), project.missing_views.end(), false);
            result.cells.reserve(available_views * levels);
        } else {
            require(levels == raw.levels.size(), "BDV HDF5 level count differs across setups");
        }

        struct LevelLayout {
            std::int64_t factor;
            Triple chunk;
        };
        std::vector<LevelLayout> layout;
        layout.reserve(levels);
        for (std::size_t level = 0; level < levels; ++level) {
            hdf5::poll(cancel);
            const auto factor = integer_factor(resolutions[level][0]);
            require(resolutions[level][1] == double(factor) && resolutions[level][2] == 1 &&
                    (level == 0 ? factor == 1 : layout.back().factor < factor),
                "BDV HDF5 pyramid requires increasing equal XY scales and unchanged Z");
            if (!first_setup)
                require(raw.levels[level].downsample == double(factor), "BDV HDF5 scales differ across setups");
            Triple chunk{};
            for (std::size_t axis = 0; axis < 3; ++axis)
                chunk[2 - axis] = static_cast<hsize_t>(integer_factor(subdivisions[level][axis]));
            layout.push_back({factor, chunk});
        }
        for (std::uint32_t time = 0; time < project.time_count; ++time) {
            const auto view_index = std::size_t(channel) * project.time_count + time;
            if (project.missing_views[view_index])
                continue;
            result.view_offsets[view_index] = result.cells.size();
            const auto first_view = raw.levels.empty();
            const auto inspect_view = [&](hid_t source, std::size_t index, std::uint32_t source_setup,
                    std::uint32_t source_time) {
                for (std::size_t level = 0; level < levels; ++level) {
                    hdf5::poll(cancel);
                    const auto& selected_level = layout[level];
                    auto key = cell_path(source_setup, source_time, level);
                    if (index)
                        require_partition_link(file, cell_path(selected_setup.id, project.first_time + time, level),
                            link_names[index - 1], key);
                    const auto selected = hdf5::dataset(source, key);
                    auto cells = inspect_cells(selected, selected_level.chunk, unsigned_type);
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        const auto extent = project.dimensions[axis];
                        const auto stride = axis == 2 ? 1 : selected_level.factor;
                        const auto lower = extent / stride;
                        const auto upper = lower + (extent % stride != 0);
                        require(cells.shape[2 - axis] == static_cast<hsize_t>(lower) ||
                            cells.shape[2 - axis] == static_cast<hsize_t>(upper),
                            "BDV HDF5 cell shape differs from the registered pyramid");
                    }
                    const auto width = static_cast<std::int64_t>(cells.shape[2]);
                    const auto height = static_cast<std::int64_t>(cells.shape[1]);
                    if (first_view)
                        raw.levels.push_back({width, height, static_cast<double>(selected_level.factor)});
                    else
                        require(raw.levels[level].width == width && raw.levels[level].height == height,
                            "BDV HDF5 cell shape differs across setups or timepoints");
                    cells.file = index;
                    cells.key = std::move(key);
                    result.cells.push_back(std::move(cells));
                }
            };
            if (project.partition_views.empty()) {
                inspect_view(file, 0, selected_setup.id, project.first_time + time);
            } else {
                const auto& view = project.partition_views[view_index];
                with_hdf5(files[view.file + 1], cancelled, [&](hid_t partition, const auto& part_lease, auto part_cancel) {
                    const auto partition_setup_path = setup_path(view.setup);
                    const auto partition_setup = hdf5::group(partition, partition_setup_path);
                    require(explicit_uint16(partition_setup) == unsigned_type &&
                        matrix(partition, partition_setup_path + "/resolutions", true, part_lease, part_cancel) == resolutions &&
                        matrix(partition, partition_setup_path + "/subdivisions", false, part_lease, part_cancel) == subdivisions,
                        "BDV HDF5 partition setup tables differ from the master");
                    inspect_view(partition, view.file + 1, view.setup, view.time);
                });
            }
        }
    }
    result.raw = std::make_shared<const RawImageMetadata>(std::move(raw));
    return result;
}

const std::uint8_t* read_chunk(hid_t dataset, const Cells& cells, const Triple& coordinate,
        const hdf5::ReadLease& lease, const hdf5::ChunkWorkspace& scratch, qpx_reader_cancel_v1 cancel) {
    hdf5::poll(cancel);
    unsigned mask = 0;
    haddr_t address = HADDR_UNDEF;
    hsize_t size = 0;
    hdf5::status(H5Dget_chunk_info_by_coord(dataset, coordinate.data(), &mask, &address, &size),
        "Cannot inspect BDV HDF5 raw chunk");
    if (!size) {
        require(address == HADDR_UNDEF, "Invalid absent BDV HDF5 chunk address");
        return nullptr;
    }
    require(address != HADDR_UNDEF && size <= cells.chunk_bytes + 65536 &&
        size <= hdf5::ChunkWorkspace::capacity && mask <= (cells.deflate ? 1u : 0u),
        "BDV HDF5 encoded chunk exceeds bounds or uses an invalid filter mask");
    lease.read(address, scratch.first.get(), static_cast<std::size_t>(size), cancel);
    if (cells.deflate && mask == 0) {
        const auto count = hdf5::inflate_chunk(scratch.first.get(), static_cast<std::size_t>(size),
            scratch.second.get(), cells.chunk_bytes + 1, cancel);
        require(count == cells.chunk_bytes, "BDV HDF5 Deflate output size differs from its chunk");
        return scratch.second.get();
    }
    require(size == cells.chunk_bytes, "BDV HDF5 raw chunk size differs from its layout");
    return scratch.first.get();
}

std::int64_t level_origin(std::int64_t coordinate, std::int64_t scale) {
    return coordinate / scale - (coordinate % scale < 0 ? 1 : 0);
}

class BdvHdf5Image final : public ImageServer {
public:
    BdvHdf5Image(const std::string& directory, std::size_t work, const std::atomic<bool>* cancelled)
        : work_(checked_work(work)),
          store_(directory, DirectoryStoreLimits{std::min(64 * hdf5::mib, work_ / 4)}, cancelled),
          source_(store_.reader_source(reader, "dataset.xml", cancelled)) {
        const auto xml = store_.read_member("dataset.xml", xml_limit, cancelled);
        require(xml.has_value(), "Missing BigDataViewer project XML");
        const auto project = parse_bdv_xml(*xml, BdvStorage::Hdf5);
        const auto add_file = [&](const std::string& path) {
            hdf5_paths_.push_back(store_.reader_source(reader, path, cancelled)->primary->path);
        };
        add_file(project.data_path);
        std::vector<bool> used_partitions(project.partition_paths.size());
        for (std::size_t index = 0; index < project.partition_views.size(); ++index) {
            if (!project.missing_views[index])
                used_partitions[project.partition_views[index].file] = true;
        }
        for (std::size_t index = 0; index < used_partitions.size(); ++index) {
            if (used_partitions[index])
                add_file(project.partition_paths[index]);
            else
                hdf5_paths_.emplace_back();
        }
        data_ = with_hdf5(hdf5_paths_.front(), cancelled, [&](hid_t file, const auto& lease, auto cancel) {
            return inspect_project(project, hdf5_paths_, file, lease, cancelled, cancel);
        });
        const auto& raw = *data_.raw;
        image_ = {raw.width, raw.height, raw.levels, raw.pixel_width_um, raw.pixel_height_um, "BigDataViewer HDF5"};
        require_raw_current(cancelled);
    }

    const ImageMetadata& metadata() const override { return image_; }
    std::string reader_id() const override { return reader; }
    std::string source_locator() const override { return store_.directory(); }
    std::shared_ptr<const ReaderSource> reader_source() const override { return source_; }
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>* cancelled) const override {
        available(cancelled);
        return data_.raw;
    }
    void require_raw_current(const std::atomic<bool>* cancelled) const override {
        available(cancelled);
        charge(0);
        store_.require_current(cancelled);
    }
    RawPixelBuffer read_raw(const RawRegionRequest& request, const std::atomic<bool>* cancelled) const override {
        return sample(request, 0, cancelled);
    }
    PixelBuffer read_region(const RegionRequest& region) const override {
        validate_region(image_, region);
        const auto bytes = region_byte_count(region.width, region.height);
        RawRegionRequest request{region};
        request.channels = std::min(3u, data_.raw->channels);
        auto foreground = sample(request, bytes, nullptr);
        PixelBuffer display(region.width, region.height);
        const auto& values = std::get<std::vector<std::uint16_t>>(foreground.storage());
        auto& rgba = display.writable_bytes();
        for (std::size_t pixel = 0; pixel < foreground.valid().size(); ++pixel) {
            if (!foreground.valid()[pixel])
                continue;
            for (std::uint32_t c = 0; c < 3; ++c) {
                const auto channel = request.channels == 1 ? 0u : c;
                if (channel < request.channels) {
                    const auto value = values[pixel * request.channels + channel];
                    rgba[pixel * 4 + c] = static_cast<std::uint8_t>((std::uint32_t(value) * 255 + 32767) / 65535);
                }
            }
            rgba[pixel * 4 + 3] = 255;
        }
        return display;
    }
    void close() override { closed_.store(true, std::memory_order_relaxed); }
    bool closed() const override { return closed_.load(std::memory_order_relaxed); }

private:
    static std::size_t checked_work(std::size_t work) {
        if (work < 16 * hdf5::mib || work > 1024 * hdf5::mib)
            throw WorkflowCaptureLimit("BDV HDF5 reader requires a 16 MiB to 1 GiB work limit");
        return work;
    }
    void charge(std::size_t bytes) const {
        workflow_capture_charge(bytes, store_.payload_bytes(), work_);
        workflow_capture_charge(bytes, store_.limits().inventory_bytes + reserve_bytes, work_);
        workflow_capture_charge(bytes, data_.payload_bytes(), work_);
        workflow_capture_charge(bytes, hdf5_paths_.capacity() * sizeof(std::string), work_);
        for (const auto& path : hdf5_paths_)
            workflow_capture_charge(bytes, path.capacity(), work_);
    }
    void available(const std::atomic<bool>* cancelled) const {
        workflow_checkpoint(cancelled);
        if (closed())
            throw std::runtime_error("BDV HDF5 image server is closed");
    }

    RawPixelBuffer sample(const RawRegionRequest& request, std::size_t reserved,
            const std::atomic<bool>* cancelled) const {
        available(cancelled);
        request.validate(*data_.raw);
        const auto level = static_cast<std::size_t>(request.region.level);
        const auto view = [&](std::uint32_t offset) -> const Cells& {
            return data_.view(request.first_channel + offset, request.t, level);
        };
        for (std::uint32_t offset = 0; offset < request.channels; ++offset)
            view(offset);
        auto region = request.region;
        const auto factor = static_cast<std::int64_t>(image_.levels[level].downsample);
        region.x = level_origin(region.x, factor);
        region.y = level_origin(region.y, factor);
        const auto count = static_cast<std::size_t>(region.width * region.height);
        charge(reserved + count * (sizeof(std::uint16_t) * request.channels + 1));
        RawPixelBuffer result(region.width, region.height, request.channels, SampleType::UInt16);
        store_.require_current(cancelled);
        const auto x0 = std::max<std::int64_t>(0, region.x);
        const auto y0 = std::max<std::int64_t>(0, region.y);
        const auto x1 = std::min(image_.levels[level].width, region.x + region.width);
        const auto y1 = std::min(image_.levels[level].height, region.y + region.height);
        if (x0 < x1 && y0 < y1) {
            const hdf5::ChunkWorkspace scratch;
            auto& samples = std::get<std::vector<std::uint16_t>>(result.writable_storage());
            auto& valid = result.writable_valid();
            for (auto row = y0; row < y1; ++row) {
                workflow_checkpoint(cancelled);
                const auto pixel = static_cast<std::size_t>((row - region.y) * region.width + (x0 - region.x));
                std::fill_n(valid.data() + pixel, static_cast<std::size_t>(x1 - x0), std::uint8_t{1});
            }
            for (std::uint32_t offset = 0; offset < request.channels;) {
                const auto source = view(offset).file;
                with_hdf5(hdf5_paths_[source], cancelled, [&](hid_t file, const auto& lease, auto cancel) {
                    for (; offset < request.channels && view(offset).file == source; ++offset) {
                        const auto& cells = view(offset);
                        const auto dataset = hdf5::dataset(file, cells.key);
                        const auto current = inspect_cells(dataset, cells.chunk, cells.unsigned_type);
                        require(current.shape == cells.shape && current.big_endian == cells.big_endian &&
                            current.deflate == cells.deflate, "BDV HDF5 cell metadata changed");
                        const auto z = hsize_t(request.z) / cells.chunk[0] * cells.chunk[0];
                        for (auto y = hsize_t(y0) / cells.chunk[1] * cells.chunk[1]; y < hsize_t(y1);
                             y += cells.chunk[1]) {
                            for (auto x = hsize_t(x0) / cells.chunk[2] * cells.chunk[2]; x < hsize_t(x1);
                                 x += cells.chunk[2]) {
                                const auto* bytes = read_chunk(dataset, cells, {z, y, x}, lease, scratch, cancel);
                                if (!bytes)
                                    continue;
                                const auto ymax = y + std::min(cells.chunk[1], hsize_t(y1) - y);
                                const auto xmax = x + std::min(cells.chunk[2], hsize_t(x1) - x);
                                const auto first_column = std::max(x, hsize_t(x0));
                                for (auto row = std::max(y, hsize_t(y0)); row < ymax; ++row) {
                                    hdf5::poll(cancel);
                                    const auto input_row = ((request.z - z) * cells.chunk[1] + row - y) * cells.chunk[2];
                                    auto pixel = static_cast<std::size_t>(std::int64_t(row) - region.y) * std::size_t(region.width)
                                        + static_cast<std::size_t>(std::int64_t(first_column) - region.x);
                                    for (auto col = first_column; col < xmax; ++col, ++pixel) {
                                        const auto* sample = bytes + (input_row + col - x) * 2;
                                        samples[pixel * request.channels + offset] = static_cast<std::uint16_t>(cells.big_endian
                                            ? (unsigned(sample[0]) << 8) | sample[1]
                                            : (unsigned(sample[1]) << 8) | sample[0]);
                                    }
                                }
                            }
                        }
                    }
                });
            }
        }
        store_.require_current(cancelled);
        available(cancelled);
        return result;
    }

    std::size_t work_;
    DirectoryStoreSnapshot store_;
    std::shared_ptr<const ReaderSource> source_;
    std::vector<std::string> hdf5_paths_;
    ProjectData data_;
    ImageMetadata image_;
    std::atomic<bool> closed_{false};
};

}

std::shared_ptr<ImageServer> open_bdv_hdf5_image(const std::string& directory,
    std::size_t work, const std::atomic<bool>* cancelled) {
    return std::make_shared<BdvHdf5Image>(directory, work, cancelled);
}
}
