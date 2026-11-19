// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "geometry_archive.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/format_plugin.h"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <windows.h>

namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
constexpr const char *provider = "org.qupathxx.geometry-tsv";
void check(bool b, const std::string &message) {
  if (!b)
    throw std::runtime_error(message);
}
template <class Exception = std::exception, class Function>
void refuses(Function &&f) {
  try {
    f();
  } catch (const Exception &e) {
    check(!std::string(e.what()).empty(), "Refusal has no diagnostic");
    return;
  }
  throw std::runtime_error("Required native refusal was absent");
}
std::string bytes(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  check(bool(in), "Cannot read owned fixture " + p.u8string());
  return {std::istreambuf_iterator<char>(in), {}};
}
void write(const fs::path &p, const std::string &s) {
  std::ofstream out(p, std::ios::binary);
  out.write(s.data(), std::streamsize(s.size()));
  out.close();
  check(bool(out), "Cannot write owned fixture");
}
Json json(const fs::path &p) { return Json::parse(bytes(p)); }
struct Error {
  std::array<char, 4096> data{};
  qpx_reader_text_v1 out{data.data(), data.size(), 0};
};
void status(qpx_reader_status_v1 actual, qpx_reader_status_v1 expected,
            const Error &e) {
  check(actual == expected,
        "ABI status " + std::to_string(actual) + " expected " +
            std::to_string(expected) + ": " +
            std::string(e.data.data(), std::size_t(std::min<std::uint64_t>(
                                           e.out.size, e.data.size()))));
  check(e.out.data == e.data.data() && e.out.capacity == e.data.size() &&
            e.out.size <= e.data.size(),
        "Error buffer mutated");
}
std::int32_t QPX_READER_CALL yes(void *) { return 1; }
struct Countdown {
  unsigned calls = 0, limit = 0;
};
std::int32_t QPX_READER_CALL countdown(void *p) {
  auto &c = *static_cast<Countdown *>(p);
  return ++c.calls >= c.limit;
}
struct Module {
  HMODULE module;
  explicit Module(const fs::path &p)
      : module(LoadLibraryExW(p.c_str(), nullptr,
                              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
    check(module != nullptr, "Cannot load exact standalone module");
  }
  ~Module() {
    if (module)
      FreeLibrary(module);
  }
};
struct Handle {
  const qpx_annotation_format_api_v2 &api;
  void *value = nullptr;
  ~Handle() {
    if (value)
      api.destroy(value);
  }
};
qpx_reader_open_v1 request(const std::string &path,
                           std::uint64_t work = 16u * 1024 * 1024) {
  return {
      sizeof(qpx_reader_open_v1), 0, {path.data(), path.size()}, {}, work, {}};
}
void abi(const fs::path &module, const fs::path &fixtures) {
  Module loaded(module);
  Error e;
  auto entry = reinterpret_cast<qpx_annotation_format_entry_fn_v2>(
      GetProcAddress(loaded.module, "qpx_annotation_format_entry_v2"));
  check(entry != nullptr, "Geometry entry is missing");
  qpx_annotation_format_api_v2 api{};
  api.struct_size = sizeof(api);
  status(entry(1, &api, &e.out), 3, e);
  status(entry(2, nullptr, &e.out), 3, e);
  api.struct_size = 4;
  status(entry(2, &api, &e.out), 3, e);
  api.struct_size = sizeof(api);
  status(entry(2, &api, &e.out), 0, e);
  check(api.abi_version == 2 && api.struct_size == sizeof(api) &&
            api.describe && api.open && api.destroy && api.metadata &&
            api.objects,
        "Geometry callback table differs");
  qpx_format_info_v1 descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  std::array<std::array<char, 258>, 6> text;
  for (auto &value : text)
    value.fill('!');
  qpx_reader_text_v1 *fields[] = {
      &descriptor.provider_id, &descriptor.format_id, &descriptor.display_name,
      &descriptor.version,     &descriptor.contract,  &descriptor.profile};
  for (unsigned i = 0; i < 6; ++i)
    *fields[i] = {text[i].data() + 1, 0, 0};
  status(api.describe(&descriptor, &e.out), 6, e);
  for (unsigned i = 0; i < 6; ++i) {
    check(std::all_of(text[i].begin(), text[i].end(),
                      [](char c) { return c == '!'; }),
          "Descriptor probe wrote payload");
    fields[i]->capacity = 256;
  }
  status(api.describe(&descriptor, &e.out), 0, e);
  const std::array<std::string, 6> expected = {
      provider,
      "geometry-tsv",
      "Pixel geometry TSV",
      "1.0.0",
      "pixel-geometry-annotation-tsv/1",
      "windows-x64-msvc1944-release-md"};
  check(descriptor.abi_version == 2 && descriptor.domain == 2 &&
            !descriptor.reserved,
        "Geometry descriptor version/frame differs");
  for (unsigned i = 0; i < 6; ++i)
    check(fields[i]->data == text[i].data() + 1 && fields[i]->capacity == 256 &&
              std::string(fields[i]->data, std::size_t(fields[i]->size)) ==
                  expected[i] &&
              text[i].front() == '!' && text[i].back() == '!',
          "Geometry descriptor identity/guards differ");
  const auto report = json(fixtures / "fixtures.json");
  for (const auto &profile : report.at("positive")) {
    const auto path =
        fs::absolute(fixtures / profile.at("file").get<std::string>())
            .u8string();
    auto open = request(path);
    const auto before = open;
    Handle h{api};
    status(api.open(&open, &h.value, &e.out), 0, e);
    check(h.value && std::memcmp(&open, &before, sizeof(open)) == 0,
          "Open mutated request or omitted handle");
    qpx_annotation_metadata_v2 metadata{};
    metadata.struct_size = sizeof(metadata);
    status(api.metadata(h.value, &metadata, {}, &e.out), 0, e);
    const auto &objects = profile.at("objects");
    std::string geometry_expected, text_expected;
    for (const auto &row : objects) {
      geometry_expected +=
          bytes(fixtures / row.at("geometry").get<std::string>());
      text_expected += row.at("id").get<std::string>() +
                       row.at("classification").get<std::string>();
    }
    check(metadata.objects == objects.size() &&
              metadata.geometry_bytes == geometry_expected.size() &&
              metadata.geometry_encoding == 1 &&
              metadata.coordinate_frame == 1 && !metadata.reserved &&
              !metadata.reserved64[0] && !metadata.reserved64[1],
          "Exact metadata differs");
    qpx_annotation_objects_request_v2 page{sizeof(page), 0, 0, objects.size()};
    const auto original_page = page;
    qpx_annotation_objects_buffer_v2 out{};
    out.struct_size = sizeof(out);
    status(api.objects(h.value, &page, &out, {}, &e.out), 6, e);
    check(out.written == objects.size() &&
              out.text.size == text_expected.size() &&
              out.geometry.size == geometry_expected.size(),
          "Probe counts differ");
    std::vector<qpx_annotation_object_v2> rows(objects.size() + 2);
    std::memset(rows.data(), 0xa5, rows.size() * sizeof(rows[0]));
    const auto canary = rows.front();
    std::vector<char> strings(text_expected.size() + 2, '!');
    std::vector<std::uint8_t> wire(geometry_expected.size() + 2, 0xa5);
    out.objects = rows.data() + 1;
    out.capacity = objects.size();
    out.text = {strings.data() + 1, text_expected.size(), 0};
    out.geometry = {wire.data() + 1, geometry_expected.size() - 1, 0};
    status(api.objects(h.value, &page, &out, {}, &e.out), 6, e);
    check(std::all_of(wire.begin(), wire.end(),
                      [](auto b) { return b == 0xa5; }) &&
              std::all_of(strings.begin(), strings.end(),
                          [](auto b) { return b == '!'; }) &&
              std::all_of(rows.begin(), rows.end(),
                          [&](const auto &r) {
                            return std::memcmp(&r, &canary, sizeof(r)) == 0;
                          }),
          "Insufficient output wrote payload");
    out.geometry.capacity = geometry_expected.size();
    status(api.objects(h.value, &page, &out, {}, &e.out), 0, e);
    check(out.objects == rows.data() + 1 && out.capacity == objects.size() &&
              out.text.data == strings.data() + 1 &&
              out.text.capacity == text_expected.size() &&
              out.geometry.data == wire.data() + 1 &&
              out.geometry.capacity == geometry_expected.size() &&
              std::memcmp(&page, &original_page, sizeof(page)) == 0,
          "Caller pointers/request mutated");
    check(std::memcmp(&rows.front(), &canary, sizeof(canary)) == 0 &&
              std::memcmp(&rows.back(), &canary, sizeof(canary)) == 0 &&
              strings.front() == '!' && strings.back() == '!' &&
              wire.front() == 0xa5 && wire.back() == 0xa5,
          "Output crossed caller guard");
    check(std::string(strings.data() + 1, std::size_t(out.text.size)) ==
                  text_expected &&
              std::memcmp(wire.data() + 1, geometry_expected.data(),
                          geometry_expected.size()) == 0,
          "Literal text/QRO values differ");
    std::uint64_t text_at = 0, geometry_at = 0;
    for (std::size_t i = 0; i < objects.size(); ++i) {
      const auto &r = rows[i + 1];
      const auto &x = objects[i];
      const auto id = x.at("id").get<std::string>(),
                 classification = x.at("classification").get<std::string>();
      const auto size =
          fs::file_size(fixtures / x.at("geometry").get<std::string>());
      check(r.c == x.at("c") && r.z == x.at("z") && r.t == x.at("t") &&
                !r.reserved && r.source_id_offset == text_at &&
                r.source_id_size == id.size() &&
                r.class_offset == text_at + id.size() &&
                r.class_size == classification.size() &&
                r.geometry_offset == geometry_at && r.geometry_size == size,
            "Exact row spans/planes differ");
      text_at += id.size() + classification.size();
      geometry_at += size;
    }
    page = {sizeof(page), 0, metadata.objects, 1};
    out = {};
    out.struct_size = sizeof(out);
    status(api.objects(h.value, &page, &out, {}, &e.out), 0, e);
    check(!out.written && !out.text.size && !out.geometry.size,
          "End page differs");
    page.first = metadata.objects + 1;
    status(api.objects(h.value, &page, &out, {}, &e.out), 3, e);
    page.first = 0;
    page.count = 0;
    status(api.objects(h.value, &page, &out, {}, &e.out), 3, e);
    page.count = 1;
    page.reserved = 1;
    status(api.objects(h.value, &page, &out, {}, &e.out), 3, e);
    page.reserved = 0;
    status(api.objects(h.value, &page, &out, {yes, nullptr}, &e.out), QPX_READER_CANCELLED, e);
    status(api.metadata(h.value, &metadata, {yes, nullptr}, &e.out), QPX_READER_CANCELLED, e);
    status(api.objects(nullptr, &page, &out, {}, &e.out), 3, e);
    status(api.objects(h.value, nullptr, &out, {}, &e.out), 3, e);
    status(api.objects(h.value, &page, nullptr, {}, &e.out), 3, e);
  }
  for (const auto &profile : report.at("negative")) {
    const auto path =
        fs::absolute(fixtures / profile.at("file").get<std::string>())
            .u8string();
    auto r = request(path);
    Handle h{api};
    status(api.open(&r, &h.value, &e.out),
           profile.at("status").get<qpx_reader_status_v1>(), e);
    check(!h.value, "Refused open returned a handle");
  }
  const auto path = fs::absolute(fixtures / "main.tsv").u8string();
  auto r = request(path);
  Handle h{api};
  r.cancel = {yes, nullptr};
  status(api.open(&r, &h.value, &e.out), QPX_READER_CANCELLED, e);
  check(!h.value, "Cancelled open returned handle");
  Countdown cancel{0, 60};
  r.cancel = {countdown, &cancel};
  status(api.open(&r, &h.value, &e.out), QPX_READER_CANCELLED, e);
  check(!h.value && cancel.calls >= 60, "Decode cancellation seam missed");
  r = request(path, 1);
  status(api.open(&r, &h.value, &e.out), 3, e);
  check(!h.value, "Work refusal returned handle");
  r = request(path, 65536);
  status(api.open(&r, &h.value, &e.out), 0, e);
  api.destroy(h.value);
  h.value = nullptr;
  const auto expanded =
      fs::absolute(fixtures / "geometry-token-bound.tsv").u8string();
  auto budgeted = request(expanded, 16384);
  check(fs::file_size(fs::u8path(expanded)) + 4096 + 8 * expanded.size() + 128 <
                16384 &&
            fs::file_size(fixtures / "geometry-token-bound.tsv.0.qro") > 16384,
        "Decoded-expansion work fixture does not prove its intended bound");
  status(api.open(&budgeted, &h.value, &e.out), 3, e);
  check(!h.value, "Decoded work refusal returned handle");
  const std::string options = "{\"unexpected\":true}";
  r.options = {options.data(), options.size()};
  status(api.open(&r, &h.value, &e.out), 3, e);
  r = request(path);
  r.path.size = 0;
  status(api.open(&r, &h.value, &e.out), 3, e);
  std::cout << "Exact geometry ABI profiles " << report.at("positive").size()
            << " and refusals " << report.at("negative").size() << " passed\n";
}
AnnotationPluginImportOptions options() {
  AnnotationPluginImportOptions o;
  o.class_map = {{"T", "Native Tumor"}, {"S", "Native Stroma"}};
  o.plugin_work_bytes = 65536;
  o.work_bytes = 32u * 1024 * 1024;
  return o;
}
std::shared_ptr<Document> target(const fs::path &f) {
  auto server = open_tiff((f / "image.tif").u8string());
  const auto metadata = server->raw_metadata();
  check(metadata && metadata->width == 16 && metadata->height == 16 &&
            metadata->channels == 3 && metadata->z_planes == 1 &&
            metadata->timepoints == 1 &&
            metadata->sample_type == SampleType::UInt8 && server->raw_view(),
        "Authored quantitative target metadata differs");
  RawRegionRequest request;
  request.region = {0, 0, 0, 16, 16};
  request.channels = 3;
  const auto raw = server->read_raw(request);
  check(std::get<std::vector<std::uint8_t>>(raw.storage()) ==
                std::vector<std::uint8_t>(768, 80) &&
            raw.valid() == std::vector<std::uint8_t>(256, 1),
        "Authored target raw samples differ");
  auto d = std::make_shared<Document>(std::move(server));
  d->set_calibration(.25, .5);
  d->set_class_definition(ObjectClass::definition("Native Tumor", 0x123456));
  d->set_class_definition(ObjectClass::definition("Native Stroma", 0x654321));
  d->add_rectangle({14, 1, 1, 1}, "Sentinel");
  d->clear_history();
  return d;
}
std::string hex(const std::vector<std::uint8_t> &value) {
  std::string out;
  for (auto b : value) {
    out.push_back("0123456789abcdef"[b >> 4]);
    out.push_back("0123456789abcdef"[b & 15]);
  }
  return out;
}
Json snapshot(const Document &d) {
  Json rows = Json::array();
  for (const auto &[id, o] : d.objects())
    rows.push_back({{"id", id},
                    {"name", o.name},
                    {"class", o.class_name},
                    {"c", o.plane.c},
                    {"z", o.plane.z},
                    {"t", o.plane.t},
                    {"geometry", hex(GeometryArchiveCodec::encode(o.geometry))},
                    {"properties", o.properties ? Json(o.properties->metadata())
                                                : Json::object()}});
  return {{"revision", d.revision()},     {"rows", rows},
          {"undo", d.can_undo()},         {"redo", d.can_redo()},
          {"undo_label", d.undo_label()}, {"redo_label", d.redo_label()}};
}
std::vector<Roi> expected() {
  return {
      Roi::rectangle({1.25, 2.5, 4, 3}),
      Roi::ellipse({8, 1, 4, 5}),
      Roi::polyline({{1, 10}, {4, 12}, {7, 11}}),
      Roi::points({{10, 10}, {12, 12}, {15, 15}}),
      Roi::polygon({{1, 1}, {7, 1}, {7, 7}, {1, 7}},
                   {{{2, 2}, {2, 3}, {3, 3}, {3, 2}}}),
      Roi::multipolygon({PolygonPart({{9, 8}, {11, 8}, {11, 10}, {9, 10}}),
                         PolygonPart({{12, 8}, {14, 8}, {14, 10}, {12, 10}})})};
}
void create(const fs::path &f, const fs::path &w) {
  auto d = target(f);
  Project base;
  base.add_document(d, "Geometry target");
  base.save((w / "base.qpproj").u8string());
  const auto before = snapshot(*d);
  auto p = preview_annotation_plugin(*d, provider, (f / "main.tsv").u8string(),
                                     options());
  check(snapshot(*d) == before && !p.consumed() && p.objects().size() == 6 &&
            p.sha256().size() == 64,
        "Preview changed target or omitted objects");
  const auto shapes = expected();
  const auto expected_rows =
      json(f / "fixtures.json").at("positive").at(0).at("objects");
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const auto &o = p.objects()[i];
    check(o.geometry.kind() == shapes[i].kind() &&
              o.geometry.equals(shapes[i]) &&
              o.name == expected_rows[i].at("id") &&
              o.plane == ObjectPlane(expected_rows[i].at("c"), 0, 0) &&
              o.properties,
          "Geometry/plane/source ID changed");
    const auto &props = o.properties->metadata();
    check(props.at("qpx.annotation-plugin.schema") ==
                  "qpx.annotation-plugin-object/2" &&
              props.at("qpx.annotation-plugin.geometry_encoding") == "qro/1" &&
              props.at("qpx.annotation-plugin.geometry_sha256") ==
                  fingerprint_file(
                      (f / expected_rows[i].at("geometry").get<std::string>())
                          .u8string())
                      ->sha256,
          "Exact geometry receipt differs");
  }
  const auto receipt = format_plugin_receipt_json(p.receipt());
  check(format_plugin_receipt_json(parse_format_plugin_receipt(receipt)) ==
            receipt,
        "Geometry ABI2 receipt roundtrip differs");
  const auto result = apply_annotation_plugin(*d, p);
  check(p.consumed() && result.objects.size() == 6 && d->object_count() == 7 &&
            d->undo_label() == "Import plugin annotations",
        "Atomic append/mapping differs");
  for (std::size_t i = 0; i < result.objects.size(); ++i)
    check(result.objects[i].source_id == p.objects()[i].name &&
              result.objects[i].object_id == i + 2,
          "Native object mapping differs");
  const auto committed = snapshot(*d);
  refuses<std::logic_error>([&] { apply_annotation_plugin(*d, p); });
  check(snapshot(*d) == committed, "Consumed preview mutated document");
  check(d->undo() && d->object_count() == 1 && !d->can_undo() && d->can_redo(),
        "One undo did not remove only imported batch");
  check(d->redo() && d->object_count() == 7 && !d->can_redo(),
        "One redo did not restore batch");
  Project project;
  project.add_document(d, "Geometry results");
  project.save((w / "geometry.qpproj").u8string());
  write(w / "retained-rows.json", snapshot(*d).at("rows").dump());
  write(w / "receipt.json", receipt);
  write(w / "preview.sha256", result.preview_sha256);
  auto reopened =
      Project::open((w / "geometry.qpproj").u8string())->document(1);
  check(snapshot(*reopened).at("rows") == snapshot(*d).at("rows"),
        "Immediate saved native geometry/receipts differ");
}
void reopen(const fs::path &w) {
  auto d = Project::open((w / "geometry.qpproj").u8string())->document(1);
  check(snapshot(*d).at("rows") == json(w / "retained-rows.json"),
        "Offline saved geometry/properties differ");
  const auto receipt = bytes(w / "receipt.json");
  check(format_plugin_receipt_json(parse_format_plugin_receipt(receipt)) ==
            receipt,
        "Offline ABI2 receipt differs");
}
void changed_source(const fs::path &f, const fs::path &w) {
  auto d = target(f);
  const auto before = snapshot(*d);
  const auto input = w / "changed.tsv";
  write(input, bytes(f / "main.tsv"));
  auto original =
      preview_annotation_plugin(*d, provider, input.u8string(), options());
  const auto receipt = original.receipt();
  const auto preview = original.sha256();
  auto data = bytes(input);
  const auto at = data.find("1.25,2.5,4,3");
  check(at != std::string::npos, "Source-change control missing");
  data.replace(at, 4, "1.5");
  write(input, data);
  refuses<WorkflowInputChanged>([&] { apply_annotation_plugin(*d, original); });
  check(snapshot(*d) == before && !original.consumed(),
        "Changed source mutated target");
  auto changed =
      preview_annotation_plugin(*d, provider, input.u8string(), options());
  check(changed.receipt().source_sha256 != receipt.source_sha256 &&
            changed.receipt().provider.implementation_sha256 ==
                receipt.provider.implementation_sha256 &&
            changed.sha256() != preview &&
            changed.objects()[0].geometry.equals(
                Roi::rectangle({1.5, 2.5, 4, 3})),
        "Changed valid source retained old identity/result");
  check(apply_annotation_plugin(*d, changed).objects.size() == 6,
        "Changed-source fresh import failed");
}
void changed_implementation(const fs::path &f, const fs::path &w) {
  const auto old = parse_format_plugin_receipt(bytes(w / "receipt.json"));
  auto d = target(f);
  auto changed = preview_annotation_plugin(
      *d, provider, (f / "main.tsv").u8string(), options());
  const auto &current = changed.receipt();
  check(current.provider.provider_id == old.provider.provider_id &&
            current.provider.version == old.provider.version &&
            current.provider.contract == old.provider.contract &&
            current.source_sha256 == old.source_sha256 &&
            current.provider.module_sha256 != old.provider.module_sha256 &&
            current.provider.implementation_sha256 !=
                old.provider.implementation_sha256 &&
            changed.sha256() != bytes(w / "preview.sha256"),
        "Same-version changed module retained old identity");
  reopen(w);
}
void refusals(const fs::path &f, const fs::path &w) {
  const auto fixture_report = json(f / "fixtures.json");
  for (const auto &item : fixture_report.at("native_negative")) {
    auto d = target(f);
    const auto before = snapshot(*d);
    const auto input = (f / item.at("file").get<std::string>()).u8string();
    if (item.at("apply").get<bool>()) {
      auto p = preview_annotation_plugin(*d, provider, input, options());
      refuses<std::invalid_argument>([&] { apply_annotation_plugin(*d, p); });
      check(!p.consumed(), "Outside bounds consumed preview");
    } else
      refuses<std::invalid_argument>(
          [&] { preview_annotation_plugin(*d, provider, input, options()); });
    check(snapshot(*d) == before, "Invalid geometry changed document");
  }
  for (const auto *mode : {"source", "document", "revision", "registry",
                           "calibration", "closed"}) {
    auto d = target(f);
    auto other = target(f);
    const auto input = w / (std::string(mode) + ".tsv");
    write(input, bytes(f / "main.tsv"));
    auto p =
        preview_annotation_plugin(*d, provider, input.u8string(), options());
    Document *dest = d.get();
    if (std::string(mode) == "source")
      write(input, bytes(input) + "\n");
    else if (std::string(mode) == "document")
      dest = other.get();
    else if (std::string(mode) == "revision")
      d->rename_annotation(1, "Changed");
    else if (std::string(mode) == "registry")
      d->set_class_definition(
          ObjectClass::definition("Native Tumor", 0x999999));
    else if (std::string(mode) == "calibration")
      d->set_calibration(.5, .5);
    else
      d->server()->close();
    const auto before = snapshot(*dest);
    refuses<WorkflowInputChanged>([&] { apply_annotation_plugin(*dest, p); });
    check(!p.consumed() && snapshot(*dest) == before,
          "Stale geometry changed target/history");
  }
  auto d = target(f);
  const auto before = snapshot(*d);
  const auto main = (f / "main.tsv").u8string();
  auto missing = options();
  missing.class_map.erase("S");
  refuses<std::invalid_argument>(
      [&] { preview_annotation_plugin(*d, provider, main, missing); });
  auto bad_frame = options();
  bad_frame.frame_id = "invented";
  refuses<std::invalid_argument>(
      [&] { preview_annotation_plugin(*d, provider, main, bad_frame); });
  auto limited = options();
  limited.max_points = 5;
  refuses<WorkflowCaptureLimit>(
      [&] { preview_annotation_plugin(*d, provider, main, limited); });
  const auto total = std::stoull(bytes(f / "geometry-total.txt"));
  limited = options();
  limited.max_points = 6;
  limited.max_geometry_bytes = total;
  auto exact = preview_annotation_plugin(*d, provider, main, limited);
  check(exact.objects().size() == 6,
        "Exact geometry/object byte boundary refused");
  --limited.max_geometry_bytes;
  refuses<WorkflowCaptureLimit>(
      [&] { preview_annotation_plugin(*d, provider, main, limited); });
  limited = options();
  limited.work_bytes = 4u * 1024 * 1024 + limited.plugin_work_bytes;
  refuses<WorkflowCaptureLimit>(
      [&] { preview_annotation_plugin(*d, provider, main, limited); });
  std::atomic<bool> cancel{true};
  refuses<WorkflowCancelled>([&] {
    preview_annotation_plugin(*d, provider, main, options(), std::nullopt,
                              &cancel);
  });
  cancel = false;
  auto p = preview_annotation_plugin(*d, provider, main, options());
  cancel = true;
  refuses<WorkflowCancelled>([&] { apply_annotation_plugin(*d, p, &cancel); });
  check(!p.consumed() && snapshot(*d) == before,
        "Cancelled apply changed target");
  cancel = false;
  auto result = apply_annotation_plugin(*d, p, &cancel);
  check(result.objects.size() == 6, "Valid retry after cancellation failed");
  for (const auto length : {256, 257}) {
    auto current = target(f);
    const auto prior = snapshot(*current);
    auto call = [&] {
      return preview_annotation_plugin(
          *current, provider,
          (f / ("id-" + std::to_string(length) + ".tsv")).u8string(),
          options());
    };
    if (length == 256)
      check(call().objects().front().name.size() == 256,
            "Exact native ID boundary differs");
    else
      refuses<std::invalid_argument>(call);
    check(snapshot(*current) == prior, "ID refusal/capture mutated target");
  }
}
void fault_case(const std::string &mode, const fs::path &f, const fs::path &w) {
  auto d = target(f);
  const auto before = snapshot(*d);
  const auto input = w / (mode + ".tsv");
  write(input, bytes(f / "rectangle.tsv"));
  std::atomic<bool> cancel{false}, done{false}, observed{false};
  std::future<void> watcher;
  if (mode == "wait-objects")
    watcher = std::async(std::launch::async, [&] {
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(8);
      while (!done.load()) {
        const auto path = fs::u8path(input.u8string() + ".events");
        if (fs::exists(path) &&
            bytes(path).find("entered\n") != std::string::npos) {
          observed = true;
          cancel = true;
          return;
        }
        if (std::chrono::steady_clock::now() > deadline)
          throw std::runtime_error("Provider cancellation seam not reached");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  try {
    auto call = [&] {
      preview_annotation_plugin(*d, provider, input.u8string(), options(),
                                std::nullopt, &cancel);
    };
    if (mode == "wait-objects")
      refuses<WorkflowCancelled>(call);
    else if (mode == "metadata-huge")
      refuses<WorkflowCaptureLimit>(call);
    else
      refuses<std::invalid_argument>(call);
    done = true;
    if (watcher.valid())
      watcher.get();
  } catch (...) {
    done = true;
    cancel = true;
    if (watcher.valid())
      watcher.wait();
    throw;
  }
  check(snapshot(*d) == before, "Malformed plugin mutated native document");
  check(bytes(fs::u8path(input.u8string() + ".events")) ==
            (mode == "wait-objects" ? "open\nentered\ndestroy\n"
                                    : "open\ndestroy\n"),
        "Faulted handle destruction/events differ");
  if (mode == "wait-objects")
    check(observed, "Provider cancellation was not observed");
}
int run(int argc, char **argv) {
  try {
    check(argc == 5, "Usage: MODE MODULE_OR_MANIFEST FIXTURES WORK");
    const std::string mode = argv[1];
    const auto f = fs::absolute(fs::u8path(argv[3])),
               w = fs::absolute(fs::u8path(argv[4]));
    if (mode == "abi")
      abi(fs::absolute(fs::u8path(argv[2])), f);
    else if (mode == "compare-projects") {
      const auto old =
          Project::open(fs::absolute(fs::u8path(argv[2])).u8string())
              ->document(1);
      const auto current = Project::open(f.u8string())->document(1);
      check(snapshot(*old).at("rows") == snapshot(*current).at("rows"),
            "Native project geometry/receipt rows differ");
    } else if (mode == "offline")
      reopen(w);
    else {
      configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());
      check(format_plugin_infos().size() == 1 &&
                format_plugin_diagnostics().empty(),
            "Matching geometry provider admission failed");
      if (mode == "create")
        create(f, w);
      else if (mode == "reopen")
        reopen(w);
      else if (mode == "refusals")
        refusals(f, w);
      else if (mode == "changed-source")
        changed_source(f, w);
      else if (mode == "changed-implementation")
        changed_implementation(f, w);
      else
        fault_case(mode, f, w);
    }
    std::cout << "Native annotation geometry " << mode << " passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
} // namespace
int wmain(int argc, wchar_t **args) {
  std::vector<std::string> values;
  for (int i = 0; i < argc; ++i)
    values.push_back(fs::path(args[i]).u8string());
  std::vector<char *> argv;
  for (auto &s : values)
    argv.push_back(s.data());
  return run(argc, argv.data());
}
