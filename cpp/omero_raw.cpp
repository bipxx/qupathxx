// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/omero.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_registry.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "strict_json.hpp"
#include "sha256.hpp"
#include "omero_io.hpp"
#include "raw_numpy_write.hpp"
#include "stored_zip_read.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <set>
namespace qpx {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::size_t raw_bundle_samples_limit = 256u * 1024 * 1024;
constexpr std::size_t raw_bundle_receipt_limit = 32u * 1024 * 1024;
constexpr std::size_t raw_bundle_framing_limit = 8192;
std::string digest(const void* p, std::size_t n) {
    Sha256 h;
    h.update(p, n);
    return h.finish();
}
std::string digest(const std::vector<std::uint8_t>& b) {
    return digest(b.data(), b.size());
}
Json parse(const std::vector<std::uint8_t>& b) {
    detail::StrictJson sax("OMERO NGFF JSON");
    Json::sax_parse(b.begin(), b.end(), &sax);
    return Json::parse(b.begin(), b.end());
}
// Only our recorded, new files and empty directories are removed. No recursive
// deletion of an inferred path, source store, cache or user output is involved.
class Scratch {
    std::vector<fs::path> files, dirs;
    std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;

  public:
    fs::path root;
    Scratch() {
        std::random_device r;
        for (unsigned i = 0; i < 32; ++i) {
            root = fs::temp_directory_path() /
                   ("qpx-omero-" + std::to_string(r()) + "-" + std::to_string(r()));
            if (fs::create_directory(root)) {
                dirs.push_back(root);
                return;
            }
        }
        throw std::runtime_error("Cannot create private OMERO scratch directory");
    }
    ~Scratch() {
        guards.clear();
        std::error_code e;
        for (auto i = files.rbegin(); i != files.rend(); ++i)
            fs::remove(*i, e);
        for (auto i = dirs.rbegin(); i != dirs.rend(); ++i)
            fs::remove(*i, e);
    }
    std::string write(const std::string& name, const void* p, std::size_t n) {
        if (name.empty() || name.front() == '/' || name.find("..") != std::string::npos ||
            name.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_-.~") !=
                std::string::npos)
            throw std::invalid_argument("Invalid native scratch member");
        auto path = root / fs::u8path(name);
        std::vector<fs::path> parents;
        for (auto d = path.parent_path(); d != root; d = d.parent_path())
            parents.push_back(d);
        for (auto i = parents.rbegin(); i != parents.rend(); ++i)
            if (fs::create_directory(*i))
                dirs.push_back(*i);
        if (fs::exists(path))
            throw std::logic_error("Duplicate OMERO scratch member");
        files.push_back(path);
        std::ofstream out(path, std::ios::binary);
        out.write(static_cast<const char*>(p), std::streamsize(n));
        out.close();
        if (!out)
            throw std::runtime_error("Cannot write bounded native scratch member");
        auto guard = std::make_unique<detail::ReaderFileGuard>(path.u8string());
        FileFingerprintOptions options;
        options.byte_limit = n;
        options.buffer_bytes = 65536;
        if (fingerprint_file(path.u8string(), options)->sha256 != digest(p, n))
            throw WorkflowInputChanged("Staged OMERO decoder input differs from owned bytes");
        guard->verify();
        guards.push_back(std::move(guard));
        return path.u8string();
    }
    std::string write(const std::string& n, const std::string& s) {
        return write(n, s.data(), s.size());
    }
    void verify() const {
        for (const auto& guard : guards)
            guard->verify();
    }
};
std::string omero_type(SampleType t) {
    switch (t) {
    case SampleType::Float32:
        return "float";
    case SampleType::Float64:
        return "double";
    default:
        return sample_name(t);
    }
}
Json selection(const RawRegionRequest& r) {
    return {{"level", r.region.level},
            {"x", r.region.x},
            {"y", r.region.y},
            {"width", r.region.width},
            {"height", r.region.height},
            {"first_channel", r.first_channel},
            {"channels", r.channels},
            {"z", r.z},
            {"t", r.t}};
}
std::string raw_hash(const RawPixelBuffer& b) {
    Sha256 h;
    std::visit(
        [&](const auto& v) {
            h.update(v.data(), v.size() * sizeof(typename std::decay_t<decltype(v)>::value_type));
        },
        b.storage());
    h.update(b.valid().data(), b.valid().size());
    return h.finish();
}
} // namespace
void OmeroRawOptions::validate() const {
    if (work_bytes < 1024 * 1024 || work_bytes > 256u * 1024 * 1024 || transfer_bytes < 1024 ||
        transfer_bytes > 256u * 1024 * 1024 || !max_chunks || max_chunks > 4096)
        throw std::invalid_argument("Invalid OMERO raw resource limits");
}
struct OmeroRawBlock::Data {
    RawPixelBuffer pixels;
    RawImageMetadata metadata;
    RawRegionRequest request;
    OmeroImageInfo image;
    std::string receipt, sha;
    Data(RawPixelBuffer p, RawImageMetadata m, RawRegionRequest r, OmeroImageInfo i)
        : pixels(std::move(p)), metadata(std::move(m)), request(r), image(std::move(i)) {}
};
OmeroRawBlock::OmeroRawBlock(std::shared_ptr<const Data> d) : data_(std::move(d)) {}
const RawPixelBuffer& OmeroRawBlock::pixels() const {
    return data_->pixels;
}
const RawImageMetadata& OmeroRawBlock::metadata() const {
    return data_->metadata;
}
const RawRegionRequest& OmeroRawBlock::request() const {
    return data_->request;
}
const OmeroImageInfo& OmeroRawBlock::image() const {
    return data_->image;
}
const std::string& OmeroRawBlock::receipt_json() const {
    return data_->receipt;
}
const std::string& OmeroRawBlock::sha256() const {
    return data_->sha;
}
std::shared_ptr<const OmeroRawBlock> OmeroClient::raw(std::uint64_t id,
                                                      const RawRegionRequest& request,
                                                      OmeroRawOptions options,
                                                      const std::atomic<bool>* cancelled) {
    options.validate();
    const auto before = image(id, cancelled);
    if (!before.ngff_url)
        throw std::invalid_argument(
            "OMERO image has no admitted explicit anonymous NGFF association");
    const auto& base = *before.ngff_url;
    std::map<std::string, std::vector<std::uint8_t>> originals;
    std::size_t transferred = 0;
    auto resource = [&](const std::string& key, std::size_t cap) {
        if (transferred == options.transfer_bytes)
            throw std::length_error("OMERO raw transfer exceeds total byte budget");
        auto b =
            store_get(base, key, std::min(cap, options.transfer_bytes - transferred), cancelled)
                .body;
        transferred += b.size();
        return b;
    };
    auto metadata = [&](const std::string& key) {
        auto b = resource(key, 2u * 1024 * 1024);
        originals.emplace(key, b);
        return parse(b);
    };
    const auto group = metadata(".zgroup");
    auto attrs = metadata(".zattrs");
    if (!attrs.at("multiscales").is_array() || attrs.at("multiscales").size() != 1)
        throw std::invalid_argument("OMERO requires one NGFF multiscale group");
    auto& multiscale = attrs.at("multiscales").at(0);
    if (!multiscale.at("datasets").is_array() || multiscale.at("datasets").empty())
        throw std::invalid_argument("OMERO NGFF has no base dataset");
    const auto dataset = multiscale.at("datasets").at(0);
    const auto path = dataset.at("path").get<std::string>();
    if (path.empty() || path.find("..") != std::string::npos || path.front() == '/' ||
        path.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_-.~") !=
            std::string::npos)
        throw std::invalid_argument("Invalid NGFF base dataset path");
    auto array = metadata(path + "/.zarray");
    const auto compressor = array.at("compressor");
    const bool blosc = compressor.is_object() && compressor.value("id", std::string{}) == "blosc";
    if (blosc) {
        const auto name = compressor.at("cname").get<std::string>();
        if (name != "lz4" && name != "lz4hc" && name != "blosclz")
            throw std::invalid_argument("NGFF Blosc codec is outside the admitted plugin profile");
        array["compressor"] = nullptr;
    }
    // This is a private read adapter for a selected level-zero block. Preserve
    // the full original metadata in the receipt; never expose this partial store.
    multiscale["datasets"] = Json::array({dataset});
    Scratch scratch;
    scratch.write(".zgroup", group.dump());
    scratch.write(".zattrs", attrs.dump());
    scratch.write(path + "/.zarray", array.dump());
    OmeZarrOptions native;
    native.chunk_bytes = 64u * 1024 * 1024;
    native.encoded_bytes = 64u * 1024 * 1024;
    native.max_files = 8192;
    const OmeZarrSource plan(scratch.root.u8string(), native, cancelled);
    const auto meta = plan.metadata();
    request.validate(meta);
    if (request.region.level || request.region.x < 0 || request.region.y < 0 ||
        request.region.width > meta.width || request.region.height > meta.height ||
        request.region.x > meta.width - request.region.width ||
        request.region.y > meta.height - request.region.height)
        throw std::invalid_argument("OMERO raw block must be inside level-zero image bounds");
    if (std::uint64_t(meta.width) != before.width || std::uint64_t(meta.height) != before.height ||
        meta.channels != before.channels || meta.z_planes != before.z_planes ||
        meta.timepoints != before.timepoints || omero_type(meta.sample_type) != before.pixel_type)
        throw std::invalid_argument("OMERO and linked NGFF raw geometry/type differ");
    const auto check_cal = [&](const std::optional<OmeroQuantity>& q,
                               const std::optional<double>& v) {
        if (q && (q->unit != "MICROMETER" || !v || *v != q->value))
            throw std::invalid_argument("OMERO and NGFF physical calibration differ");
    };
    check_cal(before.physical_x, meta.pixel_width_um);
    check_cal(before.physical_y, meta.pixel_height_um);
    if (plan.read_work_bytes(request) > options.work_bytes)
        throw std::length_error("OMERO raw block exceeds native sampling work budget");
    const auto& lev = plan.levels().at(0);
    const auto& axes = plan.axes();
    std::vector<std::uint64_t> first, last, index;
    std::uint64_t chunk_bytes = sample_bytes(meta.sample_type), count = 1;
    for (std::size_t d = 0; d < axes.size(); ++d) {
        std::uint64_t lo = 0, hi = 0;
        if (axes[d] == "x") {
            lo = request.region.x;
            hi = lo + request.region.width - 1;
        } else if (axes[d] == "y") {
            lo = request.region.y;
            hi = lo + request.region.height - 1;
        } else if (axes[d] == "c") {
            lo = request.first_channel;
            hi = lo + request.channels - 1;
        } else if (axes[d] == "z")
            lo = hi = request.z;
        else if (axes[d] == "t")
            lo = hi = request.t;
        lo /= lev.chunks[d];
        hi /= lev.chunks[d];
        first.push_back(lo);
        last.push_back(hi);
        const auto n = hi - lo + 1;
        if (n > options.max_chunks / count)
            throw std::length_error("OMERO raw block exceeds chunk-count limit");
        count *= n;
        if (lev.chunks[d] > native.chunk_bytes / chunk_bytes)
            throw std::length_error("OMERO decoded chunk exceeds byte limit");
        chunk_bytes *= lev.chunks[d];
    }
    const auto separator = array.value("dimension_separator", std::string("."));
    Json chunks = Json::array();
    std::set<std::string> codec_identities;
    index = first;
    for (std::uint64_t i = 0; i < count; ++i) {
        workflow_checkpoint(cancelled);
        std::string key = path + "/";
        for (std::size_t d = 0; d < index.size(); ++d) {
            if (d)
                key += separator;
            key += std::to_string(index[d]);
        }
        auto body = resource(key, 64u * 1024 * 1024);
        const auto encoded_sha = digest(body);
        if (blosc) {
            const auto encoded_path =
                scratch.write("encoded-" + std::to_string(i) + ".blosc", body.data(), body.size());
            const auto selected = detail::select_reader(encoded_path, "org.qupathxx.blosc-bytes");
            const auto reader = detail::open_reader(encoded_path, selected, {}, cancelled);
            const auto rm = reader->raw_metadata();
            if (!rm || rm->width != std::int64_t(chunk_bytes) || rm->height != 1 ||
                rm->channels != 1 || rm->sample_type != SampleType::UInt8)
                throw std::invalid_argument(
                    "Blosc byte-array shape differs from the native Zarr chunk contract");
            const auto decoded =
                reader->read_raw({{0, 0, 0, std::int64_t(chunk_bytes), 1}, 0, 1, 0, 0}, cancelled);
            const auto& bytes = std::get<std::vector<std::uint8_t>>(decoded.storage());
            if (std::find(decoded.valid().begin(), decoded.valid().end(), 0) !=
                decoded.valid().end())
                throw std::invalid_argument("Blosc byte array contains invalid samples");
            scratch.write(key, bytes.data(), bytes.size());
            // The catalogue pins module bytes, ABI, compiler profile and contract.
            const auto catalog = detail::reader_catalog();
            const auto found = std::find_if(catalog.begin(), catalog.end(), [](const auto& d) {
                return d.provider_id == "org.qupathxx.blosc-bytes";
            });
            if (found == catalog.end() || !found->available || found->module_sha256.size() != 64)
                throw std::invalid_argument("Blosc plugin identity unavailable");
            codec_identities.insert(found->provider_id + ":" + found->version + ":" +
                                    found->contract + ":" + found->module_sha256);
        } else
            scratch.write(key, body.data(), body.size());
        chunks.push_back({{"key", key}, {"encoded_bytes", body.size()}, {"sha256", encoded_sha}});
        for (std::size_t d = index.size(); d-- > 0;) {
            if (index[d] < last[d]) {
                ++index[d];
                break;
            }
            index[d] = first[d];
        }
    }
    // Re-fetch the small observed set. This is before/after observed-byte
    // stability, not a server-wide atomic snapshot/version guarantee.
    for (const auto& chunk : chunks) {
        auto b = resource(chunk.at("key").get<std::string>(), 64u * 1024 * 1024);
        if (digest(b) != chunk.at("sha256"))
            throw WorkflowInputChanged("NGFF raw chunk changed while retrieving block");
    }
    for (const auto& item : originals)
        if (resource(item.first, 2u * 1024 * 1024) != item.second)
            throw WorkflowInputChanged("NGFF metadata changed while retrieving block");
    if (image(id, cancelled).metadata_sha256 != before.metadata_sha256)
        throw WorkflowInputChanged("OMERO source association changed while retrieving raw block");
    scratch.verify();
    const OmeZarrSource source(scratch.root.u8string(), native, cancelled);
    auto pixels = source.read(request, options.work_bytes, cancelled);
    source.require_current(cancelled);
    scratch.verify();
    if (std::find(pixels.valid().begin(), pixels.valid().end(), 0) != pixels.valid().end())
        throw std::invalid_argument("OMERO raw block contains invalid samples");
    auto data = std::make_shared<OmeroRawBlock::Data>(std::move(pixels), meta, request, before);
    Json receipt = {{"schema", "qpx.omero-raw/1"},
                    {"representation", "original-channel-samples"},
                    {"server", server()},
                    {"image", Json::parse(omero_image_json(before))},
                    {"selection", selection(request)},
                    {"axes", axes},
                    {"units", plan.units()},
                    {"scale", lev.scale},
                    {"translation", lev.translation},
                    {"chunks", chunks},
                    {"codec_plugins", codec_identities},
                    {"metadata", Json::object()},
                    {"stability", "observed-metadata-and-selected-chunks-before-after"},
                    {"sample_type", sample_name(meta.sample_type)},
                    {"raw_sha256", raw_hash(data->pixels)}};
    receipt["transfer_bytes"] = transferred;
    for (const auto& item : originals)
        receipt["metadata"][item.first] = {{"sha256", digest(item.second)},
                                           {"value", parse(item.second)}};
    const auto canonical = receipt.dump();
    data->sha = digest(canonical.data(), canonical.size());
    receipt["result_sha256"] = data->sha;
    data->receipt = receipt.dump();
    return std::shared_ptr<const OmeroRawBlock>(new OmeroRawBlock(std::move(data)));
}
void save_omero_raw(const OmeroRawBlock& b, const std::string& output, const std::atomic<bool>* c) {
    detail::require_new_export(output);
    save_raw_numpy(b.pixels(), output, false, c);
}
void save_omero_raw_receipt(const OmeroRawBlock& b, const std::string& output,
                            const std::atomic<bool>* c) {
    const auto& s = b.receipt_json();
    detail::save_new_omero_file(output, s.data(), s.size(), c);
}
void save_omero_raw_bundle(const OmeroRawBlock& block, const std::string& output,
                           const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto path = detail::export_path(output);
    detail::require_new_export(path);
    const auto& pixels = block.pixels();
    const auto& receipt = block.receipt_json();
    if (pixels.payload_bytes() > raw_bundle_samples_limit || receipt.size() > raw_bundle_receipt_limit)
        throw std::length_error("OMERO raw bundle exceeds its native size limits");
    const auto limit = pixels.payload_bytes() + receipt.size() + raw_bundle_framing_limit;
    detail::AtomicExportFile file(path, limit, cancelled);
    const auto samples = detail::stored_zip::write_member(file, "raw.npy", cancelled,
        [&](auto& sink) { detail::write_raw_numpy(sink, pixels); });
    const auto provenance = detail::stored_zip::write_member(file, "receipt.json", cancelled,
        [&](auto& sink) { sink.write(receipt.data(), receipt.size()); });
    detail::stored_zip::finish(file, std::array{samples, provenance});
    workflow_checkpoint(cancelled);
    detail::commit_export_output(file, path, ExportWritePolicy::CreateNew);
}

namespace {
[[noreturn]] void invalid_bundle(const char* reason) {
    throw std::invalid_argument(std::string("Invalid OMERO raw bundle: ") + reason);
}

using detail::stored_zip::read_little;

std::uint64_t positive(const Json& fields, const char* key) {
    const auto& value = fields.at(key);
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > 0)
        return value.get<std::uint64_t>();
    invalid_bundle("invalid sample dimensions");
}

SampleType sample_type(const Json& receipt) {
    const auto name = receipt.at("sample_type").get<std::string>();
    for (auto type : {SampleType::UInt8, SampleType::Int8, SampleType::UInt16, SampleType::Int16,
                      SampleType::UInt32, SampleType::Int32, SampleType::UInt64, SampleType::Int64,
                      SampleType::Float32, SampleType::Float64})
        if (sample_name(type) == name)
            return type;
    invalid_bundle("unknown sample type");
}

std::uint64_t verify_numpy_header(const std::string& header, const Json& receipt,
                                 std::size_t payload) {
    const auto& selection = receipt.at("selection");
    const auto height = positive(selection, "height"), width = positive(selection, "width");
    const auto channels = positive(selection, "channels");
    const auto type = sample_type(receipt);
    if (width > raw_bundle_samples_limit / height)
        invalid_bundle("sample dimensions exceed bundle limits");
    const auto pixels = width * height;
    const auto element_size = sample_bytes(type);
    if (channels > raw_bundle_samples_limit / pixels / element_size ||
        pixels * channels * element_size != payload)
        invalid_bundle("sample extent differs from the receipt");
    auto descriptor = detail::numpy_sample_descriptor(type);
    const auto shape = std::to_string(height) + ", " + std::to_string(width) + ", " +
                       std::to_string(channels);
    struct HeaderBytes {
        std::string bytes;
        void write(const char* data, std::size_t size) { bytes.append(data, size); }
    };
    for (const char order : {'<', '>'}) {
        if (element_size > 1)
            descriptor[0] = order;
        HeaderBytes expected;
        detail::write_numpy_header(expected, "'" + descriptor + "'", shape);
        if (header == expected.bytes)
            return pixels;
    }
    invalid_bundle("NPY header differs from the receipt");
}
std::string inspect_raw_bundle(const std::string& zip, const std::string* output,
                               const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto path = detail::export_path(zip);
    const std::vector<std::string> protected_paths{path};
    std::string destination;
    if (output) {
        destination = detail::export_path(*output);
        detail::require_new_export(destination);
        detail::guard_export_output(destination, protected_paths);
    }
    const detail::ReaderFileGuard guard(path);
    const auto size = guard.version()->bytes;
    if (size > raw_bundle_samples_limit + raw_bundle_receipt_limit + raw_bundle_framing_limit)
        invalid_bundle("archive exceeds its size limit");
    detail::stored_zip::Input input(fs::u8path(path), size, cancelled, "OMERO raw bundle");
    const auto samples = input.member("raw.npy", raw_bundle_samples_limit + raw_bundle_framing_limit);
    if (samples.size < 10)
        invalid_bundle("truncated NPY header");
    const auto prefix = input.record<10>();
    const auto header_size = read_little(prefix.data() + 8, 2);
    if (header_size > samples.size - prefix.size() || header_size > raw_bundle_framing_limit)
        invalid_bundle("invalid NPY header extent");
    std::string header(prefix.data(), prefix.size());
    header.resize(prefix.size() + header_size);
    input.read(header.data() + prefix.size(), header_size);
    std::optional<detail::AtomicExportFile> extracted;
    if (output) {
        extracted.emplace(destination, samples.size, cancelled);
        extracted->write(header.data(), header.size());
    }
    detail::stored_zip::Checksum crc{cancelled};
    crc.write(header.data(), header.size());
    Sha256 raw;
    const auto payload = samples.size - header.size();
    std::array<char, 65536> bytes{};
    for (auto remaining = payload; remaining;) {
        const auto count = std::min(remaining, bytes.size());
        input.read(bytes.data(), count);
        crc.write(bytes.data(), count);
        raw.update(bytes.data(), count);
        if (extracted)
            extracted->write(bytes.data(), count);
        remaining -= count;
    }
    if (~crc.crc != samples.crc)
        invalid_bundle("NPY CRC differs");

    const auto provenance = input.member("receipt.json", raw_bundle_receipt_limit);
    std::string text(provenance.size, '\0');
    input.read(text.data(), text.size());
    detail::stored_zip::Checksum receipt_crc{cancelled};
    receipt_crc.write(text.data(), text.size());
    if (~receipt_crc.crc != provenance.crc)
        invalid_bundle("receipt CRC differs");
    input.finish(std::array{samples, provenance});

    detail::StrictJson strict("OMERO bundle receipt");
    Json::sax_parse(text, &strict);
    auto receipt = Json::parse(text);
    if (receipt.at("schema") != "qpx.omero-raw/1" ||
        receipt.at("representation") != "original-channel-samples" || receipt.dump() != text)
        invalid_bundle("unsupported or noncanonical receipt");
    const auto expected_result = receipt.at("result_sha256").get<std::string>();
    receipt.erase("result_sha256");
    const auto canonical = receipt.dump();
    if (digest(canonical.data(), canonical.size()) != expected_result)
        invalid_bundle("receipt digest differs");
    const auto pixels = verify_numpy_header(header, receipt, payload);
    bytes.fill(1);
    for (auto remaining = pixels; remaining;) {
        workflow_checkpoint(cancelled);
        const auto count = std::min<std::uint64_t>(remaining, bytes.size());
        raw.update(bytes.data(), static_cast<std::size_t>(count));
        remaining -= count;
    }
    if (raw.finish() != receipt.at("raw_sha256"))
        invalid_bundle("sample digest differs");
    guard.verify();
    workflow_checkpoint(cancelled);
    if (extracted)
        detail::commit_export_output(*extracted, destination, ExportWritePolicy::CreateNew,
                                     protected_paths);
    return text;
}
} // namespace

std::string verify_omero_raw_bundle(const std::string& zip, const std::atomic<bool>* cancelled) {
    return inspect_raw_bundle(zip, nullptr, cancelled);
}

std::string extract_omero_raw_bundle(const std::string& zip, const std::string& new_npy,
                                    const std::atomic<bool>* cancelled) {
    return inspect_raw_bundle(zip, &new_npy, cancelled);
}
} // namespace qpx
