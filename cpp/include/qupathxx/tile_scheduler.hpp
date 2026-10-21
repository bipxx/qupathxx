#pragma once
#include "qupathxx/image_server.hpp"
#include <memory>

namespace qpx {
enum class TileStatus { Queued, Running, Ready, Cancelled, Stale, Failed };
struct SchedulerOptions {
    std::size_t workers = 2;
    std::size_t cache_bytes = 64 * 1024 * 1024;
    std::size_t work_bytes = 64 * 1024 * 1024;
    std::size_t max_requests = 256;
    std::size_t max_cache_entries = 1024;
};
struct TileResult {
    std::uint64_t ticket, generation;
    TileStatus status;
    std::shared_ptr<const PixelBuffer> pixels;
    std::string error;
};
struct SchedulerStats {
    std::size_t cache_bytes = 0, reserved_bytes = 0, requests = 0, queued = 0, running = 0, ready = 0, cache_entries = 0;
    std::uint64_t reads = 0, cache_hits = 0;
    bool stopped = false;
};
// One scheduler is bound to one immutable source. Native workers never call Python.
class TileScheduler {
public:
    explicit TileScheduler(std::shared_ptr<ImageServer>, SchedulerOptions options = {});
    ~TileScheduler();
    TileScheduler(const TileScheduler&) = delete;
    TileScheduler& operator=(const TileScheduler&) = delete;
    std::uint64_t submit(const RegionRequest&, int priority = 0);
    bool cancel(std::uint64_t ticket);
    std::uint64_t advance_generation();
    std::uint64_t generation() const;
    // Terminal results are consumed. Pending results are inspected without removal.
    // Unknown/previously consumed tickets throw out_of_range.
    TileResult take(std::uint64_t ticket);
    bool wait_idle(double timeout_seconds);
    SchedulerStats stats() const;
    void clear_cache();
    void shutdown();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
