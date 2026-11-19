// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "qupathxx/directory_store.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/label_codes.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_plugin.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>
namespace fs = std::filesystem;
using namespace qpx;
namespace rd = qpx::detail;
using Json = nlohmann::json;
namespace {
void check(bool b, const std::string &s) {
  if (!b)
    throw std::runtime_error(s);
}
template <class F> void refuses(F f) {
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Expected refusal");
}
void write(const fs::path &p, const std::string &s) {
  std::ofstream f(p, std::ios::binary);
  f.write(s.data(), s.size());
  f.close();
  check(bool(f), "Cannot write owned fixture");
}
const std::vector<std::uint64_t> values = {
    0, UINT64_C(9007199254740992), UINT64_C(9007199254740993), UINT64_MAX};
fs::path store(const fs::path &work, const std::string &name) {
  const auto root = work / fs::u8path(name);
  check(!fs::exists(root), "Preserve previous source fixtures");
  fs::create_directories(root / "primary");
  fs::create_directories(root / "data");
  write(root / "primary/metadata.json",
        "{\"authored\":true,\"width\":2,\"height\":2}\n");
  write(root / "primary/other.json", "{\"alternate\":true}\n");
  std::ofstream file(root / "data/pixels", std::ios::binary);
  file.write(reinterpret_cast<const char *>(values.data()), values.size() * 8);
  file.close();
  for (unsigned i = 0; i < 80; ++i)
    write(root / "data" / ("unused-" + std::to_string(i)), std::to_string(i));
  return root;
}
std::shared_ptr<ImageServer> open(const fs::path &path,
                                  const std::string &options = {},
                                  std::size_t work = 64u * 1024 * 1024) {
  const auto &catalog = rd::reader_plugin_catalog();
  check(catalog.diagnostics.empty() && catalog.readers.size() == 1,
        "Store provider not admitted");
  return rd::open_bounded_plugin_image_server(
      path.u8string(), catalog.readers[0], work, nullptr, options);
}
RawPixelBuffer raw(const ImageServer &server) {
  RawRegionRequest r;
  r.region = {0, 0, 0, 2, 2};
  return server.read_raw(r);
}
void exact(const ImageServer &server) {
  const auto p = raw(server);
  check(p.sample_type() == SampleType::UInt64 &&
            p.valid() == std::vector<std::uint8_t>(4, 1) &&
            std::get<std::vector<std::uint64_t>>(p.storage()) == values,
        "Original UInt64 samples changed");
  const auto codes = normalize_label_codes(p);
  check(codes.source_codes.size() == 3 &&
            codes.source_codes.at(0).decimal() == "9007199254740992" &&
            codes.source_codes.at(1).decimal() == "9007199254740993" &&
            codes.source_codes.at(2).decimal() == "18446744073709551615" &&
            codes.raster.labels() == std::vector<std::uint32_t>({0, 1, 2, 3}),
        "Store-to-native label IDs narrowed");
}
std::string options(const char *fault) {
  return Json({{"fault", fault}}).dump();
}
struct Module {
  using Live = unsigned(QPX_READER_CALL *)();
  using Work = std::uint64_t(QPX_READER_CALL *)();
  Live live;
  Work work;
  explicit Module(const fs::path &manifest) {
    std::ifstream f(manifest);
    const auto j = Json::parse(f);
    const auto path =
        manifest.parent_path() /
        fs::u8path(
            j.at("plugins")[0].at("module").at("path").get<std::string>());
    const auto m = GetModuleHandleW(path.c_str());
    check(m != nullptr, "Admitted module is not loaded");
    live = reinterpret_cast<Live>(GetProcAddress(m, "qpx_test_live_handles"));
    work = reinterpret_cast<Work>(GetProcAddress(m, "qpx_test_last_work"));
    check(live && work, "Missing fixture diagnostics");
  }
};
void save(const std::shared_ptr<Document> &doc, const fs::path &p) {
  Project project;
  project.add_document(doc);
  project.save(p.u8string());
}
void validate(const fs::path &manifest, const fs::path &work) {
  check(!fs::exists(work), "Use a new test output directory");
  fs::create_directories(work);
  configure_reader_plugins(manifest.u8string());
  check(rd::reader_plugin_catalog().diagnostics.empty() &&
            rd::reader_plugin_catalog().readers.size() == 1,
        "Store catalog diagnostics");
  Module module(manifest);
  check(module.live() == 0, "Initial leaked handle");
  const auto path = store(work, "source-μ");
  auto server = open(path);
  exact(*server);
  check(module.live() == 1, "Open handle count differs");
  constexpr auto total = 64u * 1024 * 1024;
  const auto inventory = (total - 1024u * 1024) / 4;
  check(module.work() == total - 2 * inventory - 1024u * 1024,
        "Host inventory was not reserved before decoder open");
  const auto source = server->reader_source();
  check(source && source->scope == "directory_store" &&
            source->companions.empty() &&
            source->primary->path ==
                fs::absolute(path / "primary/metadata.json").lexically_normal().u8string(),
        "Explicit nested primary/owned store source differs");
  const auto snapshot = rd::capture_native_reader_plugin_store(*server);
  check(snapshot && snapshot->limits().inventory_bytes == inventory &&
            snapshot->observed_sha256() == source->directory_sha256,
        "Missing retained host inventory");
  const auto link = rd::capture_native_reader_plugin_link(*server);
  check(link != nullptr, "No saved store link");
  const auto encoded = rd::encode_reader_plugin_link(*link);
  const auto parsed = Json::parse(encoded);
  check(parsed.at("schema") == "qpx.reader-plugin-link/2" &&
            parsed.at("observed").at("schema") == "qpx.reader-source/3",
        "Wrong directory-only link/source encoding");
  check(rd::encode_reader_plugin_link(
            *rd::decode_reader_plugin_link(encoded)) == encoded,
        "Store link is not exact canonical data");
  auto wrong = Json::parse(encoded);
  wrong["schema"] = "qpx.reader-plugin-link/1";
  refuses([&] { rd::decode_reader_plugin_link(wrong.dump()); });
  auto reopened = rd::reopen_reader_plugin(path.u8string(), *link,
                                           rd::ReaderPluginOpenPolicy::Exact);
  exact(*reopened);
  reopened->close();
  auto changed = *link;
  changed.implementation.options = "{\"different\":true}";
  refuses([&] {
    rd::reopen_reader_plugin(path.u8string(), changed,
                             rd::ReaderPluginOpenPolicy::Exact);
  });
  auto doc = std::make_shared<Document>(server);
  Project project;
  project.add_document(doc);
  RawRegionRequest first;
  first.region = {0, 0, 0, 1, 1};
  const auto record = [&] {
    const RawAnalysisRequest request(*doc, first);
    return doc->keep_raw_analysis(request, analyze_raw(request));
  }();
  const auto fingerprint = fingerprint_file(source->primary->path);
  auto review = project.review_source_verification(1, *fingerprint);
  check(review->path() == path.u8string() &&
            review->coverage() == "directory_primary_only",
        "Metadata filename was mistaken for the store root");
  project.apply_source_review(review);
  project.save((work / "saved.qpproj").u8string());
  const auto current = project.source_verification(1);
  check(current && current->source.directory_sha256 == source->directory_sha256,
        "Directory verification was not retained");
  auto saved = Project::open((work / "saved.qpproj").u8string());
  exact(*saved->document(1)->server());
  const auto saved_doc=saved->document(1);
  const auto retained=saved_doc->raw_analysis_record(record)->result;
  check(retained->source()->reader == source->reader &&
            retained->source()->directory_sha256 == source->directory_sha256 &&
            retained->statistics().means[0] == 0,
        "Retained raw statistics lost plugin or directory provenance");
  {
    const auto replay = RawAnalysisRequest::replay(*saved_doc, record);
    check(saved_doc->verify_raw_analysis(record, replay, analyze_raw(replay)),
          "Exact plugin source cannot replay its own retained result");
  }
  check(saved->source_verification(1)->source.directory_sha256 ==
            source->directory_sha256,
        "QSV2 directory source changed on reopen");
  refuses([&] { project.save((path / "not-an-output.qpproj").u8string()); });
  check(!fs::exists(path / "not-an-output.qpproj"),
        "Project output appeared inside a protected store");
  // Moving a verified store names the root explicitly even with nested
  // metadata.
  const auto moved = work / "moved-store";
  fs::copy(path, moved, fs::copy_options::recursive);
  const auto moved_fp =
      fingerprint_file((moved / "primary/metadata.json").u8string());
  auto relocation = project.review_directory_source_verification(
      1, moved.u8string(), *moved_fp);
  check(relocation->path() == moved.u8string() &&
            relocation->requires_acceptance(),
        "New store root was inferred/automatically accepted");
  refuses([&] { project.apply_source_review(relocation); });
  project.apply_source_review(relocation, true);
  exact(*project.document(1)->server());
  check(project.images().front().source_path == moved.u8string(),
        "Explicit relocated store not adopted");
  {
    const auto relocated = project.document(1);
    const auto replay = RawAnalysisRequest::replay(*relocated, record);
    check(!relocated->verify_raw_analysis(record, replay, analyze_raw(replay)),
          "A different plugin store reverified the historical source");
  }
  // Whole-store identity guards unused chunks too, and survive decoder close.
  server->close();
  check(rd::capture_native_reader_plugin_store(*server) == nullptr,
        "Closed decoder retained its store allocation");
  snapshot->require_current();
  write(path / "data/unused-79", "changed");
  refuses([&] { snapshot->require_current(); });
  refuses([&] {
    rd::reopen_reader_plugin(path.u8string(), *link,
                             rd::ReaderPluginOpenPolicy::Exact);
  });
  saved->document(1)->server()->close();
  auto stale = Project::open((work / "saved.qpproj").u8string());
  refuses([&] { exact(*stale->document(1)->server()); });
  stale.reset();
  for (const auto *mode :
       {"escape", "absolute", "backslash", "dot", "empty", "missing", "switch",
        "primary-mutates", "primary-request", "open-mutates", "open-request",
        "metadata-mutates"}) {
    const auto input = store(work, std::string("refuse-") + mode);
    const auto before = module.live();
    refuses([&] { open(input, options(mode)); });
    check(module.live() == before, "Failed store open leaked a handle");
  }
  for (const auto *mode : {"read-mutates", "failed-read-mutates"}) {
    const auto input = store(work, std::string("refuse-") + mode);
    auto altered = open(input, options(mode));
    refuses([&] { raw(*altered); });
    altered->close();
  }
  for (unsigned kind = 0; kind < 3; ++kind) {
    const auto input = store(work, "mutation-" + std::to_string(kind));
    auto altered = open(input);
    if (kind == 0)
      write(input / "data/new-chunk", "new");
    if (kind == 1)
      fs::remove(input / "data/unused-0");
    if (kind == 2)
      fs::rename(input / "data/unused-0", input / "data/renamed");
    refuses([&] { altered->require_raw_current(); });
    altered->close();
  }
  const auto input = store(work, "limits");
  refuses([&] { open(input, {}, 65536); });
  std::atomic<bool> cancelled{true};
  refuses([&] {
    rd::open_bounded_plugin_image_server(input.u8string(),
                                         rd::reader_plugin_catalog().readers[0],
                                         total, &cancelled);
  });
  // New directory-only source/link fields cannot be stripped or confused with
  // the compact file source; a forged mask capability fails native validation.
  auto bad = *link;
  bad.observed.directory_sha256.clear();
  refuses([&] { bad.validate(); });
  bad = *link;
  bad.implementation.capabilities |= QPX_READER_CAP_SOURCE_FILES;
  refuses([&] { bad.validate(); });
  project.document(1)->server()->close();
  review.reset();
  relocation.reset();
  reopened.reset();
  doc.reset();
  server.reset();
  saved.reset();
  check(module.live() == 0, "Final leaked store handle");
  std::cout << Json({{"passed", true},
                     {"source_files", 83},
                     {"open_refusal_cases", 12},
                     {"read_mutation_cases", 2},
                     {"membership_cases", 3},
                     {"saved_link_schema", 2}})
                   .dump()
            << '\n';
}
int dispatch(int argc, char **argv) {
  try {
    check(argc >= 3, "reader_store_tests validate MANIFEST WORK | unavailable "
                     "PROJECT | refused MANIFEST");
    const std::string mode = argv[1];
    if (mode == "validate") {
      check(argc == 4, "validate arguments");
      validate(fs::absolute(fs::u8path(argv[2])),
               fs::absolute(fs::u8path(argv[3])));
    } else if (mode == "refused") {
      configure_reader_plugins(fs::absolute(fs::u8path(argv[2])).u8string());
      const auto &catalog = rd::reader_plugin_catalog();
      check(catalog.readers.empty() && !catalog.diagnostics.empty(),
            "Malformed store module/manifest was admitted");
    } else if (mode == "unavailable") {
      auto p = Project::open(argv[2]);
      check(p->document(1)->metadata().width == 2,
            "Missing provider hid saved metadata");
      const auto records = p->document(1)->raw_analysis_records();
      check(records.size() == 1 && records.front()->result->statistics().means[0] == 0,
            "Missing provider hid retained raw statistics");
      refuses([&] { raw(*p->document(1)->server()); });
    } else
      throw std::invalid_argument("Unknown store test mode");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  std::vector<std::string> values_;
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
    values_.push_back(std::move(s));
  }
  for (auto &s : values_)
    pointers.push_back(s.data());
  return dispatch(argc, pointers.data());
}
