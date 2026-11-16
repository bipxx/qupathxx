#include "qupathxx/omero.hpp"
#include "qupathxx/workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
using namespace qpx;
using Json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
void check(bool value, const char* why) {
    if (!value)
        throw std::runtime_error(why);
}
template <class E = std::exception, class F> void refuses(F action) {
    try {
        action();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("Invalid OMERO operation was accepted");
}
OmeroHttpResponse response(std::string text, std::string type = "application/json",
                           unsigned status = 200) {
    return {status, std::move(type), {}, std::vector<std::uint8_t>(text.begin(), text.end())};
}
const std::string server = "https://omero.example/";
std::string discovery(const std::string& s = server) {
    return Json{{"data", Json::array({{{"version", "0"}, {"url:base", s + "api/v0/"}}})}}.dump();
}
Json row(std::uint64_t id = 7) {
    return {{"@id", id},
            {"@type", "http://www.openmicroscopy.org/Schemas/OME/2016-06#Image"},
            {"Name", "Independent image"},
            {"Pixels",
             {{"@id", 19},
              {"SizeX", 256},
              {"SizeY", 128},
              {"SizeC", 3},
              {"SizeZ", 4},
              {"SizeT", 2},
              {"Type", {{"value", "uint16"}}},
              {"PhysicalSizeX", {{"Value", .5}, {"Unit", "MICROMETER"}}}}}};
}
std::string image(Json r = row()) {
    return Json{{"data", std::move(r)}}.dump();
}
struct Script : OmeroTransport {
    struct Step {
        std::string path;
        OmeroHttpResponse result;
    };
    std::vector<Step> steps;
    std::size_t at = 0;
    std::atomic<bool>* cancel_after = nullptr;
    OmeroHttpResponse get(const std::string& url, std::size_t, std::uint32_t,
                          const std::atomic<bool>* cancelled) override {
        workflow_checkpoint(cancelled);
        check(at < steps.size(), "Unexpected OMERO request");
        auto& step = steps[at++];
        check(url == server + step.path, "Protocol emitted the wrong endpoint/query");
        if (cancel_after)
            cancel_after->store(true);
        return step.result;
    }
    void add(std::string path, std::string text) {
        steps.push_back({std::move(path), response(std::move(text))});
    }
    void start() {
        add("api/", discovery());
    }
};
void protocol(const fs::path& root) {
    auto script = std::make_shared<Script>();
    script->start();
    OmeroClient client("https://OMERO.example:443", script);
    check(client.server() == server, "HTTPS base was not canonicalized");
    refuses<std::logic_error>([&] { client.image(7); });
    client.connect();
    Json page = {
        {"meta", {{"offset", 0}, {"limit", 1}, {"totalCount", 2}}},
        {"data",
         Json::array({{{"@id", 51}, {"Name", "Independent study"}, {"@type", "OME#Project"}}})}};
    script->add("api/v0/m/projects/?offset=0&limit=1", page.dump());
    const auto projects = client.projects(0, 1);
    check(projects.total == 2 && projects.objects.size() == 1 && projects.objects[0].id == 51,
          "Page lost native IDs/counts");
    page["data"][0]["@type"] = "OME#Dataset";
    script->add("api/v0/m/projects/51/datasets/?offset=0&limit=1", page.dump());
    check(client.datasets(51, 0, 1).objects[0].name == "Independent study",
          "Dataset route changed names");
    page["data"][0]["@type"] = "OME#Image";
    script->add("api/v0/m/datasets/51/images/?offset=0&limit=1", page.dump());
    check(client.images(51, 0, 1).total == 2, "Image page route differs");
    script->add("api/v0/m/images/7/", image());
    const auto info = client.image(7);
    check(info.id == 7 && info.pixels_id == 19 && info.width == 256 && info.height == 128 &&
              info.channels == 3 && info.z_planes == 4 && info.timepoints == 2,
          "Typed image geometry/planes differ");
    check(info.physical_x->value == .5 && info.physical_x->unit == "MICROMETER" &&
              !info.physical_y && info.metadata_sha256.size() == 64,
          "Metadata inferred missing calibration or lost identity");
    script->add("api/v0/m/images/7/", Json{{"data", Json::array({row()})}}.dump());
    check(client.image(7).metadata_sha256 == info.metadata_sha256,
          "Object/legacy singleton-array forms differ");
    const auto maximum = std::uint64_t(INT64_MAX);
    script->add("api/v0/m/images/" + std::to_string(maximum) + "/", image(row(maximum)));
    check(client.image(maximum).id == maximum, "OMERO ID was rounded through binary64");
    script->add("api/v0/m/images/7/", image());
    script->steps.push_back(
        {"webclient/render_thumbnail/7/", {200, "image/jpeg", {}, {0xff, 0xd8, 0xff, 0xd9}}});
    script->add("api/v0/m/images/7/", image());
    const auto preview = client.preview(7);
    check(preview->image().id == 7 && preview->server() == server && preview->jpeg().size() == 4 &&
              preview->sha256().size() == 64,
          "Owned encoded preview lost source identity");
    const auto path = (root / "encoded-preview.jpg").u8string();
    save_omero_preview(*preview, path);
    const auto size = fs::file_size(path);
    refuses([&] { save_omero_preview(*preview, path); });
    check(fs::file_size(path) == size, "Preview overwrite changed existing output");
    std::atomic<bool> cancel{true};
    refuses<WorkflowCancelled>(
        [&] { save_omero_preview(*preview, (root / "cancelled.jpg").u8string(), &cancel); });
    check(!fs::exists(root / "cancelled.jpg"), "Cancelled preview left a file");
    refuses<std::invalid_argument>([&] { client.projects(0, 201); });
    refuses<std::invalid_argument>([&] { client.projects(UINT64_MAX, 1); });
    refuses<std::invalid_argument>([&] { client.datasets(0); });
    check(std::async(std::launch::async,
                     [&] {
                         try {
                             client.image(7);
                         } catch (const std::exception&) {
                             return true;
                         }
                         return false;
                     })
              .get(),
          "Foreign owner used native OMERO client");
    check(script->at == script->steps.size(), "A refused operation made a hidden network request");
}
void failures() {
    for (const auto& url : {"http://omero.example/", "https://user:password@omero.example/",
                            "https://omero.example/?token=secret", "https://omero.example/../",
                            "https://omero.example:0/", "https://omero.example/a%2fb/"})
        refuses<std::invalid_argument>([&] { OmeroClient c(url, std::make_shared<Script>()); });
    for (const auto& content :
         {std::string("{\"data\":[],\"data\":[]}"), discovery("https://other.example/"),
          std::string(
              "{\"data\":[{\"version\":\"1\",\"url:base\":\"https://omero.example/api/v1/\"}]}")}) {
        auto s = std::make_shared<Script>();
        s->add("api/", content);
        OmeroClient c(server, s);
        refuses<std::invalid_argument>([&] { c.connect(); });
    }
    for (const auto status : {401, 403, 404, 500}) {
        auto s = std::make_shared<Script>();
        s->steps.push_back(
            {"api/", response("secret server error detail", "application/json", status)});
        OmeroClient c(server, s);
        try {
            c.connect();
            throw std::logic_error("HTTP failure accepted");
        } catch (const std::runtime_error& e) {
            check(std::string(e.what()).find("HTTP " + std::to_string(status)) !=
                          std::string::npos &&
                      std::string(e.what()).find("secret") == std::string::npos,
                  "Error lost status or disclosed server details");
        }
    }
    for (const auto& location : {"https://other.example/api/", "http://omero.example/api/",
                                 "//other.example/api/", "https://omero.example/../api/"}) {
        auto s = std::make_shared<Script>();
        auto r = response("", "text/html", 302);
        r.location = location;
        s->steps.push_back({"api/", r});
        OmeroClient c(server, s);
        refuses<std::invalid_argument>([&] { c.connect(); });
        check(s->at == 1, "Unsafe redirect was followed");
    }
    {
        auto s = std::make_shared<Script>();
        auto r = response("", "text/html", 302);
        r.location = "/api/";
        for (unsigned i = 0; i < 4; ++i)
            s->steps.push_back({"api/", r});
        OmeroClient c(server, s);
        refuses<std::invalid_argument>([&] { c.connect(); });
        check(s->at == 4, "Redirect bound differs");
    }
    {
        auto s = std::make_shared<Script>();
        auto r = response("", "text/html", 302);
        r.location = "/api/?canonical=1";
        s->steps.push_back({"api/", r});
        s->add("api/?canonical=1", discovery());
        OmeroClient c(server, s);
        c.connect();
    }
    {
        auto s = std::make_shared<Script>();
        s->steps.push_back({"api/", response("<html>login</html>", "text/html")});
        OmeroClient c(server, s);
        refuses<std::invalid_argument>([&] { c.connect(); });
    }
    {
        auto s = std::make_shared<Script>();
        s->add("api/", std::string(1025, ' '));
        OmeroLimits l;
        l.json_bytes = 1024;
        OmeroClient c(server, s, l);
        refuses<std::length_error>([&] { c.connect(); });
    }
    {
        auto s = std::make_shared<Script>();
        s->start();
        OmeroClient c(server, s);
        std::atomic<bool> stop{true};
        refuses<WorkflowCancelled>([&] { c.connect(&stop); });
        check(!s->at, "Pre-cancelled operation made a request");
        stop = false;
        s->cancel_after = &stop;
        refuses<WorkflowCancelled>([&] { c.connect(&stop); });
        check(s->at == 1, "Cancellation after I/O was ignored");
    }
    for (unsigned variant = 0; variant < 5; ++variant) {
        auto s = std::make_shared<Script>();
        s->start();
        auto bad = row();
        if (!variant)
            bad["@id"] = 8;
        else if (variant == 1)
            bad["@id"] = 7.0;
        else if (variant == 2)
            bad["Pixels"]["SizeX"] = 0;
        else if (variant == 3)
            bad["Pixels"]["Type"]["value"] = "mystery";
        else
            bad["Pixels"]["PhysicalSizeX"]["Value"] = -1;
        s->add("api/v0/m/images/7/", image(bad));
        OmeroClient c(server, s);
        c.connect();
        refuses<std::invalid_argument>([&] { c.image(7); });
    }
    {
        auto s = std::make_shared<Script>();
        s->start();
        s->add("api/v0/m/images/7/", image());
        s->steps.push_back(
            {"webclient/render_thumbnail/7/", {200, "image/jpeg", {}, {0xff, 0xd8, 0xff, 0xd9}}});
        auto changed = row();
        changed["Name"] = "Changed";
        s->add("api/v0/m/images/7/", image(changed));
        OmeroClient c(server, s);
        c.connect();
        refuses<WorkflowInputChanged>([&] { c.preview(7); });
    }
    {
        auto s = std::make_shared<Script>();
        s->start();
        Json page = {
            {"meta", {{"offset", 0}, {"limit", 2}, {"totalCount", 2}}},
            {"data", Json::array({{{"@id", 1}, {"Name", "A"}, {"@type", "OME#Project"}},
                                  {{"@id", 1}, {"Name", "B"}, {"@type", "OME#Project"}}})}};
        s->add("api/v0/m/projects/?offset=0&limit=2", page.dump());
        OmeroClient c(server, s);
        c.connect();
        refuses<std::invalid_argument>([&] { c.projects(0, 2); });
    }
}
void verify_project(const std::string& path, const std::string& id, const std::string& jpeg_sha) {
    auto p = Project::open_without_journal_recovery(path);
    check(p->images().size() == 1 && p->images()[0].embedded,
          "Preview project is not an owned offline snapshot");
    const auto d = p->document(p->images()[0].id);
    check(!d->pixel_width_um() && !d->pixel_height_um() && !d->server()->raw_metadata(),
          "Rendered thumbnail acquired raw capability/calibration");
    const auto props = d->image_data_properties();
    check(bool(props), "Preview source association missing");
    const auto& value =
        std::get<ImageDataValue::Map>(props->values().at("qpx.omero-preview/1").value);
    check(std::get<std::string>(value.at("image").value) == id &&
              std::get<std::string>(value.at("jpeg_sha256").value) == jpeg_sha &&
              std::get<std::string>(value.at("representation").value) ==
                  "uncalibrated-server-thumbnail-rgb8",
          "Native save/open lost exact preview identity/domain");
    check(d->metadata().width > 0 && d->metadata().height > 0, "Preview has no owned pixels");
    std::cout << "Public preview project: exact identity, owned RGB pixels, offline open, absent "
                 "raw/calibration pass\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--probe-public") {
            const auto r = native_omero_transport()->get(argv[2], 2u * 1024 * 1024, 30000, nullptr);
            std::cout << Json{{"status", r.status},
                              {"content_type", r.content_type},
                              {"bytes", r.body.size()}}
                             .dump()
                      << '\n';
            return r.status == 200 ? 0 : 1;
        }
        if (argc == 5 && std::string(argv[1]) == "--verify-project") {
            verify_project(argv[2], argv[3], argv[4]);
            return 0;
        }
        const auto root =
            fs::current_path() /
            ("omero-protocol-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root), "Unique OMERO fixture directory");
        protocol(root);
        failures();
        std::cout
            << "Native OMERO protocol: typed hierarchy/metadata/exact IDs, source-bound previews, "
               "redirects/HTTP/errors/limits/cancel/owner and atomic new-only export pass\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
