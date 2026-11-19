#include "qupathxx/omero.hpp"
#include "qupathxx/omero_rois.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_registry.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
using namespace qpx;
using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
const std::string origin = "https://omero.example/", store = "https://store.example/data/",
                  schema = "http://www.openmicroscopy.org/Schemas/OME/2016-06#";
void check(bool b, const char* m) {
    if (!b)
        throw std::runtime_error(m);
}
template <class E = std::exception, class F> void refuses(F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Invalid raw/ROI input accepted");
}
OmeroHttpResponse response(std::string s) {
    return {200, "application/json", {}, {s.begin(), s.end()}};
}
struct Transport : OmeroTransport {
    std::map<std::string, OmeroHttpResponse> values;
    std::map<std::string, std::size_t> calls;
    std::string drift;
    bool cancel_after = false;
    OmeroHttpResponse get(const std::string& url, std::size_t, std::uint32_t,
                          const std::atomic<bool>* c) override {
        workflow_checkpoint(c);
        auto found = values.find(url);
        check(found != values.end(), ("Unexpected request " + url).c_str());
        const auto n = ++calls[url];
        auto r = found->second;
        if (url == drift && n > 1 && !r.body.empty())
            r.body.back() ^= 1;
        if (cancel_after && c)
            const_cast<std::atomic<bool>*>(c)->store(true);
        return r;
    }
    Transport() {
        values[origin + "api/"] = response(
            Json{{"data", Json::array({{{"version", "0"}, {"url:base", origin + "api/v0/"}}})}}
                .dump());
    }
    void json(const std::string& url, const Json& j) {
        values[url] = response(j.dump());
    }
};
Json image_row(std::string type = "uint16", bool link = true) {
    Json r = {{"@id", 7},
              {"@type", schema + "Image"},
              {"Name", "Authored open test"},
              {"Pixels",
               {{"@id", 19},
                {"SizeX", 8},
                {"SizeY", 6},
                {"SizeC", 2},
                {"SizeZ", 2},
                {"SizeT", 2},
                {"Type", {{"value", type}}}}}};
    if (link)
        r["omero:details"]["externalInfo"] = {
            {"EntityType", "com.glencoesoftware.ngff:multiscales"},
            {"Lsid", "s3://store.example/data?anonymous=true"}};
    return r;
}
void set_image(Transport& s, const Json& row) {
    s.json(origin + "api/v0/m/images/7/", Json{{"data", row}});
}
void put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b[at + i] = std::uint8_t(n >> (8 * i));
}
std::vector<std::uint8_t> frame(std::vector<std::uint8_t> b, std::uint8_t type) {
    std::vector<std::uint8_t> out(b.size() + 16);
    out[0] = 2;
    out[1] = 1;
    out[2] = 2;
    out[3] = type;
    put32(out, 4, std::uint32_t(b.size()));
    put32(out, 8, std::uint32_t(b.size()));
    put32(out, 12, std::uint32_t(out.size()));
    std::copy(b.begin(), b.end(), out.begin() + 16);
    return out;
}
std::shared_ptr<Transport> raw_fixture(bool fortran = false, bool blosc = false) {
    auto s = std::make_shared<Transport>();
    set_image(*s, image_row());
    s->json(store + ".zgroup", {{"zarr_format", 2}});
    s->json(
        store + ".zattrs",
        Json::parse(
            R"({"multiscales":[{"version":"0.4","axes":[{"name":"t","type":"time","unit":"second"},{"name":"c","type":"channel"},{"name":"z","type":"space","unit":"micrometer"},{"name":"y","type":"space","unit":"micrometer"},{"name":"x","type":"space","unit":"micrometer"}],"datasets":[{"path":"0","coordinateTransformations":[{"type":"scale","scale":[3,1,2,0.5,0.25]},{"type":"translation","translation":[0,0,0,10,20]}]}]}]})"));
    s->json(store + "0/.zarray", {{"zarr_format", 2},
                                  {"shape", {2, 2, 2, 6, 8}},
                                  {"chunks", {1, 1, 1, 3, 4}},
                                  {"dtype", ">u2"},
                                  {"order", fortran ? "F" : "C"},
                                  {"dimension_separator", "/"},
                                  {"fill_value", 0},
                                  {"filters", nullptr},
                                  {"compressor", blosc ? Json{{"id", "blosc"},
                                                              {"cname", "lz4"},
                                                              {"clevel", 5},
                                                              {"shuffle", 1},
                                                              {"blocksize", 0}}
                                                       : Json(nullptr)}});
    for (unsigned t = 0; t < 2; ++t)
        for (unsigned c = 0; c < 2; ++c)
            for (unsigned z = 0; z < 2; ++z)
                for (unsigned cy = 0; cy < 2; ++cy)
                    for (unsigned cx = 0; cx < 2; ++cx) {
                        std::vector<std::uint8_t> b(24);
                        for (unsigned y = 0; y < 3; ++y)
                            for (unsigned x = 0; x < 4; ++x) {
                                const auto v =
                                    c * 1000 + z * 100 + t * 10 + (cy * 3 + y) * 8 + cx * 4 + x;
                                const auto i = fortran ? y + x * 3 : y * 4 + x;
                                b[i * 2] = std::uint8_t(v >> 8);
                                b[i * 2 + 1] = std::uint8_t(v);
                            }
                        const auto key = store + "0/" + std::to_string(t) + "/" +
                                         std::to_string(c) + "/" + std::to_string(z) + "/" +
                                         std::to_string(cy) + "/" + std::to_string(cx);
                        if (blosc)
                            b = frame(std::move(b), 2);
                        s->values[key] = {200, "application/octet-stream", {}, std::move(b)};
                    }
    return s;
}
const RawRegionRequest request{{2, 1, 0, 4, 4}, 0, 2, 1, 1};
void raw_test(const fs::path& root, bool fortran, bool blosc) {
    auto s = raw_fixture(fortran, blosc);
    OmeroClient client(origin, s);
    client.connect();
    const auto result = client.raw(7, request);
    const auto& p = result->pixels();
    const auto& v = std::get<std::vector<std::uint16_t>>(p.storage());
    check(p.width() == 4 && p.height() == 4 && p.channels() == 2 && v.size() == 32 &&
              std::all_of(p.valid().begin(), p.valid().end(), [](auto b) { return b == 1; }),
          "Raw shape/domain/validity changed");
    for (unsigned y = 0; y < 4; ++y)
        for (unsigned x = 0; x < 4; ++x)
            for (unsigned c = 0; c < 2; ++c)
                check(v[(y * 4 + x) * 2 + c] == c * 1000 + 110 + (y + 1) * 8 + x + 2,
                      "Native raw HWC/endian/axis/chunk reconstruction differs");
    check(result->metadata().pixel_width_um == .25 && result->metadata().pixel_height_um == .5 &&
              result->metadata().z_spacing_um == 2 && result->metadata().time_interval_seconds == 3,
          "NGFF calibration changed");
    const auto receipt = Json::parse(result->receipt_json());
    check(receipt.at("chunks").size() == 8 && receipt.at("translation").at(3) == 10 &&
              receipt.at("result_sha256") == result->sha256() &&
              receipt.at("codec_plugins").size() == (blosc ? 1 : 0),
          "Raw identity/transform/plugin receipt changed");
    const auto path =
        (root / (std::string("raw-") + (fortran ? "f" : "c") + (blosc ? "-blosc" : "") + ".npy"))
            .u8string();
    save_omero_raw(*result, path);
    save_omero_raw_receipt(*result, path + ".json");
    refuses([&] { save_omero_raw(*result, path); });
    const auto bundle = path + ".zip";
    save_omero_raw_bundle(*result, bundle);
    refuses<WorkflowOutputExists>([&] { save_omero_raw_bundle(*result, bundle); });
    std::atomic<bool> stop{true};
    refuses<WorkflowCancelled>([&] { save_omero_raw_bundle(*result, path + ".cancelled", &stop); });
    check(!fs::exists(fs::u8path(path + ".cancelled")), "Cancelled bundle left an output");
    refuses<WorkflowCancelled>([&] { client.raw(7, request, {}, &stop); });
    for (const auto& p : s->calls)
        if (p.first.rfind(store + "0/", 0) == 0 && p.first.find(".zarray") == std::string::npos)
            check(p.second == 2, "Chunk stability check count changed");
}
void raw_failures() {
    {
        auto s = raw_fixture();
        set_image(*s, image_row("uint16", false));
        OmeroClient c(origin, s);
        c.connect();
        refuses<std::invalid_argument>([&] { c.raw(7, request); });
        check(s->calls.size() == 2, "No-association raw request contacted an inferred store");
    }
    for (unsigned variant = 0; variant < 7; ++variant) {
        auto s = raw_fixture();
        if (variant == 0)
            s->values[store + "0/1/0/1/0/0"].status = 404;
        else if (variant == 1)
            s->drift = store + "0/1/0/1/0/0";
        else if (variant == 2) {
            auto r = image_row();
            r["Pixels"]["SizeX"] = 9;
            set_image(*s, r);
        } else if (variant == 3)
            s->values[store + ".zattrs"].content_type = "text/html";
        else if (variant == 4)
            s->values[store + "0/1/0/1/0/0"].body.resize(2);
        else if (variant == 5) {
            auto j = Json::parse(std::string(s->values[store + "0/.zarray"].body.begin(),
                                             s->values[store + "0/.zarray"].body.end()));
            j["filters"] = Json::array({{{"id", "delta"}}});
            s->json(store + "0/.zarray", j);
        } else {
            s->values[store + ".zgroup"].status = 302;
            s->values[store + ".zgroup"].location = "https://other.example/secret";
        }
        OmeroClient c(origin, s);
        c.connect();
        refuses([&] { c.raw(7, request); });
    }
    {
        auto s = raw_fixture();
        OmeroClient c(origin, s);
        c.connect();
        auto r = request;
        r.region.x = -1;
        refuses<std::invalid_argument>([&] { c.raw(7, r); });
        r = request;
        r.region.level = 1;
        refuses([&] { c.raw(7, r); });
        OmeroRawOptions o;
        o.max_chunks = 7;
        refuses<std::length_error>([&] { c.raw(7, request, o); });
        o = {};
        o.work_bytes = 1024 * 1024;
        refuses<std::length_error>([&] { c.raw(7, request, o); });
    }
}
void bits_test() {
    auto s = std::make_shared<Transport>();
    auto row = image_row("double");
    row["Pixels"]["SizeX"] = 3;
    row["Pixels"]["SizeY"] = 2;
    row["Pixels"]["SizeC"] = row["Pixels"]["SizeZ"] = row["Pixels"]["SizeT"] = 1;
    set_image(*s, row);
    s->json(store + ".zgroup", {{"zarr_format", 2}});
    s->json(
        store + ".zattrs",
        {{"multiscales",
          Json::array(
              {{{"version", "0.4"},
                {"axes", Json::array({{{"name", "y"}, {"type", "space"}},
                                      {{"name", "x"}, {"type", "space"}}})},
                {"datasets",
                 Json::array({{{"path", "0"},
                               {"coordinateTransformations",
                                Json::array({{{"type", "scale"}, {"scale", {1, 1}}}})}}})}}})}});
    s->json(store + "0/.zarray", {{"zarr_format", 2},
                                  {"shape", {2, 3}},
                                  {"chunks", {2, 3}},
                                  {"dtype", ">f8"},
                                  {"order", "C"},
                                  {"fill_value", 0},
                                  {"filters", nullptr},
                                  {"compressor", nullptr}});
    const std::uint64_t bits[] = {0x7ff8000000000042ULL, 0x8000000000000000ULL,
                                  0x7ff0000000000000ULL, 0xfff0000000000000ULL,
                                  0x3ff0000000000001ULL, 1};
    std::vector<std::uint8_t> b(48);
    for (unsigned i = 0; i < 6; ++i)
        for (unsigned k = 0; k < 8; ++k)
            b[i * 8 + k] = std::uint8_t(bits[i] >> (56 - 8 * k));
    s->values[store + "0/0.0"] = {200, "application/octet-stream", {}, b};
    OmeroClient c(origin, s);
    c.connect();
    auto result = c.raw(7, {{0, 0, 0, 3, 2}, 0, 1, 0, 0});
    const auto& v = std::get<std::vector<double>>(result->pixels().storage());
    check(std::memcmp(v.data(), bits, sizeof(bits)) == 0,
          "Raw floating bits were converted/normalized");
}
Json shape(const std::string& kind, std::uint64_t id, Json values) {
    values["@type"] = schema + kind;
    values["@id"] = id;
    return values;
}
Json roi_page() {
    return {
        {"meta", {{"offset", 0}, {"limit", 200}, {"totalCount", 1}}},
        {"data",
         Json::array(
             {{{"@type", schema + "ROI"},
               {"@id", 22},
               {"shapes",
                Json::array(
                    {shape("Rectangle", 101,
                           {{"X", 1},
                            {"Y", 2},
                            {"Width", 3},
                            {"Height", 4},
                            {"TheZ", 1},
                            {"TheT", 0},
                            {"StrokeColor", -16711936},
                            {"Text", "A"}}),
                     shape("Ellipse", 102, {{"X", 4}, {"Y", 3}, {"RadiusX", 2}, {"RadiusY", 1}}),
                     shape("Point", 103, {{"X", .5}, {"Y", 1.5}, {"TheC", 1}}),
                     shape("Line", 104, {{"X1", 1}, {"Y1", 1}, {"X2", 6}, {"Y2", 5}}),
                     shape("Polygon", 105, {{"Points", "1,1 6,1 5,4"}}),
                     shape("Polyline", 106, {{"Points", "1,1 4,2 6,5"}}),
                     shape("Label", 107, {{"X", 2}, {"Y", 3}, {"Text", "label"}})})}}})}};
}
std::shared_ptr<Transport> roi_fixture(Json page = roi_page()) {
    auto s = std::make_shared<Transport>();
    set_image(*s, image_row());
    s->json(origin + "api/v0/m/images/7/rois/?offset=0&limit=200", page);
    return s;
}
void roi_test(const fs::path& root) {
    auto s = roi_fixture();
    OmeroClient c(origin, s);
    c.connect();
    const auto result = c.rois(7);
    check(result->shapes().size() == 7 && result->shapes()[0].z == 1 && !result->shapes()[1].z &&
              !result->shapes()[1].t && result->shapes()[2].c == 1,
          "OMERO null/all-plane semantics changed");
    const auto geojson = omero_rois_geojson(*result);
    const auto out = Json::parse(omero_rois_exchange(geojson, *result));
    check(out.at("data").size() == 1 &&
              out.at("data")[0].at("shapes") == roi_page().at("data")[0].at("shapes"),
          "Unchanged ROI geometry/IDs/planes/styles did not round-trip exactly");
    auto edit = Json::parse(geojson);
    edit["features"][0]["geometry"]["coordinates"] =
        Json::array({Json::array({Json::array({2, 2}), Json::array({5, 2}), Json::array({5, 6}),
                                  Json::array({2, 6}), Json::array({2, 2})})});
    edit["features"][0]["geometry"]["qupathxx"]["bounds"] = {2, 2, 3, 4};
    edit["features"][0]["properties"]["measurements"] = {
        {"count", 12.5}, {"missing", "NaN"}, {"positive", "Infinity"}};
    edit["features"][0]["properties"]["qupathxx"]["classProbability"] = .75;
    edit["features"][0]["properties"]["name"] = "Changed";
    const auto edited = Json::parse(omero_rois_exchange(edit.dump(), *result));
    check(edited.at("data")[0].at("shapes")[0].at("X") == 2 &&
              edited.at("data")[0].at("shapes")[0].at("Text") == "Changed" &&
              edited.at("results")[0].at("measurements").at("count") == 12.5 &&
              edited.at("results")[0].at("measurements").at("missing") == "NaN" &&
              edited.at("results")[0].at("measurements").at("positive") == "Infinity" &&
              edited.at("results")[0].at("class_probability") == .75,
          "Native geometry/results edit was lost");
    auto wrong = Json::parse(geojson);
    wrong["omero"]["image"] = 8;
    refuses<std::invalid_argument>([&] { omero_rois_exchange(wrong.dump(), *result); });
    auto preview = Json::parse(geojson);
    preview["omero"]["representation"] = "uncalibrated-server-thumbnail-rgb8";
    refuses<std::invalid_argument>([&] { omero_rois_exchange(preview.dump(), *result); });
    auto dup = Json::parse(geojson);
    dup["features"].push_back(dup["features"][0]);
    refuses([&] { omero_rois_exchange(dup.dump(), *result); });
    auto altered = Json::parse(geojson);
    altered["omero_source_shapes"]["101"]["source"]["TheZ"] = 0;
    refuses<std::invalid_argument>([&] { omero_rois_exchange(altered.dump(), *result); });
    auto stale = Json::parse(geojson);
    stale["omero"]["rois_sha256"] = std::string(64, '0');
    refuses<WorkflowInputChanged>([&] { omero_rois_exchange(stale.dump(), *result); });
    save_omero_roi_text(geojson, (root / "rois.geojson").u8string());
    save_omero_roi_text(edited.dump(), (root / "roi-exchange.json").u8string());
    refuses([&] {
        auto altered = Json::parse(geojson);
        altered["omero_source_shapes"]["101"]["source"]["TheZ"] = 0;
        refuses<std::invalid_argument>([&] { omero_rois_exchange(altered.dump(), *result); });
        auto stale = Json::parse(geojson);
        stale["omero"]["rois_sha256"] = std::string(64, '0');
        refuses<WorkflowInputChanged>([&] { omero_rois_exchange(stale.dump(), *result); });
        save_omero_roi_text(geojson, (root / "rois.geojson").u8string());
    });
}
void roi_exact_ids() {
    auto page = roi_page();
    page["data"][0]["@id"] = std::uint64_t(INT64_MAX - 1);
    auto only = page["data"][0]["shapes"][0];
    only["@id"] = std::uint64_t(INT64_MAX);
    page["data"][0]["shapes"] = Json::array({only});
    auto s = roi_fixture(page);
    OmeroClient c(origin, s);
    c.connect();
    const auto result = c.rois(7);
    const auto out = Json::parse(omero_rois_exchange(omero_rois_geojson(*result), *result));
    check(out["results"][0]["roi"] == std::uint64_t(INT64_MAX - 1) &&
              out["results"][0]["shape"] == std::uint64_t(INT64_MAX),
          "Exact OMERO ROI/shape Int64 identities were rounded");
}
void roi_failures() {
    for (unsigned v = 0; v < 5; ++v) {
        auto p = roi_page();
        auto& shapes = p["data"][0]["shapes"];
        if (v == 0)
            shapes[0]["TheZ"] = 2;
        else if (v == 1)
            shapes[1]["@id"] = 101;
        else if (v == 2)
            shapes[0]["@type"] = schema + "UnknownShape";
        else if (v == 3)
            shapes[0]["Transform"] = {{"A00", 1}, {"A01", 0}, {"A02", 1},
                                      {"A10", 0}, {"A11", 1}, {"A12", 0}};
        else
            shapes[0]["Width"] = -1;
        auto s = roi_fixture(p);
        OmeroClient c(origin, s);
        c.connect();
        refuses([&] { c.rois(7); });
    }
    {
        auto page = roi_page();
        page["data"][0]["shapes"][0]["StrokeColor"] = UINT64_MAX;
        auto s = roi_fixture(page);
        OmeroClient c(origin, s);
        c.connect();
        auto result = c.rois(7);
        auto edit = Json::parse(omero_rois_geojson(*result));
        edit["features"][0]["properties"]["color"] = {12, 34, 56};
        refuses<std::invalid_argument>([&] { omero_rois_exchange(edit.dump(), *result); });
    }
    {
        auto s = roi_fixture();
        s->drift = origin + "api/v0/m/images/7/rois/?offset=0&limit=200";
        OmeroClient c(origin, s);
        c.connect();
        refuses([&] { c.rois(7); });
    }
}
void mask_test() {
    const std::vector<std::uint8_t> png = {
        0x89, 0x50, 0x4e, 0x47, 0xd,  0xa,  0x1a, 0xa,  0x0,  0x0,  0x0,  0xd,  0x49, 0x48,
        0x44, 0x52, 0x0,  0x0,  0x0,  0x4,  0x0,  0x0,  0x0,  0x3,  0x8,  0x6,  0x0,  0x0,
        0x0,  0xb4, 0xf4, 0xae, 0xc6, 0x0,  0x0,  0x0,  0x16, 0x49, 0x44, 0x41, 0x54, 0x78,
        0x9c, 0x63, 0xf8, 0xff, 0x9f, 0xa1, 0x1,  0x86, 0x19, 0x40, 0x0,  0xc6, 0x40, 0x11,
        0x40, 0xc6, 0x0,  0x36, 0x2,  0x16, 0x6f, 0x1c, 0x9e, 0xe,  0xb8, 0x0,  0x0,  0x0,
        0x0,  0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    auto page = roi_page();
    page["data"][0]["shapes"] = Json::array(
        {shape(
             "Mask", 201,
             {{"X", 1}, {"Y", 1}, {"Width", 4}, {"Height", 3}, {"FillColor", -65408}, {"TheZ", 0}}),
         shape("Mask", 202,
               {{"X", 1},
                {"Y", 1},
                {"Width", 4},
                {"Height", 3},
                {"FillColor", -65408},
                {"TheZ", 1}})});
    auto transport = [&](Json p) {
        auto s = roi_fixture(p);
        for (unsigned id : {201, 202})
            s->values[origin + "webgateway/render_shape_mask/" + std::to_string(id) + "/"] = {
                200, "image/png", {}, png};
        return s;
    };
    auto s = transport(page);
    OmeroClient c(origin, s);
    c.connect();
    OmeroRoiOptions options;
    options.z = 1;
    options.t = 0;
    const auto result = c.rois(7, options);
    check(result->shapes().size() == 1 && result->shapes()[0].shape_id == 202 &&
              result->shapes()[0].mask_bits == std::vector<std::uint8_t>({0xea, 0xf0}) &&
              result->shapes()[0].geometry.area() == 9 &&
              !result->shapes()[0].geometry.contains({2.5, 2.5}),
          "Mask pixel packing/area/holes/plane registration changed");
    check(!s->calls.count(origin + "webgateway/render_shape_mask/201/"),
          "Unselected mask pixels were fetched");
    const auto geo = omero_rois_geojson(*result);
    const auto exchanged = Json::parse(omero_rois_exchange(geo, *result));
    check(exchanged["masks"].size() == 1 && exchanged["masks"][0]["raster"]["bits_hex"] == "eaf0" &&
              exchanged["data"][0]["shapes"][0] == page["data"][0]["shapes"][1],
          "Mask raster/shape/plane round-trip changed");
    auto plane_edit = Json::parse(geo);
    plane_edit["omero"]["selected_z"] = 0;
    refuses<std::invalid_argument>([&] { omero_rois_exchange(plane_edit.dump(), *result); });
    auto bad = Json::parse(geo);
    bad["omero_source_shapes"]["202"]["mask"]["bits_hex"] = "0000";
    refuses<std::invalid_argument>([&] { omero_rois_exchange(bad.dump(), *result); });
    for (unsigned variant = 0; variant < 4; ++variant) {
        auto p = page;
        if (variant == 0)
            p["data"][0]["shapes"][1]["FillColor"] = 255;
        else if (variant == 1)
            p["data"][0]["shapes"][1]["Width"] = 5;
        else if (variant == 2)
            p["data"][0]["shapes"][1]["FillColor"] = -256;
        auto t = transport(p);
        if (variant == 3)
            t->drift = origin + "webgateway/render_shape_mask/202/";
        OmeroClient client(origin, t);
        client.connect();
        refuses([&] { client.rois(7, options); });
    }
}
void plugin_failures(const fs::path& root) {
    auto bytes = frame(std::vector<std::uint8_t>(32, 17), 1);
    auto write = [&](std::string name, const std::vector<std::uint8_t>& b) {
        auto p = root / name;
        std::ofstream f(p, std::ios::binary);
        f.write(reinterpret_cast<const char*>(b.data()), b.size());
        f.close();
        return p.u8string();
    };
    auto path = write("good.blosc", bytes);
    auto reader =
        detail::open_reader(path, detail::select_reader(path, "org.qupathxx.blosc-bytes"));
    auto raw = reader->read_raw({{-1, 0, 0, 34, 1}, 0, 1, 0, 0});
    const auto& v = std::get<std::vector<std::uint8_t>>(raw.storage());
    check(v[0] == 0 && v[1] == 17 && v[32] == 17 && v[33] == 0 && !raw.valid()[0] &&
              raw.valid()[1] && !raw.valid()[33],
          "Blosc byte-array OOB/validity changed");
    for (unsigned i = 0; i < 4; ++i) {
        auto b = bytes;
        if (i == 0)
            b.resize(8);
        else if (i == 1)
            put32(b, 4, 0x7fffffff);
        else if (i == 2)
            put32(b, 12, 999);
        else
            b.push_back(0);
        path = write("bad-" + std::to_string(i) + ".blosc", b);
        refuses([&] {
            detail::open_reader(path, detail::select_reader(path, "org.qupathxx.blosc-bytes"));
        });
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (!has_ome_zarr() || !has_interchange() || !has_geos()) {
            std::cout << "OMERO exchange checks require native Zarr, GeoJSON and GEOS support\n";
            return 77;
        }
        const bool plugins = argc == 3 && std::string(argv[1]) == "--reader-plugins";
        if (plugins)
            configure_reader_plugins(fs::absolute(fs::u8path(argv[2])).u8string());
        else if (argc != 1)
            throw std::invalid_argument("Expected optional --reader-plugins ABS_MANIFEST");
        const auto root =
            fs::current_path() /
            ("omero-exchange-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Unique test directory");
        raw_test(root, false, false);
        raw_test(root, true, false);
        raw_failures();
        bits_test();
        roi_test(root);
        roi_exact_ids();
        roi_failures();
        if (plugins) {
            raw_test(root, false, true);
            plugin_failures(root);
            mask_test();
        }
        std::cout << "Native OMERO exchange: exact raw bits/axes/endian/chunks/calibration, "
                     "bounded/stable source, ROI geometry/planes/IDs/styles/results, source-frame "
                     "refusals and new-only export pass"
                  << (plugins ? "; optional Blosc ABI passes" : "") << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
