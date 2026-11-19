// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
void check(bool ok, const std::string &message) {
  if (!ok)
    throw std::runtime_error(message);
}
template <class F> void refuses(F &&call) {
  try {
    call();
  } catch (const std::exception &e) {
    check(*e.what(), "Empty native refusal");
    return;
  }
  throw std::runtime_error("Expected ASAP XML refusal");
}
Json json(const fs::path &path) {
  std::ifstream f(path);
  check(bool(f), "Missing JSON oracle");
  return Json::parse(f);
}
std::string hex(const std::vector<std::uint8_t> &data) {
  std::string out;
  for (auto b : data) {
    out += "0123456789abcdef"[b >> 4];
    out += "0123456789abcdef"[b & 15];
  }
  return out;
}
Json objects(const Document &doc) {
  Json rows = Json::array();
  for (const auto &[id, a] : doc.objects())
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
std::shared_ptr<Document> target(const fs::path &dir) {
  auto s = open_tiff((dir / "image.tif").u8string());
  RawRegionRequest rr;
  rr.region = {0, 0, 0, 16, 16};
  rr.channels = 3;
  const auto raw = s->read_raw(rr);
  check(raw.valid() == std::vector<std::uint8_t>(256, 1) &&
            std::get<std::vector<std::uint8_t>>(raw.storage()) ==
                std::vector<std::uint8_t>(768, 80),
        "Literal source samples differ");
  auto d = std::make_shared<Document>(s);
  d->set_class_definition(ObjectClass::definition("Native", 0x123456));
  d->clear_history();
  return d;
}
AnnotationPluginImportOptions options(const Json &expected) {
  AnnotationPluginImportOptions o;
  o.plugin_work_bytes = 16u * 1024 * 1024;
  o.work_bytes = 64u * 1024 * 1024;
  for (const auto &row : expected)
    if (!row.value("class", std::string{}).empty())
      o.class_map.emplace(row.at("class").get<std::string>(), "Native");
  return o;
}
void save(Document &d, const fs::path &path) {
  Project p;
  auto shared = std::shared_ptr<Document>(&d, [](Document *) {});
  p.add_document(shared);
  p.save(path.u8string());
}
void positive(const fs::path &fixtures, const fs::path &work,
              const Json &example) {
  const auto file = example.at("file").get<std::string>();
  const auto &expected = example.at("rows");
  auto d = target(fixtures);
  const auto before = objects(*d);
  const auto rev = d->revision();
  auto p = preview_annotation_plugin(
      *d, "org.qupathxx.asap-xml", (fixtures / fs::u8path(file)).u8string(),
      options(expected), example.at("options").dump());
  check(objects(*d) == before && d->revision() == rev && !d->can_undo(),
        "Preview mutated document");
  check(p.objects().size() == expected.size(), file + ": object count differs");
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const auto &a = p.objects()[i];
    const auto &e = expected.at(i);
    const auto &m = a.properties->metadata();
    check(a.name == e.at("name") &&
              hex(GeometryArchiveCodec::encode(a.geometry)) == e.at("geometry"),
          file + ": exact geometry/name differs");
    check(m.at("qpx.annotation-plugin.source_id") == e.at("id") &&
              a.plane == ObjectPlane(e.value("c", -1), e.value("z", 0u),
                                     e.value("t", 0u)),
          file + ": source identity or plane differs");
    check(a.class_name ==
              (e.value("class", std::string{}).empty() ? "" : "Native"),
          file + ": explicit class mapping differs");
    for (const auto &[key, value] : e.at("facts").items())
      check(m.at("qpx.annotation-plugin.source." + key) ==
                value.get<std::string>(),
            file + ": source fact differs: " + key);
    check((a.properties->color_rgb() ? Json(*a.properties->color_rgb())
                                     : Json()) == e.value("color", Json()),
          file + ": preferred native color differs");
    check(!a.locked(), "ASAP source unexpectedly locked an annotation");
  }
  const auto result = apply_annotation_plugin(*d, p);
  check(result.objects.size() == expected.size() && d->can_undo() &&
            !d->can_redo(),
        "Apply/undo boundary differs");
  const auto rows = objects(*d);
  for (std::size_t i = 0; i < expected.size(); ++i)
    check(result.objects[i].source_id == expected.at(i).at("id") &&
              result.objects[i].object_id == i + 1,
          "ID mapping differs");
  d->undo();
  check(objects(*d) == before, "Undo did not remove exact import");
  d->redo();
  check(objects(*d) == rows, "Redo changed objects");
  refuses([&] { apply_annotation_plugin(*d, p); });
  const auto output = work / fs::u8path(file + ".qpproj");
  save(*d, output);
  auto reopened = Project::open(output.u8string());
  check(objects(*reopened->document(1)) == rows,
        "Persisted ASAP XML properties/geometry differ");
  std::ofstream facts(work / fs::u8path(file + ".expected.json"));
  facts << rows.dump(2);
  std::ofstream report(work / fs::u8path(file + ".preview.json"));
  report << Json({{"preview_sha256", result.preview_sha256},
                  {"receipt",
                   Json::parse(format_plugin_receipt_json(result.receipt))}})
                .dump(2);
}
void validate(const fs::path &fixtures, const fs::path &work) {
  const auto suite = json(fixtures / "fixtures.json");
  auto base = target(fixtures);
  save(*base, work / "base.qpproj");
  for (const auto &e : suite.at("positive")) {
    try {
      positive(fixtures, work, e);
    } catch (const std::exception &error) {
      throw std::runtime_error(e.at("file").get<std::string>() + ": " +
                               error.what());
    }
  }
  for (const auto &e : suite.at("negative")) {
    auto d = target(fixtures);
    const auto rows = objects(*d);
    const auto rev = d->revision();
    try {
      refuses([&] {
        preview_annotation_plugin(
            *d, "org.qupathxx.asap-xml",
            (fixtures / fs::u8path(e.at("file").get<std::string>())).u8string(),
            options(Json::array()), e.at("options").dump());
      });
    } catch (const std::exception &error) {
      throw std::runtime_error(e.at("file").get<std::string>() + ": " +
                               error.what());
    }
    check(objects(*d) == rows && d->revision() == rev && !d->can_undo(),
          "Refused ROI mutated native target");
  }
  auto d = target(fixtures);
  const auto file = fixtures / "point.asap.xml";
  auto small = options(Json::array());
  small.plugin_work_bytes = 256 * 1024;
  small.work_bytes = 64 * 1024;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml", file.u8string(),
                              small);
  });
  std::atomic<bool> cancelled{true};
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml", file.u8string(),
                              options(Json::array()), {}, &cancelled);
  });
  auto opts = options(Json::array());
  opts.max_source_bytes = 1;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml", file.u8string(),
                              opts);
  });
  opts = options(Json::array());
  opts.plugin_work_bytes = 256 * 1024;
  refuses([&] {
    preview_annotation_plugin(
        *d, "org.qupathxx.asap-xml",
        (fixtures / "parse-arena-limit.asap.xml").u8string(), opts);
  });
  auto unmapped = options(Json::array());
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml",
                              (fixtures / "group.asap.xml").u8string(),
                              unmapped);
  });
  // Actual multi-axis target: no implicit Z/T choice is permitted.
  auto volume = open_tiff((fixtures / "volume.tif").u8string());
  const auto metadata = volume->raw_metadata();
  check(metadata && metadata->z_planes == 2 && metadata->timepoints == 2,
        "Volume fixture axes differ");
  Document multidimensional(volume);
  auto plane_options = options(Json::array());
  refuses([&] {
    preview_annotation_plugin(multidimensional, "org.qupathxx.asap-xml",
                              file.u8string(), plane_options);
  });
  plane_options.unpositioned_z = 1;
  plane_options.unpositioned_time = 1;
  auto positioned =
      preview_annotation_plugin(multidimensional, "org.qupathxx.asap-xml",
                                file.u8string(), plane_options);
  check(positioned.objects().size() == 1 &&
            positioned.objects().front().plane == ObjectPlane(-1, 1, 1),
        "Native explicit unpositioned plane choices differ");
  const auto &source_plane =
      positioned.objects().front().properties->metadata();
  check(source_plane.at("qpx.annotation-plugin.source_z") == "-1" &&
            source_plane.at("qpx.annotation-plugin.source_t") == "-1",
        "Original unpositioned source facts changed");
  plane_options.unpositioned_z = 2;
  refuses([&] {
    preview_annotation_plugin(multidimensional, "org.qupathxx.asap-xml",
                              file.u8string(), plane_options);
  });
  // Changing numerically identical source bytes invalidates the reviewed
  // preview.
  const auto changed = work / "changed.asap.xml";
  fs::copy_file(file, changed);
  auto pending = preview_annotation_plugin(
      *d, "org.qupathxx.asap-xml", changed.u8string(), options(Json::array()));
  const auto unchanged = objects(*d);
  {
    std::ofstream mutate(changed, std::ios::binary | std::ios::app);
    mutate << " ";
  }
  refuses([&] { apply_annotation_plugin(*d, pending); });
  check(objects(*d) == unchanged && !pending.consumed() && !d->can_undo(),
        "Changed source modified target or consumed preview");
  // Container-object count and native geometry bytes remain native quotas.
  auto constrained = options(Json::array());
  constrained.max_points = 1;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml",
                              (fixtures / "primitives.asap.xml").u8string(),
                              constrained);
  });
  constrained = options(Json::array());
  constrained.max_geometry_bytes = 1;
  refuses([&] {
    preview_annotation_plugin(*d, "org.qupathxx.asap-xml", file.u8string(),
                              constrained);
  });
  std::cout << Json({{"positive", suite.at("positive").size()},
                     {"negative", suite.at("negative").size()},
                     {"passed", true}})
                   .dump()
            << '\n';
}
struct CancelAfter {
  unsigned polls = 0;
  static std::int32_t QPX_READER_CALL check(void *data) {
    return ++static_cast<CancelAfter *>(data)->polls >= 400;
  }
};
void direct_abi(const fs::path &manifest, const fs::path &fixtures) {
  const auto info = json(manifest);
  const auto module_path =
      manifest.parent_path() /
      fs::u8path(
          info.at("plugins").at(0).at("module").at("path").get<std::string>());
  const auto library = LoadLibraryExW(module_path.c_str(), nullptr,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                          LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  check(library != nullptr, "Direct provider load failed");
  struct Library {
    HMODULE value;
    ~Library() { FreeLibrary(value); }
  } owner{library};
  using Entry = qpx_reader_status_v1(QPX_FORMAT_CALL *)(
      std::uint32_t, qpx_annotation_format_api_v3 *, qpx_reader_text_v1 *);
  const auto entry = reinterpret_cast<Entry>(
      GetProcAddress(library, "qpx_annotation_format_entry_v3"));
  check(entry != nullptr, "ABI3 entry is absent");
  char error_bytes[4096];
  qpx_reader_text_v1 error{error_bytes, sizeof(error_bytes), 0};
  qpx_annotation_format_api_v3 api{};
  api.struct_size = sizeof(api);
  check(entry(3, &api, &error) == QPX_READER_OK && api.abi_version == 3,
        "ABI3 entry failed");
  check(entry(4, &api, &error) == QPX_READER_INVALID_REQUEST,
        "ABI3 provider accepted ABI4 request");
  qpx_reader_open_v1 request{};
  request.struct_size = sizeof(request);
  request.work_bytes = 256 * 1024;
  auto source = (fixtures / "point.asap.xml").u8string();
  request.path = {source.data(), source.size()};
  struct Handle {
    qpx_annotation_format_api_v3 *api;
    void *value = nullptr;
    ~Handle() {
      if (value)
        api->destroy(value);
    }
  } handle{&api};
  check(api.open(&request, &handle.value, &error) == QPX_READER_OK &&
            handle.value,
        "Minimum-budget direct open failed");
  qpx_annotation_metadata_v3 metadata{};
  metadata.struct_size = sizeof(metadata);
  check(api.metadata(handle.value, &metadata, {}, &error) == QPX_READER_OK &&
            metadata.objects == 1 && metadata.geometry_encodings == 1,
        "Direct source metadata differs");
  qpx_annotation_objects_request_v2 page{sizeof(page), 0, 0, 1};
  qpx_annotation_objects_buffer_v3 output{};
  output.struct_size = sizeof(output);
  check(api.objects(handle.value, &page, &output, {}, &error) ==
                QPX_READER_BUFFER_TOO_SMALL &&
            output.written == 1 && output.text.size && output.geometry.size &&
            output.properties_written,
        "Direct page probe differs");
  std::vector<qpx_annotation_object_v3> rows(1);
  std::vector<char> text(std::size_t(output.text.size));
  std::vector<std::uint8_t> geometry(std::size_t(output.geometry.size));
  std::vector<qpx_annotation_property_v3> properties(
      std::size_t(output.properties_written));
  output.objects = rows.data();
  output.capacity = rows.size();
  output.text = {text.data(), text.size(), 0};
  output.geometry = {geometry.data(), geometry.size(), 0};
  output.properties = properties.data();
  output.property_capacity = properties.size();
  check(api.objects(handle.value, &page, &output, {}, &error) ==
                QPX_READER_OK &&
            output.written == 1 && output.objects == rows.data() &&
            output.text.data == text.data() &&
            output.geometry.data == geometry.data() &&
            output.properties == properties.data() &&
            output.properties_written == properties.size(),
        "Direct page ownership/counts differ");
  check(hex(geometry) == json(fixtures / "fixtures.json")
                             .at("positive")
                             .at(0)
                             .at("rows")
                             .at(0)
                             .at("geometry"),
        "Direct point wire bytes differ from independent oracle");
  page.first = 1;
  check(api.objects(handle.value, &page, &output, {}, &error) ==
                QPX_READER_OK &&
            !output.written && !output.text.size && !output.geometry.size &&
            !output.properties_written,
        "End page differs");
  api.destroy(handle.value);
  handle.value = nullptr;
  // A source's float32 meaning must not vary with caller SSE modes.
  const auto original_mxcsr = _mm_getcsr();
  struct Restore {
    unsigned state;
    ~Restore() { _mm_setcsr(state); }
  } restore{original_mxcsr};
  for (unsigned state :
       {original_mxcsr | 0x2000u, original_mxcsr | 0x8000u,
        original_mxcsr | 0x40u,
        (original_mxcsr & ~unsigned(_MM_MASK_INEXACT)) & ~0x3fu}) {
    _mm_setcsr(state);
    check(api.open(&request, &handle.value, &error) ==
                  QPX_READER_INVALID_REQUEST &&
              !handle.value && error.size,
          "ASAP accepted a noncanonical floating environment");
    check(_mm_getcsr() == state,
          "ASAP changed the caller floating environment");
  }
  _mm_setcsr(original_mxcsr);
  source = (fixtures / "xml-arena-small.asap.xml").u8string();
  request.path = {source.data(), source.size()};
  check(api.open(&request, &handle.value, &error) ==
                QPX_READER_INVALID_REQUEST &&
            !handle.value && error.size,
        "Bounded XML parser did not refuse its exhausted arena");
  // Path conversion/opening costs are admitted before filesystem access.
  source = "C:/" + std::string(32000, 'a');
  request.path = {source.data(), source.size()};
  check(api.open(&request, &handle.value, &error) ==
                QPX_READER_INVALID_REQUEST &&
            !handle.value && error.size,
        "Long path opening budget was not refused before I/O");
  source = (fixtures / "parse-arena-limit.asap.xml").u8string();
  request.path = {source.data(), source.size()};
  request.work_bytes = 16u * 1024 * 1024;
  DWORD handles_before = 0, handles_after = 0;
  check(GetProcessHandleCount(GetCurrentProcess(), &handles_before) != 0,
        "Cannot inspect native handles");
  for (unsigned i = 0; i < 3; ++i) {
    CancelAfter cancelled;
    request.cancel = {CancelAfter::check, &cancelled};
    check(api.open(&request, &handle.value, &error) == QPX_READER_CANCELLED &&
              !handle.value && cancelled.polls == 400,
          "Parsing cancellation did not release the private handle");
  }
  check(GetProcessHandleCount(GetCurrentProcess(), &handles_after) != 0 &&
            handles_before == handles_after,
        "Cancelled private opens leaked native file handles");
}
int dispatch(int argc, char **argv) {
  try {
    check(argc == 5, "MODE MANIFEST FIXTURES WORK required");
    const std::string mode = argv[1];
    auto f = fs::absolute(fs::u8path(argv[3])),
         w = fs::absolute(fs::u8path(argv[4]));
    if (mode == "offline") {
      const auto suite = json(f / "fixtures.json");
      for (const auto &e : suite.at("positive")) {
        const auto file = e.at("file").get<std::string>();
        auto p = Project::open((w / fs::u8path(file + ".qpproj")).u8string());
        check(objects(*p->document(1)) ==
                  json(w / fs::u8path(file + ".expected.json")),
              "Offline project differs");
      }
      return 0;
    }
    if (mode == "compare") {
      auto p = Project::open(f.u8string()), q = Project::open(w.u8string());
      check(objects(*p->document(1)) == objects(*q->document(1)),
            "Native and CLI persisted results differ");
      return 0;
    }
    if (mode == "base") {
      auto d = target(f);
      save(*d, w / "base.qpproj");
      return 0;
    }
    configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());

    check(format_plugin_diagnostics().empty() &&
              format_plugin_infos().size() == 1 &&
              format_plugin_infos().front().abi == 3,
          "ASAP XML catalog failed");
    direct_abi(fs::absolute(fs::u8path(argv[2])), f);
    validate(f, w);
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
