// SPDX-License-Identifier: AGPL-3.0-only
#include "bdv_xml.hpp"
#include "utf8_text.hpp"

#include <tinyxml2.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace qpx::detail {
namespace {

using tinyxml2::XMLElement;
constexpr std::size_t xml_limit = 64 * 1024;
constexpr std::uint32_t max_view_id = 999999;
constexpr std::uint32_t max_time_count = 4096;
constexpr std::size_t max_setups = 64;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}

constexpr std::string_view xml_whitespace = " \t\r\n";

void validate_xml_characters(std::string_view xml) {
    for (const unsigned char c : xml)
        require(c >= 32 || c == '\t' || c == '\n' || c == '\r',
            "BigDataViewer XML contains a forbidden control character");
    for (std::size_t start = 0; (start = xml.find("&#", start)) != std::string_view::npos; ++start) {
        auto end = xml.find(';', start + 2);
        require(end != std::string_view::npos && end - start <= 12,
            "Invalid BigDataViewer XML character reference");
        auto digits = xml.substr(start + 2, end - start - 2);
        const bool hex = !digits.empty() && digits.front() == 'x';
        if (hex)
            digits.remove_prefix(1);
        std::uint32_t cp = 0;
        const auto number_end = digits.data() + digits.size();
        const auto parsed = std::from_chars(digits.data(), number_end, cp, hex ? 16 : 10);
        require(parsed.ec == std::errc{} && parsed.ptr == number_end,
            "Invalid BigDataViewer XML character reference");
        require(cp == 9 || cp == 10 || cp == 13 || (cp >= 32 && cp <= 0xd7ff) ||
            (cp >= 0xe000 && cp <= 0xfffd) || (cp >= 0x10000 && cp <= 0x10ffff),
            "BigDataViewer XML character reference cannot be represented");
        start = end;
    }
}

std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(xml_whitespace);
    if (start == std::string_view::npos)
        return {};
    return value.substr(start, value.find_last_not_of(xml_whitespace) - start + 1);
}

void attributes(const XMLElement& element, std::initializer_list<std::string_view> allowed) {
    for (auto attr = element.FirstAttribute(); attr; attr = attr->Next()) {
        bool known = false;
        for (auto name : allowed)
            known = known || name == attr->Name();
        require(known, "Unsupported BigDataViewer XML attribute");
    }
}

std::string_view attribute(const XMLElement& element, const char* name) {
    const auto value = element.Attribute(name);
    require(value != nullptr, "Missing BigDataViewer XML attribute");
    return value;
}

void children(const XMLElement& parent, std::initializer_list<std::string_view> allowed) {
    for (auto node = parent.FirstChild(); node; node = node->NextSibling()) {
        if (const auto child = node->ToElement()) {
            bool known = false;
            for (auto name : allowed)
                known = known || name == child->Name();
            require(known, "Unsupported BigDataViewer XML element");
        } else if (!node->ToComment()) {
            const auto value = node->ToText();
            require(value && trim(value->Value()).empty(), "Unexpected BigDataViewer XML content");
        }
    }
}

const XMLElement& one(const XMLElement& parent, const char* name) {
    const auto child = parent.FirstChildElement(name);
    require(child && !child->NextSiblingElement(name), "Missing or duplicate BigDataViewer XML element");
    return *child;
}

std::string_view content(const XMLElement& element) {
    const tinyxml2::XMLText* value = nullptr;
    for (auto node = element.FirstChild(); node; node = node->NextSibling()) {
        if (node->ToComment())
            continue;
        const auto text = node->ToText();
        require(text && !value, "Expected single BigDataViewer XML text value");
        value = text;
    }
    require(value != nullptr, "Missing BigDataViewer XML text value");
    const auto result = trim(value->Value());
    require(!result.empty(), "Empty BigDataViewer XML text value");
    return result;
}

std::string_view leaf(const XMLElement& element) {
    attributes(element, {});
    return content(element);
}

std::string_view token(std::string_view& remaining) {
    const auto start = remaining.find_first_not_of(xml_whitespace);
    require(start != std::string_view::npos, "Too few BigDataViewer XML numbers");
    remaining.remove_prefix(start);
    const auto end = remaining.find_first_of(xml_whitespace);
    const auto result = remaining.substr(0, end);
    remaining.remove_prefix(result.size());
    return result;
}

template <class T>
T integer(std::string_view value) {
    T result{};
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    require(parsed.ec == std::errc{} && parsed.ptr == end, "Invalid BigDataViewer XML integer");
    return result;
}

std::uint32_t view_id(std::string_view value) {
    const auto result = integer<std::uint32_t>(value);
    require(result <= max_view_id, "BigDataViewer setup or timepoint ID exceeds limit");
    return result;
}

double real(std::string_view value) {
    double result{};
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result, std::chars_format::general);
    require(parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(result),
        "Invalid BigDataViewer XML number");
    return result;
}

template <std::size_t Count, class Read>
auto numbers(std::string_view value, Read read) {
    std::array<decltype(read(value)), Count> result{};
    for (auto& item : result)
        item = read(token(value));
    require(trim(value).empty(), "Too many BigDataViewer XML numbers");
    return result;
}

double micrometer_scale(std::string_view unit) {
    if (unit == "micrometer" || unit == "um" || unit == u8"µm" || unit == u8"μm")
        return 1.0;
    if (unit == "nanometer" || unit == "nm")
        return 0.001;
    if (unit == "millimeter" || unit == "mm")
        return 1000.0;
    throw std::invalid_argument("Unsupported BigDataViewer physical length unit");
}

bool reserved_windows_name(std::string_view part) {
    const auto stem = trim(part.substr(0, part.find('.')));
    std::string upper(stem);
    for (auto& c : upper)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
        upper == "CONIN$" || upper == "CONOUT$")
        return true;
    if (upper.rfind("COM", 0) != 0 && upper.rfind("LPT", 0) != 0)
        return false;
    const auto suffix = std::string_view(upper).substr(3);
    return (suffix.size() == 1 && suffix[0] >= '1' && suffix[0] <= '9') ||
        suffix == u8"¹" || suffix == u8"²" || suffix == u8"³";
}

std::string local_path(const XMLElement& element, bool windows_separators) {
    attributes(element, {"type"});
    require(attribute(element, "type") == "relative", "BigDataViewer data path must be relative");
    auto path = std::string(content(element));
    if (windows_separators)
        std::replace(path.begin(), path.end(), '\\', '/');
    require(path.size() <= 1024 && path.find_first_of("\\:<>\"|?*") == std::string_view::npos,
        "BigDataViewer data path contains an unsupported character");
    auto remaining = std::string_view(path);
    for (;;) {
        const auto separator = remaining.find('/');
        const auto part = remaining.substr(0, separator);
        require(!part.empty() && part.front() != ' ' && part.back() != '.' && part.back() != ' ' &&
            !reserved_windows_name(part), "BigDataViewer data path is not canonical and local");
        for (const unsigned char c : part)
            require(c >= 32 && c != 127, "BigDataViewer data path contains a control character");
        if (separator == std::string_view::npos)
            break;
        remaining.remove_prefix(separator + 1);
    }
    return path;
}

std::vector<std::uint32_t> partition_ids(const XMLElement& element) {
    std::vector<std::uint32_t> ids;
    std::set<std::uint32_t> seen;
    auto text = leaf(element);
    while (!trim(text).empty()) {
        const auto id = view_id(token(text));
        require(ids.size() < max_time_count && seen.insert(id).second,
            "BigDataViewer partition has too many or duplicate IDs");
        ids.push_back(id);
    }
    return ids;
}

std::size_t view_index(const BdvXmlProject& project, std::uint32_t id, std::uint32_t time) {
    const auto setup = std::lower_bound(project.setups.begin(), project.setups.end(), id,
        [](const BdvXmlSetup& candidate, std::uint32_t value) { return candidate.id < value; });
    require(setup != project.setups.end() && setup->id == id && time >= project.first_time &&
        time - project.first_time < project.time_count, "Unknown BigDataViewer setup or timepoint");
    return static_cast<std::size_t>(setup - project.setups.begin()) * project.time_count +
        (time - project.first_time);
}

void parse_missing_views(const XMLElement& sequence, BdvXmlProject& project) {
    project.missing_views.resize(project.setups.size() * project.time_count);
    if (!sequence.FirstChildElement("MissingViews"))
        return;
    const auto& missing = one(sequence, "MissingViews");
    attributes(missing, {});
    children(missing, {"MissingView"});
    for (auto view = missing.FirstChildElement(); view; view = view->NextSiblingElement()) {
        attributes(*view, {"setup", "timepoint"});
        children(*view, {});
        const auto index = view_index(project, view_id(attribute(*view, "setup")), view_id(attribute(*view, "timepoint")));
        require(!project.missing_views[index], "Duplicate BigDataViewer missing view");
        project.missing_views[index] = true;
    }
    require(std::find(project.missing_views.begin(), project.missing_views.end(), false) != project.missing_views.end(),
        "BigDataViewer project has no available views");
}

void parse_loader(const XMLElement& loader, BdvStorage storage, BdvXmlProject& project) {
    const bool hdf5 = storage == BdvStorage::Hdf5;
    attributes(loader, {"format"});
    require(attribute(loader, "format") == (hdf5 ? "bdv.hdf5" : "bdv.n5"),
        "BigDataViewer loader differs from the selected backend");
    if (hdf5)
        children(loader, {"hdf5", "partition"});
    else
        children(loader, {"n5"});
    project.data_path = local_path(one(loader, hdf5 ? "hdf5" : "n5"), hdf5);
    const auto* part = loader.FirstChildElement("partition");
    if (!part)
        return;
    const auto views = project.setups.size() * project.time_count;
    project.partition_views.resize(views, {views, 0, 0});
    std::set<std::string> paths{project.data_path};
    for (; part; part = part->NextSiblingElement("partition")) {
        attributes(*part, {});
        children(*part, {"path", "timepoints", "timepointsMapped", "setups", "setupsMapped"});
        auto path = local_path(one(*part, "path"), true);
        require(paths.insert(path).second, "BigDataViewer partition file is declared more than once");
        const auto times = partition_ids(one(*part, "timepoints"));
        const auto setups = partition_ids(one(*part, "setups"));
        const auto local_times = part->FirstChildElement("timepointsMapped") ?
            partition_ids(one(*part, "timepointsMapped")) : times;
        const auto local_setups = part->FirstChildElement("setupsMapped") ?
            partition_ids(one(*part, "setupsMapped")) : setups;
        require(times.size() == local_times.size() && setups.size() == local_setups.size() &&
            times.size() <= project.time_count && setups.size() <= project.setups.size(),
            "BigDataViewer partition map has invalid lengths");
        const auto file = project.partition_paths.size();
        for (std::size_t s = 0; s < setups.size(); ++s) {
            for (std::size_t t = 0; t < times.size(); ++t) {
                auto& view = project.partition_views[view_index(project, setups[s], times[t])];
                require(view.file == views, "Overlapping BigDataViewer partition views");
                view = {file, local_setups[s], local_times[t]};
            }
        }
        project.partition_paths.push_back(std::move(path));
    }
    for (std::size_t index = 0; index < views; ++index)
        require(project.missing_views[index] || project.partition_views[index].file != views,
            "Incomplete BigDataViewer partition views");
}

void registration(const XMLElement& registrations, const BdvXmlProject& project) {
    attributes(registrations, {});
    children(registrations, {"ViewRegistration"});
    std::vector<bool> seen(project.setups.size() * project.time_count);
    for (auto view = registrations.FirstChildElement(); view; view = view->NextSiblingElement()) {
        attributes(*view, {"setup", "timepoint"});
        const auto id = view_id(attribute(*view, "setup"));
        const auto time = view_id(attribute(*view, "timepoint"));
        const auto index = view_index(project, id, time);
        require(!seen[index], "Duplicate BigDataViewer view registration");
        seen[index] = true;
        children(*view, {"ViewTransform"});
        const auto& transform = one(*view, "ViewTransform");
        attributes(transform, {"type"});
        require(attribute(transform, "type") == "affine", "Unsupported BigDataViewer registration transform");
        children(transform, {"Name", "affine"});
        const auto affine = numbers<12>(leaf(one(transform, "affine")), real);
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 4; ++column)
                require(affine[row * 4 + column] == (row == column ? project.voxel_size[row] : 0.0),
                    "BigDataViewer affine must match the declared voxel size and zero origin");
    }
    for (std::size_t index = 0; index < seen.size(); ++index)
        require(seen[index] || project.missing_views[index], "Missing BigDataViewer view registration");
}

std::set<std::uint32_t> channel_ids(const XMLElement& catalog) {
    children(catalog, {"Channel"});
    std::set<std::uint32_t> ids;
    for (auto channel = catalog.FirstChildElement(); channel; channel = channel->NextSiblingElement()) {
        attributes(*channel, {});
        children(*channel, {"id", "name"});
        require(ids.insert(view_id(leaf(one(*channel, "id")))).second,
            "Duplicate BigDataViewer channel ID");
    }
    return ids;
}

struct ViewRole {
    const char* name;
    const char* tag;
};
constexpr std::array<ViewRole, 3> shared_roles{{
    {"tile", "Tile"}, {"angle", "Angle"}, {"illumination", "Illumination"}
}};

std::uint32_t shared_role_id(const XMLElement& catalog, const ViewRole& role) {
    children(catalog, {role.tag});
    const auto& definition = one(catalog, role.tag);
    attributes(definition, {});
    const auto id = view_id(leaf(one(definition, "id")));
    if (definition.FirstChildElement("name"))
        bounded_utf8(std::string(leaf(one(definition, "name"))), 256);
    if (std::string_view(role.name) == "tile") {
        children(definition, {"id", "name", "location"});
        if (definition.FirstChildElement("location"))
            require(numbers<3>(leaf(one(definition, "location")), real) == std::array<double, 3>{},
                "BigDataViewer tile location differs from the zero-origin profile");
    } else if (std::string_view(role.name) == "angle") {
        children(definition, {"id", "name", "axis", "degrees"});
        if (definition.FirstChildElement("axis") || definition.FirstChildElement("degrees")) {
            const auto axis = numbers<3>(leaf(one(definition, "axis")), real);
            require(axis != std::array<double, 3>{} && real(leaf(one(definition, "degrees"))) == 0,
                "BigDataViewer acquisition angle differs from the zero-rotation profile");
        }
    } else {
        children(definition, {"id", "name"});
    }
    return id;
}

struct SetupCatalog {
    std::set<std::uint32_t> channels;
    std::array<std::optional<std::uint32_t>, shared_roles.size()> shared;
};

SetupCatalog setup_catalog(const XMLElement& setups) {
    SetupCatalog result;
    bool has_channel = false;
    for (auto catalog = setups.FirstChildElement("Attributes"); catalog;
            catalog = catalog->NextSiblingElement("Attributes")) {
        attributes(*catalog, {"name"});
        const auto name = attribute(*catalog, "name");
        if (name == "channel") {
            require(!has_channel, "Duplicate BigDataViewer channel catalogue");
            result.channels = channel_ids(*catalog);
            has_channel = true;
            continue;
        }
        const auto role = std::find_if(shared_roles.begin(), shared_roles.end(),
            [&](const ViewRole& candidate) { return name == candidate.name; });
        require(role != shared_roles.end(), "Unsupported BigDataViewer acquisition role");
        auto& id = result.shared[static_cast<std::size_t>(role - shared_roles.begin())];
        require(!id.has_value(), "Duplicate BigDataViewer acquisition role catalogue");
        id = shared_role_id(*catalog, *role);
    }
    require(has_channel, "Missing BigDataViewer channel catalogue");
    return result;
}

void parse_setups(const XMLElement& declaration, BdvXmlProject& project) {
    attributes(declaration, {});
    children(declaration, {"Attributes", "ViewSetup"});
    const auto first = declaration.FirstChildElement("ViewSetup");
    require(first != nullptr, "BigDataViewer requires a declared setup");
    const bool multiple = first->NextSiblingElement("ViewSetup") != nullptr;
    const auto available = multiple ? setup_catalog(declaration) : SetupCatalog{};
    std::set<std::uint32_t> used_channels;
    std::set<std::uint32_t> used_setups;
    for (auto setup = first; setup; setup = setup->NextSiblingElement("ViewSetup")) {
        require(project.setups.size() < max_setups, "BigDataViewer setup count exceeds limit");
        attributes(*setup, {});
        children(*setup, {"id", "name", "size", "voxelSize", "attributes"});
        BdvXmlSetup result;
        result.id = view_id(leaf(one(*setup, "id")));
        require(used_setups.insert(result.id).second, "Duplicate BigDataViewer setup ID");
        result.name = leaf(one(*setup, "name"));
        bounded_utf8(result.name, 256);
        if (multiple) {
            const auto& roles = one(*setup, "attributes");
            attributes(roles, {});
            children(roles, {"channel", "tile", "angle", "illumination"});
            const auto channel = view_id(leaf(one(roles, "channel")));
            require(available.channels.count(channel) && used_channels.insert(channel).second,
                "Each BigDataViewer setup requires a distinct declared channel");
            for (std::size_t index = 0; index < shared_roles.size(); ++index) {
                const auto* name = shared_roles[index].name;
                const auto& id = available.shared[index];
                if (id)
                    require(view_id(leaf(one(roles, name))) == *id,
                        "BigDataViewer channels must share an acquisition role");
                else
                    require(!roles.FirstChildElement(name), "Undeclared BigDataViewer acquisition role");
            }
        }
        const auto dimensions = numbers<3>(leaf(one(*setup, "size")), integer<std::int64_t>);
        for (const auto dimension : dimensions)
            require(dimension > 0, "BigDataViewer view dimensions must be positive");
        const auto& voxel = one(*setup, "voxelSize");
        attributes(voxel, {});
        children(voxel, {"unit", "size"});
        const auto multiplier = micrometer_scale(leaf(one(voxel, "unit")));
        const auto size = numbers<3>(leaf(one(voxel, "size")), real);
        std::array<double, 3> micrometers{};
        for (std::size_t axis = 0; axis < size.size(); ++axis) {
            micrometers[axis] = size[axis] * multiplier;
            require(std::isfinite(micrometers[axis]) && micrometers[axis] > 0,
                "BigDataViewer voxel spacing must be positive and finite");
        }
        if (project.setups.empty()) {
            project.dimensions = dimensions;
            project.voxel_size = size;
            project.voxel_um = micrometers;
        } else {
            require(project.dimensions == dimensions && project.voxel_size == size &&
                project.voxel_um == micrometers, "BigDataViewer channel geometries differ");
        }
        project.setups.push_back(std::move(result));
    }
    std::sort(project.setups.begin(), project.setups.end(),
        [](const BdvXmlSetup& left, const BdvXmlSetup& right) { return left.id < right.id; });
}

}

BdvXmlProject parse_bdv_xml(const std::vector<std::uint8_t>& bytes, BdvStorage storage) {
    require(!bytes.empty() && bytes.size() <= xml_limit, "BigDataViewer XML is empty or exceeds 64 KiB");
    const std::string xml(bytes.begin(), bytes.end());
    bounded_utf8(xml, xml_limit);
    validate_xml_characters(xml);
    require(xml.find("<!DOCTYPE") == std::string::npos && xml.find("<!ENTITY") == std::string::npos,
        "BigDataViewer XML forbids DTDs and entity declarations");
    tinyxml2::XMLDocument document;
    require(document.Parse(xml.data(), xml.size()) == tinyxml2::XML_SUCCESS,
        "Cannot parse BigDataViewer XML");
    const auto root = document.RootElement();
    require(root && std::string_view(root->Name()) == "SpimData" && !root->NextSiblingElement(),
        "BigDataViewer project requires one SpimData root");
    for (auto node = document.FirstChild(); node; node = node->NextSibling()) {
        if (node == root || node->ToComment())
            continue;
        const auto decl = node->ToDeclaration();
        require(decl && std::string_view(decl->Value()).find("xml ") == 0,
            "Unsupported top-level BigDataViewer XML node");
    }
    attributes(*root, {"version"});
    require(attribute(*root, "version") == "0.2", "BigDataViewer project requires SpimData 0.2");
    children(*root, {"BasePath", "SequenceDescription", "ViewRegistrations"});
    const auto& base = one(*root, "BasePath");
    attributes(base, {"type"});
    require(attribute(base, "type") == "relative" && content(base) == ".",
        "BigDataViewer project base path must be its own directory");
    const auto& sequence = one(*root, "SequenceDescription");
    attributes(sequence, {});
    children(sequence, {"ImageLoader", "ViewSetups", "Timepoints", "MissingViews"});

    BdvXmlProject result;
    parse_setups(one(sequence, "ViewSetups"), result);

    const auto& times = one(sequence, "Timepoints");
    attributes(times, {"type"});
    require(attribute(times, "type") == "range", "BigDataViewer timepoints require a contiguous range");
    children(times, {"first", "last"});
    result.first_time = view_id(leaf(one(times, "first")));
    const auto last = view_id(leaf(one(times, "last")));
    require(last >= result.first_time && last - result.first_time < max_time_count,
        "BigDataViewer timepoint range is invalid or exceeds limit");
    result.time_count = last - result.first_time + 1;
    require(result.setups.size() <= max_time_count / result.time_count, "BigDataViewer view count exceeds limit");
    parse_missing_views(sequence, result);
    parse_loader(one(sequence, "ImageLoader"), storage, result);
    registration(one(*root, "ViewRegistrations"), result);
    return result;
}

}
