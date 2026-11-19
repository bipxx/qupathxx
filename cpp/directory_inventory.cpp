// SPDX-License-Identifier: AGPL-3.0-only
#include "directory_inventory.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace qpx::detail {
namespace fs = std::filesystem;
namespace {

void require(bool okay, const std::string& message) {
    if (!okay)
        throw std::invalid_argument("Directory store: " + message);
}

bool link(const fs::path& path) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        throw std::runtime_error("Cannot inspect directory store path");
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        return true;
#endif
    return fs::is_symlink(fs::symlink_status(path));
}

void text(Sha256& hash, const std::string& value) {
    hash.integer(value.size());
    hash.update(value.data(), value.size());
}

}

void require_directory_path_bytes(const fs::path& path, std::size_t maximum) {
    if (maximum != directory_inventory_legacy_unbounded)
        require(path.u8string().size() <= maximum, "absolute path exceeds byte limit");
}

void require_directory_real_ancestors(const fs::path& root, const std::atomic<bool>* cancelled) {
    for (auto path = root; !path.empty(); path = path.parent_path()) {
        workflow_checkpoint(cancelled);
        require(!link(path), "ancestors cannot be links/reparse points");
        if (path == path.parent_path())
            break;
    }
}

DirectoryInventory observe_directory_inventory(const fs::path& root,
    const DirectoryInventoryLimits& limits, const std::atomic<bool>* cancelled) {
    // The legacy observer already enforces these limits. Keep multiplication
    // safe even for an internal caller that bypasses its options validation.
    require(limits.inventory_bytes && limits.inventory_bytes <= 128u * 1024 * 1024 &&
        limits.max_files && limits.max_files <= 1000000, "invalid inventory limits");
    require_directory_path_bytes(root, limits.absolute_path_bytes);
    require(fs::is_directory(root) && !link(root), "local store must be a real directory");
    DirectoryInventory files;
    std::size_t used = 0, nodes = 0;
    for (auto iterator = fs::recursive_directory_iterator(root);
         iterator != fs::recursive_directory_iterator(); ++iterator) {
        workflow_checkpoint(cancelled);
        require(++nodes <= limits.max_files * 4, "directory-entry limit exceeded");
        const auto& entry = *iterator;
        require_directory_path_bytes(entry.path(), limits.absolute_path_bytes);
        require(!link(entry.path()), "links/reparse points are unsupported");
        if (entry.is_directory()) {
            // Root is directory depth0. Refuse a directory that would enter
            // depth max_depth+1 BEFORE recursive iterator increment/open.
            require(limits.max_depth == directory_inventory_legacy_unbounded ||
                std::size_t(iterator.depth()) < limits.max_depth,
                "directory depth limit exceeded");
            continue;
        }
        require(entry.is_regular_file(), "unsupported store object");
        require(files.size() < limits.max_files, "file-count limit exceeded");
        ReaderFileGuard guard(entry.path().u8string());
        guard.verify();
        auto version = *guard.version();
        const auto name = entry.path().lexically_relative(root).generic_u8string();
        workflow_capture_charge(used, version.payload_bytes() + name.capacity() + 128,
            limits.inventory_bytes);
        files.emplace(name, std::move(version));
    }
    return files;
}

std::size_t directory_inventory_payload(const DirectoryInventory& files) {
    std::size_t payload = 0;
    for (const auto& item : files)
        payload += item.second.payload_bytes() + item.first.capacity() + 128;
    return payload;
}

std::string directory_inventory_sha256(const fs::path& root, const DirectoryInventory& files) {
    Sha256 hash;
    text(hash, "qpx.ome-zarr-observed-store/1");
    text(hash, root.u8string());
    for (const auto& item : files) {
        const auto& version = item.second;
        text(hash, item.first);
        text(hash, version.identity_scheme);
        text(hash, version.identity);
        hash.integer(version.bytes);
        text(hash, version.stamp_scheme);
        text(hash, version.modified_token);
        text(hash, version.change_token);
    }
    return hash.finish();
}

void require_directory_file_version(const FileVersion& expected, const FileVersion& current) {
    if (expected.path != current.path || expected.identity != current.identity ||
        expected.identity_scheme != current.identity_scheme || expected.bytes != current.bytes ||
        expected.modified_token != current.modified_token || expected.change_token != current.change_token ||
        expected.stamp_scheme != current.stamp_scheme)
        throw WorkflowInputChanged("Directory store file changed");
}

void require_directory_inventory_current(const DirectoryInventory& expected, const DirectoryInventory& current) {
    if (current.size() != expected.size())
        throw WorkflowInputChanged("Directory store membership changed");
    auto before = expected.begin(), now = current.begin();
    for (; before != expected.end(); ++before, ++now) {
        if (before->first != now->first)
            throw WorkflowInputChanged("Directory store membership changed");
        require_directory_file_version(before->second, now->second);
    }
}

}
