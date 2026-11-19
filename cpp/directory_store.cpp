// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/directory_store.hpp"
#include "directory_inventory.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "qupathxx/workflow.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace qpx {
namespace fs = std::filesystem;
namespace {

detail::DirectoryInventoryLimits inventory_options(const DirectoryStoreLimits& limits) {
    return {limits.inventory_bytes, limits.max_files, limits.max_depth, limits.absolute_path_bytes};
}

fs::path absolute_directory(const std::string& path, const DirectoryStoreLimits& limits,
                               const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    limits.validate();
    detail::bounded_utf8(path, 4096);
    auto directory = fs::u8path(path);
    if (path.empty() || !directory.is_absolute())
        throw std::invalid_argument("Directory store requires an absolute directory");
    directory = directory.lexically_normal();
    // A trailing separator is not a distinct observed store.
    while (directory.has_relative_path() && directory.filename().empty())
        directory = directory.parent_path();
    detail::require_directory_path_bytes(directory, limits.absolute_path_bytes);
    return directory;
}

void member_key(const std::string& key) {
    detail::bounded_utf8(key, 4096);
    if (key.empty() || key.find('\\') != std::string::npos ||
        key.find(':') != std::string::npos || fs::u8path(key).has_root_path())
        throw std::invalid_argument("Invalid directory store member key");
    std::size_t begin = 0;
    while (begin <= key.size()) {
        const auto end = key.find('/', begin);
        const auto length = (end == std::string::npos ? key.size() : end) - begin;
        if (!length || (length == 1 && key[begin] == '.') ||
            (length == 2 && key[begin] == '.' && key[begin + 1] == '.'))
            throw std::invalid_argument("Invalid directory store member key component");
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
}

}

void DirectoryStoreLimits::validate() const {
    if (!inventory_bytes || inventory_bytes > 128u * 1024 * 1024 ||
        !max_files || max_files > 1000000 || !max_depth || max_depth > 64 ||
        !absolute_path_bytes || absolute_path_bytes > 4096)
        throw std::invalid_argument("Invalid directory store inventory limits");
}

void DirectoryStoreFingerprintLimits::validate() const {
    constexpr auto maximum = UINT64_C(16) * 1024 * 1024 * 1024;
    if (!file_bytes || file_bytes > maximum || !total_bytes || total_bytes > maximum || file_bytes > total_bytes)
        throw std::invalid_argument("Invalid directory store content byte limits");
}

struct DirectoryStoreSnapshot::Data {
    DirectoryStoreLimits limits;
    fs::path root;
    std::string directory;
    detail::DirectoryInventory files;
    std::string id;
    std::size_t inventory_payload = 0;

    Data(const std::string& path, DirectoryStoreLimits selected,
         const std::atomic<bool>* cancelled)
        : limits(selected), root(absolute_directory(path, selected, cancelled)),
          directory(root.u8string()) {
        detail::require_directory_real_ancestors(root, cancelled);
        files = detail::observe_directory_inventory(root, inventory_options(limits), cancelled);
        inventory_payload = detail::directory_inventory_payload(files);
        id = detail::directory_inventory_sha256(root, files);
        workflow_checkpoint(cancelled);
    }
};

DirectoryStoreSnapshot::DirectoryStoreSnapshot(const std::string& directory,
    DirectoryStoreLimits limits, const std::atomic<bool>* cancelled)
    : data_(std::make_shared<const Data>(directory, limits, cancelled)) {}

const std::string& DirectoryStoreSnapshot::directory() const { return data_->directory; }
const std::string& DirectoryStoreSnapshot::observed_sha256() const { return data_->id; }
const DirectoryStoreLimits& DirectoryStoreSnapshot::limits() const { return data_->limits; }

std::size_t DirectoryStoreSnapshot::payload_bytes() const {
    return sizeof(Data) + data_->inventory_payload + data_->directory.capacity() +
        data_->id.capacity() + data_->root.native().capacity() * sizeof(fs::path::value_type) + 128;
}

void DirectoryStoreSnapshot::require_current(const std::atomic<bool>* cancelled) const {
    workflow_checkpoint(cancelled);
    detail::require_directory_real_ancestors(data_->root, cancelled);
    const auto current = detail::observe_directory_inventory(data_->root, inventory_options(data_->limits), cancelled);
    detail::require_directory_inventory_current(data_->files, current);
    detail::require_directory_real_ancestors(data_->root, cancelled);
    workflow_checkpoint(cancelled);
}

std::optional<std::vector<std::uint8_t>> DirectoryStoreSnapshot::read_member(
    const std::string& key, std::size_t byte_limit, const std::atomic<bool>* cancelled) const {
    workflow_checkpoint(cancelled);
    member_key(key);
    if (!byte_limit || byte_limit > 128u * 1024 * 1024)
        throw std::invalid_argument("Invalid directory member read limit");
    const auto relative = fs::u8path(key);
    const auto path = data_->root / relative;
    detail::require_directory_path_bytes(path, data_->limits.absolute_path_bytes);
    const auto found = data_->files.find(key);
    if (found == data_->files.end()) {
        // The absent path may have absent parents. Inspect only the path prefix
        // that still exists and require all existing ancestors to be real.
        auto prefix = data_->root;
        detail::require_directory_real_ancestors(prefix, cancelled);
        for (const auto& component : relative) {
            workflow_checkpoint(cancelled);
            prefix /= component;
            std::error_code error;
            const auto status = fs::symlink_status(prefix, error);
            if (error == std::errc::no_such_file_or_directory)
                return std::nullopt;
            if (error)
                throw fs::filesystem_error("Cannot inspect an absent directory member", prefix, error);
            if (status.type() == fs::file_type::not_found)
                return std::nullopt;
            detail::require_directory_real_ancestors(prefix, cancelled);
            if (prefix == path || !fs::is_directory(status))
                throw WorkflowInputChanged("Unobserved directory member or conflicting path appeared");
        }
        throw std::logic_error("Directory member path had no components");
    }
    detail::require_directory_real_ancestors(path, cancelled);
    detail::ReaderFileGuard guard(path.u8string());
    detail::require_directory_file_version(found->second, *guard.version());
    if (found->second.bytes > byte_limit)
        throw WorkflowCaptureLimit("Directory member exceeds the read limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open an observed directory member");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(found->second.bytes));
    for (std::size_t at = 0; at < bytes.size();) {
        workflow_checkpoint(cancelled);
        const auto amount = std::min<std::size_t>(65536, bytes.size() - at);
        if (!input.read(reinterpret_cast<char*>(bytes.data() + at), static_cast<std::streamsize>(amount)))
            throw WorkflowInputChanged("Directory member read differs from its snapshot");
        at += amount;
    }
    detail::require_directory_real_ancestors(path, cancelled);
    guard.verify();
    workflow_checkpoint(cancelled);
    return bytes;
}

DirectoryStoreContentFingerprint DirectoryStoreSnapshot::content_fingerprint(
    DirectoryStoreFingerprintLimits limits, const std::atomic<bool>* cancelled) const {
    limits.validate();
    require_current(cancelled);
    DirectoryStoreContentFingerprint result;
    result.files = data_->files.size();
    Sha256 hash;
    const std::string domain = "qpx.directory-store-content/1";
    hash.integer(domain.size());
    hash.update(domain.data(), domain.size());
    hash.integer(result.files);
    std::array<char, 65536> buffer;
    for (const auto& [key, expected] : data_->files) {
        workflow_checkpoint(cancelled);
        if (expected.bytes > limits.file_bytes || expected.bytes > limits.total_bytes - result.bytes)
            throw WorkflowCaptureLimit("Directory store content exceeds member/total byte limit");
        const auto path = data_->root / fs::u8path(key);
        detail::require_directory_path_bytes(path, data_->limits.absolute_path_bytes);
        detail::require_directory_real_ancestors(path, cancelled);
        detail::ReaderFileGuard guard(path.u8string());
        detail::require_directory_file_version(expected, *guard.version());
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot read directory store content");
        hash.integer(key.size());
        hash.update(key.data(), key.size());
        hash.integer(expected.bytes);
        auto remaining = expected.bytes;
        while (remaining) {
            workflow_checkpoint(cancelled);
            const auto count = std::min<std::uint64_t>(remaining, buffer.size());
            input.read(buffer.data(), std::streamsize(count));
            if (input.gcount() != std::streamsize(count) || input.bad())
                throw WorkflowInputChanged("Directory store content read differs from snapshot");
            hash.update(buffer.data(), std::size_t(count));
            remaining -= count;
        }
        char extra = 0;
        input.read(&extra, 1);
        if (input.gcount() || input.bad())
            throw WorkflowInputChanged("Directory store content size differs from snapshot");
        guard.verify();
        detail::require_directory_file_version(expected, *guard.version());
        result.bytes += expected.bytes;
    }
    require_current(cancelled);
    result.content_sha256 = hash.finish();
    return result;
}

std::shared_ptr<const ReaderSource> DirectoryStoreSnapshot::reader_source(
    std::string reader_binding, const std::string& primary_relative_key,
    const std::atomic<bool>* cancelled) const {
    workflow_checkpoint(cancelled);
    detail::bounded_utf8(reader_binding, 256);
    if (reader_binding.empty())
        throw std::invalid_argument("Directory store requires a reader binding");
    member_key(primary_relative_key);
    const auto found = data_->files.find(primary_relative_key);
    if (found == data_->files.end())
        throw std::invalid_argument("Directory store primary file was not observed");
    const auto path = data_->root / fs::u8path(primary_relative_key);
    detail::require_directory_path_bytes(path, data_->limits.absolute_path_bytes);
    detail::require_directory_real_ancestors(path, cancelled);
    detail::ReaderFileGuard primary(path.u8string());
    detail::require_directory_file_version(found->second, *primary.version());
    primary.verify();
    require_current(cancelled);
    detail::require_directory_real_ancestors(path, cancelled);
    primary.verify();
    auto source = make_directory_reader_source(std::move(reader_binding), data_->directory,
        primary.version(), data_->id);
    workflow_checkpoint(cancelled);
    return source;
}

}
