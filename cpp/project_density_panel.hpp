#pragma once
// Private desktop consumer; no new SDK/binding contract.
#include "qupathxx/spatial_density.hpp"
#include <array>
#include <atomic>
#include <future>
#include <string>
#include <vector>
namespace qpx::detail {
struct DensityPanelRecipe {
    std::vector<std::uint64_t> images;
    std::array<std::vector<std::string>,4> classes; // windows,tissue,excluded,cells
    bool all_cells=true;
    SpatialDensityOptions options;
    void validate() const;
};
struct DensityPanelImage {
    std::uint64_t id=0,record=0,checkpoint_image=0;
    std::string name,error,state="preview",checkpoint,input_sha256,result_sha256;
    std::array<std::vector<std::uint64_t>,4> ids;
    std::optional<double> x,y;
    std::size_t rows=0;
};
struct DensityPanelReport {
    std::string source,source_sha256,project_id,state="idle",error,output,csv,report;
    DensityPanelRecipe recipe;
    std::vector<DensityPanelImage> images;
    std::size_t completed=0,failed=0,rows=0;
    bool valid() const;
};
struct DensityPanelProgress {
    std::atomic<std::size_t> completed{0},total{0};
    std::atomic<std::uint64_t> image{0};
    std::atomic<int> phase{0}; // opening,preview,compute,save,publish
};
DensityPanelReport preview_project_density(const std::string&,const DensityPanelRecipe&,
    const std::atomic<bool>* cancelled=nullptr,DensityPanelProgress* progress=nullptr);
DensityPanelReport run_project_density(const DensityPanelReport&,const std::string&,
    const std::atomic<bool>* cancelled=nullptr,DensityPanelProgress* progress=nullptr,
    std::size_t csv_limit=64u*1024*1024);
class ProjectDensityPanelJob {
    std::future<DensityPanelReport> future_;
    std::atomic<bool> cancelled_{false};
    DensityPanelProgress progress_;
    std::shared_ptr<const DensityPanelReport> result_;
    std::string error_;
public:
    ~ProjectDensityPanelJob();
    bool busy() const {return future_.valid();}
    bool preview(const std::string&,const DensityPanelRecipe&);
    bool run(const DensityPanelReport&,const std::string&);
    void cancel() {cancelled_.store(true);}
    bool cancelling() const {return busy() && cancelled_.load();}
    void poll();void finish();
    const DensityPanelProgress& progress() const {return progress_;}
    const std::string& error() const {return error_;}
    std::shared_ptr<const DensityPanelReport> result() const {return result_;}
private:
    void reset();
};
}
