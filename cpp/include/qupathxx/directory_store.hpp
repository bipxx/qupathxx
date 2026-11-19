// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/source_identity.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace qpx {

struct DirectoryStoreLimits {
    std::size_t inventory_bytes = 64u * 1024 * 1024;
    std::size_t max_files = 100000;
    std::size_t max_depth = 64;
    std::size_t absolute_path_bytes = 4096;
    // Inventory bytes must be 1 byte..128 MiB; file count must be 1..1,000,000.
    // At most max_files * 4 directory entries may be inspected.
    // Depth is 1..64 descendant directory levels (root depth0), refused before
    // descending beyond it. Absolute UTF-8 path bytes are 1..4096, including root
    // and every entry. Callers may tighten either bound, never disable it.
    void validate() const;
};

struct DirectoryStoreFingerprintLimits {
    std::uint64_t file_bytes = UINT64_C(16) * 1024 * 1024 * 1024;
    std::uint64_t total_bytes = UINT64_C(16) * 1024 * 1024 * 1024;
    // Both limits are 1 byte..16 GiB; per-file must not exceed total.
    void validate() const;
};
struct DirectoryStoreContentFingerprint {
    std::string content_sha256;
    std::uint64_t files = 0, bytes = 0;
};

// Immutable observation of an absolute, lexically normalized local directory,
// including all regular files below it. Trailing separators are removed except
// at a filesystem root. Links/reparse points in its store/ancestors are refused.
// This observes membership and OS file identity/version metadata. Its SHA-256
// is NEVER a content fingerprint, decoder identity or proof of unchanged bytes
// when the filesystem cannot supply trustworthy file-version metadata.
class DirectoryStoreSnapshot final {
public:
    explicit DirectoryStoreSnapshot(const std::string& absolute_directory,
        DirectoryStoreLimits = {}, const std::atomic<bool>* cancelled = nullptr);

    const std::string& directory() const;
    const std::string& observed_sha256() const;
    const DirectoryStoreLimits& limits() const;
    std::size_t payload_bytes() const;

    // A second bounded inventory is live alongside this retained snapshot.
    // Callers must reserve payload_bytes() + limits().inventory_bytes, plus
    // 1 MiB filesystem/path working storage for the supported Windows/MSVC
    // profile, when accounting currentness checks (excluding OS caches).
    // An observation is not an atomic filesystem transaction or a content hash.
    void require_current(const std::atomic<bool>* cancelled = nullptr) const;

    // Bounded read of an observed canonical member, with version checks around
    // the read. A member that was absent and is still absent returns nullopt.
    // Present empty files return an empty buffer. New or replaced members fail.
    std::optional<std::vector<std::uint8_t>> read_member(const std::string& relative_key,
        std::size_t byte_limit, const std::atomic<bool>* cancelled = nullptr) const;

    // Canonical relative keys, file lengths and complete content, streamed with
    // a fixed64KiB buffer and per-file native guards. Rechecks the observed
    // store before/after hashing. Reserve the ordinary simultaneous-inventory
    // allowance above plus1MiB fingerprint/guard work; no whole-file retention.
    // This content digest is distinct from observed_sha256 and excludes the
    // absolute root/OS file versions. It is not an atomic filesystem snapshot.
    DirectoryStoreContentFingerprint content_fingerprint(
        DirectoryStoreFingerprintLimits = {},
        const std::atomic<bool>* cancelled = nullptr) const;

    // Select an existing primary file explicitly; no filename/schema is guessed.
    // The key is a nonempty slash-separated relative UTF-8 file key with no
    // empty, dot/dot-dot, absolute, backslash, NUL or colon/ADS components. Keys
    // are at most4096 UTF-8 bytes; their absolute path must fit the configured
    // absolute_path_bytes limit too.
    // The returned directory_store ReaderSource owns its strings and primary
    // FileVersion, represents all files without a companion list, and outlives
    // this snapshot.
    // Assembly performs the same second-inventory currentness check while the
    // primary guard is alive; also reserve the source's at-most-128 KiB payload.
    std::shared_ptr<const ReaderSource> reader_source(std::string reader_binding,
        const std::string& primary_relative_key,
        const std::atomic<bool>* cancelled = nullptr) const;

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

}
