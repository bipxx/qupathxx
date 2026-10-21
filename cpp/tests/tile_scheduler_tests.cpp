#include "qupathxx/tile_scheduler.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>

using namespace qpx;
void check(bool condition) { if (!condition) throw std::runtime_error("scheduler check failed"); }
template<class Exception, class Action> void rejects(Action action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("expected exception was not raised");
}
// First decode is gated, so queue order/cancellation tests never rely on sleeps.
class Reader final : public ImageServer {
public:
    ImageMetadata meta;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;
    mutable std::vector<std::int64_t> order;
    bool released = false;
    std::atomic<bool> is_closed{false};
    bool fail = false;
    Reader() { meta.width=1000; meta.height=1; meta.levels.push_back({1000,1,1}); }
    const ImageMetadata& metadata() const override { return meta; }
    PixelBuffer read_region(const RegionRequest& r) const override {
        std::unique_lock<std::mutex> lock(mutex);
        order.push_back(r.x); cv.notify_all();
        if (!cv.wait_for(lock,std::chrono::seconds(5),[&]{ return released; })) throw std::runtime_error("test reader gate timed out");
        if (fail || is_closed) throw std::runtime_error("test decode failure");
        PixelBuffer p(r.width,r.height); p.writable_bytes()[0]=static_cast<std::uint8_t>(r.x); return p;
    }
    void close() override { is_closed = true; }
    bool closed() const override { return is_closed; }
    void entered() {
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock,std::chrono::seconds(5),[&]{ return !order.empty(); })) throw std::runtime_error("worker did not enter reader");
    }
    void release() { std::lock_guard<std::mutex> lock(mutex); released=true; cv.notify_all(); }
};
SchedulerOptions options() { SchedulerOptions c; c.workers=1; c.cache_bytes=8; c.work_bytes=64; return c; }
void priority_and_dedup() {
    auto r=std::make_shared<Reader>(); TileScheduler s(r,options());
    auto first=s.submit({0,0,0,1,1}); r->entered();
    auto low=s.submit({1,0,0,1,1},0), mid=s.submit({2,0,0,1,1},10), high=s.submit({3,0,0,1,1},20);
    check(s.submit({2,0,0,1,1},30)==mid);
    r->release(); check(s.wait_idle(5));
    check(r->order==std::vector<std::int64_t>({0,2,3,1}));
    check(s.stats().reads==4);
    for (auto id : {first,low,mid,high}) check(s.take(id).status==TileStatus::Ready);
    check(s.stats().reserved_bytes==0);
}
void generation_and_cancel() {
    auto r=std::make_shared<Reader>(); auto c=options(); c.work_bytes=8; c.workers=4;
    TileScheduler s(r,c); auto running=s.submit({0,0,0,1,1}); r->entered();
    auto queued=s.submit({1,0,0,1,1}); check(s.advance_generation()==2);
    check(s.take(running).status==TileStatus::Stale); check(s.take(queued).status==TileStatus::Stale);
    check(s.stats().reserved_bytes==8); // An uninterruptible read still owns its reservation.
    auto fresh=s.submit({2,0,0,1,1}); r->release(); check(s.wait_idle(5));
    auto result=s.take(fresh); check(result.status==TileStatus::Ready && result.generation==2);
    check(s.stats().reads==2 && s.stats().cache_entries==1);
    check(result.pixels->bytes()[0]==2);
    auto cached=s.submit({2,0,0,1,1}); check(s.wait_idle(5));
    check(s.cancel(cached)); check(s.take(cached).status==TileStatus::Cancelled);
    check(s.stats().reserved_bytes==0);
    rejects<std::out_of_range>([&]{ s.take(cached); });
}
void cache_and_backpressure() {
    auto r=std::make_shared<Reader>(); r->release(); auto c=options(); c.work_bytes=8; c.max_requests=2; c.max_cache_entries=1;
    TileScheduler s(r,c);
    auto a=s.submit({0,0,0,1,1}); check(s.wait_idle(5));
    auto b=s.submit({1,0,0,1,1}); check(!s.wait_idle(0));
    rejects<std::runtime_error>([&]{ s.submit({2,0,0,1,1}); });
    auto retained=s.take(a); check(s.wait_idle(5)); check(s.take(b).status==TileStatus::Ready);
    check(s.stats().cache_entries==1 && s.stats().cache_bytes==4);
    auto cached=s.submit({1,0,0,1,1}); check(s.wait_idle(5)); check(s.take(cached).status==TileStatus::Ready);
    check(s.stats().cache_hits==1 && s.stats().reads==2);
    auto evicted=s.submit({0,0,0,1,1}); check(s.wait_idle(5)); check(s.take(evicted).status==TileStatus::Ready);
    check(s.stats().reads==3); s.clear_cache(); check(s.stats().cache_bytes==0);
    s.shutdown(); check(retained.pixels->bytes().size()==4); check(!r->closed());
    rejects<std::runtime_error>([&]{ s.submit({0,0,0,1,1}); });
}
void shutdown_and_failure() {
    auto r=std::make_shared<Reader>(); TileScheduler s(r,options());
    auto id=s.submit({0,0,0,1,1}); r->entered();
    check(s.cancel(id)); check(s.take(id).status==TileStatus::Cancelled);
    check(s.stats().reserved_bytes==8);
    auto queued=s.submit({1,0,0,1,1});
    auto shutdown=std::async(std::launch::async,[&]{ s.shutdown(); });
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (!s.stats().stopped && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    check(s.stats().stopped);
    r->release(); check(shutdown.wait_for(std::chrono::seconds(5))==std::future_status::ready); shutdown.get();
    check(s.take(queued).status==TileStatus::Cancelled); check(s.stats().reserved_bytes==0);
    s.shutdown(); check(s.wait_idle(0));
    auto broken=std::make_shared<Reader>(); broken->fail=true; broken->release(); TileScheduler fail(broken,options());
    auto bad=fail.submit({0,0,0,1,1}); check(fail.wait_idle(5)); auto result=fail.take(bad);
    check(result.status==TileStatus::Failed && !result.error.empty() && !result.pixels);
    check(fail.stats().reserved_bytes==0 && fail.stats().cache_bytes==0);
}
int main() {
    try { priority_and_dedup(); generation_and_cancel(); cache_and_backpressure(); shutdown_and_failure(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
