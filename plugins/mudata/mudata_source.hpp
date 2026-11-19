// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../format-common/hdf5_input.hpp"
#include "../format-common/json_tree.hpp"
#include <qupathxx/format_plugin.h>
#include <utility>

static_assert(H5_VERS_MAJOR == 2 && H5_VERS_MINOR == 2 && H5_VERS_RELEASE == 0);
static_assert(ZLIB_VERNUM == 0x1320);

namespace qpx_mudata {
using namespace qpx_binary;
namespace hdf = qpx_hdf5;
namespace json = qpx_native_json;
constexpr std::size_t chunk_limit = 4 * hdf::mib;
constexpr std::size_t fixed_work = 24 * hdf::mib;
constexpr std::uint64_t axis_limit = 1000000;

inline bool child_key(std::string_view name) {
    return !name.empty() && name.size() <= 256 && name != "." && name != ".." &&
           name.find('/') == std::string_view::npos;
}

struct Options {
    std::string modality, matrix, domain;
    unsigned value_type;
};

inline Options select(qpx_reader_string_v1 input, qpx_reader_cancel_v1 cancel) {
    require(input.data && input.size && input.size <= 4096, "MuData requires explicit modality, matrix and value domain JSON");
    Arena arena(65536);
    json::Tree tree({input.data, std::size_t(input.size)}, &arena.memory, cancel, 3, 4, 1);
    const auto& root = tree.root();
    tree.keys(root, {"modality", "matrix", "value_domain"});
    const auto modality = json::string(tree.get(root, "modality"));
    const auto matrix = json::string(tree.get(root, "matrix"));
    const auto domain = json::string(tree.get(root, "value_domain"));
    for (const auto label : {modality, matrix, domain}) utf8(label, cancel);
    require(child_key(modality), "MuData modality must be a single group name");
    require(matrix == "X" || (matrix.substr(0, 7) == "layers/" && child_key(matrix.substr(7))),
            "MuData matrix must name X or layers/NAME");
    require(!domain.empty() && domain.size() <= 128, "MuData requires an explicit value domain of 1–128 bytes");
    return {std::string(modality), std::string(matrix), std::string(domain),
            domain == "raw-counts" ? QPX_FORMAT_COUNTS_UINT64 : QPX_FORMAT_CONTINUOUS_FLOAT64};
}

inline void encoding(hid_t object, std::string_view kind, std::string_view version, qpx_reader_cancel_v1 cancel) {
    valid(hdf::string_attribute(object, "encoding-type", cancel) == kind &&
          hdf::string_attribute(object, "encoding-version", cancel) == version,
          "MuData selected object has an unsupported encoding or version");
}

inline hdf::NumericKind numeric_kind(hdf::Dataset& data) {
    hdf::Handle type(H5Dget_type(data));
    return hdf::numeric_type(type);
}

struct Entry {
    std::uint64_t logical, bits;
};

struct Source {
    hdf::ReadLease lease;
    hdf::ChunkWorkspace scratch;
    hdf::Handle file;
    hdf::Dataset observation_ids, feature_ids, dense;
    Options chosen;
    std::uint64_t observations = 0, features = 0;
    hdf::NumericKind dense_kind = hdf::NumericKind::Unsigned;
    std::vector<Entry> sparse;

    Source(const std::filesystem::path& path, Options options, std::uint64_t file_limit,
           std::size_t work, qpx_reader_cancel_v1 cancel)
        : lease(path, file_limit), file(hdf::open_file(path.u8string())), chosen(std::move(options)) {
        lease.verify_file(file);
        encoding(file, "MuData", "0.1.0", cancel);
        const auto base = "/mod/" + chosen.modality;
        auto modality = hdf::group(file, base);
        encoding(modality, "anndata", "0.1.0", cancel);
        observation_ids = axis(base + "/obs", observations, cancel);
        feature_ids = axis(base + "/var", features, cancel);
        if (chosen.matrix != "X") {
            auto layers = hdf::group(file, base + "/layers");
            encoding(layers, "dict", "0.1.0", cancel);
        }
        const auto matrix = base + "/" + chosen.matrix;
        hdf::hard_path(file, matrix);
        H5O_info2_t info{};
        hdf::status(H5Oget_info_by_name3(file, matrix.c_str(), &info, H5O_INFO_BASIC, H5P_DEFAULT),
                    "MuData cannot inspect selected matrix");
        if (info.type == H5O_TYPE_DATASET) {
            dense = hdf::Dataset(file, matrix, chunk_limit);
            encoding(dense, "array", "0.2.0", cancel);
            const auto& shape = dense.plan.dimensions;
            valid(shape.rank == 2 && shape.extent[0] == observations && shape.extent[1] == features,
                  "MuData selected dense matrix shape differs from modality axes");
            dense_kind = numeric_kind(dense);
        } else {
            valid(info.type == H5O_TYPE_GROUP, "MuData selected matrix must be a dataset or sparse group");
            auto group = hdf::group(file, matrix);
            index_sparse(group, matrix, work - fixed_work, cancel);
        }
    }

    hdf::Dataset axis(const std::string& key, std::uint64_t& extent, qpx_reader_cancel_v1 cancel) {
        auto group = hdf::group(file, key);
        encoding(group, "dataframe", "0.2.0", cancel);
        const auto index = hdf::string_attribute(group, "_index", cancel);
        valid(child_key(index), "MuData axis index is not a local dataset name");
        hdf::Dataset data(file, key + "/" + index, chunk_limit);
        encoding(data, "string-array", "0.2.0", cancel);
        hdf::Handle type(H5Dget_type(data));
        hdf::utf8_string_type(type);
        const auto& shape = data.plan.dimensions;
        valid(shape.rank == 1 && shape.extent[0] && shape.extent[0] <= axis_limit,
              "MuData selected axis must be a bounded nonempty string vector");
        extent = shape.extent[0];
        return data;
    }

    std::uint64_t value_bits(const hdf::Number& number) const {
        if (chosen.value_type == QPX_FORMAT_COUNTS_UINT64) return hdf::count(number);
        const auto value = hdf::continuous(number);
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    template<class Sink>
    void vector_block(hdf::Dataset& data, hdf::NumericKind kind, std::uint64_t first,
                      std::size_t length, Sink&& sink, qpx_reader_cancel_v1 cancel) {
        hdf::numeric_block(data, lease, scratch, kind, {first, 0}, {length, 1}, length,
                           std::forward<Sink>(sink), cancel);
    }

    void index_sparse(hid_t group, const std::string& path, std::size_t budget, qpx_reader_cancel_v1 cancel) {
        const auto kind = hdf::string_attribute(group, "encoding-type", cancel);
        valid((kind == "csr_matrix" || kind == "csc_matrix") &&
              hdf::string_attribute(group, "encoding-version", cancel) == "0.1.0",
              "MuData selected sparse encoding must be CSR or CSC 0.1.0");
        const bool csr = kind == "csr_matrix";
        {
            hdf::Handle attr(H5Aopen(group, "shape", H5P_DEFAULT)), space(H5Aget_space(attr)), type(H5Aget_type(attr));
            hsize_t length = 0;
            valid(H5Sget_simple_extent_ndims(space) == 1 && H5Sget_simple_extent_dims(space, &length, nullptr) == 1 &&
                  length == 2 && hdf::numeric_type(type) != hdf::NumericKind::Floating,
                  "MuData sparse shape must contain two integers");
            std::array<std::uint64_t, 2> shape{};
            hdf::status(H5Aread(attr, H5T_NATIVE_UINT64, shape.data()), "Cannot read MuData sparse shape");
            valid(shape == std::array<std::uint64_t, 2>{observations, features}, "MuData sparse shape differs from modality axes");
        }
        hdf::Dataset values(file, path + "/data", chunk_limit), indices(file, path + "/indices", chunk_limit),
                     pointers(file, path + "/indptr", chunk_limit);
        const auto major = csr ? observations : features;
        const auto minor = csr ? features : observations;
        const auto entries = values.plan.dimensions.extent[0];
        valid(values.plan.dimensions.rank == 1 && indices.plan.dimensions.rank == 1 &&
              pointers.plan.dimensions.rank == 1 && indices.plan.dimensions.extent[0] == entries &&
              pointers.plan.dimensions.extent[0] == major + 1 && entries <= observations * features,
              "MuData sparse vectors differ from their declared extents");
        const auto value_kind = numeric_kind(values), index_kind = numeric_kind(indices), pointer_kind = numeric_kind(pointers);
        valid(index_kind != hdf::NumericKind::Floating && pointer_kind != hdf::NumericKind::Floating,
              "MuData sparse indices and pointers must be integers");
        const auto pointers_bytes = (major + 1) * sizeof(std::uint64_t);
        require(pointers_bytes <= budget && entries <= (budget - pointers_bytes) / sizeof(Entry),
                "MuData sparse index exceeds requested work budget");
        std::vector<std::uint64_t> offsets(std::size_t(major + 1));
        for (std::uint64_t first = 0; first <= major; first += 4096) {
            const auto length = std::size_t(std::min<std::uint64_t>(4096, major + 1 - first));
            vector_block(pointers, pointer_kind, first, length, [&](std::size_t i, const hdf::Number& value) {
                const auto index = first + i;
                const auto offset = hdf::count(value);
                valid(offset <= entries && (!index ? offset == 0 : offset >= offsets[index - 1]),
                      "MuData sparse pointers are outside bounds or decreasing");
                offsets[index] = offset;
            }, cancel);
        }
        valid(offsets.back() == entries, "MuData final sparse pointer differs from entry count");
        sparse.resize(std::size_t(entries));
        std::uint64_t row = 0;
        for (std::uint64_t first = 0; first < entries; first += 4096) {
            const auto length = std::size_t(std::min<std::uint64_t>(4096, entries - first));
            vector_block(indices, index_kind, first, length, [&](std::size_t i, const hdf::Number& value) {
                const auto index = first + i;
                while (index >= offsets[row + 1]) {
                    if (!(row & 4095)) poll(cancel);
                    ++row;
                }
                const auto col = hdf::count(value);
                valid(col < minor, "MuData sparse index exceeds modality axis");
                sparse[index].logical = csr ? row * features + col : col * features + row;
            }, cancel);
            vector_block(values, value_kind, first, length, [&](std::size_t i, const hdf::Number& value) {
                sparse[first + i].bits = value_bits(value);
            }, cancel);
        }
        std::size_t comparisons = 0;
        std::sort(sparse.begin(), sparse.end(), [&](const Entry& left, const Entry& right) {
            if (!(comparisons++ & 4095)) poll(cancel);
            return left.logical < right.logical;
        });
        for (std::size_t i = 1; i < sparse.size(); ++i) {
            if (!(i & 4095)) poll(cancel);
            valid(sparse[i-1].logical != sparse[i].logical, "MuData sparse matrix has duplicate entries");
        }
    }
};
} // namespace qpx_mudata
