// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/directory_store.hpp"
#include "qupathxx/workflow.hpp"

namespace qpx::detail {

class N5Store {
public:
    N5Store(const std::string& directory, std::size_t inventory_bytes,
            const std::atomic<bool>* cancelled, const char* provider = "org.qupathxx.n5",
            const char* primary = "attributes.json")
        : snapshot_(directory, DirectoryStoreLimits{inventory_bytes}, cancelled),
          source_(snapshot_.reader_source(provider, primary, cancelled)) {}

    std::vector<std::uint8_t> read(const std::string& key, std::size_t limit,
            bool missing_allowed = false, const std::atomic<bool>* cancelled = nullptr) const {
        auto member = snapshot_.read_member(key, limit, cancelled);
        if (!member) {
            if (missing_allowed)
                return {};
            throw std::invalid_argument("Required N5 member is absent: " + key);
        }
        if (member->empty())
            throw std::invalid_argument("Observed N5 member is empty: " + key);
        return std::move(*member);
    }

    void require_current(const std::atomic<bool>* cancelled) const {
        snapshot_.require_current(cancelled);
    }
    std::size_t payload_bytes() const {
        return sizeof(*this) + snapshot_.payload_bytes() + source_->payload_bytes();
    }
    std::size_t inventory_budget() const { return snapshot_.limits().inventory_bytes; }
    const std::string& directory() const { return snapshot_.directory(); }
    const std::shared_ptr<const ReaderSource>& reader_source() const { return source_; }

private:
    DirectoryStoreSnapshot snapshot_;
    std::shared_ptr<const ReaderSource> source_;
};

}
