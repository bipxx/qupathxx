// SPDX-License-Identifier: AGPL-3.0-only
#include "reader_plugin_profile.hpp"
#include "support.hpp"
#include <array>
#include <cmath>
#include <qupathxx/ome_zarr.hpp>

namespace {
using namespace qpx_ngff;
using qpx::OmeZarrSource;
using qpx::RawPixelBuffer;
using qpx::RawRegionRequest;
using qpx::SampleType;
constexpr auto provider = "org.qupathxx.ngff-labels";
constexpr auto contract = "ngff04-zarr2-integral-labels/1";
struct LabelProperty {
  std::string properties;
  Json rgba = nullptr;
  bool color_seen = false, property_seen = false;
};
struct Handle {
  std::unique_ptr<OmeZarrSource> labels, reference;
  qpx::RawImageMetadata projected;
  Selection selected;
  std::string description;
  std::map<Code, std::array<std::uint8_t, 4>> colors;
  std::size_t allowance = 0, reserved = 0;
  void current(const std::atomic<bool> *c) const {
    labels->require_current(c);
    reference->require_current(c);
    checkpoint(c);
  }
};
std::uint32_t sample_code(SampleType t) {
  switch (t) {
  case SampleType::UInt8:
    return QPX_READER_UINT8;
  case SampleType::UInt16:
    return QPX_READER_UINT16;
  case SampleType::Int8:
    return QPX_READER_INT8;
  case SampleType::Int16:
    return QPX_READER_INT16;
  case SampleType::UInt32:
    return QPX_READER_UINT32;
  case SampleType::Int32:
    return QPX_READER_INT32;
  case SampleType::UInt64:
    return QPX_READER_UINT64;
  case SampleType::Int64:
    return QPX_READER_INT64;
  default:
    fail("NGFF label source requires an integral sample type",
         QPX_READER_UNSUPPORTED);
  }
}
std::string resolved_reference(const Selection &selection,
                               const std::string &value) {
  valid(!value.empty() && value.size() <= 4096 &&
            value.find_first_of("\\:\0", 0, 3) == std::string::npos,
        "NGFF image-label source must be a bounded relative within-store path");
  const auto p = fs::u8path(value);
  valid(!p.has_root_path() && !p.is_absolute(),
        "NGFF source reference cannot be absolute or external");
  auto joined = (fs::u8path(selection.label_group) / p).lexically_normal();
  while (joined != joined.root_path() && !joined.empty() &&
         joined.filename().empty())
    joined = joined.parent_path();
  auto key = joined.generic_u8string();
  if (key == ".")
    key.clear();
  valid(relative_key(key, true),
        "NGFF source reference escapes the selected store");
  valid(key == selection.reference_group,
        "NGFF actual/default reference differs from the explicit expected "
        "reference group");
  return key;
}
Json frame(const OmeZarrSource &source) {
  const auto &axes = source.axes();
  const auto &units = source.units();
  const auto &levels = source.levels();
  valid(!levels.empty() && axes.size() == units.size(),
        "Native NGFF frame is incomplete");
  const auto &l = levels[0];
  valid(l.shape.size() == axes.size() && l.scale.size() == axes.size() &&
            l.translation.size() == axes.size(),
        "Native NGFF frame ranks differ");
  Json a = Json::array();
  for (std::size_t i = 0; i < axes.size(); ++i) {
    valid(l.shape[i] > 0 && std::isfinite(l.scale[i]) && l.scale[i] > 0 &&
              std::isfinite(l.translation[i]),
          "Native NGFF frame is not finite");
    a.push_back({{"name", axes[i]},
                 {"unit", units[i]},
                 {"extent", std::to_string(l.shape[i])},
                 {"scale", l.scale[i]},
                 {"translation", l.translation[i]}});
  }
  return Json{{"axes", std::move(a)}};
}
void strict_group(const fs::path &root, const std::string &key,
                  const std::atomic<bool> *c) {
  (void)json(metadata_text(root, child(key, ".zgroup"), c), c);
}
void strict_array(const fs::path &root, const std::string &group,
                  const std::string &path, const std::atomic<bool> *c,
                  bool integral_labels) {
  checkpoint(c);
  valid(relative_key(path),
        "NGFF dataset path is not within its group");
  const auto spec =
      json(metadata_text(root, child(group, (path + "/.zarray").c_str()), c),
           c);
  if (integral_labels) {
    valid(spec.is_object() && spec.contains("fill_value"),
          "NGFF integral-label array lacks fill_value");
    const auto &fill = spec.at("fill_value");
    valid(fill.is_null() || fill.is_number_integer() || fill.is_number_unsigned(),
          "NGFF integral-label fill_value must be a JSON integer or null");
  }
}
void strict_arrays(const fs::path &root, const std::string &group,
                   const OmeZarrSource &source, const std::atomic<bool> *c,
                   bool integral_labels = false) {
  // The native service alone interprets dtype, layout and transforms. The
  // supplemental pass refuses duplicate/ambiguous JSON and non-integer label
  // fills before open succeeds or any raw decode can expose scientific output.
  for (const auto &level : source.levels()) {
    strict_array(root, group, level.path, c, integral_labels);
  }
}
// Charge the existing JSON containers before the fixed lexical arena is
// allocated. The per-node/map allowance includes allocator bookkeeping; string
// and array capacities charge their actual retained storage conservatively.
void charge_json(std::size_t &used, const Json &value, std::size_t work,
                 const std::atomic<bool> *c) {
  checkpoint(c);
  charge(used, sizeof(Json) + 64, work);
  if (value.is_object()) {
    charge(used, sizeof(Json::object_t), work);
    for (auto i = value.begin(); i != value.end(); ++i) {
      charge(used, 128 + i.key().capacity(), work);
      charge_json(used, i.value(), work, c);
    }
  } else if (value.is_array()) {
    const auto &a = value.get_ref<const Json::array_t &>();
    charge(used, sizeof(Json::array_t), work);
    charge(used, product(a.capacity(), sizeof(Json)), work);
    for (const auto &entry : a)
      charge_json(used, entry, work, c);
  } else if (value.is_string()) {
    const auto &s = value.get_ref<const Json::string_t &>();
    charge(used, s.capacity() + 1, work);
  }
}
std::map<Code, LabelProperty>
properties(const Json &image, const qpx_native_json::Tree &lexical,
           const std::atomic<bool> *c) {
  std::map<Code, LabelProperty> rows;
  if (image.contains("colors")) {
    const auto &colors = image.at("colors");
    valid(colors.is_array(), "NGFF label colors must be an array");
    for (const auto &entry : colors) {
      checkpoint(c);
      valid(entry.is_object() && entry.contains("label-value"),
            "NGFF label color lacks label-value");
      auto &row = rows[code(entry.at("label-value"))];
      valid(!row.color_seen, "Duplicate NGFF color label-value");
      row.color_seen = true;
      if (entry.contains("rgba")) {
        const auto &rgba = entry.at("rgba");
        valid(rgba.is_array() && rgba.size() == 4,
              "NGFF label RGBA must contain four UInt8 values");
        for (const auto &v : rgba)
          valid(integer(v) <= 255, "NGFF label RGBA is outside UInt8");
        row.rgba = rgba;
      }
    }
  }
  if (image.contains("properties")) {
    const auto &values = image.at("properties");
    valid(values.is_array(), "NGFF label properties must be an array");
    const auto &raw_image = lexical.get(lexical.root(), "image-label");
    const auto &raw_values =
        lexical.array(lexical.get(raw_image, "properties"));
    valid(raw_values.size() == values.size(),
          "NGFF semantic/lexical property arrays differ");
    std::size_t index = 0;
    for (const auto &entry : values) {
      checkpoint(c);
      valid(entry.is_object() && entry.contains("label-value"),
            "NGFF label properties lack label-value");
      auto &row = rows[code(entry.at("label-value"))];
      valid(!row.property_seen, "Duplicate NGFF property label-value");
      row.property_seen = true;
      const auto &raw_entry = lexical.nodes[raw_values[index++]];
      valid(raw_entry.kind == qpx_native_json::Kind::Object,
            "NGFF lexical property must be an object");
      // A property is opaque application data. Copy its complete original JSON
      // value instead of serializing nlohmann's numeric approximation. Array
      // order matches the semantic list; the exact ID check above is unchanged.
      row.properties.assign(raw_entry.raw.data(), raw_entry.raw.size());
    }
  }
  return rows;
}
std::unique_ptr<Handle> make_handle(const fs::path &root, Selection selected,
                                    std::size_t work,
                                    const std::atomic<bool> *c) {
  checkpoint(c);
  require(work >= 16 * mib && work <= 256 * mib,
          "NGFF labels require16..256MiB private decoder work");
  valid(qpx::has_ome_zarr(),
        "The matching native SDK does not provide OME-Zarr decoding");
  checked_path(root, selected.label_group, true);
  checked_path(root, selected.reference_group, true);
  strict_group(root, selected.label_group, c);
  strict_group(root, selected.reference_group, c);
  const auto label_text =
      metadata_text(root, child(selected.label_group, ".zattrs"), c);
  const auto reference_text =
      metadata_text(root, child(selected.reference_group, ".zattrs"), c);
  // Only label attributes contain opaque property values. Array/group JSON,
  // reference metadata and selection options retain strict integer fallback
  // refusal; interpreted color/property IDs still require integer Json values.
  auto label_attrs = json(label_text, c, true);
  auto reference_attrs = json(reference_text, c);
  valid(label_attrs.is_object() && reference_attrs.is_object(),
        "NGFF group attributes must be JSON objects");
  valid(!label_attrs.contains("source"),
        "Root-sibling NGFF source metadata is ambiguous; use "
        "image-label.source.image");
  valid(label_attrs.contains("image-label") &&
            label_attrs.at("image-label").is_object(),
        "Selected NGFF group is not an image-label group");
  const auto &image = label_attrs.at("image-label");
  valid(image.contains("version") && image.at("version") == "0.4",
        "NGFF label profile requires image-label.version0.4");
  std::string original = "../../", origin = "default";
  if (image.contains("source")) {
    const auto &source = image.at("source");
    valid(source.is_object(), "NGFF image-label.source must be an object");
    for (auto i = source.begin(); i != source.end(); ++i)
      valid(i.key() == "image", "Unsupported NGFF source-reference metadata");
    if (source.contains("image")) {
      original = string(source.at("image"));
      origin = "explicit";
    }
  }
  (void)resolved_reference(selected, original);
  std::map<Code, LabelProperty> rows;
  {
    constexpr std::size_t lexical_bytes = 8 * mib;
    std::size_t phase = 4 * mib;
    charge(phase, lexical_bytes, work);
    charge(phase, label_text.capacity() + reference_text.capacity() +
                      selected.label_group.capacity() +
                      selected.reference_group.capacity() + original.capacity() +
                      origin.capacity(),
           work);
    charge_json(phase, label_attrs, work, c);
    charge_json(phase, reference_attrs, work, c);
    // Each declaration can create at most one map row. Raw property objects
    // are disjoint spans of label_text, so their combined text fits that input.
    std::size_t declarations = 0;
    for (const auto name : {"colors", "properties"})
      if (image.contains(name))
        declarations += image.at(name).size();
    charge(phase, product(declarations, 256), work);
    charge(phase, product(label_text.capacity(), 2), work);
    qpx_binary::Arena arena(lexical_bytes);
    TreeCancel cancel{c};
    qpx_native_json::Tree lexical(label_text, &arena.memory, cancel.callback(),
                                 16384, 16384, 32);
    rows = properties(image, lexical, c);
  } // No lexical nodes/arena overlap the native source allocations below.
  qpx::OmeZarrOptions limits;
  limits.metadata_bytes = attrs_limit;
  limits.inventory_bytes = std::min<std::size_t>(4 * mib, work / 16);
  limits.chunk_bytes = std::min<std::size_t>(4 * mib, work / 16);
  limits.encoded_bytes = std::min<std::size_t>(4 * mib, work / 16);
  limits.max_files = 100000;
  auto h = std::make_unique<Handle>();
  h->allowance = work;
  h->selected = std::move(selected);
  h->labels = std::make_unique<OmeZarrSource>(
      checked_path(root, h->selected.label_group, true).u8string(), limits, c);
  h->reference = std::make_unique<OmeZarrSource>(
      checked_path(root, h->selected.reference_group, true).u8string(), limits,
      c);
  valid(h->labels->levels().size() == h->reference->levels().size(),
        "NGFF label/reference dataset counts differ");
  const auto &raw = h->labels->metadata();
  valid(raw.channels == 1 && raw.series == 0 && raw.series_count == 1,
        "NGFF labels must expose one integral label channel");
  (void)sample_code(raw.sample_type);
  valid(h->selected.z < raw.z_planes && h->selected.t < raw.timepoints,
        "Selected NGFF label Z/T is outside the source");
  strict_arrays(root, h->selected.label_group, *h->labels, c, true);
  strict_arrays(root, h->selected.reference_group, *h->reference, c);
  valid(metadata_text(root, child(h->selected.label_group, ".zattrs"), c) ==
                label_text &&
            metadata_text(root, child(h->selected.reference_group, ".zattrs"),
                          c) == reference_text,
        "NGFF attributes changed during native open");
  Json values = Json::array();
  for (const auto &[value, p] : rows) {
    checkpoint(c);
    values.push_back({{"code", value.decimal()},
                      {"properties", p.properties},
                      {"rgba", p.rgba}});
    if (!p.rgba.is_null()) {
      std::array<std::uint8_t, 4> rgba{};
      for (unsigned i = 0; i < 4; ++i)
        rgba[i] = std::uint8_t(integer(p.rgba[i]));
      h->colors.emplace(value, rgba);
    }
  }
  Json description = {
      {"schema", "qpx.label-mask-description/1"},
      {"format", "ome-ngff-labels-0.4-zarr2/1"},
      {"label_group", h->selected.label_group},
      {"reference_group", h->selected.reference_group},
      {"source_reference", {{"path", original}, {"origin", origin}}},
      {"level", 0},
      {"z", h->selected.z},
      {"t", h->selected.t},
      {"sample_type", qpx::sample_name(raw.sample_type)},
      {"label_frame", frame(*h->labels)},
      {"reference_frame", frame(*h->reference)},
      {"label_metadata", label_text},
      {"reference_metadata", reference_text},
      {"label_properties", std::move(values)}};
  h->description =
      description.dump(-1, ' ', false, Json::error_handler_t::strict);
  valid(!h->description.empty() && h->description.size() <= description_limit,
        "Complete NGFF label description exceeds65536 bytes");
  h->projected = raw;
  h->projected.channels = 1;
  h->projected.z_planes = 1;
  h->projected.timepoints = 1;
  h->projected.series = 0;
  h->projected.series_count = 1;
  h->projected.levels = {raw.levels.at(0)};
  h->projected.channel_names = {"Label"};
  h->projected.dimension_order = "YX";
  // Reserve fixed metadata/JSON/path/worker scratch, both retained native
  // services and simultaneous store re-observation. A read adds its own bounded
  // output/chunk workspace below; caller output memory is not plugin-owned.
  std::size_t used = 4 * mib;
  charge(used, h->labels->payload_bytes(), work);
  charge(used, h->reference->payload_bytes(), work);
  charge(used, 2 * limits.inventory_bytes, work);
  charge(used, h->projected.payload_bytes(), work);
  charge(used,
         h->description.capacity() + h->selected.label_group.capacity() +
             h->selected.reference_group.capacity(),
         work);
  charge(used, product(h->colors.size(), 128), work);
  h->reserved = used;
  h->current(c);
  return h;
}
Handle &handle(void *p) {
  require(p != nullptr, "Invalid NGFF label handle");
  return *static_cast<Handle *>(p);
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1 *out,
                                              qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out && out->struct_size >= sizeof(*out),
            "Invalid NGFF descriptor structure");
    out->abi_version = 1;
    out->capabilities = 27;
    bool ok = text(out->provider_id, provider);
    ok = text(out->display_name, "OME NGFF integral labels") && ok;
    ok = text(out->version, "1.0.0") && ok;
    ok = text(out->contract, contract) && ok;
    return ok ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_READER_CALL primary(const qpx_reader_open_v1 *r,
                                             qpx_reader_text_v1 *out,
                                             qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(r && out && r->struct_size >= sizeof(*r) && !r->reserved &&
                r->work_bytes >= 2 * mib,
            "Invalid NGFF store-primary request/work");
    return native(r->cancel, [&](const std::atomic<bool> *c) {
      const auto root = root_path(r->path);
      const auto selected = selection(r->options, c);
      const auto key = child(selected.label_group, ".zattrs");
      (void)checked_path(root, key);
      return text(*out, key) ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
    });
  });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1 *r,
                                          void **out,
                                          qpx_reader_text_v1 *error) {
  if (out)
    *out = nullptr;
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(r && out && r->struct_size >= sizeof(*r) && !r->reserved,
            "Invalid NGFF label open request");
    auto h = native(r->cancel, [&](const std::atomic<bool> *c) {
      const auto root = root_path(r->path);
      return make_handle(root, selection(r->options, c),
                         std::size_t(r->work_bytes), c);
    });
    *out = h.release();
    return QPX_READER_OK;
  });
}
void QPX_READER_CALL destroy(void *value) noexcept {
  delete static_cast<Handle *>(value);
}
qpx_reader_status_v1 QPX_READER_CALL metadata(void *p,
                                              qpx_reader_metadata_v1 *out,
                                              qpx_reader_cancel_v1 c,
                                              qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out && out->struct_size >= sizeof(*out),
            "Invalid NGFF metadata structure");
    auto &h = handle(p);
    native(c, [&](const std::atomic<bool> *stop) { h.current(stop); });
    const auto &m = h.projected;
    out->level_count = 1;
    out->width = m.width;
    out->height = m.height;
    out->has_raw = 1;
    out->channels = 1;
    out->sample_type = sample_code(m.sample_type);
    out->z_planes = out->timepoints = out->series_count = 1;
    out->series = 0;
    out->calibration_flags = 0;
    out->pixel_width_um = out->pixel_height_um = out->z_spacing_um =
        out->time_interval_seconds = 0;
    if (m.pixel_width_um) {
      out->calibration_flags |= QPX_READER_CALIBRATION_X;
      out->pixel_width_um = *m.pixel_width_um;
    }
    if (m.pixel_height_um) {
      out->calibration_flags |= QPX_READER_CALIBRATION_Y;
      out->pixel_height_um = *m.pixel_height_um;
    }
    if (m.z_spacing_um) {
      out->calibration_flags |= QPX_READER_CALIBRATION_Z;
      out->z_spacing_um = *m.z_spacing_um;
    }
    if (m.time_interval_seconds) {
      out->calibration_flags |= QPX_READER_CALIBRATION_TIME;
      out->time_interval_seconds = *m.time_interval_seconds;
    }
    bool ok = text(out->vendor, "OME NGFF integral labels");
    ok = text(out->dimension_order, m.dimension_order) && ok;
    return ok ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
qpx_reader_status_v1 QPX_READER_CALL level(void *p, std::uint32_t index,
                                           qpx_reader_level_v1 *out,
                                           qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out && out->struct_size >= sizeof(*out) && index == 0,
            "Invalid NGFF level request");
    auto &h = handle(p);
    out->reserved = 0;
    out->width = h.projected.width;
    out->height = h.projected.height;
    out->downsample = 1;
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void *p, std::uint32_t index,
                                                  qpx_reader_text_v1 *out,
                                                  qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    (void)handle(p);
    require(out && index == 0, "Invalid NGFF label channel-name request");
    return text(*out, "Label") ? QPX_READER_OK : QPX_READER_BUFFER_TOO_SMALL;
  });
}
RawRegionRequest region(const qpx_reader_region_v1 *r, bool rgba) {
  require(r && r->struct_size >= sizeof(*r) && r->level == 0 && r->z == 0 &&
              r->t == 0 && r->first_channel == 0 &&
              r->channels == (rgba ? 0u : 1u),
          "Invalid NGFF projected raw/preview selection");
  RawRegionRequest out;
  out.region = {r->x, r->y, 0, r->width, r->height};
  out.channels = 1;
  return out;
}
void output(qpx_reader_pixels_v1 *out, const RawRegionRequest &r,
            std::uint32_t type, std::size_t bytes, bool rgba) {
  require(out && out->struct_size >= sizeof(*out) && out->reserved == 0,
          "Invalid NGFF output structure");
  const auto channels = rgba ? 4u : 1u;
  const auto pixel = product(channels, bytes);
  const auto row = product(std::size_t(r.region.width), pixel);
  const auto n =
      product(std::size_t(r.region.width), std::size_t(r.region.height));
  require(out->sample_type == type && out->width == r.region.width &&
              out->height == r.region.height && out->channels == channels &&
              out->channel_stride == bytes && out->pixel_stride == pixel &&
              out->row_stride == row,
          "NGFF output type/shape/strides differ");
  require(out->data && out->capacity >= product(n, pixel),
          "NGFF output data capacity is insufficient");
  require(rgba ? (!out->validity && !out->validity_capacity)
               : (out->validity && out->validity_capacity >= n),
          "NGFF output validity capacity differs");
}
RawPixelBuffer read(Handle &h, RawRegionRequest request,
                    const std::atomic<bool> *c) {
  request.validate(h.projected);
  request.z = h.selected.z;
  request.t = h.selected.t;
  const auto own = h.labels->payload_bytes();
  require(own <= h.reserved, "NGFF native payload accounting changed");
  const auto other = h.reserved - own;
  require(other < h.allowance &&
              h.labels->read_work_bytes(request) <= h.allowance - other,
          "NGFF raw read exceeds remaining private allowance");
  h.current(c);
  auto raw = h.labels->read(request, h.allowance - other, c);
  h.current(c);
  valid(raw.width() == request.region.width &&
            raw.height() == request.region.height && raw.channels() == 1 &&
            raw.sample_type() == h.projected.sample_type,
        "Native NGFF read returned another lattice");
  return raw;
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void *p,
                                              const qpx_reader_region_v1 *r,
                                              qpx_reader_pixels_v1 *out,
                                              qpx_reader_cancel_v1 c,
                                              qpx_reader_text_v1 *error) {
  if (out) {
    out->written = 0;
    out->validity_written = 0;
  }
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &h = handle(p);
    const auto request = region(r, false);
    request.validate(h.projected);
    output(out, request, sample_code(h.projected.sample_type),
           qpx::sample_bytes(h.projected.sample_type), false);
    auto raw = native(c, [&](const std::atomic<bool> *stop) {
      return read(h, request, stop);
    });
    std::visit(
        [&](const auto &v) {
          const auto bytes = product(
              v.size(), sizeof(typename std::decay_t<decltype(v)>::value_type));
          std::memcpy(out->data, v.data(), bytes);
          out->written = bytes;
        },
        raw.storage());
    std::memcpy(out->validity, raw.valid().data(), raw.valid().size());
    out->validity_written = raw.valid().size();
    return QPX_READER_OK;
  });
}
std::array<std::uint8_t, 4> color(const Handle &h, Code value) {
  if (!value.magnitude)
    return {0, 0, 0, 0};
  const auto found = h.colors.find(value);
  if (found != h.colors.end())
    return found->second;
  std::uint64_t hash = UINT64_C(14695981039346656037);
  const auto add = [&](std::uint8_t b) {
    hash ^= b;
    hash *= UINT64_C(1099511628211);
  };
  add(value.negative ? 1 : 0);
  for (unsigned i = 0; i < 8; ++i)
    add(std::uint8_t(value.magnitude >> (8 * i)));
  return {std::uint8_t(64 + (hash & 127)),
          std::uint8_t(64 + ((hash >> 8) & 127)),
          std::uint8_t(64 + ((hash >> 16) & 127)), 255};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void *p,
                                               const qpx_reader_region_v1 *r,
                                               qpx_reader_pixels_v1 *out,
                                               qpx_reader_text_v1 *error) {
  if (out) {
    out->written = 0;
    out->validity_written = 0;
  }
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    auto &h = handle(p);
    const auto request = region(r, true);
    request.validate(h.projected);
    output(out, request, QPX_READER_UINT8, 1, true);
    // ABI1's RGBA callback has no cancellation argument. Keep this bounded,
    // synchronous and independent of the no-longer-live open callback.
    auto raw = read(h, request, nullptr);
    std::visit(
        [&](const auto &v) {
          using T = typename std::decay_t<decltype(v)>::value_type;
          if constexpr (std::is_integral_v<T>)
            for (std::size_t i = 0; i < v.size(); ++i) {
              auto rgba = raw.valid()[i]
                              ? color(h, code(v[i]))
                              : std::array<std::uint8_t, 4>{0, 0, 0, 0};
              if (!rgba[3])
                rgba = {0, 0, 0, 0};
              std::memcpy(out->data + 4 * i, rgba.data(), 4);
            }
          else
            fail("NGFF preview received a non-integral source",
                 QPX_READER_UNSUPPORTED);
        },
        raw.storage());
    out->written = product(raw.valid().size(), 4);
    return QPX_READER_OK;
  });
}
qpx_reader_status_v1 QPX_READER_CALL
label_description(void *p, qpx_reader_text_v1 *out, qpx_reader_cancel_v1 c,
                  qpx_reader_text_v1 *error) {
  if (out)
    out->size = 0;
  const auto status = boundary(error, [&]() -> qpx_reader_status_v1 {
    require(out != nullptr, "Missing NGFF description output");
    auto &h = handle(p);
    native(c, [&](const std::atomic<bool> *stop) { h.current(stop); });
    return text(*out, h.description) ? QPX_READER_OK
                                     : QPX_READER_BUFFER_TOO_SMALL;
  });
  if (out && status != QPX_READER_OK && status != QPX_READER_BUFFER_TOO_SMALL)
    out->size = 0;
  return status;
}
} // namespace
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_entry_v1(std::uint32_t abi, qpx_reader_api_v1 *out,
                    qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(abi == 1 && out && out->struct_size >= sizeof(*out),
            "NGFF reader ABI mismatch");
    *out = {sizeof(*out), 1,     describe,     open,      destroy,
            metadata,     level, channel_name, read_rgba, read_raw};
    return QPX_READER_OK;
  });
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_store_entry_v1(std::uint32_t abi, qpx_reader_store_api_v1 *out,
                          qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(abi == 1 && out && out->struct_size >= sizeof(*out),
            "NGFF store ABI mismatch");
    *out = {sizeof(*out), 1, primary};
    return QPX_READER_OK;
  });
}
extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL
qpx_reader_label_entry_v1(std::uint32_t abi, qpx_reader_label_api_v1 *out,
                          qpx_reader_text_v1 *error) {
  return boundary(error, [&]() -> qpx_reader_status_v1 {
    require(abi == 1 && out && out->struct_size >= sizeof(*out),
            "NGFF label-description ABI mismatch");
    *out = {sizeof(*out), 1, label_description};
    return QPX_READER_OK;
  });
}
