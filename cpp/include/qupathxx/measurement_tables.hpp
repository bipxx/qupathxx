#pragma once
#include "qupathxx/tissue.hpp"
namespace qpx {
enum class MeasurementTableKind { Roi, Tissue };
struct MeasurementTableOptions {
    bool require_current=false;
    std::size_t byte_limit=256*1024*1024,snapshot_budget=256*1024*1024;
    void validate() const;
};
class MeasurementTableRequest {
public:
    MeasurementTableRequest(const Document&,MeasurementTableKind,const std::vector<std::uint64_t>& record_ids={},MeasurementTableOptions={});
    MeasurementTableRequest(const Project&,MeasurementTableKind,const std::vector<std::uint64_t>& image_ids={},MeasurementTableOptions={});
    MeasurementTableKind kind() const;
    std::size_t record_count() const;
    std::uint64_t row_count() const;
    std::size_t snapshot_bytes() const;
    double capture_ms() const;
    std::uint64_t document_revision() const;
    bool project_scope() const;
    std::size_t image_count() const;
    std::string project_identity() const;
    std::uint64_t project_generation() const;
    std::uint64_t project_revision() const;
    MeasurementTableOptions options() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    friend struct MeasurementTableWriter;
};
std::string measurements_csv(const MeasurementTableRequest&,const std::atomic<bool>* cancelled=nullptr);
void save_measurements_csv(const MeasurementTableRequest&,const std::string& path,const std::atomic<bool>* cancelled=nullptr);
struct MeasurementTableExportStats {
    std::string state="idle",path,error;
    std::uint64_t rows=0,total_rows=0,completed=0,document_revision=0;
    std::size_t bytes=0,snapshot_bytes=0;
    double capture_ms=0,write_ms=0;
};
class MeasurementTableExportJob {
public:
    MeasurementTableExportJob();~MeasurementTableExportJob();MeasurementTableExportJob(const MeasurementTableExportJob&)=delete;
    bool request(const MeasurementTableRequest&,const std::string& path);
    MeasurementTableExportStats poll();MeasurementTableExportStats wait();void await_completion();void cancel();bool busy() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
