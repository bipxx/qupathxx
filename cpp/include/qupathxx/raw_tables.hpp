#pragma once
#include "qupathxx/raw_analysis.hpp"

namespace qpx {
// Owner capture, worker serialization. Rows retain immutable native records and
// status-at-capture values, never a live document or decoder handle.
class RawTableRequest {
public:
    RawTableRequest(const Document&,const std::vector<std::uint64_t>& record_ids={},
        bool require_current=false,std::size_t snapshot_bytes=128u*1024*1024,const std::vector<std::string>& protected_paths={});
    std::uint64_t rows() const;
    std::size_t records() const;
    std::size_t payload_bytes() const;
private:
    friend struct RawTableKernel;
    struct Data;std::shared_ptr<const Data> data_;
};
std::string raw_analysis_csv(const RawTableRequest&,std::size_t max_bytes=64u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
std::size_t save_raw_analysis_csv(const RawTableRequest&,const std::string& path,
    bool replace_existing=false,std::size_t max_bytes=1024u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
struct RawTableStats {
    std::string state="idle",error,path;
    std::uint64_t generation=0,completed_rows=0,total_rows=0;
    std::size_t bytes=0;
    double elapsed_ms=0;
};
class RawTableJob {
public:
    RawTableJob();~RawTableJob();RawTableJob(const RawTableJob&)=delete;
    bool request(const RawTableRequest&,const std::string& path,bool replace_existing=false,std::size_t max_bytes=1024u*1024*1024);
    RawTableStats poll();RawTableStats wait();void await_completion();void cancel();bool busy() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
