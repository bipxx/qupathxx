#pragma once
#include "qupathxx/project.hpp"
#include "qupathxx/cells.hpp"
#include "qupathxx/pixel_stream.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/raw_pixel_stream_file.hpp"
#include <functional>
namespace qpx {
enum class ProjectBatchKind { Roi, Tissue, Cells, Pixels, Raw, RawPixels };
enum class ProjectBatchFailure { Stop, Continue };
struct ProjectBatchOptions {
    ProjectBatchKind kind=ProjectBatchKind::Roi;
    ProjectBatchFailure failure=ProjectBatchFailure::Stop;
    AnalysisKind roi_kind=AnalysisKind::OpticalDensityMeans;
    ObjectKind object_kind=ObjectKind::Annotation;
    bool nucleus=false,resume=true,publish_cells=false,tma_only=false,include_missing_tma=false;
    std::size_t roi_tile_size=256,snapshot_budget=256*1024*1024,materialize_budget=512*1024*1024,result_budget=256*1024*1024,csv_limit=64*1024*1024;
    std::uint64_t checkpoint_limit=1024ULL*1024*1024;
    TissueParameters tissue;CellParameters cells;
    std::optional<GridCellSampling> cell_grid;
    GridCellParameters grid_cells;
    std::optional<double> cell_min_nucleus_area_um2,cell_max_nucleus_area_um2;
    std::optional<PixelClassifier> pixel_model;
    PixelStreamOptions pixels;
    std::uint64_t pixel_file_limit=1ULL<<40,pixel_total_limit=1ULL<<40;
    std::optional<RawPixelClassifier> raw_pixel_model;
    // Omitted axes use a unit level-zero lattice per image. Explicit axes keep
    // their exact origins/extents/sample counts; never silently rebase a model.
    std::optional<ScientificGridAxis> raw_pixel_x,raw_pixel_y;
    RawPixelStreamOptions raw_pixels;
    std::size_t raw_pixel_file_work_bytes=256u*1024*1024;
    std::optional<RawGridSampling> raw_grid;std::size_t raw_grid_work_bytes=256u*1024*1024;
    RawRegionRequest raw;bool raw_follow_display=true;std::uint64_t raw_max_pixels=1000000000;
    RoiSamplingPolicy roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
    void validate() const;
};
struct ProjectBatchSelection {std::uint64_t image_id=0;std::vector<std::uint64_t> object_ids;};
class ProjectBatchRequest {
public:
    ProjectBatchRequest(const Project&,const StainTransform&,const std::vector<ProjectBatchSelection>& selection={},ProjectBatchOptions={});
    std::size_t image_count() const;std::size_t object_count() const;std::size_t snapshot_bytes() const;double capture_ms() const;
    std::string parameters_json() const;std::string parameters_sha256() const;
private:
    friend struct ProjectBatchKernel;struct Data;std::shared_ptr<const Data> data_;
};
struct ProjectBatchImageResult {
    std::uint64_t image_id=0;std::string image_name,state,path,error,input_sha256,pixel_sha256;
    std::size_t objects=0,records=0;double elapsed_ms=0;
    std::uint64_t inferred_blocks=0,hashed_blocks=0,verified_blocks=0,stream_bytes=0;
    std::uint64_t raw_tiles=0,raw_hashed_tiles=0;
};
struct ProjectBatchReport {
    std::string state="idle",error,report_path,parameters_sha256,source_identity;
    std::uint64_t source_generation=0,source_revision=0;
    std::size_t saved=0,reused=0,failed=0,snapshot_bytes=0;double elapsed_ms=0,capture_ms=0;
    std::vector<ProjectBatchImageResult> images;
};
struct ProjectBatchStats {
    std::string state="idle",phase="idle",error;
    std::uint64_t image_id=0;std::size_t completed_images=0,total_images=0,completed_objects=0,total_objects=0;
    double capture_ms=0,elapsed_ms=0;
};
ProjectBatchReport run_project_batch(const ProjectBatchRequest&,const std::string& output_directory,const std::atomic<bool>* cancelled=nullptr,const std::function<void(const ProjectBatchImageResult&)>& on_image={});
std::string project_batch_image_json(const ProjectBatchImageResult&);
std::string project_batch_report_json(const ProjectBatchReport&);
std::string batch_checkpoint_csv(const std::string& path);
void save_batch_checkpoint_csv(const std::string& checkpoint,const std::string& path);
class ProjectBatchJob {
public:
    ProjectBatchJob();~ProjectBatchJob();ProjectBatchJob(const ProjectBatchJob&)=delete;
    bool request(const ProjectBatchRequest&,const std::string& output_directory);
    ProjectBatchStats poll();ProjectBatchStats wait();void cancel();bool busy() const;
    std::shared_ptr<const ProjectBatchReport> result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
