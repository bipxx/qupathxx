// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "annotation_geometry_value.hpp"
#include "geometry_archive.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace fs = std::filesystem;
using namespace qpx;
using Json = nlohmann::json;
namespace {
constexpr const char *provider = "org.qupathxx.test.annotation-source";
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class Exception = std::exception, class F> void refuses(F &&call) {
  try {
    call();
  } catch (const Exception &e) {
    check(!std::string(e.what()).empty(), "Empty native error");
    return;
  }
  throw std::runtime_error("Missing source contract refusal");
}
std::string bytes(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  check(bool(in), "Cannot read owned source control");
  return {std::istreambuf_iterator<char>(in), {}};
}
void write(const fs::path &path, const std::string &value) {
  std::ofstream out(path, std::ios::binary);
  out << value;
  out.close();
  check(bool(out), "Cannot write owned source control");
}
std::string hex(const std::vector<std::uint8_t> &data) {
  std::string out;
  for (auto b : data) {
    out.push_back("0123456789abcdef"[b >> 4]);
    out.push_back("0123456789abcdef"[b & 15]);
  }
  return out;
}
Json rows(const Document &d) {
  Json out = Json::array();
  for (const auto &[id, a] : d.objects())
    out.push_back({{"id", id},
                   {"name", a.name},
                   {"c", a.plane.c},
                   {"z", a.plane.z},
                   {"t", a.plane.t},
                   {"geometry", hex(GeometryArchiveCodec::encode(a.geometry))},
                   {"class", a.class_name},
                   {"properties", a.properties ? Json(a.properties->metadata())
                                               : Json::object()},
                   {"color", a.properties && a.properties->color_rgb()
                                 ? Json(*a.properties->color_rgb())
                                 : Json()}});
  return out;
}
Json snapshot(const Document &d) {
  return {{"revision", d.revision()},     {"rows", rows(d)},
          {"undo", d.can_undo()},         {"redo", d.can_redo()},
          {"undo_label", d.undo_label()}, {"redo_label", d.redo_label()}};
}
std::shared_ptr<Document> target(const fs::path &f, bool volume = false) {
  auto server =
      open_tiff((f / (volume ? "volume.tif" : "image.tif")).u8string());
  const auto metadata = server->raw_metadata();
  check(metadata && metadata->width == 16 && metadata->height == 16 &&
            metadata->channels == 3 &&
            metadata->z_planes == (volume ? 2u : 1u) &&
            metadata->timepoints == (volume ? 2u : 1u) &&
            metadata->sample_type == SampleType::UInt8,
        "Quantitative source axes/type differ");
  for (std::uint32_t t = 0; t < metadata->timepoints; ++t)
    for (std::uint32_t z = 0; z < metadata->z_planes; ++z) {
      RawRegionRequest request;
      request.region = {0, 0, 0, 16, 16};
      request.channels = 3;
      request.z = z;
      request.t = t;
      const auto raw = server->read_raw(request);
      check(std::get<std::vector<std::uint8_t>>(raw.storage()) ==
                    std::vector<std::uint8_t>(768, 80) &&
                raw.valid() == std::vector<std::uint8_t>(256, 1),
            "Literal native volume samples differ");
    }
  auto doc = std::make_shared<Document>(std::move(server));
  doc->set_class_definition(ObjectClass::definition("Native", 0x654321));
  doc->add_rectangle({14, 1, 1, 1}, "Sentinel");
  doc->clear_history();
  return doc;
}
AnnotationPluginImportOptions options() {
  AnnotationPluginImportOptions o;
  o.class_map = {{"N", "Native"}};
  o.plugin_work_bytes = 65536;
  o.work_bytes = 32u * 1024 * 1024;
  return o;
}
AnnotationPluginPreview
preview(const Document &d, const fs::path &input,
        const AnnotationPluginImportOptions &o = options()) {
  return preview_annotation_plugin(d, provider, input.u8string(), o);
}
void source_case(const fs::path &f, const fs::path &w) {
  auto d = target(f);
  write(w / "source.txt", "valid\n");
  const auto before = snapshot(*d);
  auto p = preview(*d, w / "source.txt");
  check(snapshot(*d) == before && p.objects().size() == 2 && !p.consumed(),
        "Source preview changed target");
  for (unsigned i = 0; i < 2; ++i) {
    const auto &a = p.objects()[i];
    const auto &m = a.properties->metadata();
    check(a.name == "Shared display μ" &&
              a.geometry.equals(Roi::rectangle({1.25, 2.5, 4, 3})) &&
              a.plane == ObjectPlane{},
          "Source name/geometry/unique-plane interpretation differs");
    check(m.at("qpx.annotation-plugin.source_id") ==
                  std::string(i ? "1" : "0001") &&
              m.at("qpx.annotation-plugin.source.source_id") ==
                  "opaque source fact" &&
              m.at("qpx.annotation-plugin.source.note") == "α\nβ\tμ" &&
              m.at("qpx.annotation-plugin.source_z") == "-1" &&
              m.at("qpx.annotation-plugin.canonical_geometry_sha256").size() ==
                  64,
          "Source facts/namespace/original plane differ");
    check(i ? !a.properties->color_rgb()
            : a.properties->color_rgb() == 0x123456,
          "Native RGB preference differs");
  }
  const auto result = apply_annotation_plugin(*d, p);
  check(p.consumed() && result.objects.size() == 2 &&
            result.objects[0].source_id == "0001" &&
            result.objects[1].source_id == "1" &&
            result.objects[0].object_id == 2 &&
            result.objects[1].object_id == 3,
        "Source mapping used duplicate display names");
  check(d->undo() && d->object_count() == 1 && d->redo() &&
            d->object_count() == 3,
        "Metadata append is not one undoable batch");
  const auto committed = snapshot(*d);
  refuses<std::logic_error>([&] { apply_annotation_plugin(*d, p); });
  check(snapshot(*d) == committed, "Consumed preview changed target");
  Project project;
  project.add_document(d, "Source facts");
  project.save((w / "source.qpproj").u8string());
  write(w / "source-rows.json", rows(*d).dump());
  write(w / "receipt.json", format_plugin_receipt_json(result.receipt));
  auto reopened = Project::open((w / "source.qpproj").u8string())->document(1);
  check(rows(*reopened) == rows(*d), "Source facts/color/name did not persist");
  auto changed = target(f);
  write(w / "changed.txt", "value-b\n");
  auto other = preview(*changed, w / "changed.txt");
  check(other.sha256() != p.sha256() &&
            other.objects()[0].properties->metadata().at(
                "qpx.annotation-plugin.row_sha256") !=
                p.objects()[0].properties->metadata().at(
                    "qpx.annotation-plugin.row_sha256"),
        "Changed source facts retained original row identity");
}
void offline(const fs::path &w) {
  auto doc = Project::open((w / "source.qpproj").u8string())->document(1);
  check(rows(*doc) == Json::parse(bytes(w / "source-rows.json")),
        "Offline source facts/geometry differ");
  const auto receipt = bytes(w / "receipt.json");
  check(format_plugin_receipt_json(parse_format_plugin_receipt(receipt)) ==
            receipt,
        "ABI3 receipt cannot reopen offline");
}
void planes(const fs::path &f, const fs::path &w) {
  auto d = target(f, true);
  write(w / "unpositioned.txt", "valid\n");
  const auto before = snapshot(*d);
  refuses<std::invalid_argument>([&] { preview(*d, w / "unpositioned.txt"); });
  check(snapshot(*d) == before, "Ambiguous plane refusal mutated document");
  auto o = options();
  o.unpositioned_z = 1;
  o.unpositioned_time = 1;
  auto p = preview(*d, w / "unpositioned.txt", o);
  for (const auto &a : p.objects())
    check(a.plane == ObjectPlane(-1, 1, 1),
          "Explicit native plane choice was ignored");
  o.unpositioned_z = 0;
  auto other = preview(*d, w / "unpositioned.txt", o);
  check(other.sha256() != p.sha256(),
        "Plane interpretation aliases another preview");
  write(w / "explicit.txt", "explicit\n");
  auto explicit_plane = preview(*d, w / "explicit.txt");
  for (const auto &a : explicit_plane.objects())
    check(a.plane == ObjectPlane(0, 1, 1), "Source explicit plane was changed");
  write(w / "linear.txt", "linear\n");
  refuses<std::invalid_argument>([&] { preview(*d, w / "linear.txt"); });
  o = options();
  o.unpositioned_z = 1;
  o.unpositioned_time = 1;
  for (const auto axis :
       {AnnotationLinearAxis::Channel, AnnotationLinearAxis::Z,
        AnnotationLinearAxis::Time}) {
    o.legacy_position_axis = axis;
    auto linear = preview(*d, w / "linear.txt", o);
    const auto expected =
        axis == AnnotationLinearAxis::Channel ? ObjectPlane(0, 1, 1)
        : axis == AnnotationLinearAxis::Z     ? ObjectPlane(-1, 0, 1)
                                              : ObjectPlane(-1, 1, 0);
    for (const auto &a : linear.objects())
      check(a.plane == expected, "Explicit legacy linear axis was not honored");
  }
  o = options();
  o.unpositioned_z = 2;
  o.unpositioned_time = 1;
  refuses([&] { preview(*d, w / "unpositioned.txt", o); });
  check(snapshot(*d) == before, "Plane failure changed document/history");
  o.unpositioned_z = 65536;
  refuses<std::invalid_argument>([&] { o.validate(); });
  o = options();
  o.legacy_position_axis = static_cast<AnnotationLinearAxis>(99);
  refuses<std::invalid_argument>([&] { o.validate(); });
}
void contours(const fs::path &f, const fs::path &w) {
  auto d = target(f);
  write(w / "contours.txt", "contours\n");
  auto p = preview(*d, w / "contours.txt");
  const auto &roi = p.objects()[0].geometry;
  check(roi.kind() == RoiKind::MultiPolygon && roi.area() == 53 &&
            roi.contains({1.5, 1.5}) && !roi.contains({2.5, 2.5}) &&
            roi.contains({3.5, 3.5}) && roi.contains({13, 13}),
        "Even-odd holes/islands/disjoint parts differ");
  auto applied = apply_annotation_plugin(*d, p);
  check(applied.objects.size() == 2 && d->annotation(2).geometry.equals(roi),
        "Contour result publication changed shape");
  const auto before = snapshot(*d);
  write(w / "crossing.txt", "crossing\n");
  refuses<std::invalid_argument>([&] { preview(*d, w / "crossing.txt"); });
  check(snapshot(*d) == before,
        "Crossing contours were repaired or mutated target");
}
void refusals(const fs::path &f, const fs::path &w) {
  auto d = target(f);
  const auto before = snapshot(*d);
  write(w / "source.txt", "valid\n");
  auto o = options();
  o.max_source_bytes = 1;
  refuses<WorkflowCaptureLimit>([&] { preview(*d, w / "source.txt", o); });
  o = options();
  o.work_bytes = 4u * 1024 * 1024 + o.plugin_work_bytes;
  refuses<WorkflowCaptureLimit>([&] { preview(*d, w / "source.txt", o); });
  std::atomic<bool> cancel{true};
  refuses<WorkflowCancelled>([&] {
    preview_annotation_plugin(*d, provider, (w / "source.txt").u8string(),
                              options(), std::nullopt, &cancel);
  });
  cancel = false;
  auto p = preview(*d, w / "source.txt");
  cancel = true;
  refuses<WorkflowCancelled>([&] { apply_annotation_plugin(*d, p, &cancel); });
  check(!p.consumed() && snapshot(*d) == before,
        "Cancelled source preview/apply mutated target");
  cancel = false;
  write(w / "source.txt", "value-b\n");
  refuses<WorkflowInputChanged>([&] { apply_annotation_plugin(*d, p); });
  check(snapshot(*d) == before && !p.consumed(),
        "Changed source input consumed old preview");
  write(w / "duplicate.txt", "duplicate-id\n");
  refuses<std::invalid_argument>([&] { preview(*d, w / "duplicate.txt"); });
  check(snapshot(*d) == before, "Duplicate source IDs were admitted");
}
void fault(const std::string &mode, const fs::path &f, const fs::path &w) {
  auto d = target(f);
  const auto before = snapshot(*d);
  const auto input = w / (mode + ".txt");
  write(input, mode + "\n");
  if (mode == "metadata-source-huge")
    refuses<WorkflowCaptureLimit>([&] { preview(*d, input); });
  else
    refuses<std::invalid_argument>([&] { preview(*d, input); });
  check(snapshot(*d) == before, "Invalid metadata source mutated document");
  check(bytes(fs::u8path(input.u8string() + ".events")) == "open\ndestroy\n",
        "Source handle was not destroyed exactly once");
}
int run(int argc, char **argv) {
  try {
    check(argc == 5, "Usage: MODE MANIFEST FIXTURES WORK");
    const std::string mode = argv[1];
    const auto f = fs::absolute(fs::u8path(argv[3])),
               w = fs::absolute(fs::u8path(argv[4]));
    if (mode == "offline")
      offline(w);
    else {
      configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());
      check(format_plugin_diagnostics().empty() &&
                format_plugin_infos().size() == 1,
            "ABI3 controlled provider did not load");
      if (mode == "source")
        source_case(f, w);
      else if (mode == "planes")
        planes(f, w);
      else if (mode == "contours")
        contours(f, w);
      else if (mode == "refusals")
        refusals(f, w);
      else
        fault(mode, f, w);
    }
    std::cout << "Native source contract " << mode << " passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
} // namespace
int wmain(int argc, wchar_t **args) {
  std::vector<std::string> owned;
  for (int i = 0; i < argc; ++i)
    owned.push_back(fs::path(args[i]).u8string());
  std::vector<char *> argv;
  for (auto &s : owned)
    argv.push_back(s.data());
  return run(argc, argv.data());
}
