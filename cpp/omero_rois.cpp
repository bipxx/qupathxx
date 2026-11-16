// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/omero_rois.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "omero_io.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include "omero_mask.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
namespace qpx {
namespace {
using Json = nlohmann::json;
constexpr std::size_t max_bytes = 16u * 1024 * 1024, max_shapes = 10000;
std::string hash(const std::string& s) {
    Sha256 h;
    h.update(s.data(), s.size());
    return h.finish();
}
Json parse(const std::string& s) {
    if (s.size() > max_bytes)
        throw std::length_error("OMERO ROI exchange exceeds16MiB");
    detail::StrictJson sax("OMERO ROI JSON");
    Json::sax_parse(s.begin(), s.end(), &sax);
    return Json::parse(s);
}
std::uint64_t integer(const Json& j, std::uint64_t max = INT64_MAX) {
    std::uint64_t n;
    if (j.is_number_unsigned())
        n = j.get<std::uint64_t>();
    else if (j.is_number_integer() && j.get<std::int64_t>() >= 0)
        n = std::uint64_t(j.get<std::int64_t>());
    else
        throw std::invalid_argument("OMERO ROI requires an exact nonnegative integer");
    if (n > max)
        throw std::invalid_argument("OMERO ROI integer is out of range");
    return n;
}
std::string text(const Json& j, std::size_t n = 4096) {
    if (!j.is_string())
        throw std::invalid_argument("OMERO ROI text has invalid type");
    auto s = j.get<std::string>();
    detail::bounded_utf8(s, n);
    if (s.find('\0') != std::string::npos)
        throw std::invalid_argument("OMERO ROI text contains NUL");
    return s;
}
double real(const Json& j) {
    if (!j.is_number())
        throw std::invalid_argument("OMERO coordinate must be numeric");
    const auto x = j.get<double>();
    if (!std::isfinite(x))
        throw std::invalid_argument("OMERO coordinate must be finite");
    return x;
}
std::string kind(const Json& j) {
    const auto t = text(j.at("@type"), 256);
    const std::string prefix = "http://www.openmicroscopy.org/Schemas/OME/2016-06#";
    if (t.rfind(prefix, 0) != 0)
        throw std::invalid_argument("Unknown OMERO ROI schema");
    return t.substr(prefix.size());
}
Ring points(const std::string& s) {
    Ring out;
    const char* p = s.data();
    const auto end = p + s.size();
    while (p != end) {
        while (p != end && *p == ' ')
            ++p;
        if (p == end)
            break;
        double x, y;
        auto a = std::from_chars(p, end, x);
        if (a.ec != std::errc{} || a.ptr == end || *a.ptr != ',')
            throw std::invalid_argument("Invalid OMERO points syntax");
        auto b = std::from_chars(a.ptr + 1, end, y);
        if (b.ec != std::errc{} || (b.ptr != end && *b.ptr != ' ') || !std::isfinite(x) ||
            !std::isfinite(y))
            throw std::invalid_argument("Invalid OMERO points syntax");
        out.emplace_back(x, y);
        if (out.size() > 100000)
            throw std::length_error("OMERO shape vertex limit");
        p = b.ptr;
    }
    return out;
}
void transform(const Json& j) {
    if (!j.contains("Transform") || j.at("Transform").is_null())
        return;
    const auto& t = j.at("Transform");
    for (const auto& p : {std::make_pair("A00", 1.),
                          {"A01", 0.},
                          {"A02", 0.},
                          {"A10", 0.},
                          {"A11", 1.},
                          {"A12", 0.}})
        if (real(t.at(p.first)) != p.second)
            throw std::invalid_argument(
                "OMERO nonidentity affine transforms require an explicit geometry conversion");
}
Roi geometry(const Json& j) {
    transform(j);
    auto n = [&](const char* key) { return real(j.at(key)); };
    const auto k = kind(j);
    if (k == "Rectangle")
        return Roi::rectangle({n("X"), n("Y"), n("Width"), n("Height")});
    if (k == "Ellipse") {
        const auto x = n("X"), y = n("Y"), rx = n("RadiusX"), ry = n("RadiusY");
        if (rx <= 0 || ry <= 0)
            throw std::invalid_argument("OMERO ellipse radii must be positive");
        return Roi::ellipse({x - rx, y - ry, 2 * rx, 2 * ry});
    }
    if (k == "Point" || k == "Label")
        return Roi::points({{n("X"), n("Y")}});
    if (k == "Line")
        return Roi::polyline({{n("X1"), n("Y1")}, {n("X2"), n("Y2")}});
    if (k == "Polygon")
        return Roi::polygon(points(text(j.at("Points"), 1024 * 1024)));
    if (k == "Polyline")
        return Roi::polyline(points(text(j.at("Points"), 1024 * 1024)));
    throw std::invalid_argument("Unsupported OMERO shape: " + k + "; no geometry was dropped");
}
std::optional<std::uint32_t> plane(const Json& j, const char* key, std::uint32_t count) {
    if (!j.contains(key) || j.at(key).is_null())
        return {};
    return std::uint32_t(integer(j.at(key), count - 1));
}
Json binding(const std::string& server, const OmeroImageInfo& i) {
    return {{"schema", "qpx.omero-coordinate-frame/1"},
            {"representation", "original-image-pixels"},
            {"server", server},
            {"image", i.id},
            {"pixels", i.pixels_id},
            {"width", i.width},
            {"height", i.height},
            {"channels", i.channels},
            {"z_planes", i.z_planes},
            {"timepoints", i.timepoints},
            {"metadata_sha256", i.metadata_sha256}};
}
// A geometry-only frame: deliberately has no rendered or quantitative pixels,
// calibration, raw view or file identity. Existing interchange handles geometry.
class Frame final : public ImageServer {
    ImageMetadata info;

  public:
    explicit Frame(const OmeroImageInfo& i) {
        info.width = std::int64_t(i.width);
        info.height = std::int64_t(i.height);
        info.levels = {{info.width, info.height, 1}};
        info.vendor = "OMERO original-coordinate exchange frame; no pixels";
    }
    const ImageMetadata& metadata() const override {
        return info;
    }
    PixelBuffer read_region(const RegionRequest&) const override {
        throw std::logic_error("Coordinate exchange frame has no pixels");
    }
    void close() override {}
    bool closed() const override {
        return false;
    }
};
std::string point_string(const Ring& p) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(17);
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (i)
            s << ' ';
        s << p[i].x << ',' << p[i].y;
    }
    return s.str();
}
Json edited_shape(const Roi& roi, Json shape) {
    const auto previous = geometry(shape);
    if (previous.equals(roi))
        return shape;
    const auto old_kind = kind(shape);
    const auto b = roi.bounds();
    for (const auto* key : {"X", "Y", "Width", "Height", "RadiusX", "RadiusY", "X1", "Y1", "X2",
                            "Y2", "Points", "Transform"})
        shape.erase(key);
    std::string k;
    if (roi.kind() == RoiKind::Rectangle) {
        k = "Rectangle";
        shape["X"] = b.x;
        shape["Y"] = b.y;
        shape["Width"] = b.width;
        shape["Height"] = b.height;
    } else if (roi.kind() == RoiKind::Ellipse) {
        k = "Ellipse";
        shape["X"] = b.x + b.width / 2;
        shape["Y"] = b.y + b.height / 2;
        shape["RadiusX"] = b.width / 2;
        shape["RadiusY"] = b.height / 2;
    } else if (roi.kind() == RoiKind::Points) {
        const auto p = roi.paths().at(0);
        if (p.size() != 1)
            throw std::invalid_argument("OMERO point exchange requires one point per shape");
        k = old_kind == "Label" ? "Label" : "Point";
        shape["X"] = p[0].x;
        shape["Y"] = p[0].y;
    } else if (roi.kind() == RoiKind::Polyline) {
        const auto p = roi.paths().at(0);
        if (old_kind == "Line" && p.size() == 2) {
            k = "Line";
            shape["X1"] = p[0].x;
            shape["Y1"] = p[0].y;
            shape["X2"] = p[1].x;
            shape["Y2"] = p[1].y;
        } else {
            k = "Polyline";
            shape["Points"] = point_string(p);
        }
    } else if (roi.kind() == RoiKind::Polygon) {
        const auto p = roi.polygon_parts();
        if (p.size() != 1 || !p[0].holes.empty())
            throw std::invalid_argument("OMERO polygon exchange cannot discard holes/parts");
        k = "Polygon";
        shape["Points"] = point_string(p[0].shell);
    } else
        throw std::invalid_argument("OMERO exchange cannot discard multi-part geometry");
    shape["@type"] = "http://www.openmicroscopy.org/Schemas/OME/2016-06#" + k;
    if (!geometry(shape).equals(roi))
        throw std::invalid_argument("Edited geometry is not exactly representable by OMERO shape");
    return shape;
}
} // namespace
struct OmeroRois::Data {
    OmeroImageInfo image;
    OmeroRoiOptions options;
    std::string server, sha;
    std::vector<OmeroShape> shapes;
};
OmeroRois::OmeroRois(std::shared_ptr<const Data> d) : data_(std::move(d)) {}
const OmeroImageInfo& OmeroRois::image() const {
    return data_->image;
}
const std::string& OmeroRois::server() const {
    return data_->server;
}
const std::vector<OmeroShape>& OmeroRois::shapes() const {
    return data_->shapes;
}
const std::string& OmeroRois::sha256() const {
    return data_->sha;
}
const OmeroRoiOptions& OmeroRois::options() const {
    return data_->options;
}
std::shared_ptr<const OmeroRois> OmeroClient::rois(std::uint64_t id, OmeroRoiOptions options,
                                                   const std::atomic<bool>* cancelled) {
    if (!options.max_mask_pixels || options.max_mask_pixels > 16u * 1024 * 1024 ||
        options.mask_bytes < 1024 || options.mask_bytes > 64u * 1024 * 1024)
        throw std::invalid_argument("Invalid OMERO mask resource limits");
    auto data = std::make_shared<OmeroRois::Data>();
    data->image = image(id, cancelled);
    data->options = options;
    data->server = server();
    if (options.z && *options.z >= data->image.z_planes ||
        options.t && *options.t >= data->image.timepoints)
        throw std::invalid_argument("Selected OMERO ROI plane is unavailable");
    std::set<std::uint64_t> ids, shape_ids;
    Json pages = Json::array();
    std::uint64_t offset = 0, total = 0;
    std::size_t bytes = 0, mask_pixels = 0, mask_bytes = 0;
    do {
        const auto b = json_get("api/v0/m/images/" + std::to_string(id) +
                                    "/rois/?offset=" + std::to_string(offset) + "&limit=200",
                                cancelled);
        bytes += b.size();
        if (bytes > max_bytes)
            throw std::length_error("OMERO ROI response exceeds16MiB");
        const auto page = parse(std::string(b.begin(), b.end()));
        const auto& rows = page.at("data");
        const auto& meta = page.at("meta");
        const auto observed = integer(meta.at("totalCount"), max_shapes);
        if (!offset)
            total = observed;
        else if (total != observed)
            throw WorkflowInputChanged("OMERO ROI count changed during pagination");
        if (!rows.is_array() || rows.size() > 200 || integer(meta.at("offset")) != offset ||
            integer(meta.at("limit"), 200) < rows.size() || offset > total ||
            rows.size() > total - offset || (rows.empty() && offset < total))
            throw std::invalid_argument("Invalid OMERO ROI pagination");
        for (const auto& row : rows) {
            workflow_checkpoint(cancelled);
            const auto rid = integer(row.at("@id"));
            if (!rid || kind(row) != "ROI" || !ids.insert(rid).second)
                throw std::invalid_argument("Invalid/duplicate OMERO ROI identity");
            if (!row.at("shapes").is_array())
                throw std::invalid_argument("OMERO ROI shapes must be an array");
            for (const auto& shape : row.at("shapes")) {
                workflow_checkpoint(cancelled);
                const auto sid = integer(shape.at("@id"));
                if (!sid || !shape_ids.insert(sid).second || shape_ids.size() > max_shapes)
                    throw std::invalid_argument("Invalid/duplicate/excess OMERO shape identity");
                OmeroShape s;
                s.roi_id = rid;
                s.shape_id = sid;
                s.c = plane(shape, "TheC", data->image.channels);
                s.z = plane(shape, "TheZ", data->image.z_planes);
                s.t = plane(shape, "TheT", data->image.timepoints);
                if (options.z && s.z && options.z != s.z || options.t && s.t && options.t != s.t)
                    continue;
                s.source_json = shape.dump();
                if (kind(shape) == "Mask") {
                    transform(shape);
                    const auto pixels = detail::omero_mask_pixels(shape, data->image);
                    if (pixels > options.max_mask_pixels - mask_pixels)
                        throw std::length_error(
                            "OMERO selected masks exceed pixel budget; select Z/T explicitly");
                    mask_pixels += pixels;
                    if (mask_bytes == options.mask_bytes)
                        throw std::length_error("OMERO mask transfer exceeds byte budget");
                    const auto png = mask_get(
                        sid,
                        std::min<std::size_t>(16u * 1024 * 1024, options.mask_bytes - mask_bytes),
                        cancelled);
                    mask_bytes += png.size();
                    detail::decode_omero_mask(s, data->image, png, cancelled);
                } else
                    s.geometry = geometry(shape);
                data->shapes.push_back(std::move(s));
            }
        }
        pages.push_back(page);
        offset += rows.size();
    } while (offset < total);
    // Repeat the bounded ROI pages to catch observed geometry changes too.
    offset = 0;
    for (const auto& old : pages) {
        const auto b = json_get("api/v0/m/images/" + std::to_string(id) +
                                    "/rois/?offset=" + std::to_string(offset) + "&limit=200",
                                cancelled);
        if (parse(std::string(b.begin(), b.end())) != old)
            throw WorkflowInputChanged("OMERO ROI geometry changed while retrieving collection");
        offset += old.at("data").size();
    }
    Json masks = Json::object();
    for (const auto& shape : data->shapes)
        if (!shape.mask_bits.empty()) {
            if (mask_bytes == options.mask_bytes)
                throw std::length_error("OMERO mask stability check exceeds byte budget");
            const auto png =
                mask_get(shape.shape_id,
                         std::min<std::size_t>(16u * 1024 * 1024, options.mask_bytes - mask_bytes),
                         cancelled);
            mask_bytes += png.size();
            if (hash(std::string(png.begin(), png.end())) != shape.mask_png_sha256)
                throw WorkflowInputChanged("OMERO mask pixels changed while retrieving collection");
            masks[std::to_string(shape.shape_id)] = detail::omero_mask_value(shape);
        }
    if (image(id, cancelled).metadata_sha256 != data->image.metadata_sha256)
        throw WorkflowInputChanged("OMERO image changed while retrieving ROIs");
    data->sha = hash(Json{{"source", binding(server(), data->image)},
                          {"pages", pages},
                          {"masks", masks},
                          {"z", options.z ? Json(*options.z) : Json(nullptr)},
                          {"t", options.t ? Json(*options.t) : Json(nullptr)}}
                         .dump());
    return std::shared_ptr<const OmeroRois>(new OmeroRois(std::move(data)));
}
std::string omero_rois_geojson(const OmeroRois& source) {
    Document frame(std::make_shared<Frame>(source.image()));
    std::vector<Annotation> objects;
    Json originals = Json::object();
    for (const auto& s : source.shapes()) {
        const auto j = parse(s.source_json);
        Annotation a{s.shape_id,
                     j.contains("Text") && !j.at("Text").is_null()
                         ? text(j.at("Text"))
                         : std::string("OMERO shape ") + std::to_string(s.shape_id),
                     s.geometry};
        a.properties = std::make_shared<const ObjectProperties>(
            std::map<std::string, std::string>{{"qpx.omero.roi", std::to_string(s.roi_id)},
                                               {"qpx.omero.shape", std::to_string(s.shape_id)}},
            std::map<std::string, double>{}, j.value("Locked", false));
        objects.push_back(std::move(a));
        originals[std::to_string(s.shape_id)] = {{"source", j},
                                                 {"mask", detail::omero_mask_value(s)}};
    }
    frame.append_objects(objects, {}, "Import original-coordinate OMERO ROIs");
    auto report = export_geojson(frame, {}, true);
    auto j = parse(report.text);
    j["omero"] = binding(source.server(), source.image());
    j["omero"]["rois_sha256"] = source.sha256();
    j["omero"]["selected_z"] = source.options().z ? Json(*source.options().z) : Json(nullptr);
    j["omero"]["selected_t"] = source.options().t ? Json(*source.options().t) : Json(nullptr);
    j["omero"]["plane_contract"] = "OMERO TheC/TheZ/TheT retained in original shapes; omitted/null "
                                   "means all planes; GeoJSON is an XY projection";
    j["omero_source_shapes"] = std::move(originals);
    const auto out = j.dump();
    if (out.size() > max_bytes)
        throw std::length_error("OMERO GeoJSON exceeds16MiB");
    return out;
}
std::string omero_rois_exchange(const std::string& geojson, const OmeroRois& current) {
    const auto& expected = current.image();
    auto root = parse(geojson);
    auto source = root.at("omero");
    const auto frame_binding = binding(current.server(), expected);
    for (const auto& item : frame_binding.items())
        if (!source.contains(item.key()) || source.at(item.key()) != item.value())
            throw std::invalid_argument(
                "OMERO ROI source/frame differs from the selected original image");
    if (source.at("rois_sha256") != current.sha256())
        throw WorkflowInputChanged(
            "OMERO ROI collection differs from the edited file's exact source");
    if (source.at("selected_z") !=
            (current.options().z ? Json(*current.options().z) : Json(nullptr)) ||
        source.at("selected_t") !=
            (current.options().t ? Json(*current.options().t) : Json(nullptr)))
        throw std::invalid_argument("OMERO ROI plane selection was altered");
    std::map<std::uint64_t, const OmeroShape*> originals;
    for (const auto& s : current.shapes())
        originals.emplace(s.shape_id, &s);
    Document frame(std::make_shared<Frame>(expected));
    auto native = root;
    native.erase("omero");
    native.erase("omero_source_shapes");
    const auto imported = import_geojson(frame, native.dump(), false, false);
    if (imported.features > max_shapes)
        throw std::length_error("OMERO ROI exchange shape count exceeds10000");
    // Compare edits in the same native interchange projection. GeoJSON chooses
    // ring winding; the original OMERO point ordering/raster bytes stay exact.
    auto baseline = parse(omero_rois_geojson(current));
    baseline.erase("omero");
    baseline.erase("omero_source_shapes");
    Document baseline_frame(std::make_shared<Frame>(expected));
    import_geojson(baseline_frame, baseline.dump(), false, false);
    std::map<std::string, Roi> projected;
    for (const auto& p : baseline_frame.objects())
        projected.emplace(p.second.properties->metadata().at("qpx.omero.shape"), p.second.geometry);
    std::map<std::uint64_t, Json> rois;
    std::set<std::uint64_t> shape_ids;
    Json results = Json::array(), masks = Json::array();
    for (const auto& p : frame.objects()) {
        const auto& a = p.second;
        if (a.parent_id || a.nucleus || a.kind != ObjectKind::Annotation)
            throw std::invalid_argument(
                "OMERO exchange cannot discard hierarchy/nuclei/object kind");
        if (!a.properties)
            throw std::invalid_argument("OMERO source shape identity is missing");
        const auto& m = a.properties->metadata();
        const auto sid = integer(parse(m.at("qpx.omero.shape")));
        auto shape = root.at("omero_source_shapes").at(std::to_string(sid)).at("source");
        const auto rid = integer(parse(m.at("qpx.omero.roi")));
        if (!rid || !sid || integer(shape.at("@id")) != sid || !shape_ids.insert(sid).second)
            throw std::invalid_argument("OMERO shape/ROI identity is invalid or duplicated");
        const auto found = originals.find(sid);
        if (found == originals.end() || found->second->roi_id != rid ||
            parse(found->second->source_json) != shape)
            throw std::invalid_argument("OMERO original shape/ROI identity or facts were altered");
        plane(shape, "TheC", expected.channels);
        plane(shape, "TheZ", expected.z_planes);
        plane(shape, "TheT", expected.timepoints);
        const auto unchanged = a.geometry.equals(projected.at(std::to_string(sid)));
        if (!found->second->mask_bits.empty()) {
            const auto mask = detail::omero_mask_value(*found->second);
            if (root.at("omero_source_shapes").at(std::to_string(sid)).at("mask") != mask ||
                !unchanged)
                throw std::invalid_argument(
                    "OMERO mask raster/geometry was changed; explicit raster editing is required");
            masks.push_back({{"roi", rid}, {"shape", sid}, {"raster", mask}});
        } else if (!unchanged)
            shape = edited_shape(a.geometry, std::move(shape));
        const auto old_name = shape.contains("Text") && !shape.at("Text").is_null()
                                  ? text(shape.at("Text"))
                                  : std::string("OMERO shape ") + std::to_string(sid);
        if (a.name != old_name)
            shape["Text"] = a.name;
        if (shape.contains("Locked") || a.locked())
            shape["Locked"] = a.locked();
        if (const auto rgb = a.color_rgb()) {
            const auto alpha = shape.contains("StrokeColor")
                                   ? detail::omero_rgba_int(shape.at("StrokeColor")) & 255u
                                   : 255u;
            const auto packed = (*rgb << 8) | alpha;
            shape["StrokeColor"] = packed > INT32_MAX ? std::int64_t(packed) - INT64_C(4294967296)
                                                      : std::int64_t(packed);
        }
        if (!rois.count(rid))
            rois.emplace(rid,
                         Json{{"@id", rid},
                              {"@type", "http://www.openmicroscopy.org/Schemas/OME/2016-06#ROI"},
                              {"shapes", Json::array()}});
        rois.at(rid)["shapes"].push_back(shape);
        Json measurements = Json::object();
        for (const auto& v : a.properties->measurements())
            measurements[v.first] = std::isfinite(v.second) ? Json(v.second)
                                    : std::isnan(v.second)
                                        ? Json("NaN")
                                        : Json(v.second > 0 ? "Infinity" : "-Infinity");
        results.push_back({{"roi", rid},
                           {"shape", sid},
                           {"name", a.name},
                           {"class", a.class_name},
                           {"measurements", measurements},
                           {"metadata", m},
                           {"locked", a.locked()}});
        if (const auto probability = a.class_probability())
            results.back()["class_probability"] =
                std::isfinite(*probability) ? Json(*probability)
                                            : Json(*probability > 0 ? "Infinity" : "-Infinity");
    }
    Json rows = Json::array();
    for (const auto& item : rois)
        rows.push_back(item.second);
    Json out = {{"schema", "qpx.omero-roi-exchange/1"},
                {"source", source},
                {"operation", "offline-shape-updates-and-results; no remote mutation"},
                {"data", rows},
                {"masks", masks},
                {"results", results}};
    const auto canonical = out.dump();
    out["sha256"] = hash(canonical);
    const auto s = out.dump();
    if (s.size() > max_bytes)
        throw std::length_error("OMERO exchange exceeds16MiB");
    return s;
}
void save_omero_roi_text(const std::string& text, const std::string& path,
                         const std::atomic<bool>* c) {
    workflow_checkpoint(c);
    if (text.size() > max_bytes)
        throw std::length_error("OMERO ROI text exceeds16MiB");
    detail::save_new_omero_file(path, text.data(), text.size(), c);
}
} // namespace qpx
