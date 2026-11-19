// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "annotation_geometry_value.hpp"
#include "geometry_archive.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/format_plugin.h"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <xmmintrin.h>
namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
void check(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void refuses(F &&f) {
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Expected refusal");
}
Json read(const fs::path &p) {
  std::ifstream f(p);
  check(bool(f), "Missing oracle");
  return Json::parse(f);
}
std::string hex(const std::vector<std::uint8_t> &b) {
  std::string s;
  for (auto v : b) {
    s += "0123456789abcdef"[v >> 4];
    s += "0123456789abcdef"[v & 15];
  }
  return s;
}
Json objects(const Document &d) {
  Json rows = Json::array();
  for (const auto &[id, a] : d.objects())
    rows.push_back({{"id", id},
                    {"name", a.name},
                    {"class", a.class_name},
                    {"geometry", hex(GeometryArchiveCodec::encode(a.geometry))},
                    {"c", a.plane.c},
                    {"z", a.plane.z},
                    {"t", a.plane.t},
                    {"properties", a.properties ? Json(a.properties->metadata())
                                                : Json::object()},
                    {"locked", a.locked()},
                    {"color", a.properties && a.properties->color_rgb()
                                  ? Json(*a.properties->color_rgb())
                                  : Json()}});
  return rows;
}
std::shared_ptr<Document> target(const fs::path &fixtures) {
  auto d = std::make_shared<Document>(
      open_tiff((fixtures / "image.tif").u8string()));
  d->set_class_definition(ObjectClass::definition("Native", 0x123456));
  d->set_class_definition(ObjectClass::definition("Other", 0xabcdef));
  d->clear_history();
  return d;
}
AnnotationPluginImportOptions options(const Json &example) {
  AnnotationPluginImportOptions o;
  o.work_bytes = 128u * 1024 * 1024;
  if (example.contains("rows"))
    for (const auto &r : example.at("rows"))
      o.class_map.emplace(r.at("class").get<std::string>(),
                          r.value("native_class", std::string("Native")));
  return o;
}
void save(const std::shared_ptr<Document> &d, const fs::path &p) {
  Project project;
  project.add_document(d);
  project.save(p.u8string());
}
void coverage(const Roi &roi, const Json &e) {
  const auto &pixels = e.at("pixels");
  std::size_t area = 0;
  check(pixels.size() == 256, "Oracle canvas differs");
  for (unsigned y = 0; y < 16; ++y)
    for (unsigned x = 0; x < 16; ++x) {
      const bool wanted = pixels.at(y * 16 + x).get<int>() != 0;
      check(roi.contains(Point(x + .5, y + .5)) == wanted,
            "Exact pixel-center membership differs at " + std::to_string(x) +
                "," + std::to_string(y));
      // Whole-cell overlap establishes geometry registration, not just centers.
      const auto cell = Roi::rectangle({double(x), double(y), 1, 1});
      const auto overlap = roi.boolean_operation(cell, BooleanOp::Intersection);
      check((overlap ? overlap->area() : 0.) == (wanted ? 1. : 0.),
            "Exact unit-cell area differs");
      area += wanted;
    }
  check(roi.area() == double(area), "Pixel area was not conserved");
  std::size_t holes = 0;
  for (const auto &part : roi.polygon_parts())
    holes += part.holes.size();
  if (e.contains("parts"))
    check(roi.polygon_parts().size() == e.at("parts"),
          "Disconnected part count differs");
  if (e.contains("holes"))
    check(holes == e.at("holes"), "Hole count differs");
}
void positive(const fs::path &f, const fs::path &work, const Json &e) {
  auto d = target(f);
  const auto before = objects(*d);
  const auto revision = d->revision();
  const auto path = f / fs::u8path(e.at("file").get<std::string>());
  auto p = preview_annotation_plugin(*d, "org.qupathxx.coco", path.u8string(),
                                     options(e), e.at("options").dump());
  check(objects(*d) == before && d->revision() == revision && !d->can_undo(),
        "Preview mutated target");
  check(p.receipt().provider.abi == 5 &&
            p.objects().size() == e.at("rows").size(),
        "ABI/object count differs");
  for (std::size_t i = 0; i < p.objects().size(); ++i) {
    const auto &a = p.objects()[i];
    const auto &row = e.at("rows")[i];
    const auto &m = a.properties->metadata();
    coverage(a.geometry, row);
    check(a.name == row.at("id") &&
              a.class_name ==
                  row.value("native_class", std::string("Native")) &&
              a.plane == ObjectPlane(-1, 0, 0),
          "Independent identity/class/plane differs");
    check(m.at("qpx.annotation-plugin.source_id") == row.at("id") &&
              m.at("qpx.annotation-plugin.source_class") == row.at("class") &&
              m.at("qpx.annotation-plugin.geometry_encoding") ==
                  "binary-mask/1",
          "Source identity/encoding differs");
    check(!a.locked() && !a.properties->color_rgb(),
          "COCO invented lock/color");
    for (const auto &[k, v] : row.at("facts").items())
      check(m.at("qpx.annotation-plugin.source." + k) == v.get<std::string>(),
            "Preserved source fact differs: " + k);
  }
  const auto r = apply_annotation_plugin(*d, p);
  const auto after = objects(*d);
  for (std::size_t i = 0; i < r.objects.size(); ++i)
    check(r.objects[i].source_id == e.at("rows")[i].at("id") &&
              r.objects[i].object_id == i + 1,
          "Instance mapping differs");
  refuses([&] { apply_annotation_plugin(*d, p); });
  d->undo();
  check(objects(*d) == before, "Undo differs");
  d->redo();
  check(objects(*d) == after, "Redo differs");
  const auto stem = e.at("file").get<std::string>();
  save(d, work / fs::u8path(stem + ".qpproj"));
  check(objects(*Project::open((work / fs::u8path(stem + ".qpproj")).u8string())
                     ->document(1)) == after,
        "Save/reopen changed exact result");
  std::ofstream(work / fs::u8path(stem + ".expected.json")) << after.dump(2);
  std::ofstream(work / fs::u8path(stem + ".preview.json"))
      << Json({{"preview_sha256", r.preview_sha256},
               {"receipt", Json::parse(format_plugin_receipt_json(r.receipt))}})
             .dump(2);
}
void native_controls(const fs::path &f, const fs::path &w, const Json &suite) {
  const auto &e = suite.at("positive")[0];
  auto d = target(f);
  const auto path = (f / e.at("file").get<std::string>()).u8string();
  const auto opts = e.at("options").dump();
  auto o = options(e);
  o.class_map.clear();
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.coco", path, o, opts);
  });
  o = options(e);
  o.max_geometry_bytes = 1;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.coco", path, o, opts);
  });
  o = options(e);
  o.max_source_bytes = 1;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.coco", path, o, opts);
  });
  o = options(e);
  o.plugin_work_bytes = 256 * 1024;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.coco",
                              (f / "arena.json").u8string(), o, opts);
  });
  std::atomic<bool> cancelled{true};
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.coco", path, options(e), opts,
                              &cancelled);
  });
  auto p = preview_annotation_plugin(*d, "org.qupathxx.coco", path, options(e),
                                     opts);
  refuses([&] { apply_annotation_plugin(*d, p, &cancelled); });
  check(!p.consumed() && !d->can_undo(), "Cancelled apply consumed preview");
  d->set_class_definition(ObjectClass::definition("Native", 0x654321));
  const auto stale_objects = objects(*d);
  const auto stale_revision = d->revision();
  const auto stale_undo = d->can_undo();
  refuses([&] { apply_annotation_plugin(*d, p); });
  check(!p.consumed() && objects(*d) == stale_objects &&
            d->revision() == stale_revision && d->can_undo() == stale_undo,
        "Stale class capture consumed/mutated preview");
  d = target(f);
  const auto changed = w / "changed.json";
  fs::copy_file(fs::u8path(path), changed);
  auto pending = preview_annotation_plugin(
      *d, "org.qupathxx.coco", changed.u8string(), options(e), opts);
  std::ofstream(changed, std::ios::app) << " ";
  refuses([&] { apply_annotation_plugin(*d, pending); });
  check(!pending.consumed() && !d->can_undo(), "Changed source was published");
  Document volume(open_tiff((f / "volume.ome.tif").u8string()));
  volume.set_class_definition(ObjectClass::definition("Native", 0x123456));
  refuses([&] {
    preview_annotation_plugin(volume, "org.qupathxx.coco", path, options(e),
                              opts);
  });
  o = options(e);
  o.unpositioned_z = 1;
  o.unpositioned_time = 1;
  auto positioned =
      preview_annotation_plugin(volume, "org.qupathxx.coco", path, o, opts);
  check(positioned.objects()[0].plane == ObjectPlane(-1, 1, 1),
        "Explicit target plane differs");
  o.unpositioned_z = 2;
  refuses([&] {
    preview_annotation_plugin(volume, "org.qupathxx.coco", path, o, opts);
  });
  // Native transport validation is independent of the producer parser.
  std::vector<std::uint8_t> b = {'Q', 'B', 'M', 1, 16, 0, 0, 0, 16, 0, 0, 0};
  b.resize(44, 0);
  b[12] = 128;
  auto decoded = detail::decode_annotation_pixel_mask(b.data(), b.size(),
                                                      16u * 1024 * 1024);
  check(decoded.width == 16 && decoded.height == 16 &&
            decoded.geometry.area() == 1 &&
            decoded.geometry.contains(Point(.5, .5)),
        "QBM1 grid differs");
  refuses(
      [&] { detail::decode_annotation_pixel_mask(b.data(), b.size(), 65536); });
  refuses([&] {
    detail::decode_annotation_pixel_mask(b.data(), b.size(), 16u * 1024 * 1024,
                                         &cancelled);
  });
  for (unsigned mode = 0; mode < 6; ++mode) {
    auto bad = b;
    if (mode == 0)
      bad.pop_back();
    if (mode == 1)
      bad.push_back(0);
    if (mode == 2)
      bad[4] = 0;
    if (mode == 3)
      bad[0] = 'X';
    if (mode == 4)
      bad[12] = 0;
    if (mode == 5) {
      bad = {'Q', 'B', 'M', 1, 3, 0, 0, 0, 1, 0, 0, 0, 129};
    }
    refuses([&] {
      detail::decode_annotation_pixel_mask(bad.data(), bad.size(),
                                           16u * 1024 * 1024);
    });
  }
}
struct Counter {
  std::uint64_t calls = 0, stop = 0;
};
std::int32_t QPX_READER_CALL cancel(void *p) {
  auto &c = *static_cast<Counter *>(p);
  return ++c.calls >= c.stop;
}
void direct(const fs::path &manifest, const fs::path &fixtures) {
  const auto m = read(manifest);
  const auto module =
      manifest.parent_path() /
      fs::u8path(m.at("plugins")[0].at("module").at("path").get<std::string>());
  const auto dll = LoadLibraryExW(module.c_str(), nullptr,
                                  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  check(dll != nullptr, "Cannot load plugin");
  const auto entry = reinterpret_cast<qpx_annotation_format_entry_fn_v5>(
      GetProcAddress(dll, "qpx_annotation_format_entry_v5"));
  check(entry != nullptr, "Missing ABI5 entry");
  check(GetProcAddress(dll, "qpx_annotation_format_entry_v4") == nullptr,
        "Mask module exposed a lossy old entry");
  qpx_annotation_format_api_v5 api{};
  api.struct_size = sizeof(api);
  char message[4096]{};
  qpx_reader_text_v1 error{message, sizeof(message), 0};
  check(entry(4, &api, &error) == QPX_READER_INVALID_REQUEST,
        "Wrong ABI accepted");
  check(entry(5, &api, &error) == QPX_READER_OK, "Entry failed");
  auto open = [&](const char *file, std::uint64_t work, qpx_reader_cancel_v1 c,
                  void **h) {
    const auto p = (fixtures / file).u8string();
    const std::string opts = "{\"image_id\":7}";
    qpx_reader_open_v1 r{};
    r.struct_size = sizeof(r);
    r.path = {p.data(), p.size()};
    r.options = {opts.data(), opts.size()};
    r.work_bytes = work;
    r.cancel = c;
    return api.open(&r, h, &error);
  };
  void *handle = nullptr;
  check(open("rectangle.json", 256 * 1024, {}, &handle) == QPX_READER_OK &&
            handle,
        "Minimum-work ABI open failed");
  qpx_annotation_metadata_v3 meta{};
  meta.struct_size = sizeof(meta);
  check(api.metadata(handle, &meta, {}, &error) == QPX_READER_OK &&
            meta.objects == 1 && meta.geometry_encodings == 4,
        "Mask metadata differs");
  qpx_annotation_objects_request_v2 request{sizeof(request), 0, 0, 1};
  qpx_annotation_objects_buffer_v3 out{};
  out.struct_size = sizeof(out);
  check(api.objects(handle, &request, &out, {}, &error) ==
                QPX_READER_BUFFER_TOO_SMALL &&
            out.geometry.size == 44 && out.written == 1,
        "Mask probe differs");
  std::vector<qpx_annotation_object_v3> rows(1);
  std::vector<char> text(out.text.size);
  std::vector<std::uint8_t> geometry(out.geometry.size);
  std::vector<qpx_annotation_property_v3> props(out.properties_written);
  out.objects = rows.data();
  out.capacity = 1;
  out.text = {text.data(), text.size(), 0};
  out.geometry = {geometry.data(), geometry.size(), 0};
  out.properties = props.data();
  out.property_capacity = props.size();
  check(api.objects(handle, &request, &out, {}, &error) == QPX_READER_OK &&
            rows[0].geometry_encoding == 3,
        "Mask fill differs");
  request.first = 1;
  out = {};
  out.struct_size = sizeof(out);
  check(api.objects(handle, &request, &out, {}, &error) == QPX_READER_OK &&
            !out.written,
        "End page differs");
  api.destroy(handle);
  for (auto stop : {UINT64_C(1), UINT64_C(400), UINT64_C(3000)}) {
    Counter c{0, stop};
    handle = reinterpret_cast<void *>(1);
    const auto status =
        open("arena.json", 16u * 1024 * 1024, {cancel, &c}, &handle);
    check(status == QPX_READER_CANCELLED && !handle && c.calls == stop,
          "Parse-time cancellation leaked/opened a handle");
  }
  const auto original = _mm_getcsr();
  for (auto mode : {unsigned(_MM_ROUND_DOWN), unsigned(_MM_FLUSH_ZERO_ON),
                    unsigned(0x40), unsigned(_MM_MASK_INEXACT)}) {
    const auto altered =
        mode == _MM_MASK_INEXACT ? original & ~mode : original | mode;
    _mm_setcsr(altered);
    handle = nullptr;
    const auto status = open("rectangle.json", 1024 * 1024, {}, &handle);
    const auto after = _mm_getcsr();
    _mm_setcsr(original);
    if (handle)
      api.destroy(handle);
    check(status != QPX_READER_OK && !handle &&
              (after & 0xffc0) == (altered & 0xffc0),
          "Polygon FP guard changed mode/accepted altered semantics");
  }
  FreeLibrary(dll);
}
int dispatch(int argc, char **argv) {
  try {
    check(
        argc == 5 || argc == 4,
        "coco_tests validate MANIFEST FIXTURES WORK | reopen PROJECT EXPECTED");
    if (std::string(argv[1]) == "reopen") {
      check(objects(*Project::open(argv[2])->document(1)) == read(argv[3]),
            "Provider-free reopen differs");
      return 0;
    }
    if (std::string(argv[1]) == "host-faults") {
      check(argc == 5, "host-faults MANIFEST FIXTURES WORK");
      configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());
      check(format_plugin_diagnostics().empty() &&
                format_plugin_infos().size() == 3,
            "Fault-provider catalogue differs");
      const auto fixtures = fs::absolute(fs::u8path(argv[3]));
      const auto work = fs::absolute(fs::u8path(argv[4]));
      fs::create_directories(work);
      for (auto mode : {'v', 'l', 'c', 't', 's', 'e', 'f'}) {
        const auto file = work / (std::string(1, mode) + ".bin");
        std::ofstream(file, std::ios::binary) << mode;
        for (auto abi : {3, 4, 5}) {
          auto d = target(fixtures);
          const auto before = objects(*d);
          const auto id = "org.qupathxx.mask-test-" + std::to_string(abi);
          if (abi == 5 && (mode == 'v' || mode == 'l')) {
            auto p = preview_annotation_plugin(*d, id, file.u8string());
            check(p.objects().size() == 1 &&
                      p.objects()[0].geometry.area() == 1 &&
                      p.objects()[0].locked() == (mode == 'l'),
                  "ABI5 coverage/lock differs");
            apply_annotation_plugin(*d, p);
            const auto rows = objects(*d);
            const auto output = work / (std::string(1, mode) + ".qpproj");
            save(d, output);
            check(objects(*Project::open(output.u8string())->document(1)) ==
                      rows,
                  "ABI5 persisted native state differs");
          } else {
            refuses(
                [&] { preview_annotation_plugin(*d, id, file.u8string()); });
            check(objects(*d) == before && !d->can_undo(),
                  "Malformed mask mutated target");
          }
        }
      }
      std::cout << Json({{"passed", true}, {"controlled_cases", 21}}).dump()
                << '\n';
      return 0;
    }
    check(argc == 5, "validate arguments");
    const auto manifest = fs::absolute(fs::u8path(argv[2]));
    const auto f = fs::absolute(fs::u8path(argv[3]));
    const auto w = fs::absolute(fs::u8path(argv[4]));
    check(std::string(argv[1]) == "validate", "Unknown test mode");
    fs::create_directories(w);
    configure_format_plugins(manifest.u8string());
    check(format_plugin_diagnostics().empty() &&
              format_plugin_infos().size() == 1 &&
              format_plugin_infos()[0].abi == 5,
          "COCO catalog admission failed");
    direct(manifest, f);
    const auto suite = read(f / "fixtures.json");
    save(target(f), w / "base.qpproj");
    for (const auto &e : suite.at("positive"))
      try {
        positive(f, w, e);
      } catch (const std::exception &error) {
        throw std::runtime_error(e.at("file").get<std::string>() + ": " +
                                 error.what());
      }
    for (const auto &e : suite.at("negative")) {
      auto d = target(f);
      const auto before = objects(*d);
      const auto rev = d->revision();
      try {
        refuses([&] {
          preview_annotation_plugin(
              *d, "org.qupathxx.coco",
              (f / e.at("file").get<std::string>()).u8string(), options(e),
              e.at("options").dump());
        });
      } catch (const std::exception &error) {
        throw std::runtime_error(e.at("file").get<std::string>() + ": " +
                                 error.what());
      }
      check(objects(*d) == before && d->revision() == rev && !d->can_undo(),
            "Refused input mutated document");
    }
    native_controls(f, w, suite);
    std::cout << Json({{"passed", true},
                       {"positive", suite.at("positive").size()},
                       {"negative", suite.at("negative").size()}})
                     .dump()
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  std::vector<std::string> values;
  std::vector<char *> pointers;
  for (int i = 0; i < argc; ++i) {
    const auto n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i],
                                       -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0)
      return 2;
    std::string s(std::size_t(n), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
                             s.data(), n, nullptr, nullptr))
      return 2;
    s.pop_back();
    values.push_back(std::move(s));
  }
  for (auto &s : values)
    pointers.push_back(s.data());
  return dispatch(argc, pointers.data());
}
