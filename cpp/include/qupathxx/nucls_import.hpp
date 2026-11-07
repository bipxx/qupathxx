#pragma once
#include "qupathxx/workflow.hpp"
#include <future>
#include <map>
#include <memory>
#include <optional>
namespace qpx {
struct NuclsImportLimits {
    std::size_t images=100,pixels=16000000,rows=100000,vertices=1000000,issues_per_image=50;
    std::size_t png_bytes=32u*1024*1024,csv_bytes=16u*1024*1024,report_bytes=16u*1024*1024;
    std::size_t snapshot_bytes=512u*1024*1024,capture_bytes=512u*1024*1024;
    std::uint64_t file_bytes=1024ull*1024*1024;
    void validate() const;
};
struct NuclsImportOptions {
    double mpp_x=0,mpp_y=0;
    std::string representation="outlines",class_column="main_classification";
    std::optional<std::vector<std::string>> names;
    NuclsImportLimits limits;
    bool pause_after_pair=false;
    void validate() const;
};
struct NuclsPairReport {
    std::string name,image,annotations,image_sha256,csv_sha256;
    std::size_t rows=0,imported=0,source_bounds_outside=0,issue_count=0;
    std::map<std::string,std::size_t> classes;
    std::vector<std::string> issues;
    std::optional<unsigned> width,height;
};
struct NuclsImportReport {
    bool valid=false,written=false,cancelled=false;
    std::size_t selected_pairs=0;
    NuclsImportOptions options;
    std::vector<NuclsPairReport> images;
    std::string output,json;
};
struct NuclsImportProgress {
    std::atomic<std::size_t> pairs{0},total{0},rows{0};
    std::atomic<int> phase{0}; // opening,pairs,verify,publish
    WorkflowEvents events;
    void reset(){pairs.store(0);total.store(0);rows.store(0);phase.store(0);events.reset();}
};
NuclsImportReport import_nucls(const std::string& images,const std::string& annotations,
    const NuclsImportOptions&,const std::string& output="",const std::atomic<bool>* cancelled=nullptr,
    NuclsImportProgress* progress=nullptr);
class NuclsImportJob {
    std::future<NuclsImportReport> future_;
    std::thread::id owner_=std::this_thread::get_id();
    std::exception_ptr failure_;
    std::atomic<bool> cancelled_{false};
    NuclsImportProgress progress_;
    std::shared_ptr<const NuclsImportReport> result_;
    std::string error_;
public:
    ~NuclsImportJob();
    bool busy() const {workflow_require_owner(owner_);return future_.valid();}
    bool run(const std::string& images,const std::string& annotations,const NuclsImportOptions&,const std::string& output="");
    void cancel(){cancelled_.store(true);}
    void poll();void finish();
    void abort_control(std::string error){workflow_require_owner(owner_);progress_.events.fail(std::move(error));}
    void rethrow_error() const {workflow_require_owner(owner_);if(failure_)std::rethrow_exception(failure_);}
    std::vector<std::string> take_pairs(){workflow_require_owner(owner_);return progress_.events.take();}
    void continue_pairs(){workflow_require_owner(owner_);progress_.events.acknowledge();}
    const NuclsImportProgress& progress() const {workflow_require_owner(owner_);return progress_;}
    const std::string& error() const {workflow_require_owner(owner_);return error_;}
    std::shared_ptr<const NuclsImportReport> result() const {workflow_require_owner(owner_);return result_;}
};
}
