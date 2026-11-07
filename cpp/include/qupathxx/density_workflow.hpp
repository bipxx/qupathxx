#pragma once
// Saved-project density workflow; independent of Python and the desktop UI.
#include "qupathxx/spatial_density.hpp"
#include <array>
#include <atomic>
#include <future>
#include <string>
#include <vector>
namespace qpx {
struct ProjectDensitySelection {
    std::uint64_t image_id=0,record_id=0;
    std::array<std::vector<std::uint64_t>,4> ids; // windows,tissue,excluded,cells
};
struct ProjectDensityRecipe {
    // Explicit per-image cohorts and class selectors are alternative inputs.
    std::vector<ProjectDensitySelection> selections;
    std::vector<std::uint64_t> images;
    std::array<std::vector<std::string>,4> classes; // windows,tissue,excluded,cells
    bool all_images=false; // Resolve the saved image catalogue; mutually exclusive with explicit images.
    bool all_cells=true;
    SpatialDensityOptions options;
    void validate() const;
};
class ProjectDensityManifest {
public:
    const std::string& path() const;
    const std::string& sha256() const;
    const std::string& project() const;
    const ProjectDensityRecipe& recipe() const;
    void verify(bool rehash=false,const std::atomic<bool>* cancelled=nullptr) const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
    explicit ProjectDensityManifest(std::shared_ptr<const Impl>);
    friend ProjectDensityManifest load_project_density_manifest(const std::string&,const std::atomic<bool>*);
};
ProjectDensityManifest load_project_density_manifest(const std::string&,const std::atomic<bool>* cancelled=nullptr);
struct ProjectDensityImage {
    std::uint64_t id=0,record=0,checkpoint_image=0,hint_record=0;
    std::string name,error,state="preview",checkpoint,input_sha256,result_sha256;
    std::vector<std::string> issues;
    bool image_present=false;
    std::string reuse="computed",reuse_reason,checkpoint_sha256,result_input_sha256;
    std::array<std::vector<std::uint64_t>,4> ids;
    std::optional<double> x,y;
    std::size_t rows=0;
};
struct ProjectDensityReport {
    std::string source,source_sha256,project_id,state="idle",error,output,csv,report;
    ProjectDensityRecipe recipe;
    std::vector<ProjectDensityImage> images;
    std::size_t completed=0,computed=0,reused=0,failed=0,rows=0;
    bool valid() const;
};
struct ProjectDensityManifestReport {
    ProjectDensityReport preview;
    std::string manifest_json,json,output;
    bool written=false;
};
// Duplicate class names normalize to one exact selector; empty images require all_images.
// Optional publication is save-new and occurs only after every native capture succeeds.
ProjectDensityManifestReport generate_project_density_manifest(const std::string& project,
    const ProjectDensityRecipe&,const std::string& output="",const std::atomic<bool>* cancelled=nullptr);
struct ProjectDensityProgress {
    std::atomic<std::size_t> completed{0},total{0};
    std::atomic<std::size_t> kernel_calls{0},reused{0};
    std::atomic<std::uint64_t> image{0};
    std::atomic<int> phase{0}; // opening,preview,compute,save,publish
};
ProjectDensityReport preview_project_density(const std::string&,const ProjectDensityRecipe&,
    const std::atomic<bool>* cancelled=nullptr,ProjectDensityProgress* progress=nullptr);
ProjectDensityReport run_project_density(const ProjectDensityReport&,const std::string&,
    const std::atomic<bool>* cancelled=nullptr,ProjectDensityProgress* progress=nullptr,
    std::size_t csv_limit=64u*1024*1024,const std::string& resume_from="");
class ProjectDensityJob {
    std::future<ProjectDensityReport> future_;
    std::atomic<bool> cancelled_{false};
    ProjectDensityProgress progress_;
    std::shared_ptr<const ProjectDensityReport> result_;
    std::string error_;
public:
    ~ProjectDensityJob();
    bool busy() const {return future_.valid();}
    bool preview(const std::string&,const ProjectDensityRecipe&);
    bool run(const ProjectDensityReport&,const std::string&,const std::string& resume_from="");
    void cancel() {cancelled_.store(true);}
    bool cancelling() const {return busy() && cancelled_.load();}
    void poll();void finish();
    const ProjectDensityProgress& progress() const {return progress_;}
    const std::string& error() const {return error_;}
    std::shared_ptr<const ProjectDensityReport> result() const {return result_;}
private:
    void reset();
};
}
