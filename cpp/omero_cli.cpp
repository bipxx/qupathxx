#include "omero_cli.hpp"
#include "qupathxx/omero_rois.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "export_output.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace qpx::detail {
namespace {
using Json = nlohmann::json;
enum class Command {
    Projects,
    Datasets,
    Images,
    Image,
    Preview,
    PreviewProject,
    Raw,
    Rois,
    RoiExchange
};

struct Options {
    Command command;
    std::string server, output, manifest, input, receipt;
    std::uint64_t id = 0, offset = 0;
    std::uint32_t limit = 100;
    OmeroLimits limits;
    OmeroRawOptions raw_options;
    OmeroRoiOptions roi_options;
    RawRegionRequest raw;
};

constexpr const char* usage =
    "Usage: qupathxx_omero "
    "projects|datasets|images|image|preview|preview-project|raw|rois|roi-exchange --server "
    "HTTPS_URL [--id ID --offset N --limit N --output NEW_FILE --reader-plugins ABS_MANIFEST "
    "--timeout-ms N --preview-bytes N]\n"
    "raw: --x N --y N --width N --height N [--first-channel N --channels N --z N --t N] --output "
    "NEW.npy --receipt NEW.json [--transfer-bytes N --max-chunks N]\n"
    "rois: --id IMAGE [--z N --t N] --output NEW.geojson [--reader-plugins ABS_MANIFEST]\n"
    "roi-exchange: --id IMAGE [--z N --t N] --input EDITED.geojson --output NEW.json "
    "[--reader-plugins ABS_MANIFEST]\n"
    "Public, GET-only OMERO.web API v0. Raw blocks require an explicit anonymous open NGFF "
    "association. ROI updates/results are offline files; no server writes. Previews remain "
    "uncalibrated rendered thumbnails.\n";

Command command(const std::string& name) {
    static const std::pair<const char*, Command> commands[] = {
        {"projects", Command::Projects},
        {"datasets", Command::Datasets},
        {"images", Command::Images},
        {"image", Command::Image},
        {"preview", Command::Preview},
        {"preview-project", Command::PreviewProject},
        {"raw", Command::Raw},
        {"rois", Command::Rois},
        {"roi-exchange", Command::RoiExchange}};
    for (const auto& entry : commands)
        if (name == entry.first)
            return entry.second;
    throw std::invalid_argument("Unknown OMERO command");
}

bool lists_objects(Command c) {
    return c == Command::Projects || c == Command::Datasets || c == Command::Images;
}
bool previews_image(Command c) {
    return c == Command::Preview || c == Command::PreviewProject;
}
bool writes_file(Command c) {
    return previews_image(c) || c == Command::Raw || c == Command::Rois ||
           c == Command::RoiExchange;
}
bool reads_plugins(Command c) {
    return c == Command::PreviewProject || c == Command::Raw || c == Command::Rois ||
           c == Command::RoiExchange;
}
bool selects_roi_plane(Command c) {
    return c == Command::Rois || c == Command::RoiExchange;
}

std::uint64_t integer(const std::string& text) {
    std::uint64_t n = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), n);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Expected an unsigned OMERO argument");
    return n;
}
std::uint64_t bounded_integer(const std::string& text, std::uint64_t maximum, const char* message) {
    const auto n = integer(text);
    if (n > maximum)
        throw std::invalid_argument(message);
    return n;
}
bool contains(std::initializer_list<const char*> values, const std::string& value) {
    for (const auto* candidate : values)
        if (value == candidate)
            return true;
    return false;
}
bool raw_option(const std::string& flag) {
    return contains({"--x", "--y", "--width", "--height", "--first-channel", "--channels", "--z",
                     "--t", "--transfer-bytes", "--max-chunks"},
                    flag);
}
void require_option(Command c, const std::string& flag) {
    bool allowed;
    if (flag == "--server" || flag == "--timeout-ms")
        allowed = true;
    else if (flag == "--id")
        allowed = c != Command::Projects;
    else if (flag == "--offset" || flag == "--limit")
        allowed = lists_objects(c);
    else if (flag == "--output")
        allowed = writes_file(c);
    else if (flag == "--preview-bytes")
        allowed = previews_image(c);
    else if (flag == "--reader-plugins")
        allowed = reads_plugins(c);
    else if (flag == "--receipt")
        allowed = c == Command::Raw;
    else if (flag == "--input")
        allowed = c == Command::RoiExchange;
    else if (flag == "--z" || flag == "--t")
        allowed = c == Command::Raw || selects_roi_plane(c);
    else if (raw_option(flag))
        allowed = c == Command::Raw;
    else
        throw std::invalid_argument("Unknown OMERO option: " + flag);
    if (!allowed)
        throw std::invalid_argument("OMERO option does not match the selected command: " + flag);
}

void parse_raw_option(Options& options, const std::string& flag, const std::string& value) {
    const auto n = bounded_integer(value, INT64_MAX, "Raw argument exceeds Int64");
    if (flag == "--x")
        options.raw.region.x = std::int64_t(n);
    else if (flag == "--y")
        options.raw.region.y = std::int64_t(n);
    else if (flag == "--width")
        options.raw.region.width = std::int64_t(n);
    else if (flag == "--height")
        options.raw.region.height = std::int64_t(n);
    else if (flag == "--transfer-bytes")
        options.raw_options.transfer_bytes = std::size_t(n);
    else {
        if (n > UINT32_MAX)
            throw std::invalid_argument("Raw channel/plane/count exceeds UInt32");
        const auto axis = std::uint32_t(n);
        if (flag == "--first-channel")
            options.raw.first_channel = axis;
        else if (flag == "--channels")
            options.raw.channels = axis;
        else if (flag == "--z")
            options.raw.z = axis;
        else if (flag == "--t")
            options.raw.t = axis;
        else
            options.raw_options.max_chunks = axis;
    }
}

Options parse_options(const std::vector<std::string>& args) {
    Options options{command(args.front())};
    std::set<std::string> seen;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto& flag = args[i];
        if (!seen.insert(flag).second || ++i == args.size())
            throw std::invalid_argument("Duplicate/missing OMERO option value");
        require_option(options.command, flag);
        const auto& value = args[i];
        if (flag == "--server")
            options.server = value;
        else if (flag == "--output")
            options.output = value;
        else if (flag == "--reader-plugins")
            options.manifest = value;
        else if (flag == "--input")
            options.input = value;
        else if (flag == "--receipt")
            options.receipt = value;
        else if (flag == "--id")
            options.id = integer(value);
        else if (flag == "--offset")
            options.offset = integer(value);
        else if (flag == "--limit") {
            options.limit =
                std::uint32_t(bounded_integer(value, 200, "OMERO page limit must be1..200"));
            if (!options.limit)
                throw std::invalid_argument("OMERO page limit must be1..200");
        } else if (flag == "--timeout-ms")
            options.limits.timeout_ms =
                std::uint32_t(bounded_integer(value, 60000, "OMERO timeout exceeds60000ms"));
        else if (flag == "--preview-bytes")
            options.limits.preview_bytes = std::size_t(
                bounded_integer(value, 64u * 1024 * 1024, "OMERO preview limit exceeds64MiB"));
        else if (selects_roi_plane(options.command) && (flag == "--z" || flag == "--t")) {
            const auto axis =
                std::uint32_t(bounded_integer(value, UINT32_MAX, "ROI plane exceeds UInt32"));
            if (flag == "--z")
                options.roi_options.z = axis;
            else
                options.roi_options.t = axis;
        } else
            parse_raw_option(options, flag, value);
    }
    if (options.server.empty() || (options.command != Command::Projects && !options.id) ||
        (writes_file(options.command) && options.output.empty()) ||
        (seen.count("--reader-plugins") && options.manifest.empty()) ||
        (options.command == Command::Raw &&
         (options.receipt.empty() || !seen.count("--width") || !seen.count("--height"))) ||
        (options.command == Command::RoiExchange && options.input.empty()))
        throw std::invalid_argument("OMERO arguments do not match the selected command");
    options.limits.validate();
    options.raw_options.validate();
    return options;
}

std::string read_roi_input(const std::string& path) {
    constexpr std::size_t max_bytes = 16u * 1024 * 1024;
    if (std::filesystem::file_size(std::filesystem::u8path(path)) > max_bytes)
        throw std::length_error("OMERO ROI input exceeds16MiB");
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open OMERO ROI input");
    std::string text(max_bytes + 1, '\0');
    file.read(text.data(), std::streamsize(text.size()));
    const auto bytes = file.gcount();
    if (file.bad())
        throw std::runtime_error("Cannot read OMERO ROI input");
    if (bytes > max_bytes)
        throw std::length_error("OMERO ROI input grew beyond16MiB");
    text.resize(std::size_t(bytes));
    return text;
}

void preflight(const Options& options) {
    if (writes_file(options.command))
        require_new_export(export_path(options.output));
    if (options.command == Command::Raw) {
        require_new_export(export_path(options.receipt));
        guard_export_output(options.receipt, {options.output});
    }
}

OmeroPage list_page(OmeroClient& client, const Options& options,
                    const std::atomic<bool>* cancelled) {
    switch (options.command) {
    case Command::Projects:
        return client.projects(options.offset, options.limit, cancelled);
    case Command::Datasets:
        return client.datasets(options.id, options.offset, options.limit, cancelled);
    case Command::Images:
        return client.images(options.id, options.offset, options.limit, cancelled);
    default:
        throw std::logic_error("OMERO command is not a listing");
    }
}

Json execute(const Options& options, const std::atomic<bool>& cancelled,
             std::shared_ptr<OmeroTransport> transport) {
    preflight(options);
    const auto input =
        options.command == Command::RoiExchange ? read_roi_input(options.input) : std::string{};
    OmeroClient client(options.server, transport ? std::move(transport) : native_omero_transport(),
                       options.limits);
    client.connect(&cancelled);
    Json report = {{"schema", "qpx.omero-cli/1"}, {"server", client.server()}};
    if (lists_objects(options.command)) {
        const auto page = list_page(client, options, &cancelled);
        report["state"] = "listed";
        report["offset"] = page.offset;
        report["total"] = page.total;
        report["objects"] = Json::array();
        for (const auto& object : page.objects)
            report["objects"].push_back({{"id", object.id}, {"name", object.name}});
    } else if (options.command == Command::Image) {
        report["state"] = "image";
        report["image"] = Json::parse(omero_image_json(client.image(options.id, &cancelled)));
    } else if (options.command == Command::Raw) {
        const auto result = client.raw(options.id, options.raw, options.raw_options, &cancelled);
        save_omero_raw(*result, options.output, &cancelled);
        save_omero_raw_receipt(*result, options.receipt, &cancelled);
        report["state"] = "saved";
        report["representation"] = "original-channel-samples";
        report["receipt"] = Json::parse(result->receipt_json());
    } else if (options.command == Command::Rois) {
        const auto result = client.rois(options.id, options.roi_options, &cancelled);
        save_omero_roi_text(omero_rois_geojson(*result), options.output, &cancelled);
        report["state"] = "saved";
        report["shapes"] = result->shapes().size();
        report["rois_sha256"] = result->sha256();
    } else if (options.command == Command::RoiExchange) {
        const auto current = client.rois(options.id, options.roi_options, &cancelled);
        const auto exchanged = omero_rois_exchange(input, *current);
        save_omero_roi_text(exchanged, options.output, &cancelled);
        report["state"] = "saved-offline-exchange";
        report["shapes"] = Json::parse(exchanged).at("results").size();
    } else {
        const auto result = client.preview(options.id, &cancelled);
        if (options.command == Command::PreviewProject)
            save_omero_preview_project(*result, options.output, options.limits, &cancelled);
        else
            save_omero_preview(*result, options.output, &cancelled);
        report["state"] = "saved";
        report["representation"] = "uncalibrated-server-thumbnail-rgb8";
        report["jpeg_sha256"] = result->sha256();
        report["image"] = Json::parse(omero_image_json(result->image()));
    }
    return report;
}
} // namespace

int omero_cli(const std::vector<std::string>& args, const std::atomic<bool>& cancelled,
              std::shared_ptr<OmeroTransport> transport) {
    try {
        if (args.empty() || args == std::vector<std::string>{"--help"}) {
            std::cout << usage;
            return args.empty() ? 2 : 0;
        }
        const auto options = parse_options(args);
        if (!options.manifest.empty())
            configure_reader_plugins(options.manifest);
        try {
            std::cout << execute(options, cancelled, std::move(transport)).dump() << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return cancelled.load() ? 130 : 1;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
} // namespace qpx::detail
