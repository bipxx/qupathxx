#include "qupathxx/omero.hpp"
#include "qupathxx/workflow.hpp"
#include "strict_json.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include "omero_io.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <thread>
namespace qpx {
namespace {
using Json = nlohmann::json;
std::string hash(const void* p, std::size_t n) {
    Sha256 h;
    h.update(p, n);
    return h.finish();
}
std::uint64_t number(const Json& v, std::uint64_t max = INT64_MAX) {
    std::uint64_t n;
    if (v.is_number_unsigned())
        n = v.get<std::uint64_t>();
    else if (v.is_number_integer() && v.get<std::int64_t>() >= 0)
        n = std::uint64_t(v.get<std::int64_t>());
    else
        throw std::invalid_argument("OMERO integer has an invalid type/sign");
    if (n > max)
        throw std::invalid_argument("OMERO integer exceeds its contract");
    return n;
}
std::string text(const Json& j, std::size_t max = 4096) {
    if (!j.is_string())
        throw std::invalid_argument("OMERO text has an invalid type");
    auto s = j.get<std::string>();
    detail::bounded_utf8(s, max);
    if (s.find('\0') != std::string::npos)
        throw std::invalid_argument("OMERO text contains NUL");
    return s;
}
void identifier(std::uint64_t id) {
    if (!id || id > INT64_MAX)
        throw std::invalid_argument("OMERO ID must be a positive Int64");
}
std::string base_url(std::string s) {
    if (s.size() > 2048 || s.rfind("https://", 0) != 0)
        throw std::invalid_argument("OMERO requires an HTTPS server URL");
    const auto slash = s.find('/', 8);
    const auto host = s.substr(8, slash == std::string::npos ? slash : slash - 8);
    if (host.empty() || host.size() > 255 ||
        host.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:") !=
            std::string::npos)
        throw std::invalid_argument("OMERO server must be a DNS/IPv4 hostname without credentials");
    const auto colon = host.find(':');
    auto name = host.substr(0, colon);
    if (name.empty() || name.front() == '.' || name.back() == '.' ||
        name.find("..") != std::string::npos)
        throw std::invalid_argument("Invalid OMERO hostname");
    unsigned port = 443;
    if (colon != std::string::npos) {
        const auto value = host.substr(colon + 1);
        const auto r = std::from_chars(value.data(), value.data() + value.size(), port);
        if (value.empty() || r.ec != std::errc{} || r.ptr != value.data() + value.size() || !port ||
            port > 65535)
            throw std::invalid_argument("Invalid OMERO HTTPS port");
    }
    for (auto& c : name)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    auto path = slash == std::string::npos ? std::string("/") : s.substr(slash);
    if (path.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_-.~") !=
            std::string::npos ||
        path.find("//") != std::string::npos || path.find("/../") != std::string::npos ||
        path.find("/./") != std::string::npos ||
        (path.size() >= 3 && path.substr(path.size() - 3) == "/..") ||
        (path.size() >= 2 && path.substr(path.size() - 2) == "/."))
        throw std::invalid_argument("Invalid OMERO server base path");
    if (path.back() != '/')
        path += '/';
    return "https://" + name + (port == 443 ? std::string{} : ":" + std::to_string(port)) + path;
}
std::string media(std::string s) {
    s = s.substr(0, s.find(';'));
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}
Json parse(const std::vector<std::uint8_t>& bytes) {
    detail::StrictJson sax("OMERO JSON");
    Json::sax_parse(bytes.begin(), bytes.end(), &sax);
    return Json::parse(bytes.begin(), bytes.end());
}
std::optional<OmeroQuantity> quantity(const Json& pixels, const char* key) {
    if (!pixels.contains(key))
        return {};
    const auto& j = pixels.at(key);
    if (!j.is_object() || !j.contains("Value") || !j.at("Value").is_number())
        throw std::invalid_argument("Invalid OMERO physical quantity");
    const auto value = j.at("Value").get<double>();
    if (!std::isfinite(value) || value <= 0)
        throw std::invalid_argument("Nonpositive/nonfinite OMERO physical quantity");
    return OmeroQuantity{value, text(j.at("Unit"), 128)};
}
Json image_value(const OmeroImageInfo& i) {
    Json j = {{"image", i.id},
              {"pixels", i.pixels_id},
              {"name", i.name},
              {"width", i.width},
              {"height", i.height},
              {"channels", i.channels},
              {"z_planes", i.z_planes},
              {"timepoints", i.timepoints},
              {"pixel_type", i.pixel_type},
              {"metadata_sha256", i.metadata_sha256}};
    if (i.ngff_url)
        j["ngff_url"] = *i.ngff_url;
    for (const auto& pair :
         {std::make_pair("physical_x", i.physical_x), std::make_pair("physical_y", i.physical_y)})
        if (pair.second)
            j[pair.first] = {{"value", pair.second->value}, {"unit", pair.second->unit}};
    return j;
}
std::optional<std::string> ngff_link(const Json& row) {
    if (!row.contains("omero:details") || !row.at("omero:details").contains("externalInfo"))
        return {};
    const auto& e = row.at("omero:details").at("externalInfo");
    if (!e.is_object() ||
        e.value("EntityType", std::string{}) != "com.glencoesoftware.ngff:multiscales")
        return {};
    auto url = text(e.at("Lsid"), 4096);
    const std::string suffix = "?anonymous=true";
    if (url.rfind("s3://", 0) != 0 || url.size() <= suffix.size() ||
        url.substr(url.size() - suffix.size()) != suffix)
        return {};
    url = "https://" + url.substr(5, url.size() - 5 - suffix.size());
    return base_url(url);
}
} // namespace
void OmeroLimits::validate() const {
    if (json_bytes < 1024 || json_bytes > 16u * 1024 * 1024 || preview_bytes < 1024 ||
        preview_bytes > 64u * 1024 * 1024 || !max_preview_pixels ||
        max_preview_pixels > 16u * 1024 * 1024 || timeout_ms < 100 || timeout_ms > 60000)
        throw std::invalid_argument("Invalid OMERO response/pixel/timeout limits");
}
struct OmeroClient::Impl {
    std::thread::id owner = std::this_thread::get_id();
    std::string server;
    std::shared_ptr<OmeroTransport> transport;
    OmeroLimits limits;
    bool connected = false;
    Impl(std::string s, std::shared_ptr<OmeroTransport> t, OmeroLimits l)
        : server(base_url(std::move(s))), transport(std::move(t)), limits(l) {
        limits.validate();
        if (!transport)
            throw std::invalid_argument("OMERO needs a native transport");
    }
    void ready() const {
        workflow_require_owner(owner);
        if (!connected)
            throw std::logic_error("Connect to OMERO before querying data");
    }
    OmeroHttpResponse get(const std::string& path, const char* type, std::size_t cap,
                          const std::atomic<bool>* cancelled) {
        workflow_require_owner(owner);
        auto url = server + path;
        const auto origin = server.substr(0, server.find('/', 8));
        for (unsigned redirects = 0;; ++redirects) {
            workflow_checkpoint(cancelled);
            auto r = transport->get(url, cap, limits.timeout_ms, cancelled);
            workflow_checkpoint(cancelled);
            if (r.body.size() > cap)
                throw std::length_error("OMERO response exceeds byte limit");
            if (r.status == 301 || r.status == 302 || r.status == 303 || r.status == 307 ||
                r.status == 308) {
                if (redirects == 3 || r.location.empty() || r.location.size() > 4096 ||
                    r.location.find_first_of("\r\n\\#") != std::string::npos ||
                    r.location.find('\0') != std::string::npos)
                    throw std::invalid_argument("Invalid/excessive OMERO redirects");
                if (r.location.rfind("https://", 0) == 0)
                    url = r.location;
                else if (r.location[0] == '/' && r.location.rfind("//", 0) != 0)
                    url = origin + r.location;
                else
                    throw std::invalid_argument(
                        "OMERO redirect must be an absolute same-server URL/path");
                if (url.rfind(server, 0) != 0 || url.find("/../") != std::string::npos ||
                    url.find("/./") != std::string::npos || url.find('%') != std::string::npos)
                    throw std::invalid_argument(
                        "OMERO redirect leaves the selected HTTPS server/base path");
                continue;
            }
            if (r.status != 200)
                throw std::runtime_error("OMERO request failed with HTTP " +
                                         std::to_string(r.status));
            if (media(r.content_type) != type)
                throw std::invalid_argument("OMERO returned an unexpected content type (a "
                                            "login/viewer page is not image data)");
            return r;
        }
    }
    OmeroPage page(const std::string& path, const std::string& kind, std::uint64_t offset,
                   std::uint32_t limit, const std::atomic<bool>* cancelled) {
        ready();
        if (!limit || limit > 200 || offset > INT64_MAX - limit)
            throw std::invalid_argument("OMERO page needs limit1..200 and a bounded offset");
        const auto r =
            get(path + "?offset=" + std::to_string(offset) + "&limit=" + std::to_string(limit),
                "application/json", limits.json_bytes, cancelled);
        const auto j = parse(r.body);
        if (!j.is_object() || !j.at("data").is_array() || j.at("data").size() > limit)
            throw std::invalid_argument("Invalid OMERO page shape/count");
        OmeroPage out;
        out.offset = number(j.at("meta").at("offset"));
        out.total = number(j.at("meta").at("totalCount"));
        if (out.offset != offset || number(j.at("meta").at("limit"), 200) < j.at("data").size() ||
            out.total < offset && !j.at("data").empty() ||
            out.total >= offset && j.at("data").size() > out.total - offset)
            throw std::invalid_argument("Inconsistent OMERO pagination");
        std::set<std::uint64_t> seen;
        for (const auto& row : j.at("data")) {
            workflow_checkpoint(cancelled);
            const auto type = text(row.at("@type"), 256);
            const auto suffix = "#" + kind;
            if (type.size() < suffix.size() || type.substr(type.size() - suffix.size()) != suffix)
                throw std::invalid_argument("OMERO returned another object kind");
            const auto id = number(row.at("@id"));
            identifier(id);
            if (!seen.insert(id).second)
                throw std::invalid_argument("Duplicate OMERO object ID in page");
            out.objects.push_back({id, text(row.at("Name"))});
        }
        return out;
    }
};
OmeroClient::OmeroClient(std::string s, std::shared_ptr<OmeroTransport> t, OmeroLimits l)
    : impl_(std::make_unique<Impl>(std::move(s), std::move(t), l)) {}
OmeroClient::~OmeroClient() = default;
const std::string& OmeroClient::server() const {
    return impl_->server;
}
void OmeroClient::connect(const std::atomic<bool>* cancelled) {
    auto& d = *impl_;
    workflow_require_owner(d.owner);
    d.connected = false;
    const auto r = d.get("api/", "application/json", d.limits.json_bytes, cancelled);
    const auto j = parse(r.body);
    bool found = false;
    if (!j.is_object() || !j.at("data").is_array() || j.at("data").size() > 32)
        throw std::invalid_argument("Invalid OMERO API discovery");
    for (const auto& v : j.at("data"))
        if (text(v.at("version"), 16) == "0") {
            if (found || text(v.at("url:base"), 4096) != d.server + "api/v0/")
                throw std::invalid_argument("OMERO API base differs from selected HTTPS server");
            found = true;
        }
    if (!found)
        throw std::invalid_argument("OMERO JSON API v0 is unavailable");
    d.connected = true;
}
OmeroPage OmeroClient::projects(std::uint64_t o, std::uint32_t l, const std::atomic<bool>* c) {
    return impl_->page("api/v0/m/projects/", "Project", o, l, c);
}
OmeroPage OmeroClient::datasets(std::uint64_t id, std::uint64_t o, std::uint32_t l,
                                const std::atomic<bool>* c) {
    identifier(id);
    return impl_->page("api/v0/m/projects/" + std::to_string(id) + "/datasets/", "Dataset", o, l,
                       c);
}
OmeroPage OmeroClient::images(std::uint64_t id, std::uint64_t o, std::uint32_t l,
                              const std::atomic<bool>* c) {
    identifier(id);
    return impl_->page("api/v0/m/datasets/" + std::to_string(id) + "/images/", "Image", o, l, c);
}
OmeroImageInfo OmeroClient::image(std::uint64_t id, const std::atomic<bool>* cancelled) {
    impl_->ready();
    identifier(id);
    const auto r = impl_->get("api/v0/m/images/" + std::to_string(id) + "/", "application/json",
                              impl_->limits.json_bytes, cancelled);
    const auto j = parse(r.body);
    auto row = j.at("data");
    if (row.is_array()) {
        if (row.size() != 1)
            throw std::invalid_argument("OMERO single-image response has multiple/no images");
        row = row.at(0);
    }
    const auto type = text(row.at("@type"), 256);
    if (type.size() < 6 || type.substr(type.size() - 6) != "#Image")
        throw std::invalid_argument("OMERO response is not an image");
    const auto& p = row.at("Pixels");
    OmeroImageInfo out;
    out.id = number(row.at("@id"));
    out.pixels_id = number(p.at("@id"));
    identifier(out.pixels_id);
    if (out.id != id)
        throw std::invalid_argument("OMERO returned another image ID");
    out.name = text(row.at("Name"));
    out.width = number(p.at("SizeX"), 1000000000000ULL);
    out.height = number(p.at("SizeY"), 1000000000000ULL);
    out.channels = std::uint32_t(number(p.at("SizeC"), 65535));
    out.z_planes = std::uint32_t(number(p.at("SizeZ"), 65535));
    out.timepoints = std::uint32_t(number(p.at("SizeT"), 65535));
    if (!out.width || !out.height || !out.channels || !out.z_planes || !out.timepoints)
        throw std::invalid_argument("OMERO dimensions must be positive");
    out.pixel_type = text(p.at("Type").at("value"), 32);
    if (std::set<std::string>{"bit", "int8", "uint8", "int16", "uint16", "int32", "uint32", "float",
                              "double", "complex", "double-complex"}
            .count(out.pixel_type) != 1)
        throw std::invalid_argument("Unknown OMERO pixel type");
    out.physical_x = quantity(p, "PhysicalSizeX");
    out.physical_y = quantity(p, "PhysicalSizeY");
    out.ngff_url = ngff_link(row);
    const auto canonical = row.dump();
    out.metadata_sha256 = hash(canonical.data(), canonical.size());
    return out;
}
std::vector<std::uint8_t> OmeroClient::json_get(const std::string& path,
                                                const std::atomic<bool>* c) {
    impl_->ready();
    return impl_->get(path, "application/json", impl_->limits.json_bytes, c).body;
}
std::vector<std::uint8_t> OmeroClient::mask_get(std::uint64_t id, std::size_t cap,
                                                const std::atomic<bool>* c) {
    impl_->ready();
    identifier(id);
    return impl_
        ->get("webgateway/render_shape_mask/" + std::to_string(id) + "/", "image/png", cap, c)
        .body;
}
OmeroHttpResponse OmeroClient::store_get(const std::string& base, const std::string& key,
                                         std::size_t cap, const std::atomic<bool>* c) {
    impl_->ready();
    if (base_url(base) != base || key.empty() || key.front() == '/' ||
        key.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_-.~") !=
            std::string::npos ||
        key.find("..") != std::string::npos || key.find("//") != std::string::npos)
        throw std::invalid_argument("Invalid anonymous NGFF resource");
    workflow_checkpoint(c);
    auto r = impl_->transport->get(base + key, cap, impl_->limits.timeout_ms, c);
    workflow_checkpoint(c);
    if (r.body.size() > cap)
        throw std::length_error("NGFF resource exceeds byte limit");
    // External stores get independent anonymous requests. Never follow an
    // object-store redirect or interpret an unavailable chunk as a fill chunk.
    if (r.status != 200)
        throw std::runtime_error("NGFF request failed with HTTP " + std::to_string(r.status));
    const auto type = media(r.content_type);
    if (type != "application/octet-stream" && type != "application/json" &&
        type != "binary/octet-stream")
        throw std::invalid_argument("NGFF returned an unexpected content type");
    return r;
}
struct OmeroPreview::Data {
    OmeroImageInfo image;
    std::string server, digest;
    std::vector<std::uint8_t> bytes;
};
OmeroPreview::OmeroPreview(std::shared_ptr<const Data> d) : data_(std::move(d)) {}
const OmeroImageInfo& OmeroPreview::image() const {
    return data_->image;
}
const std::string& OmeroPreview::server() const {
    return data_->server;
}
const std::vector<std::uint8_t>& OmeroPreview::jpeg() const {
    return data_->bytes;
}
const std::string& OmeroPreview::sha256() const {
    return data_->digest;
}
std::shared_ptr<const OmeroPreview> OmeroClient::preview(std::uint64_t id,
                                                         const std::atomic<bool>* cancelled) {
    const auto before = image(id, cancelled);
    auto r = impl_->get("webclient/render_thumbnail/" + std::to_string(id) + "/", "image/jpeg",
                        impl_->limits.preview_bytes, cancelled);
    if (r.body.size() < 4 || r.body[0] != 0xff || r.body[1] != 0xd8 ||
        r.body[r.body.size() - 2] != 0xff || r.body.back() != 0xd9)
        throw std::invalid_argument("OMERO preview is not a complete JPEG stream");
    if (image(id, cancelled).metadata_sha256 != before.metadata_sha256)
        throw WorkflowInputChanged("OMERO image metadata changed while retrieving preview");
    auto d = std::make_shared<OmeroPreview::Data>();
    d->image = before;
    d->server = impl_->server;
    d->digest = hash(r.body.data(), r.body.size());
    d->bytes = std::move(r.body);
    return std::shared_ptr<const OmeroPreview>(new OmeroPreview(std::move(d)));
}
std::string omero_image_json(const OmeroImageInfo& i) {
    return image_value(i).dump();
}
void save_omero_preview(const OmeroPreview& p, const std::string& output,
                        const std::atomic<bool>* cancelled) {
    detail::save_new_omero_file(output, p.jpeg().data(), p.jpeg().size(), cancelled);
}
} // namespace qpx
