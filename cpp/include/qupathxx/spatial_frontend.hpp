#pragma once
#include "qupathxx/spatial_analysis.hpp"
#include "qupathxx/project.hpp"
namespace qpx {
struct GuiSpatialOptions {
    bool open=false,run=false;
    std::vector<std::uint64_t> sources,targets;
    SpatialDistanceOptions analysis;
    std::uint64_t record_id=0;
    std::string csv_path;
    std::size_t csv_bytes=64u*1024*1024;
    void validate() const;
};
struct GuiSpatialReport {
    std::string state="idle",error,record_status,export_state="idle",export_error,path;
    bool busy=false,export_committed=false;
    std::uint64_t record_id=0;
    std::size_t rows=0,sources=0,targets=0,export_bytes=0,export_rows=0;
    SpatialDistanceStats analysis;
    std::shared_ptr<const SpatialDistanceResult> result;
};
namespace detail {
// One owner, one operation, immutable worker inputs. Desktop and queued viewer
// consumers share this controller; neither owns a second analysis model.
class SpatialFrontend {
public:
    SpatialFrontend();~SpatialFrontend();
    SpatialFrontend(const SpatialFrontend&)=delete;
    bool busy() const;bool same_document(const Document&) const;
    void measure(const Document&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,SpatialDistanceOptions={});
    void inspect(const Document&,std::uint64_t);
    void verify(const Document&,std::uint64_t);
    std::uint64_t keep(Document&,std::string name="Spatial distances");
    void export_csv(const Document&,const std::string&,std::size_t byte_limit,
        const std::vector<std::string>& protected_paths={},std::size_t snapshot_bytes=16u*1024*1024);
    void poll(Document&);void cancel();void finish(Document&,bool wait);
    GuiSpatialReport report() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Catalog is already captured by the GUI/managed owner. Never copy the whole
// Project graph into a worker; bound the protected-path snapshot to16 MiB.
std::vector<std::string> spatial_project_paths(const Project*,const std::vector<ProjectImage>*,std::size_t snapshot_bytes=16u*1024*1024);
}
}
