#pragma once
#include "qupathxx/cells.hpp"
namespace qpx {
struct CellTableOptions {
    bool require_current=false;
    std::size_t byte_limit=256*1024*1024,snapshot_budget=256*1024*1024;
    void validate() const;
};
class CellTableRequest {
public:
    CellTableRequest(const Document&,const std::vector<std::uint64_t>& record_ids={},CellTableOptions={});
    std::size_t record_count() const;
    std::uint64_t row_count() const;
    std::size_t snapshot_bytes() const;
    double capture_ms() const;
    std::uint64_t document_revision() const;
    CellTableOptions options() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    friend struct CellTableWriter;
};
std::string cell_measurements_csv(const CellTableRequest&,const std::atomic<bool>* cancelled=nullptr);
void save_cell_measurements_csv(const CellTableRequest&,const std::string& path,const std::atomic<bool>* cancelled=nullptr);
struct CellTableExportStats {
    std::string state="idle",path,error;
    std::uint64_t rows=0,total_rows=0,completed=0,document_revision=0;
    std::size_t bytes=0,snapshot_bytes=0;
    double capture_ms=0,write_ms=0;
};
class CellTableExportJob {
public:
    CellTableExportJob();~CellTableExportJob();CellTableExportJob(const CellTableExportJob&)=delete;
    bool request(const CellTableRequest&,const std::string& path);
    CellTableExportStats poll();CellTableExportStats wait();void cancel();bool busy() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
