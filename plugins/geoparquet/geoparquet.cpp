// SPDX-License-Identifier: AGPL-3.0-only
#include "../format-common/json_tree.hpp"
#include "../format-common/parquet_file.hpp"
#include "../format-common/wkb_geometry.hpp"
#include "format_plugin_profile.hpp"
#include "geoarrow_geometry.hpp"
#include "../../cpp/include/qupathxx/detail/affine_numeric.hpp"
#include <arrow/table.h>
#include <arrow/util/key_value_metadata.h>
#include <parquet/arrow/reader.h>
#include <optional>
#include <set>

namespace {
using namespace qpx_wkb;
using namespace qpx_parquet;
namespace json = qpx_native_json;
constexpr std::size_t pool_limit = 4 * 1024 * 1024;

struct Options {
    std::string_view geometry, id, name, classification;
    std::string_view crs;
    std::array<double, 6> affine{};
};

std::string_view epsg_code(std::string_view code) {
    valid(!code.empty() && code.size() <= 9 && code.front() != '0' &&
              std::all_of(code.begin(), code.end(), [](char c) { return c >= '0' && c <= '9'; }),
          "Expected a positive canonical EPSG code");
    return code;
}

Options options(const json::Tree& tree, qpx_reader_cancel_v1 cancel) {
    const auto& root = tree.root();
    tree.keys(root, {"coordinate_space", "geometry", "id", "name", "class", "source_crs", "source_to_pixels"});
    const auto space = json::string(tree.get(root, "coordinate_space"));
    require(space == "image-pixels" || space == "projected-xy",
            "GeoParquet requires explicit image-pixels or projected-xy");

    auto name = [&](std::string_view key, bool required) -> std::string_view {
        const auto* field = tree.find(root, key);
        require(field || !required, "GeoParquet requires explicit geometry and ID columns");
        if (!field)
            return {};
        const auto value = json::string(*field);
        require(!value.empty() && value.size() <= 256, "Invalid GeoParquet column name length");
        utf8(value, cancel);
        return value;
    };
    Options out{name("geometry", true), name("id", true), name("name", false), name("class", false), {}};
    const std::array selected{out.geometry, out.id, out.name, out.classification};
    for (auto at = selected.begin(); at != selected.end(); ++at)
        require(at->empty() || std::find(selected.begin(), at, *at) == at, "GeoParquet columns must be distinct");
    const auto* crs = tree.find(root, "source_crs");
    const auto* affine = tree.find(root, "source_to_pixels");
    if (space == "image-pixels") {
        require(!crs && !affine, "Image-pixel GeoParquet cannot specify a source CRS or registration");
        return out;
    }
    require(crs && affine, "Projected GeoParquet requires source_crs and source_to_pixels");
    out.crs = json::string(*crs);
    require(out.crs.starts_with("EPSG:"), "Projected GeoParquet requires an EPSG source_crs");
    epsg_code(out.crs.substr(5));
    const auto& coefficients = tree.array(*affine);
    require(coefficients.size() == out.affine.size(), "GeoParquet affine requires six row-major coefficients");
    for (std::size_t i = 0; i < coefficients.size(); ++i)
        out.affine[i] = json::real(tree.nodes[coefficients[i]]);
    try {
        qpx::detail::affine_inverse_linear(out.affine);
    } catch (const std::invalid_argument&) {
        fail("GeoParquet affine must be invertible and well-conditioned", QPX_READER_INVALID_REQUEST);
    }
    return out;
}

struct GeometryMetadata {
    std::string_view version;
    unsigned types = 0;
    unsigned native_kind = 0;
    bool has_bbox = false;
    std::array<double, 4> bbox{};
};

std::string_view geo_entry(const File& file) {
    const auto& kv = file.key_value_metadata();
    std::string_view geo;
    if (kv) {
        for (int64_t i = 0; i < kv->size(); ++i) {
            if (kv->key(i) != "geo")
                continue;
            valid(geo.empty() && !kv->value(i).empty(), "GeoParquet must contain one nonempty geo metadata entry");
            geo = kv->value(i);
        }
    }
    valid(!geo.empty() && geo.size() <= 65536, "Missing or oversized GeoParquet geo metadata");
    return geo;
}

void check_crs(const json::Tree& tree, const json::Node* crs, std::string_view selected) {
    if (selected.empty()) {
        valid(crs && crs->kind == json::Kind::Null, "Image-pixel GeoParquet requires explicit null CRS");
        return;
    }
    valid(crs && crs->kind == json::Kind::Object &&
              json::string(tree.get(*crs, "type")) == "ProjectedCRS",
          "GeoParquet registration requires an explicit projected CRS");
    const auto& coordinates = tree.get(*crs, "coordinate_system");
    valid(json::string(tree.get(coordinates, "subtype")) == "Cartesian" &&
              tree.array(tree.get(coordinates, "axis")).size() == 2, "GeoParquet CRS must use two Cartesian axes");
    const auto& id = tree.get(*crs, "id");
    const auto& code = tree.get(id, "code");
    const auto declared = code.kind == json::Kind::String ? std::string_view(code.value) : code.raw;
    valid(json::string(tree.get(id, "authority")) == "EPSG" && epsg_code(declared) == selected.substr(5),
          "Selected GeoParquet source_crs differs from the declared EPSG");
}

GeometryMetadata geometry_metadata(const json::Tree& tree, const Options& options) {
    const auto& root = tree.root();
    const auto version = json::string(tree.get(root, "version"));
    valid(version == "1.0.0" || version == "1.1.0", "Expected GeoParquet version 1.0.0 or 1.1.0");
    const auto primary = json::string(tree.get(root, "primary_column"));
    const auto& columns = tree.get(root, "columns");
    valid(!primary.empty() && tree.find(columns, primary), "GeoParquet primary geometry has no column metadata");
    const auto& field = tree.get(columns, options.geometry);
    const auto encoding = json::string(tree.get(field, "encoding"));
    constexpr std::array<std::string_view, 6> native_encodings = {
        "point", "linestring", "polygon", "multipoint", "multilinestring", "multipolygon"};
    check_crs(tree, tree.find(field, "crs"), options.crs);
    const auto* edges = tree.find(field, "edges");
    valid((!edges || json::string(*edges) == "planar") && !tree.find(field, "epoch"),
          "Image-pixel GeoParquet requires planar edges and no coordinate epoch");
    const auto* orientation = tree.find(field, "orientation");
    valid(!orientation || json::string(*orientation) == "counterclockwise", "Invalid GeoParquet ring orientation");

    GeometryMetadata out{version};
    if (encoding != "WKB") {
        const auto format = std::find(native_encodings.begin(), native_encodings.end(), encoding);
        valid(version == "1.1.0" && format != native_encodings.end(),
              "Selected GeoParquet encoding requires WKB or a 1.1.0 separated XY layout");
        out.native_kind = static_cast<unsigned>(1 + (format - native_encodings.begin()));
    }
    constexpr std::array<std::string_view, 7> supported = {
        "Point", "LineString", "Polygon", "MultiPoint", "MultiLineString", "MultiPolygon", "GeometryCollection"};
    for (auto entry : tree.array(tree.get(field, "geometry_types"))) {
        const auto type = json::string(tree.nodes[entry]);
        const auto match = std::find(supported.begin(), supported.end(), type);
        valid(match != supported.end(), "GeoParquet geometry_types must contain basic XY types");
        const auto bit = 1u << (1 + (match - supported.begin()));
        valid(!(out.types & bit), "Duplicate GeoParquet geometry type");
        out.types |= bit;
    }
    valid(!out.native_kind || !out.types || out.types == (1u << out.native_kind),
          "GeoArrow encoding differs from the declared geometry_types");
    if (const auto* box = tree.find(field, "bbox")) {
        const auto& dimensions = tree.array(*box);
        valid(dimensions.size() == 4, "GeoParquet XY bbox must have four bounds");
        for (std::size_t i = 0; i < dimensions.size(); ++i)
            out.bbox[i] = json::real(tree.nodes[dimensions[i]]);
        valid((!options.crs.empty() || (out.bbox[0] >= 0 && out.bbox[1] >= 0)) &&
                  out.bbox[0] <= out.bbox[2] && out.bbox[1] <= out.bbox[3],
              "Invalid GeoParquet XY bbox");
        out.has_bbox = true;
    }
    return out;
}

struct Labels {
    std::string_view id;
    std::pmr::string name, classification;
    explicit Labels(std::pmr::memory_resource* memory) : name(memory), classification(memory) {}
};

struct GeometryImport {
    Handle& handle;
    const Options& selection;
    const GeometryMetadata& metadata;
    qpx_reader_cancel_v1 cancel;
    Bounds bounds;
    std::pmr::set<std::pmr::string, std::less<>> ids;
    std::string id_type;

    GeometryImport(Handle& h, const Options& selected, const GeometryMetadata& meta, qpx_reader_cancel_v1 stop)
        : handle(h), selection(selected), metadata(meta), cancel(stop), ids(&h.arena.memory) {}

    std::string_view id(std::string_view text) {
        const auto [found, inserted] = ids.emplace(text);
        valid(inserted, "GeoParquet source IDs are not unique");
        return *found;
    }

    void record_id_type(const NumericColumn& type) {
        id_type = integer_id_type(type);
    }

    void append(Node& node, std::string_view source_id, std::string_view name, std::string_view classification) {
        valid(!metadata.types || (metadata.types & (1u << node.type)),
              "Geometry type is absent from GeoParquet geometry_types");
        if (metadata.has_bbox)
            bounds.include(node, cancel);
        if (!selection.crs.empty()) {
            auto project = [&](Point& source) {
                const auto xy = qpx::detail::affine_map_xy(selection.affine, source.x, source.y);
                source = {xy[0], xy[1]};
            };
            try {
                visit_points(node, cancel, project);
            } catch (const std::invalid_argument&) {
                fail("GeoParquet affine coordinate overflow");
            }
        }
        Facts facts(&handle.arena.memory);
        fact(facts, "geoparquet.version", metadata.version);
        fact(facts, "geoparquet.geometry_column", selection.geometry);
        fact(facts, "geoparquet.source_id", source_id);
        if (!id_type.empty())
            fact(facts, "geoparquet.source_id_type", id_type);
        fact(facts, "geoparquet.crs", selection.crs.empty() ? "null" : selection.crs);
        const auto first = handle.rows.size();
        emit(handle, node, "", facts, name, cancel, source_id);
        for (auto i = first; i < handle.rows.size(); ++i)
            handle.rows[i].classification = classification;
    }
};

void decode_wkb(File& source, GeometryImport& output) {
    source.require_flat();
    const auto& selection = output.selection;
    const auto cancel = output.cancel;
    auto* memory = &output.handle.arena.memory;
    const auto& schema = source.schema();
    const auto geometry = column_index(schema, selection.geometry);
    const auto& column = *schema.Column(geometry);
    valid(column.physical_type() == parquet::Type::BYTE_ARRAY && column.logical_type()->is_none(),
          "GeoParquet WKB requires unannotated Parquet BYTE_ARRAY storage");
    auto text_index = [&](std::string_view label) {
        if (label.empty())
            return -1;
        const auto index = column_index(schema, label);
        valid(text_column(*schema.Column(index)), "GeoParquet name and class columns must be UTF-8");
        return index;
    };
    const int id = column_index(schema, selection.id), name = text_index(selection.name),
              classification = text_index(selection.classification);
    const auto integer = numeric_id_column(schema, id);
    if (integer)
        output.record_id_type(*integer);
    std::pmr::vector<Labels> labels(memory);
    source.groups(cancel, [&](std::size_t, const Group& group) {
        while (labels.size() < std::size_t(group.rows))
            labels.emplace_back(memory);
        auto add_id = [&](std::size_t row, std::string_view text) {
            labels[row].id = output.id(text);
        };
        if (integer)
            read_integer_ids(group, *integer, add_id);
        else
            group.strings(id, false, 256, add_id);
        if (name >= 0)
            group.strings(name, true, 4096, [&](std::size_t row, std::string_view text) { labels[row].name = text; });
        if (classification >= 0)
            group.strings(classification, true, 256, [&](std::size_t row, std::string_view text) {
                labels[row].classification = text;
            });
        group.read<parquet::ByteArrayType>(geometry, false, [&](std::size_t row, const parquet::ByteArray* value) {
            valid(value->ptr && value->len >= 5 && value->len <= 1024 * 1024, "Invalid GeoParquet WKB byte length");
            Wkb parser{{value->ptr, value->len}, 0, memory, {0, 0, cancel, !selection.crs.empty()}};
            auto node = parser.value();
            valid(parser.at == value->len, "Trailing GeoParquet WKB bytes");
            const auto& label = labels[row];
            output.append(node, label.id, label.name, label.classification);
        });
    });
}

struct NativeProjection {
    std::vector<int> columns;
    std::optional<NumericColumn> integer_id;
};

NativeProjection native_projection(const parquet::SchemaDescriptor& schema, const Options& selection) {
    NativeProjection projection;
    for (const auto name : {selection.geometry, selection.id, selection.name, selection.classification}) {
        if (name.empty())
            continue;
        const bool geometry = name == selection.geometry;
        int matches = 0;
        for (int i = 0; i < schema.group_node()->field_count(); ++i)
            matches += schema.group_node()->field(i)->name() == name;
        valid(matches == 1, "Selected GeoArrow root field is missing or ambiguous");
        std::size_t leaves = 0;
        for (int i = 0; i < schema.num_columns(); ++i) {
            const auto* column = schema.Column(i);
            const auto path = column->path();
            if (path->ToDotVector().front() != name)
                continue;
            ++leaves;
            valid(geometry || (!column->max_repetition_level() && path->ToDotVector().size() == 1),
                  "GeoArrow ID, name and class columns must be flat");
            if (name == selection.id) {
                projection.integer_id = numeric_id_column(schema, i);
                if (projection.integer_id)
                    continue;
            } else if (!geometry) {
                valid(text_column(*column), "GeoArrow name and class columns must be UTF-8");
            }
            projection.columns.push_back(i);
        }
        valid(leaves == (geometry ? 2 : 1), "GeoArrow column projection differs from its encoding");
    }
    return projection;
}

std::string_view text_at(const arrow::Array* input, int64_t row, std::size_t limit, bool optional,
                         qpx_reader_cancel_v1 cancel) {
    if (!input || input->IsNull(row)) {
        valid(optional, "GeoArrow has a missing or null ID");
        return {};
    }
    std::string_view value;
    if (input->type_id() == arrow::Type::STRING)
        value = static_cast<const arrow::StringArray&>(*input).GetView(row);
    else if (input->type_id() == arrow::Type::LARGE_STRING)
        value = static_cast<const arrow::LargeStringArray&>(*input).GetView(row);
    else
        fail("GeoArrow ID, name and class columns must use UTF-8 string arrays");
    valid((optional || !value.empty()) && value.size() <= limit, "Invalid GeoArrow text length");
    utf8(value, cancel);
    return value;
}

void decode_native(File& source, GeometryImport& output) {
    const auto& selected = output.selection;
    const unsigned kind = output.metadata.native_kind;
    const auto projection = native_projection(source.schema(), selected);
    auto* memory = &output.handle.arena.memory;
    std::pmr::vector<std::string_view> integer_ids(memory);
    if (projection.integer_id) {
        // Decode at physical width before Arrow can narrow a logical integer.
        output.record_id_type(*projection.integer_id);
        integer_ids.resize(source.rows());
        source.groups(output.cancel, [&](std::size_t offset, const Group& group) {
            read_integer_ids(group, *projection.integer_id, [&](std::size_t row, std::string_view text) {
                integer_ids[offset + row] = output.id(text);
            });
        });
    }
    parquet::ArrowReaderProperties properties;
    properties.set_use_threads(false);
    auto opened = parquet::arrow::FileReader::Make(source.pool(), source.take_reader(), properties);
    valid(opened.ok(), "Could not create the GeoArrow projection reader");
    auto reader = std::move(opened).ValueOrDie();
    std::size_t total = 0;
    for (int group = 0; group < source.group_count(); ++group) {
        poll(output.cancel);
        const auto meta = source.projection(group, projection.columns);
        valid(std::uint64_t(meta->num_rows()) <= source.rows() - total, "GeoArrow row counts differ from the footer");
        auto read = reader->ReadRowGroup(group, projection.columns);
        valid(read.ok(), "GeoArrow row group decoding failed");
        auto table = std::move(read).ValueOrDie();
        valid(table->num_rows() == meta->num_rows() && table->ValidateFull().ok(),
              "Invalid GeoArrow row group arrays");
        const auto geometry_index = table->schema()->GetFieldIndex(std::string(selected.geometry));
        valid(geometry_index >= 0, "GeoArrow geometry is missing from the projected group");
        qpx_geoarrow::check_type(*table->field(geometry_index)->type(), qpx_geoarrow::list_depth(kind));
        const auto id_index = table->schema()->GetFieldIndex(std::string(selected.id));
        const auto name_index = table->schema()->GetFieldIndex(std::string(selected.name));
        const auto class_index = table->schema()->GetFieldIndex(std::string(selected.classification));
        valid((projection.integer_id || id_index >= 0) && (selected.name.empty() || name_index >= 0) &&
                  (selected.classification.empty() || class_index >= 0), "GeoArrow labels are missing from the projection");
        arrow::TableBatchReader batches(std::move(table));
        std::size_t offset = total;
        for (;;) {
            std::shared_ptr<arrow::RecordBatch> batch;
            valid(batches.ReadNext(&batch).ok(), "Could not read GeoArrow row group batch");
            if (!batch)
                break;
            const auto& geometry = *batch->column(geometry_index);
            const auto ids = id_index >= 0 ? batch->column(id_index) : nullptr;
            const auto name = name_index >= 0 ? batch->column(name_index) : nullptr;
            const auto classification = class_index >= 0 ? batch->column(class_index) : nullptr;
            for (int64_t row = 0; row < batch->num_rows(); ++row) {
                poll(output.cancel);
                const auto id = projection.integer_id
                    ? integer_ids.at(offset + std::size_t(row))
                    : output.id(text_at(ids.get(), row, 256, false, output.cancel));
                Limits limits{0, 0, output.cancel, !selected.crs.empty()};
                auto node = qpx_geoarrow::geometry(geometry, row, kind, memory, limits);
                output.append(node, id, text_at(name.get(), row, 4096, true, output.cancel),
                              text_at(classification.get(), row, 256, true, output.cancel));
            }
            offset += std::size_t(batch->num_rows());
        }
        total += static_cast<std::size_t>(meta->num_rows());
    }
    valid(total == source.rows(), "GeoArrow row counts differ from the footer");
}

void decode_geometries(Handle& handle, Bytes bytes, const Options& selection, qpx_reader_cancel_v1 cancel) {
    File source(bytes, pool_limit, File::Layout::Nested);
    const json::Tree tree(geo_entry(source), &handle.arena.memory, cancel, 128, 8192, 16);
    const auto metadata = geometry_metadata(tree, selection);
    GeometryImport output(handle, selection, metadata, cancel);
    if (metadata.native_kind)
        decode_native(source, output);
    else
        decode_wkb(source, output);
    valid(!metadata.has_bbox || output.bounds.xy() == metadata.bbox, "GeoParquet bbox differs from decoded XY bounds");
}

inline constexpr char provider_version[] = QPX_GEOPARQUET_VERSION;
struct Definition {
    static constexpr const char *id = "org.qupathxx.geoparquet", *format = "geoparquet",
        *name = "GeoParquet image-pixel annotations", *contract = QPX_GEOPARQUET_CONTRACT,
        *profile = QPX_FORMAT_PLUGIN_PROFILE_ID;
    static void validate_options(qpx_reader_string_v1 options) {
        require(options.size && options.size <= 4096, "GeoParquet requires explicit columns and image-pixel options");
    }
    static void decode(Handle& handle, Bytes file, const qpx_reader_open_v1& request) {
        const json::Tree tree({request.options.data, std::size_t(request.options.size)},
                              &handle.arena.memory, request.cancel, 7, 14, 3);
        try {
            decode_geometries(handle, file, options(tree, request.cancel), request.cancel);
        } catch (const parquet::ParquetException&) {
            fail("GeoParquet page or metadata decoding failed");
        }
    }
};
} // namespace

extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi, qpx_annotation_format_api_v3* out,
                               qpx_reader_text_v1* error) {
    using Plugin = qpx_annotation_source::Plugin<Definition, 3,
        provider_version, 2 * pool_limit>;
    return Plugin::entry(abi, out, error);
}
