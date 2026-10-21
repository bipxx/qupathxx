#include "qupathxx/tile_scheduler.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace qpx {
namespace {
using Key = std::tuple<std::int64_t,std::int64_t,std::int32_t,std::int64_t,std::int64_t>;
Key key(const RegionRequest& r) { return {r.x,r.y,r.level,r.width,r.height}; }
bool pending(TileStatus s) { return s == TileStatus::Queued || s == TileStatus::Running; }
}
struct TileScheduler::Impl {
    struct Job {
        std::uint64_t ticket, generation;
        RegionRequest request;
        int priority;
        TileStatus status = TileStatus::Queued;
        std::shared_ptr<const PixelBuffer> pixels;
        std::string error;
        std::size_t reserved = 0;
        bool decoding = false;
    };
    struct Cached { std::shared_ptr<const PixelBuffer> pixels; std::list<Key>::iterator position; };
    std::shared_ptr<ImageServer> server;
    SchedulerOptions options;
    mutable std::mutex mutex;
    std::mutex shutdown_mutex;
    std::condition_variable cv;
    std::map<std::uint64_t,std::shared_ptr<Job>> jobs;
    std::map<Key,Cached> cache;
    std::list<Key> lru;
    std::vector<std::thread> workers;
    std::size_t reserved = 0, cache_size = 0, active = 0;
    std::uint64_t generation = 1, next_ticket = 1, reads = 0, hits = 0;
    bool stopped = false;

    Impl(std::shared_ptr<ImageServer> source, SchedulerOptions config) : server(std::move(source)), options(config) {
        if (!server || server->closed()) throw std::invalid_argument("scheduler requires an open image server");
        if (!options.workers || options.workers > 64 || !options.work_bytes || !options.max_requests)
            throw std::invalid_argument("invalid scheduler limits");
        try {
            for (std::size_t i = 0; i < options.workers; ++i) workers.emplace_back([this]{ work(); });
        } catch (...) { shutdown(); throw; }
    }
    ~Impl() { shutdown(); }
    void release(Job& j) { reserved -= j.reserved; j.reserved = 0; }
    void invalidate(Job& j, TileStatus status) {
        j.status = status; j.pixels.reset(); j.error.clear();
        if (!j.decoding) release(j);
    }
    std::shared_ptr<Job> candidate() const {
        std::shared_ptr<Job> best;
        for (const auto& [id,j] : jobs) {
            if (j->status != TileStatus::Queued || region_byte_count(j->request.width,j->request.height)*2 > options.work_bytes-reserved) continue;
            if (!best || j->priority > best->priority) best = j;
        }
        return best;
    }
    void cache_insert(const Key& k, std::shared_ptr<const PixelBuffer> pixels) {
        const auto bytes = pixels->bytes().size();
        if (!options.max_cache_entries || bytes > options.cache_bytes) return;
        auto old = cache.find(k);
        if (old != cache.end()) { lru.splice(lru.begin(),lru,old->second.position); return; }
        while (!lru.empty() && (bytes > options.cache_bytes-cache_size || cache.size() >= options.max_cache_entries)) {
            auto it = cache.find(lru.back()); cache_size -= it->second.pixels->bytes().size(); cache.erase(it); lru.pop_back();
        }
        // Allocate bookkeeping before changing byte accounting. Failure is handled
        // by the worker as a failed job; caching never leaks its LRU node.
        lru.push_front(k);
        try { cache.emplace(k,Cached{std::move(pixels),lru.begin()}); }
        catch (...) { lru.pop_front(); throw; }
        cache_size += bytes;
    }
    void work() {
        for (;;) {
            std::shared_ptr<Job> j;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock,[&]{ return stopped || static_cast<bool>(candidate()); });
                if (stopped) return;
                j = candidate();
                j->reserved = region_byte_count(j->request.width,j->request.height)*2; reserved += j->reserved;
                auto found = cache.find(key(j->request));
                if (found != cache.end()) {
                    ++hits; lru.splice(lru.begin(),lru,found->second.position);
                    j->pixels = found->second.pixels; j->status = TileStatus::Ready; cv.notify_all(); continue;
                }
                j->status = TileStatus::Running; j->decoding = true; ++active; ++reads;
            }
            std::shared_ptr<const PixelBuffer> result;
            std::string error;
            try { result = std::make_shared<PixelBuffer>(server->read_region(j->request)); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "unknown image reader failure"; }
            {
                std::lock_guard<std::mutex> lock(mutex);
                j->decoding = false; --active;
                if (stopped || j->status != TileStatus::Running || j->generation != generation) {
                    result.reset(); release(*j);
                } else if (!error.empty()) {
                    j->status = TileStatus::Failed; j->error = std::move(error); release(*j);
                } else {
                    try {
                        cache_insert(key(j->request),result);
                        j->pixels = std::move(result); j->status = TileStatus::Ready;
                    } catch (const std::exception& e) {
                        j->status = TileStatus::Failed; j->error = e.what(); result.reset(); release(*j);
                    }
                }
                cv.notify_all();
            }
        }
    }
    void shutdown() {
        // Serialize joins; never hold the state mutex while waiting for workers.
        std::lock_guard<std::mutex> joining(shutdown_mutex);
        {
            std::lock_guard<std::mutex> lock(mutex); stopped = true;
            for (auto& [id,j] : jobs) invalidate(*j,TileStatus::Cancelled);
            cache.clear(); lru.clear(); cache_size = 0;
        }
        cv.notify_all();
        for (auto& thread : workers) if (thread.joinable()) thread.join();
    }
};
TileScheduler::TileScheduler(std::shared_ptr<ImageServer> server, SchedulerOptions options)
    : impl_(std::make_unique<Impl>(std::move(server),options)) {}
TileScheduler::~TileScheduler() = default;
std::uint64_t TileScheduler::submit(const RegionRequest& r, int priority) {
    auto& s = *impl_; validate_region(s.server->metadata(),r);
    const auto required = region_byte_count(r.width,r.height)*2;
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.stopped) throw std::runtime_error("tile scheduler is shut down");
    if (required > s.options.work_bytes) throw std::invalid_argument("tile exceeds scheduler work budget");
    for (const auto& [id,j] : s.jobs) if (j->generation == s.generation &&
            (pending(j->status) || j->status == TileStatus::Ready) && key(j->request) == key(r)) {
        j->priority = std::max(j->priority,priority); s.cv.notify_all(); return id;
    }
    if (s.jobs.size() >= s.options.max_requests) throw std::runtime_error("request limit reached; consume terminal tickets");
    if (s.next_ticket == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("ticket identity exhausted");
    auto j = std::make_shared<Impl::Job>(Impl::Job{s.next_ticket,s.generation,r,priority});
    s.jobs.emplace(j->ticket,j); ++s.next_ticket; s.cv.notify_all(); return j->ticket;
}
bool TileScheduler::cancel(std::uint64_t ticket) {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    auto found = s.jobs.find(ticket); if (found == s.jobs.end()) return false;
    if (found->second->status == TileStatus::Cancelled || found->second->status == TileStatus::Stale) return false;
    s.invalidate(*found->second,TileStatus::Cancelled); s.cv.notify_all(); return true;
}
std::uint64_t TileScheduler::advance_generation() {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    if (s.stopped) throw std::runtime_error("tile scheduler is shut down");
    if (s.generation == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("generation exhausted");
    ++s.generation;
    for (auto& [id,j] : s.jobs) s.invalidate(*j,TileStatus::Stale);
    s.cv.notify_all(); return s.generation;
}
std::uint64_t TileScheduler::generation() const { auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex); return s.generation; }
TileResult TileScheduler::take(std::uint64_t ticket) {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.jobs.find(ticket); if (it == s.jobs.end()) throw std::out_of_range("unknown or consumed ticket");
    auto j = it->second;
    TileResult result{j->ticket,j->generation,j->status,j->pixels,j->error};
    if (!pending(j->status)) {
        if (!j->decoding) s.release(*j);
        s.jobs.erase(it); s.cv.notify_all();
    }
    return result;
}
bool TileScheduler::wait_idle(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 86400) throw std::invalid_argument("timeout must be between 0 and 86400 seconds");
    auto& s = *impl_; std::unique_lock<std::mutex> lock(s.mutex);
    return s.cv.wait_for(lock,std::chrono::duration<double>(seconds),[&]{
        if (s.active) return false;
        for (const auto& [id,j] : s.jobs) if (pending(j->status)) return false;
        return true;
    });
}
SchedulerStats TileScheduler::stats() const {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    SchedulerStats result;
    result.cache_bytes = s.cache_size; result.reserved_bytes = s.reserved; result.requests = s.jobs.size();
    result.running = s.active; result.cache_entries = s.cache.size(); result.reads = s.reads; result.cache_hits = s.hits; result.stopped = s.stopped;
    for (const auto& [id,j] : s.jobs) {
        if (j->status == TileStatus::Queued) ++result.queued;
        if (j->status == TileStatus::Ready) ++result.ready;
    }
    return result;
}
void TileScheduler::clear_cache() {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex); s.cache.clear(); s.lru.clear(); s.cache_size = 0;
}
void TileScheduler::shutdown() { impl_->shutdown(); }
}
