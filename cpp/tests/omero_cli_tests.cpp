#include "omero_cli.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/interchange.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using namespace qpx;
using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
const std::string server = "https://omero.example/";
const std::string store = "https://store.example/data/";
const std::string schema = "http://www.openmicroscopy.org/Schemas/OME/2016-06#";
void check(bool value, const char* why) {
    if (!value)
        throw std::runtime_error(why);
}
struct Transport : OmeroTransport {
    std::map<std::string, OmeroHttpResponse> responses;
    std::vector<std::string> requests;
    OmeroHttpResponse get(const std::string& url, std::size_t, std::uint32_t,
                          const std::atomic<bool>*) override {
        requests.push_back(url);
        const auto found = responses.find(url);
        if (found == responses.end())
            throw std::runtime_error("Unexpected CLI request: " + url);
        return found->second;
    }
    void json(const std::string& url, const Json& value) {
        const auto text = value.dump();
        responses[url] = {200, "application/json", {}, {text.begin(), text.end()}};
    }
};
std::shared_ptr<Transport> fixture() {
    auto transport = std::make_shared<Transport>();
    transport->json(
        server + "api/",
        {{"data", Json::array({{{"version", "0"}, {"url:base", server + "api/v0/"}}})}});
    const Json image = {{"@id", 7},
                        {"@type", schema + "Image"},
                        {"Name", "CLI fixture"},
                        {"Pixels",
                         {{"@id", 19},
                          {"SizeX", 2},
                          {"SizeY", 2},
                          {"SizeC", 1},
                          {"SizeZ", 1},
                          {"SizeT", 1},
                          {"Type", {{"value", "uint16"}}}}},
                        {"omero:details",
                         {{"externalInfo",
                           {{"EntityType", "com.glencoesoftware.ngff:multiscales"},
                            {"Lsid", "s3://store.example/data?anonymous=true"}}}}}};
    transport->json(server + "api/v0/m/images/7/", {{"data", image}});
    auto page = [](const std::string& kind) {
        return Json{
            {"meta", {{"offset", 2}, {"limit", 3}, {"totalCount", 3}}},
            {"data", Json::array({{{"@id", 9}, {"@type", schema + kind}, {"Name", "Result"}}})}};
    };
    transport->json(server + "api/v0/m/projects/?offset=2&limit=3", page("Project"));
    transport->json(server + "api/v0/m/projects/7/datasets/?offset=2&limit=3", page("Dataset"));
    transport->json(server + "api/v0/m/datasets/7/images/?offset=2&limit=3", page("Image"));
    transport->responses[server + "webclient/render_thumbnail/7/"] = {
        200, "image/jpeg", {}, {0xff, 0xd8, 0xff, 0xd9}};
    transport->json(
        server + "api/v0/m/images/7/rois/?offset=0&limit=200",
        {{"meta", {{"offset", 0}, {"limit", 200}, {"totalCount", 1}}},
         {"data", Json::array({{{"@id", 22},
                                {"@type", schema + "ROI"},
                                {"shapes", Json::array({{{"@id", std::uint64_t(INT64_MAX)},
                                                         {"@type", schema + "Point"},
                                                         {"X", .5},
                                                         {"Y", 1.5},
                                                         {"TheZ", 0},
                                                         {"TheT", 0}}})}}})}});
    transport->json(store + ".zgroup", {{"zarr_format", 2}});
    transport->json(
        store + ".zattrs",
        Json::parse(
            R"({"multiscales":[{"version":"0.4","axes":[{"name":"y","type":"space","unit":"micrometer"},{"name":"x","type":"space","unit":"micrometer"}],"datasets":[{"path":"0","coordinateTransformations":[{"type":"scale","scale":[0.5,0.5]}]}]}]})"));
    transport->json(store + "0/.zarray", {{"zarr_format", 2},
                                          {"shape", {2, 2}},
                                          {"chunks", {2, 2}},
                                          {"dtype", ">u2"},
                                          {"order", "C"},
                                          {"fill_value", 0},
                                          {"filters", nullptr},
                                          {"compressor", nullptr}});
    transport->responses[store + "0/0.0"] = {
        200, "application/octet-stream", {}, {0, 0, 0, 1, 0, 100, 255, 255}};
    return transport;
}
struct Streams {
    std::ostringstream output, error;
    std::streambuf* old_output = std::cout.rdbuf(output.rdbuf());
    std::streambuf* old_error = std::cerr.rdbuf(error.rdbuf());
    ~Streams() {
        std::cout.rdbuf(old_output);
        std::cerr.rdbuf(old_error);
    }
};
struct Result {
    int status;
    std::string output, error;
};
Result invoke(std::vector<std::string> args, const std::shared_ptr<Transport>& transport,
              bool cancelled = false) {
    Streams streams;
    std::atomic<bool> cancellation{cancelled};
    const auto status = detail::omero_cli(args, cancellation, transport);
    return {status, streams.output.str(), streams.error.str()};
}
Result run(const std::string& command, std::vector<std::string> flags,
           const std::shared_ptr<Transport>& transport) {
    std::vector<std::string> args{command, "--server", server};
    args.insert(args.end(), flags.begin(), flags.end());
    return invoke(std::move(args), transport);
}
Json report(const Result& result) {
    check(result.status == 0 && result.error.empty(), result.error.c_str());
    auto json = Json::parse(result.output);
    check(json.at("schema") == "qpx.omero-cli/1" && json.at("server") == server,
          "CLI report contract changed");
    return json;
}
void routes(const fs::path& root, const std::shared_ptr<Transport>& transport) {
    check(invoke({"--help"}, transport).status == 0 && transport->requests.empty(),
          "Help made a request");
    for (const auto& command : {"projects", "datasets", "images"}) {
        std::vector<std::string> flags{"--offset", "2", "--limit", "3"};
        if (command != std::string("projects"))
            flags.insert(flags.end(), {"--id", "7"});
        const auto value = report(run(command, flags, transport));
        check(value.at("objects")[0].at("id") == 9 && value.at("offset") == 2,
              "CLI lost list IDs/pagination");
    }
    check(report(run("image", {"--id", "7"}, transport)).at("image").at("pixels") == 19,
          "CLI lost Pixels ID");
    const auto preview = root / "preview.jpg";
    check(report(run("preview", {"--id", "7", "--output", preview.u8string()}, transport))
                  .at("representation") == "uncalibrated-server-thumbnail-rgb8",
          "CLI preview acquired original sample semantics");
    check(fs::file_size(preview) == 4, "CLI did not save owned preview bytes");
    const auto raw = root / "raw.npy", receipt = root / "raw.json";
    const auto value = report(run("raw",
                                  {"--id",
                                   "7",
                                   "--x",
                                   "1",
                                   "--y",
                                   "0",
                                   "--width",
                                   "1",
                                   "--height",
                                   "2",
                                   "--first-channel",
                                   "0",
                                   "--channels",
                                   "1",
                                   "--z",
                                   "0",
                                   "--t",
                                   "0",
                                   "--max-chunks",
                                   "1",
                                   "--transfer-bytes",
                                   "4096",
                                   "--output",
                                   raw.u8string(),
                                   "--receipt",
                                   receipt.u8string()},
                                  transport));
    check(value.at("receipt").at("selection").at("x") == 1 &&
              value.at("receipt").at("selection").at("height") == 2,
          "CLI changed native raw selection");
    std::ifstream file(raw, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(file)), {});
    check(bytes.size() >= 4 && bytes.substr(bytes.size() - 4) == std::string("\1\0\xff\xff", 4),
          "CLI raw export lost exact selected values/endian");
    const auto rois = root / "rois.geojson", exchange = root / "exchange.json";
    check(report(run("rois", {"--id", "7", "--z", "0", "--t", "0", "--output", rois.u8string()},
                     transport))
                  .at("shapes") == 1,
          "CLI lost ROI plane selection");
    check(report(run("roi-exchange",
                     {"--id", "7", "--z", "0", "--t", "0", "--input", rois.u8string(), "--output",
                      exchange.u8string()},
                     transport))
                  .at("state") == "saved-offline-exchange",
          "CLI exchange claims remote mutation");
    std::ifstream exchanged(exchange);
    Json result;
    exchanged >> result;
    check(result.at("results")[0].at("shape") == std::uint64_t(INT64_MAX),
          "CLI exchange rounded original shape identity");
}
void refusals(const fs::path& root) {
    const std::vector<std::vector<std::string>> invalid = {
        {},
        {"unknown"},
        {"projects"},
        {"projects", "--server", server, "--id", "7"},
        {"image", "--server", server, "--id", "7", "--offset", "0"},
        {"image", "--server", server, "--id", "7", "--output", "unused"},
        {"image", "--server", server, "--id", "7", "--server", server},
        {"image", "--server", server, "--id"},
        {"image", "--server", server, "--id", "-1"},
        {"image", "--server", server, "--id", "7", "--timeout-ms", "99"},
        {"rois", "--server", server, "--id", "7", "--z", "4294967296", "--output", "unused"},
        {"raw", "--server", server, "--id", "7", "--width", "1", "--output", "unused", "--receipt",
         "unused-json"}};
    for (const auto& args : invalid) {
        auto transport = fixture();
        const auto result = invoke(args, transport);
        check(result.status == 2 && transport->requests.empty(),
              "Invalid CLI arguments performed I/O or changed exit status");
    }
    auto before_io = [&](const std::string& command, std::vector<std::string> flags) {
        auto transport = fixture();
        const auto result = run(command, std::move(flags), transport);
        check(result.status == 1 && !result.error.empty() && transport->requests.empty(),
              "CLI preflight made a remote request");
    };
    before_io("preview", {"--id", "7", "--output", (root / "preview.jpg").u8string()});
    before_io("raw", {"--id", "7", "--width", "1", "--height", "1", "--output",
                      (root / "same").u8string(), "--receipt", (root / "same").u8string()});
#ifdef _WIN32
    before_io("raw",
              {"--id", "7", "--width", "1", "--height", "1", "--output",
               (root / "CaseAlias").u8string(), "--receipt", (root / "casealias").u8string()});
#endif
    before_io("roi-exchange", {"--id", "7", "--input", (root / "missing.geojson").u8string(),
                               "--output", (root / "missing-output").u8string()});
    auto transport = fixture();
    check(invoke({"image", "--server", server, "--id", "7"}, transport, true).status == 130 &&
              transport->requests.empty(),
          "Cancelled CLI request performed I/O");
    transport = fixture();
    transport->responses[server + "api/"].status = 403;
    check(run("image", {"--id", "7"}, transport).status == 1,
          "HTTP failure was reported as CLI success");
}
void preview_project(const fs::path& root, const std::string& manifest, const fs::path& jpeg) {
    auto transport = fixture();
    std::ifstream input(jpeg, std::ios::binary);
    check(bool(input), "Cannot open admitted preview fixture");
    transport->responses[server + "webclient/render_thumbnail/7/"].body = {
        std::istreambuf_iterator<char>(input), {}};
    const auto project = root / "preview.qpx";
    report(run("preview-project",
               {"--id", "7", "--reader-plugins", manifest, "--output", project.u8string()},
               transport));
    const auto saved = Project::open_without_journal_recovery(project.u8string());
    check(saved->images().size() == 1, "CLI preview project lost owned image");
    const auto doc = saved->document(saved->images()[0].id);
    check(!doc->server()->raw_metadata() && !doc->pixel_width_um() && !doc->pixel_height_um(),
          "CLI preview project gained original raw/calibration");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (!has_ome_zarr() || !has_interchange()) {
            std::cout << "Extended OMERO CLI checks require native Zarr and GeoJSON support\n";
            return 77;
        }
        check(argc == 1 || argc == 5,
              "Expected optional --reader-plugins MANIFEST --preview-jpeg JPEG");
        const auto root =
            fs::current_path() /
            ("omero-cli-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Unique CLI fixture directory");
        auto transport = fixture();
        routes(root, transport);
        refusals(root);
        if (argc == 5) {
            check(std::string(argv[1]) == "--reader-plugins" &&
                      std::string(argv[3]) == "--preview-jpeg",
                  "Invalid optional CLI test arguments");
            preview_project(root, argv[2], fs::u8path(argv[4]));
        }
        std::cout << "Native OMERO CLI: " << (argc == 5 ? 9 : 8)
                  << " command routes, exact raw/ROI facts, cancellation, invalid arguments and "
                     "preflight before I/O pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
