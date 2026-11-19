// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "qupathxx/directory_store.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_plugin.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;
namespace rd = qpx::detail;
using namespace qpx;
using Json = nlohmann::json;
namespace {
constexpr std::size_t mib = 1024u * 1024;
constexpr auto label_caps = QPX_READER_CAP_RGBA | QPX_READER_CAP_RAW |
    QPX_READER_CAP_SOURCE_STORE | QPX_READER_CAP_LABEL_DESCRIPTION;

void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template<class Exception = std::exception, class F> void refuses(F&& call) {
  try {
    call();
  } catch (const Exception& error) {
    check(!std::string(error.what()).empty(), "Missing native refusal explanation");
    return;
  }
  throw std::runtime_error("Missing label-description contract refusal");
}
void write(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(bytes.data(), std::streamsize(bytes.size()));
  file.close();
  check(bool(file), "Cannot write authored label-description fixture");
}
fs::path store(const fs::path& root, const std::string& name) {
  const auto path = root / fs::u8path(name);
  check(!fs::exists(path), "Preserve earlier authored label-description fixtures");
  fs::create_directories(path);
  write(path / "primary/metadata.json", "{\"authored\":true}");
  write(path / "primary/other.json", "{\"alternate\":true}");
  const std::uint64_t pixels[]{0, UINT64_C(9007199254740993), UINT64_MAX, 42};
  write(path / "data/pixels", std::string(reinterpret_cast<const char*>(pixels), sizeof(pixels)));
  for (unsigned i = 0; i < 80; ++i)
    write(path / "data" / ("unused-" + std::to_string(i)), std::to_string(i));
  return path;
}
std::string options(const char* fault) { return Json({{"fault", fault}}).dump(); }

struct Module {
  using Count = unsigned(QPX_READER_CALL*)();
  using Work = std::uint64_t(QPX_READER_CALL*)();
  Count live = nullptr, calls = nullptr, polls = nullptr;
  Work work = nullptr;
  explicit Module(const fs::path& manifest) {
    std::ifstream file(manifest);
    const auto data = Json::parse(file);
    const auto path = manifest.parent_path() /
        fs::u8path(data.at("plugins")[0].at("module").at("path").get<std::string>());
    const auto module = GetModuleHandleW(path.c_str());
    check(module != nullptr, "Label-description fixture module not loaded");
    live = reinterpret_cast<Count>(GetProcAddress(module, "qpx_test_live_handles"));
    calls = reinterpret_cast<Count>(GetProcAddress(module, "qpx_test_label_calls"));
    polls = reinterpret_cast<Count>(GetProcAddress(module, "qpx_test_label_polls"));
    work = reinterpret_cast<Work>(GetProcAddress(module, "qpx_test_last_work"));
    check(live && calls && polls && work, "Missing authored fixture diagnostics");
  }
};

std::shared_ptr<ImageServer> open(const fs::path& path,
    const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin,
    const std::string& selected = {}, std::size_t total = 64 * mib,
    const std::atomic<bool>* cancelled = nullptr) {
  return rd::open_bounded_plugin_image_server(path.u8string(), plugin, total, cancelled, selected);
}
void raw(const ImageServer& server) {
  RawRegionRequest request;
  request.region = {0, 0, 0, 2, 2};
  const auto pixels = server.read_raw(request);
  check(pixels.sample_type() == SampleType::UInt64 &&
      pixels.valid() == std::vector<std::uint8_t>(4, 1) &&
      std::get<std::vector<std::uint64_t>>(pixels.storage()) ==
          std::vector<std::uint64_t>{0, UINT64_C(9007199254740993), UINT64_MAX, 42},
      "Label-description transport changed original integral samples");
}

class NativeControl final : public ImageServer {
  ImageMetadata image_;
  bool closed_ = false;
public:
  NativeControl() { image_.width = image_.height = 1; image_.levels.push_back({1, 1, 1}); }
  const ImageMetadata& metadata() const override { return image_; }
  PixelBuffer read_region(const RegionRequest& request) const override { return PixelBuffer(request.width, request.height); }
  void close() override { closed_ = true; }
  bool closed() const override { return closed_; }
};

void offline() {
  NativeControl server;
  check(!rd::capture_native_reader_plugin_label_description(server),
      "Native/OFF source fabricated plugin label metadata");
  server.close();
  check(!rd::capture_native_reader_plugin_label_description(server),
      "Closed native/OFF source fabricated plugin label metadata");
  std::cout << "{\"passed\":true,\"label_description\":null}\n";
}

void validate(const fs::path& manifest, const fs::path& output) {
  check(!fs::exists(output), "Use a new label-description output directory");
  fs::create_directories(output);
  configure_reader_plugins(manifest.u8string());
  const auto& catalog = rd::reader_plugin_catalog();
  check(catalog.diagnostics.empty() && catalog.readers.size() == 1,
      "Label-description provider was not admitted");
  const auto plugin = catalog.readers.front();
  check(plugin->info.capabilities == label_caps && plugin->label.struct_size == 16 &&
      plugin->label.abi_version == 1 && plugin->label.describe,
      "Separate label-description capability/table negotiation differs");
  Module module(manifest);
  check(module.live() == 0, "Initial leaked label-description handle");

  const auto input = store(output, "owned-source");
  const auto calls_before = module.calls();
  auto server = open(input, plugin);
  auto description = rd::capture_native_reader_plugin_label_description(*server);
  check(description && module.calls() == calls_before + 1,
      "Description was not captured exactly once after open");
  const auto value = Json::parse(*description);
  check(value.at("schema") == "qpx.authored-label-description/1" &&
      value.at("codes")[0] == "9007199254740993" &&
      value.at("codes")[1] == "18446744073709551615",
      "Owned neutral description bytes changed");
  raw(*server);
  check(rd::capture_native_reader_plugin_label_description(*server) == description &&
      module.calls() == calls_before + 1, "Metadata getter/read repeated the module callback");
  const auto link = rd::capture_native_reader_plugin_link(*server);
  const auto encoded = rd::encode_reader_plugin_link(*link);
  const auto saved = Json::parse(encoded);
  check(saved.at("schema") == "qpx.reader-plugin-link/2" &&
      saved.at("implementation").at("capabilities") == label_caps &&
      rd::encode_reader_plugin_link(*rd::decode_reader_plugin_link(encoded)) == encoded,
      "Label capability changed the saved directory-link schema or canonical bytes");
  auto no_raw = *link;
  no_raw.raw.reset();
  refuses<std::invalid_argument>([&] { no_raw.validate(); });
  const auto source = server->reader_source();
  server->close();
  check(rd::capture_native_reader_plugin_label_description(*server) == description,
      "Closing decoder discarded the owned metadata snapshot");
  server.reset();
  check(Json::parse(*description) == value && source->scope == "directory_store" &&
      module.live() == 0, "Owned description/source did not survive decoder destruction");

  // Exact host reservation boundaries for LABEL, then the unchanged STORE path.
  for (const auto total : {8 * mib, 64 * mib, 256 * mib}) {
    const auto path = store(output, "budget-" + std::to_string(total));
    const auto before = module.calls();
    auto bounded = open(path, plugin, {}, total);
    const auto inventory = std::min<std::size_t>(64 * mib, (total - 2 * mib) / 4);
    const auto snapshot = rd::capture_native_reader_plugin_store(*bounded);
    check(snapshot && snapshot->limits().inventory_bytes == inventory &&
        module.work() == total - 2 * inventory - 2 * mib && module.calls() == before + 1,
        "Label host inventories/extra descriptor work did not precede decoder open");
    bounded->close();
  }
  const auto plain_path = store(output, "plain-store-budget");
  auto plain = std::make_shared<rd::AdmittedReaderPlugin>(*plugin);
  plain->info.capabilities &= ~QPX_READER_CAP_LABEL_DESCRIPTION;
  const auto plain_before = module.calls();
  auto plain_server = open(plain_path, plain, {}, 4 * mib);
  const auto plain_inventory = (4 * mib - mib) / 4;
  check(module.work() == 4 * mib - 2 * plain_inventory - mib &&
      module.calls() == plain_before && !rd::capture_native_reader_plugin_label_description(*plain_server),
      "Plain STORE budget or optional-callback behavior changed");
  plain_server->close();
  for (const auto total : {4 * mib, 8 * mib - 1, 256 * mib + 1})
    refuses<std::invalid_argument>([&] { open(input, plugin, {}, total); });

  for (const auto fault : {"label-valid-max", "label-opaque"}) {
    const auto path = store(output, fault);
    auto accepted = open(path, plugin, options(fault));
    const auto captured = rd::capture_native_reader_plugin_label_description(*accepted);
    check(captured && (std::string(fault) != "label-valid-max" || captured->size() == 65536),
        "Valid description boundary was narrowed or schema interpreted by bridge");
    check(Json::parse(*captured).is_object(), "Authored neutral metadata control is malformed");
    accepted->close();
  }

  unsigned refused_outputs = 0;
  for (const auto fault : {"label-empty", "label-invalid-utf8", "label-nul", "label-oversize",
      "label-buffer-too-small", "label-wrong-required-size", "label-pointer", "label-capacity",
      "label-error-pointer", "label-error-capacity", "label-error-oversize", "label-success-error",
      "label-error-invalid-utf8", "label-failed-output", "label-failure", "label-throw",
      "label-unknown-status", "label-no-raw"}) {
    const auto path = store(output, fault);
    const auto before = module.live();
    refuses([&] { open(path, plugin, options(fault)); });
    check(module.live() == before, "Failed description open leaked its decoder handle");
    ++refused_outputs;
  }
  unsigned refused_mutations = 0;
  for (const auto fault : {"label-mutates", "label-primary-mutates", "label-failed-mutates", "label-throw-mutates"}) {
    const auto path = store(output, fault);
    const auto before = module.live();
    refuses<WorkflowInputChanged>([&] { open(path, plugin, options(fault)); });
    check(module.live() == before, "Callback store mutation leaked a decoder handle");
    ++refused_mutations;
  }

  for (unsigned kind = 0; kind < 7; ++kind) {
    auto bad = std::make_shared<rd::AdmittedReaderPlugin>(*plugin);
    if (kind == 0) bad->info.capabilities &= ~QPX_READER_CAP_RAW;
    if (kind == 1) bad->info.capabilities &= ~QPX_READER_CAP_SOURCE_STORE;
    if (kind == 2) bad->info.capabilities |= QPX_READER_CAP_SOURCE_FILES;
    if (kind == 3) bad->info.capabilities |= UINT64_C(32);
    if (kind == 4) bad->label.struct_size = 8;
    if (kind == 5) bad->label.abi_version = 2;
    if (kind == 6) bad->label.describe = nullptr;
    refuses<std::invalid_argument>([&] { open(input, bad); });
    if (kind < 4) {
      auto implementation = link->implementation;
      implementation.capabilities = bad->info.capabilities;
      refuses<std::invalid_argument>([&] { implementation.validate(); });
    }
  }

  const auto cancel_path = store(output, "cancel-callback");
  const auto before_polls = module.polls(), before_calls = module.calls();
  std::atomic<bool> cancelled{false};
  std::thread setter([&] {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (module.polls() == before_polls && std::chrono::steady_clock::now() < until)
      std::this_thread::yield();
    cancelled.store(true);
  });
  bool stopped = false;
  std::exception_ptr unexpected;
  try { open(cancel_path, plugin, options("label-wait-cancel"), 64 * mib, &cancelled); }
  catch (const WorkflowCancelled&) { stopped = true; }
  catch (...) { unexpected = std::current_exception(); }
  setter.join();
  if (unexpected) std::rethrow_exception(unexpected);
  check(stopped && module.calls() == before_calls + 1 && module.polls() > before_polls &&
      module.live() == 0, "Description callback did not poll cancellation/clean up");
  const auto pre_cancel_calls = module.calls();
  refuses<WorkflowCancelled>([&] { open(input, plugin, {}, 64 * mib, &cancelled); });
  check(module.calls() == pre_cancel_calls, "Pre-cancelled open reached the description callback");
  const auto status_path = store(output, "cancel-status");
  refuses<WorkflowCancelled>([&] { open(status_path, plugin, options("label-cancel")); });
  check(module.live() == 0, "Final leaked label-description handle");
  std::cout << Json({{"passed", true}, {"capabilities", label_caps}, {"description_table_bytes", 16},
      {"output_refusals", refused_outputs}, {"callback_mutations", refused_mutations},
      {"native_api_refusals", 7}, {"max_description_bytes", 65536}, {"saved_link_schema", 2}}).dump() << '\n';
}

int dispatch(int argc, char** argv) {
  try {
    check(argc >= 2, "reader_label_description_tests validate MANIFEST WORK | refused MANIFEST | off");
    const std::string mode = argv[1];
    if (mode == "validate") {
      check(argc == 4, "validate arguments");
      validate(fs::absolute(fs::u8path(argv[2])).lexically_normal(),
          fs::absolute(fs::u8path(argv[3])).lexically_normal());
    } else if (mode == "refused") {
      check(argc == 3, "refused arguments");
      configure_reader_plugins(fs::absolute(fs::u8path(argv[2])).lexically_normal().u8string());
      const auto& catalog = rd::reader_plugin_catalog();
      check(catalog.readers.empty() && !catalog.diagnostics.empty(),
          "Malformed label-description manifest/module was admitted");
      std::cout << "{\"passed\":true,\"admission_refused\":true}\n";
    } else if (mode == "off") {
      check(argc == 2, "off arguments");
      offline();
    } else throw std::invalid_argument("Unknown label-description test mode");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
}

int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> values;
  std::vector<char*> pointers;
  for (int i = 0; i < argc; ++i) {
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        argv[i], -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return 2;
    std::string value(std::size_t(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
        value.data(), size, nullptr, nullptr)) return 2;
    value.pop_back();
    values.push_back(std::move(value));
  }
  for (auto& value : values) pointers.push_back(value.data());
  return dispatch(argc, pointers.data());
}
