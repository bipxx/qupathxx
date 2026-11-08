#pragma once
#include "qupathxx/raw_pixel_file_jobs.hpp"
#include "utf8_text.hpp"
#include <chrono>
#include <future>
#include <thread>

namespace qpx::detail {
// Common lifecycle only. Concrete jobs retain their typed request and implement
// source/document/file validation; this helper never knows a live Document.
template<class Result> struct RawPixelFileJobState {
    const std::thread::id owner=std::this_thread::get_id();
    std::atomic<bool> cancelled{false};std::future<Result> future;
    std::optional<Result> pending;RawPixelFileJobStats stats;
    bool stale=false;std::chrono::steady_clock::time_point started;
    ~RawPixelFileJobState() {cancelled.store(true);if(future.valid()) future.wait();}
    void on_owner() const {if(owner!=std::this_thread::get_id()) throw std::runtime_error("raw file controller must run on its owner thread");}
    bool busy() const {on_owner();return future.valid() && future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
    void join() {on_owner();if(future.valid()) future.wait();}
    template<class Work> bool start(Work work,const char* phase,std::uint64_t total=0) {
        on_owner();if(future.valid() || pending) return false;
        if(stats.generation==UINT64_MAX) throw std::overflow_error("raw file job generation exhausted");
        const bool old=cancelled.exchange(false);std::future<Result> next;
        try {next=std::async(std::launch::async,[this,work=std::move(work)] {return work(&cancelled);});}
        catch(...) {cancelled.store(old);throw;}
        future=std::move(next);stale=false;const auto generation=stats.generation+1;stats={};stats.generation=generation;stats.state="running";stats.phase=phase;stats.total_windows=total;started=std::chrono::steady_clock::now();return true;
    }
    void error(const std::exception& e) {try {stats.error=e.what();bounded_utf8(stats.error,8192);}catch(const std::exception&) {stats.error="Raw file operation failed";}}
    template<class Matches,class Publish> RawPixelFileJobStats poll(Matches matches,Publish publish) {
        on_owner();
        if((future.valid() || pending) && !matches()) {stale=true;cancelled.store(true);pending.reset();stats.state=busy()?"cancelling":"stale";}
        if(future.valid() && future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {auto out=future.get();if(stale) stats.state="stale";else if(cancelled.load()) stats.state="cancelled";else {pending=std::move(out);stats.state="ready";}}
            catch(const std::exception& e) {error(e);stats.state=stale?"stale":cancelled.load()?"cancelled":"failed";pending.reset();}
            catch(...) {stats.error="Unknown raw file failure";stats.state=stale?"stale":cancelled.load()?"cancelled":"failed";pending.reset();}
        }
        if(pending && !cancelled.load()) {
            try {if(!publish(*pending)) {stale=true;pending.reset();stats.state="stale";}}
            catch(const std::exception& e) {error(e);stale=true;pending.reset();stats.state="stale";}
        }
        if(cancelled.load() && (future.valid() || pending || stats.state=="ready" || stats.state=="running")) {pending.reset();stats.state=busy()?"cancelling":stale?"stale":"cancelled";}
        if(!future.valid() && stats.generation) {stats.phase=stats.state=="ready"?"waiting for consumer":"finished";stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();}
        return stats;
    }
    std::optional<Result> take() {on_owner();if(!pending || cancelled.load() || stats.state!="ready") return {};auto out=std::move(pending);pending.reset();stats.state="delivered";stats.phase="finished";return out;}
};
}
