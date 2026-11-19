// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/directory_store.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/workflow.hpp"
#include "directory_inventory.hpp"
#include "ome_zarr_internal.hpp"
#include "reader_file_guard.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

namespace fs = std::filesystem;
using namespace qpx;
namespace {

void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}

template<class Exception = std::exception, class F>
void refuses(F&& call) {
    try {
        call();
    } catch (const Exception& error) {
        check(!std::string(error.what()).empty(), "Native refusal has no explanation");
        return;
    }
    throw std::runtime_error("Missing directory store refusal");
}

template<class F>
void refuses_with(F&& call, const char* expected) {
    try {
        call();
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(expected) != std::string::npos,
            "Directory refusal did not come from the declared depth/path bound");
        return;
    }
    throw std::runtime_error("Missing bounded directory store refusal");
}

void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), std::streamsize(bytes.size()));
    stream.close();
    check(bool(stream), "Cannot write authored directory store file");
}

fs::path store(const fs::path& root, const std::string& name) {
    const auto path = root / name;
    check(fs::create_directory(path), "Test store already exists");
    write(path / "metadata/primary.json", "authored metadata");
    write(path / "data/chunk", "authored chunk");
    return path;
}

// Windows directory junctions exercise actual reparse points without requiring
// developer mode or symlink privileges. Every target is inside this test's own
// newly authored tree. No shell, external data or recursive cleanup is used.
bool directory_link(const fs::path& link, const fs::path& target, std::error_code& error) {
#ifdef _WIN32
    fs::create_directory(link, error);
    if (error)
        return false;
    const auto handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = std::error_code(int(GetLastError()), std::system_category());
        std::error_code ignored;
        fs::remove(link, ignored);
        return false;
    }
    struct MountPointHeader {
        DWORD tag;
        WORD data_length, reserved;
        WORD substitute_offset, substitute_length, print_offset, print_length;
    };
    static_assert(sizeof(MountPointHeader) == 16, "Unexpected junction header layout");
    const auto print = fs::absolute(target).lexically_normal().native();
    const auto substitute = print.rfind(L"\\\\", 0) == 0
        ? std::wstring(L"\\??\\UNC\\") + print.substr(2)
        : std::wstring(L"\\??\\") + print;
    const auto strings = (substitute.size() + print.size() + 2) * sizeof(wchar_t);
    const MountPointHeader header{IO_REPARSE_TAG_MOUNT_POINT,
        WORD(strings + 8), 0, 0, WORD(substitute.size() * sizeof(wchar_t)),
        WORD((substitute.size() + 1) * sizeof(wchar_t)), WORD(print.size() * sizeof(wchar_t))};
    std::vector<unsigned char> buffer(sizeof(header) + strings, 0);
    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(header), substitute.data(), substitute.size() * sizeof(wchar_t));
    std::memcpy(buffer.data() + sizeof(header) + header.print_offset,
        print.data(), print.size() * sizeof(wchar_t));
    DWORD returned = 0;
    const auto okay = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT,
        buffer.data(), DWORD(buffer.size()), nullptr, 0, &returned, nullptr);
    const auto code = okay ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!okay) {
        error = std::error_code(int(code), std::system_category());
        std::error_code ignored;
        fs::remove(link, ignored);
        return false;
    }
    error.clear();
    return true;
#else
    fs::create_directory_symlink(target, link, error);
    return !error;
#endif
}

bool file_link(const fs::path& link, const fs::path& target, std::error_code& error) {
#ifdef _WIN32
    // Flag 2 permits unprivileged creation when Windows developer mode allows it.
    if (CreateSymbolicLinkW(link.c_str(), target.c_str(), 2)) {
        error.clear();
        return true;
    }
    auto code = GetLastError();
    if (code == ERROR_INVALID_PARAMETER && CreateSymbolicLinkW(link.c_str(), target.c_str(), 0)) {
        error.clear();
        return true;
    }
    if (code == ERROR_INVALID_PARAMETER)
        code = GetLastError();
    error = std::error_code(int(code), std::system_category());
    return false;
#else
    fs::create_symlink(target, link, error);
    return !error;
#endif
}

void require_link(const fs::path& link, const fs::path& target) {
    std::error_code error;
    if (!directory_link(link, target, error))
        throw std::runtime_error("Cannot create authored directory link: " + error.message());
}

void limits_and_paths(const fs::path& root) {
    DirectoryStoreLimits defaults;
    defaults.validate();
    check(defaults.inventory_bytes == 64u * 1024 * 1024 && defaults.max_files == 100000 &&
        defaults.max_depth == 64 && defaults.absolute_path_bytes == 4096,
        "Directory store default limits changed");
    DirectoryStoreLimits maximum{128u * 1024 * 1024, 1000000};
    maximum.validate();
    for (const auto bad : std::vector<DirectoryStoreLimits>{{0, 1}, {128u * 1024 * 1024 + 1, 1},
             {1, 0}, {1, 1000001}, {1, 1, 0, 4096}, {1, 1, 65, 4096},
             {1, 1, 64, 0}, {1, 1, 64, 4097}})
        refuses<std::invalid_argument>([&] { bad.validate(); });
    refuses<std::invalid_argument>([] { DirectoryStoreSnapshot snapshot(""); });
    refuses<std::invalid_argument>([] { DirectoryStoreSnapshot snapshot("."); });
    refuses<std::invalid_argument>([] { DirectoryStoreSnapshot snapshot("relative/store"); });
    refuses<std::invalid_argument>([&] {
        DirectoryStoreSnapshot snapshot(root.u8string() + std::string("\0suffix", 7));
    });
    refuses<std::invalid_argument>([&] {
        DirectoryStoreSnapshot snapshot(root.u8string() + std::string("\xc0\xaf", 2));
    });
    write(root / "ordinary-file", "not a directory");
    refuses([&] { DirectoryStoreSnapshot snapshot((root / "ordinary-file").u8string()); });
    refuses([&] { DirectoryStoreSnapshot snapshot((root / "absent").u8string()); });

    const auto empty = root / "empty";
    fs::create_directory(empty);
    DirectoryStoreSnapshot snapshot(empty.u8string());
    snapshot.require_current();
    check(snapshot.observed_sha256().size() == 64, "Empty store lacks an observation digest");
    refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "metadata.json"); });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { DirectoryStoreSnapshot stopped(empty.u8string(), {}, &cancelled); });
    refuses<WorkflowCancelled>([&] { snapshot.require_current(&cancelled); });
    refuses<WorkflowCancelled>([&] { snapshot.reader_source("reader/1", "metadata.json", &cancelled); });
}

void large_owned_source(const fs::path& root) {
    const auto path = root / "large-generic-store";
    fs::create_directory(path);
    write(path / ".zattrs", "decoy primary; no schema is inferred");
    write(path / "labels/cells/.zattrs", "explicit nested metadata");
    for (int i = 0; i < 80; ++i)
        write(path / ("payload/chunk-" + std::to_string(i)), "chunk " + std::to_string(i));

    std::shared_ptr<const ReaderSource> owned;
    std::string digest;
    {
        std::string directory = (path / ".").u8string();
        std::string reader = "org.qupathxx.test.directory/1";
        std::string primary = "labels/cells/.zattrs";
        DirectoryStoreSnapshot snapshot(directory);
        digest = snapshot.observed_sha256();
        check(snapshot.directory() == path.u8string(), "Snapshot directory is not absolute/normalized");
        check(snapshot.payload_bytes() > 82 * sizeof(FileVersion), "Snapshot does not charge retained inventory");
        snapshot.require_current();
        owned = snapshot.reader_source(reader, primary);
        const auto again = snapshot.reader_source(reader, primary);
        check(again->generation != owned->generation && again->directory_sha256 == owned->directory_sha256,
            "Generation and observed directory identity were conflated");
        directory.assign(directory.size(), 'x');
        reader.assign(reader.size(), 'x');
        primary.assign(primary.size(), 'x');
    }
    owned->validate();
    check(owned->reader == "org.qupathxx.test.directory/1" && owned->locator == path.u8string() &&
        owned->scope == "directory_store" && owned->directory_sha256 == digest && owned->companions.empty(),
        "Directory source lost owned strings or exceeded companion scope");
    check(owned->primary && owned->primary->path == (path / "labels/cells/.zattrs").lexically_normal().u8string(),
        "Directory source guessed its primary instead of selecting the nested key");
    detail::ReaderFileGuard expected((path / "labels/cells/.zattrs").u8string());
    check(expected.version()->identity == owned->primary->identity &&
        expected.version()->bytes == owned->primary->bytes, "Owned explicit primary version differs");
    expected.verify();
    check(owned->payload_bytes() <= 128 * 1024, "Directory source expanded into an unbounded file list");

    detail::ZarrStore legacy(path.u8string(), {}, nullptr);
    check(digest == legacy.id, "Directory wrapper changed the legacy observed-store digest");
    DirectoryStoreSnapshot repeated(path.u8string());
    check(repeated.observed_sha256() == digest, "Unchanged observation identity is unstable");
    DirectoryStoreLimits exact_count;
    exact_count.max_files = 82;
    DirectoryStoreSnapshot exactly(path.u8string(), exact_count);
    exactly.require_current();
    exact_count.max_files = 81;
    refuses<std::invalid_argument>([&] { DirectoryStoreSnapshot too_many(path.u8string(), exact_count); });
    DirectoryStoreLimits tiny;
    tiny.inventory_bytes = 1;
    refuses<WorkflowCaptureLimit>([&] { DirectoryStoreSnapshot too_large(path.u8string(), tiny); });

    const auto copied = root / "same-bytes-other-store";
    fs::copy(path, copied, fs::copy_options::recursive);
    DirectoryStoreSnapshot other(copied.u8string());
    check(other.observed_sha256() != digest,
        "Observed path/file-version identity was replaced by a content-only digest");
}

void invalid_primary_keys(const fs::path& root) {
    const auto path = store(root, "invalid-primary");
    DirectoryStoreSnapshot snapshot(path.u8string());
    std::vector<std::string> keys{"", ".", "..", "./metadata/primary.json", "../primary.json",
        "metadata/../primary.json", "metadata/./primary.json", "/metadata/primary.json",
        "C:/metadata/primary.json", "C:metadata/primary.json", "metadata\\primary.json",
        "metadata/primary.json:stream", "metadata//primary.json", "metadata/primary.json/",
        "metadata/", "//server/share/file", (path / "metadata/primary.json").u8string(),
        std::string("metadata/primary.json\0tail", 26), std::string("\xed\xa0\x80", 3)};
    for (const auto& key : keys)
        refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", key); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "absent.json"); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "metadata"); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source("", "metadata/primary.json"); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source(std::string(257, 'x'), "metadata/primary.json"); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source(std::string("bad\0reader", 10), "metadata/primary.json"); });
    refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", std::string(4097, 'x')); });
    snapshot.reader_source("reader/1", "metadata/primary.json")->validate();
}

void depth_and_path_bounds(const fs::path& root) {
    {
        // A mostly-empty tree consumes traversal state despite having no file
        // inventory payload. The fourth directory must be refused before its
        // iterator is opened, independently of file-count and memory limits.
        const auto path = root / "deep-empty";
        fs::create_directories(path / "a/b/c/d");
        DirectoryStoreLimits limits;
        limits.max_depth = 3;
        refuses_with([&] { DirectoryStoreSnapshot snapshot(path.u8string(), limits); },
            "directory depth limit exceeded");
        detail::ZarrStore legacy(path.u8string(), {}, nullptr);
        legacy.require_current(nullptr);
    }
    {
        const auto path = root / "depth-boundary";
        fs::create_directories(path / "a/b/c");
        write(path / "a/b/c/primary", "metadata at allowed directory depth3");
        DirectoryStoreLimits limits;
        limits.max_depth = 3;
        DirectoryStoreSnapshot snapshot(path.u8string(), limits);
        snapshot.require_current();
        check(snapshot.limits().max_depth == 3, "Selected native depth bound was not retained");
        snapshot.reader_source("reader/1", "a/b/c/primary")->validate();
        fs::create_directory(path / "a/b/c/d");
        refuses_with([&] { snapshot.require_current(); }, "directory depth limit exceeded");
        refuses_with([&] { snapshot.reader_source("reader/1", "a/b/c/primary"); },
            "directory depth limit exceeded");
    }
    {
        const auto path = root / "path-boundary";
        fs::create_directory(path);
        write(path / "p", "explicit primary");
        DirectoryStoreLimits limits;
        limits.absolute_path_bytes = path.u8string().size() + 2;
        limits.validate();
        DirectoryStoreSnapshot snapshot(path.u8string(), limits);
        snapshot.reader_source("reader/1", "p")->validate();
        check(snapshot.limits().absolute_path_bytes == limits.absolute_path_bytes,
            "Selected native path bound was not retained");
        fs::create_directory(path / "longer-empty-directory");
        refuses_with([&] { snapshot.require_current(); }, "absolute path exceeds byte limit");
        refuses_with([&] { snapshot.reader_source("reader/1", "p"); },
            "absolute path exceeds byte limit");
        refuses_with([&] { DirectoryStoreSnapshot too_long(path.u8string(), limits); },
            "absolute path exceeds byte limit");
        detail::ZarrStore legacy(path.u8string(), {}, nullptr);
        legacy.require_current(nullptr);
        limits.absolute_path_bytes = path.u8string().size() - 1;
        refuses_with([&] { DirectoryStoreSnapshot long_root(path.u8string(), limits); },
            "absolute path exceeds byte limit");
    }
    {
        const auto path = root / "file-path-bound";
        fs::create_directory(path);
        write(path / "longer-file-name", "file path is bounded too");
        DirectoryStoreLimits limits;
        limits.absolute_path_bytes = path.u8string().size() + 2;
        refuses_with([&] { DirectoryStoreSnapshot long_file(path.u8string(), limits); },
            "absolute path exceeds byte limit");
    }
}

void legacy_digest_vector() {
    // Pinned independently with .NET SHA256 over the pre-extraction domain,
    // sorted keys and UInt64 little-endian string lengths/file bytes. This
    // catches a shared digest regression even when both readers use the helper.
    detail::DirectoryInventory files;
    FileVersion metadata;
    metadata.path = "/ignored/metadata";
    metadata.identity_scheme = "test-id/1";
    metadata.identity = "42";
    metadata.bytes = 7;
    metadata.stamp_scheme = "test-stamp/1";
    metadata.modified_token = "100";
    metadata.change_token = "101";
    auto chunk = metadata;
    chunk.path = "/ignored/chunk";
    chunk.identity = "43";
    chunk.bytes = 0;
    chunk.modified_token = "102";
    chunk.change_token = "103";
    files.emplace("labels/cells/0.0", chunk);
    files.emplace(".zattrs", metadata);
    const auto digest = detail::directory_inventory_sha256(fs::u8path("/qpx/store"), files);
    check(digest == "7e225202da50783905341af4fb87e91ccced1224d128cc97813be14591d2d123",
        "Legacy observed-store hash domain/order/fields changed");
    files.at(".zattrs").observed_unix_ms = 123;
    check(detail::directory_inventory_sha256(fs::u8path("/qpx/store"), files) == digest,
        "Observation time was added to legacy file-version identity");
}

void mutations(const fs::path& root) {
    {
        const auto path = store(root, "added");
        DirectoryStoreSnapshot snapshot(path.u8string());
        write(path / "data/new-chunk", "new");
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
        refuses<WorkflowInputChanged>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
    }
    {
        const auto path = store(root, "deleted");
        DirectoryStoreSnapshot snapshot(path.u8string());
        check(fs::remove(path / "data/chunk"), "Cannot remove authored chunk");
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
        refuses<WorkflowInputChanged>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
    }
    {
        const auto path = store(root, "renamed");
        DirectoryStoreSnapshot snapshot(path.u8string());
        fs::rename(path / "data/chunk", path / "data/renamed-chunk");
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
    }
    {
        const auto path = store(root, "changed");
        DirectoryStoreSnapshot snapshot(path.u8string());
        write(path / "data/chunk", "changed chunk with different size");
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
    }
    {
        const auto path = store(root, "same-size-version-change");
        DirectoryStoreSnapshot snapshot(path.u8string());
        const auto modified = fs::last_write_time(path / "data/chunk");
        write(path / "data/chunk", "different text"); // Same size as "authored chunk".
        fs::last_write_time(path / "data/chunk", modified + std::chrono::seconds(2));
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
    }
    {
        const auto path = store(root, "primary-changed");
        DirectoryStoreSnapshot snapshot(path.u8string());
        const auto prior = snapshot.reader_source("reader/1", "metadata/primary.json");
        write(path / "metadata/primary.json", "changed primary metadata and size");
        refuses<WorkflowInputChanged>([&] { snapshot.require_current(); });
        refuses<WorkflowInputChanged>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
        check(prior->primary->bytes == std::string("authored metadata").size(),
            "Previously returned source was mutated by a later filesystem write");
    }
    {
        const auto path = store(root, "primary-deleted");
        DirectoryStoreSnapshot snapshot(path.u8string());
        fs::remove(path / "metadata/primary.json");
        refuses([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
    }
    {
        const auto path = store(root, "count-currentness-bound");
        DirectoryStoreLimits limits;
        limits.max_files = 2;
        DirectoryStoreSnapshot snapshot(path.u8string(), limits);
        write(path / "third-file", "new");
        refuses<std::invalid_argument>([&] { snapshot.require_current(); });
    }
    {
        const auto path = store(root, "memory-currentness-bound");
        detail::ZarrStore prior(path.u8string(), {}, nullptr);
        DirectoryStoreLimits limits;
        limits.inventory_bytes = prior.payload_bytes() + 128;
        DirectoryStoreSnapshot snapshot(path.u8string(), limits);
        write(path / "new-inventory-entry", "new");
        refuses<WorkflowCaptureLimit>([&] { snapshot.require_current(); });
    }
    {
        const auto path = root / "directory-entry-bound";
        fs::create_directory(path);
        for (int i = 0; i < 5; ++i)
            fs::create_directory(path / ("empty-" + std::to_string(i)));
        DirectoryStoreLimits limits;
        limits.max_files = 1;
        refuses<std::invalid_argument>([&] { DirectoryStoreSnapshot snapshot(path.u8string(), limits); });
    }
    {
        const auto path = store(root, "cancel-nonempty");
        DirectoryStoreSnapshot snapshot(path.u8string());
        std::atomic<bool> cancelled{true};
        refuses<WorkflowCancelled>([&] { DirectoryStoreSnapshot stopped(path.u8string(), {}, &cancelled); });
        refuses<WorkflowCancelled>([&] { snapshot.require_current(&cancelled); });
        refuses<WorkflowCancelled>([&] { snapshot.reader_source("reader/1", "metadata/primary.json", &cancelled); });
    }
}

void legacy_reader(const fs::path& root) {
    const auto path = root / "authored-legacy-ngff";
    fs::create_directory(path);
    write(path / ".zgroup", R"({"zarr_format":2})");
    write(path / ".zattrs", R"({"multiscales":[{"version":"0.4","axes":[{"name":"y","type":"space"},{"name":"x","type":"space"}],"datasets":[{"path":"0","coordinateTransformations":[{"type":"scale","scale":[1,1]}]}]}]})");
    write(path / "0/.zarray", R"({"zarr_format":2,"shape":[2,3],"chunks":[2,3],"dtype":"|u1","compressor":null,"fill_value":0,"order":"C","filters":null})");
    write(path / "0/0.0", std::string("\1\2\3\4\5\6", 6));
    DirectoryStoreSnapshot snapshot(path.u8string());
    if (!has_ome_zarr()) {
        refuses([&] { OmeZarrSource unavailable(path.u8string()); });
        check(snapshot.reader_source("reader/1", "0/.zarray")->scope == "directory_store",
            "Directory observation requires the optional NGFF decoder");
        return;
    }
    OmeZarrSource legacy(path.u8string());
    check(legacy.sha256() == snapshot.observed_sha256() &&
        legacy.reader_source()->directory_sha256 == snapshot.observed_sha256(),
        "Frozen legacy reader and new directory wrapper identities diverge");
    check(snapshot.reader_source("reader/1", "0/.zarray")->primary->path == (path / "0/.zarray").lexically_normal().u8string(),
        "Directory wrapper forced the legacy .zattrs primary");
    RawRegionRequest request;
    request.region = {0, 0, 0, 3, 2};
    const auto pixels = legacy.read(request);
    check(pixels.sample_type() == SampleType::UInt8 &&
        std::get<std::vector<std::uint8_t>>(pixels.storage()) == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6} &&
        pixels.valid() == std::vector<std::uint8_t>(6, 1), "Retained legacy Zarr reader samples changed");
    legacy.require_current();
}

void links(const fs::path& root) {
    const auto target = store(root, "link-probe-target");
    std::error_code error;
    if (!directory_link(root / "directory-link-probe", target, error)) {
        std::cout << "SKIP directory link/reparse cases: " << error.message() << '\n';
    } else {
        fs::remove(root / "directory-link-probe");
        {
            require_link(root / "linked-root", target);
            refuses<std::invalid_argument>([&] { DirectoryStoreSnapshot snapshot((root / "linked-root").u8string()); });
            fs::remove(root / "linked-root");
        }
        {
            const auto real = root / "real-ancestor";
            fs::create_directory(real);
            store(real, "store");
            require_link(root / "linked-ancestor", real);
            refuses<std::invalid_argument>([&] {
                DirectoryStoreSnapshot snapshot((root / "linked-ancestor/store").u8string());
            });
            fs::remove(root / "linked-ancestor");
        }
        {
            const auto path = store(root, "link-inside");
            require_link(path / "linked-directory", target);
            refuses<std::invalid_argument>([&] { DirectoryStoreSnapshot snapshot(path.u8string()); });
            fs::remove(path / "linked-directory");
        }
        {
            const auto path = store(root, "new-link-inside");
            DirectoryStoreSnapshot snapshot(path.u8string());
            require_link(path / "linked-directory", target);
            refuses<std::invalid_argument>([&] { snapshot.require_current(); });
            fs::remove(path / "linked-directory");
        }
        {
            const auto path = store(root, "root-replaced");
            DirectoryStoreSnapshot snapshot(path.u8string());
            const auto moved = root / "root-replaced-original";
            fs::rename(path, moved);
            require_link(path, moved);
            refuses<std::invalid_argument>([&] { snapshot.require_current(); });
            refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
            fs::remove(path);
        }
        {
            const auto ancestor = root / "ancestor-replaced";
            fs::create_directory(ancestor);
            const auto path = store(ancestor, "store");
            DirectoryStoreSnapshot snapshot(path.u8string());
            const auto moved = root / "ancestor-replaced-original";
            fs::rename(ancestor, moved);
            require_link(ancestor, moved);
            refuses<std::invalid_argument>([&] { snapshot.require_current(); });
            refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
            fs::remove(ancestor);
        }
        {
            const auto path = store(root, "primary-directory-replaced");
            DirectoryStoreSnapshot snapshot(path.u8string());
            const auto moved = root / "primary-directory-original";
            fs::rename(path / "metadata", moved);
            require_link(path / "metadata", moved);
            refuses<std::invalid_argument>([&] { snapshot.reader_source("reader/1", "metadata/primary.json"); });
            refuses<std::invalid_argument>([&] { snapshot.require_current(); });
            fs::remove(path / "metadata");
        }
    }
    {
        const auto path = store(root, "file-link-inside");
        if (!file_link(path / "linked-file", target / "data/chunk", error)) {
            std::cout << "SKIP file symlink case: " << error.message() << '\n';
        } else {
            refuses<std::invalid_argument>([&] { DirectoryStoreSnapshot snapshot(path.u8string()); });
            fs::remove(path / "linked-file");
        }
    }
}

}

namespace {
void member_reads(const fs::path& root) {
    const auto path = store(root, "member-read");
    write(path / "data/empty", "");
    DirectoryStoreSnapshot snapshot(path.u8string());
    const auto expected = std::string("authored chunk");
    const auto bytes = snapshot.read_member("data/chunk", 128);
    check(bytes && std::string(bytes->begin(), bytes->end()) == expected,
        "Observed bytes did not round trip");
    const auto empty = snapshot.read_member("data/empty", 1);
    check(empty && empty->empty(), "Present empty member was treated as absent");
    check(!snapshot.read_member("absent/parent/chunk", 128), "Absent member became data");
    refuses<WorkflowCaptureLimit>([&] { snapshot.read_member("data/chunk", 1); });
    refuses<std::invalid_argument>([&] { snapshot.read_member("data/../attributes.json", 128); });
    std::atomic<bool> cancelled{true};
    refuses<WorkflowCancelled>([&] { snapshot.read_member("data/chunk", 128, &cancelled); });
    write(path / "absent/parent/chunk", "new");
    refuses<WorkflowInputChanged>([&] { snapshot.read_member("absent/parent/chunk", 128); });
    write(path / "data/chunk", "changed bytes");
    refuses<WorkflowInputChanged>([&] { snapshot.read_member("data/chunk", 128); });
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 2)
            throw std::invalid_argument("Usage: qupathxx_directory_store_tests [new-output-directory]");
        const auto root = argc == 2 ? fs::absolute(fs::u8path(argv[1])).lexically_normal()
            : fs::current_path() / ("directory-store-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(!fs::exists(root), "Directory store test output must be a new path");
        check(fs::create_directories(root), "Cannot create authored test root");
        limits_and_paths(root);
        std::cout << "PASS absolute paths, limits, empty-store observation and cancellation\n";
        large_owned_source(root);
        std::cout << "PASS 82-file inventory, explicit nested primary, ownership and legacy digest\n";
        member_reads(root);
        std::cout << "PASS bounded member reads, verified absence, mutation, cancellation and key guards\n";
        invalid_primary_keys(root);
        std::cout << "PASS escaping/absolute/backslash/ADS/dot/empty-component/UTF-8 primary refusals\n";
        depth_and_path_bounds(root);
        std::cout << "PASS deep empty trees and directory/file absolute-path bounds before descent/open\n";
        mutations(root);
        std::cout << "PASS file add/delete/change/rename, primary mutation and bounded reobservation\n";
        legacy_reader(root);
        legacy_digest_vector();
        std::cout << (has_ome_zarr() ? "PASS authored NGFF legacy source identity and exact raw samples\n" :
            "PASS directory observation independent of the disabled NGFF decoder\n");
        links(root);
        std::cout << "PASS supported link/reparse and ancestor-replacement refusals\n";
        std::cout << "Authored evidence retained at " << root.u8string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
