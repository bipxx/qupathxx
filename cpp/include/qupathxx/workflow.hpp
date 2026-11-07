#pragma once
#include <stdexcept>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
namespace qpx {
class WorkflowInputChanged : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class WorkflowOutputExists : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
class WorkflowEvents {
    std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::string> events_;
    std::size_t published_=0,acknowledged_=0,bytes_=0;
public:
    void reset() {std::lock_guard<std::mutex> lock(mutex_);events_.clear();published_=acknowledged_=bytes_=0;}
    std::vector<std::string> take() {std::lock_guard<std::mutex> lock(mutex_);auto result=std::move(events_);events_.clear();bytes_=0;return result;}
    void acknowledge() {{std::lock_guard<std::mutex> lock(mutex_);acknowledged_=published_;}condition_.notify_all();}
    void publish(std::string json,bool pause,const std::atomic<bool>* cancelled) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(published_==1000 || json.size()>16u*1024*1024-bytes_)throw std::length_error("Workflow event queue limit exceeded");
        bytes_+=json.size();events_.push_back(std::move(json));const auto sequence=++published_;
        while(pause && acknowledged_<sequence && !(cancelled && cancelled->load()))condition_.wait_for(lock,std::chrono::milliseconds(20));
    }
};
}
