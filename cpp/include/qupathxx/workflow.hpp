#pragma once
#include <stdexcept>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <exception>
#include <utility>
namespace qpx {
class WorkflowInputChanged : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class WorkflowOutputExists : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class WorkflowCancelled : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class WorkflowCaptureLimit : public std::length_error {
public:
    using std::length_error::length_error;
};
class WorkflowControlError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
inline void workflow_capture_charge(std::size_t& bytes,std::size_t extra,std::size_t limit) {
    if(bytes>limit || extra>limit-bytes)throw WorkflowCaptureLimit("Workflow capture payload exceeds byte limit");bytes+=extra;
}
inline void workflow_path_argument(const std::string& path,bool optional=false) {
    if((path.empty() && !optional) || path.size()>32768 || path.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid workflow path argument");
}
inline void workflow_checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed))throw WorkflowCancelled("Workflow cancelled");
}
inline void workflow_require_owner(std::thread::id owner) {
    if(owner!=std::this_thread::get_id())throw std::logic_error("Workflow controller requires its owner thread");
}
inline std::string workflow_failure_message(const std::exception_ptr& failure) {
    try{if(failure)std::rethrow_exception(failure);}catch(const std::exception& e){return e.what();}catch(...){return "Unknown native workflow failure";}return {};
}
class WorkflowEvents {
    std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::string> events_;
    std::size_t published_=0,acknowledged_=0,bytes_=0;
    std::string failure_;
public:
    void reset() {std::lock_guard<std::mutex> lock(mutex_);events_.clear();published_=acknowledged_=bytes_=0;failure_.clear();}
    std::vector<std::string> take() {std::lock_guard<std::mutex> lock(mutex_);auto result=std::move(events_);events_.clear();bytes_=0;return result;}
    void acknowledge() {{std::lock_guard<std::mutex> lock(mutex_);acknowledged_=published_;}condition_.notify_all();}
    void fail(std::string error) {
        if(error.empty())error="Workflow control failed";
        if(error.size()>8192){auto end=std::size_t(8192);while(end && (static_cast<unsigned char>(error[end])&0xc0)==0x80)--end;error.resize(end);}
        {std::lock_guard<std::mutex> lock(mutex_);if(failure_.empty())failure_=std::move(error);}condition_.notify_all();
    }
    void publish(std::string json,bool pause,const std::atomic<bool>* cancelled) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(published_==1000 || json.size()>16u*1024*1024-bytes_)throw std::length_error("Workflow event queue limit exceeded");
        bytes_+=json.size();events_.push_back(std::move(json));const auto sequence=++published_;
        while(pause && acknowledged_<sequence && failure_.empty() && !(cancelled && cancelled->load()))condition_.wait_for(lock,std::chrono::milliseconds(20));
        if(!failure_.empty())throw WorkflowControlError(failure_);
    }
};
}
