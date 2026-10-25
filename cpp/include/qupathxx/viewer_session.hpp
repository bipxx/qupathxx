#pragma once
#include "qupathxx/project.hpp"
#include "qupathxx/cells.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/tma_dearray.hpp"
namespace qpx {
namespace detail {class ViewerOwner;}
struct ViewerLimits {
    std::size_t tickets=128,queued_bytes=16*1024*1024,result_bytes=64*1024*1024;
    std::size_t command_bytes=4*1024*1024,max_edits=1024,region_bytes=16*1024*1024,snapshot_bytes=64*1024*1024;
    void validate() const;
};
struct ViewerEvent {
    std::uint64_t ticket=0,epoch=0,image_id=0,object_id=0,record_id=0,document_revision=0;
    std::string action,state="queued",error,path;
    bool committed=false,verified=false;
    double elapsed_ms=0;
    std::vector<std::uint64_t> object_ids;
    std::vector<ProjectImage> images;
    std::optional<Annotation> object;
    std::optional<ImageMetadata> metadata;
    std::optional<AnalysisResult> analysis;
    std::shared_ptr<const TissueResult> tissue;
    std::shared_ptr<const CellResult> cells;
    std::shared_ptr<const TmaDearrayResult> tma_dearray;
    std::shared_ptr<const PixelBuffer> pixels;
    std::shared_ptr<const RawAnalysisResult> raw_analysis;
    std::shared_ptr<const RawPixelBuffer> raw_pixels;
    std::shared_ptr<const RawImageMetadata> raw_metadata;
    std::optional<TiffView> tiff_view;
    std::uint64_t samples=0,components=0,rows=0;
    std::size_t bytes=0;
};
struct ViewerStats {
    std::string state="created",error,project_identity;
    std::uint64_t epoch=1,frame=0,active_image=0,document_revision=0,active_ticket=0,completed=0;
    std::size_t objects=0,queued=0,queued_bytes=0,retained_bytes=0,tickets=0;
    double max_command_ms=0;
};
// Thread-safe data-only endpoint. It never exposes the live Project/Document.
// GUI execution stays on the caller/main thread; producers can use Python threads.
class ViewerSession {
public:
    explicit ViewerSession(std::string project_path,ViewerLimits={});
    ~ViewerSession();ViewerSession(const ViewerSession&)=delete;
    std::string project_path() const;ViewerLimits limits() const;ViewerStats stats() const;
    ViewerEvent poll(std::uint64_t ticket) const;
    ViewerEvent wait(std::uint64_t ticket,double timeout_seconds=30) const;
    ViewerEvent take(std::uint64_t ticket);
    bool cancel(std::uint64_t ticket);
    std::uint64_t catalog(std::uint64_t after_id=0,std::size_t limit=64);
    std::uint64_t inspect(std::uint64_t image_id,std::uint64_t object_id=0);
    std::uint64_t execute(std::uint64_t image_id,const std::vector<ObjectEdit>&,std::string label="Python edits",std::optional<std::uint64_t> expected_revision={});
    std::uint64_t insert_tma_row(std::uint64_t image_id,std::size_t index,std::string label="",std::optional<std::vector<Rect>> bounds={},bool missing=false,std::optional<std::uint64_t> expected_revision={});
    std::uint64_t insert_tma_column(std::uint64_t image_id,std::size_t index,std::string label="",std::optional<std::vector<Rect>> bounds={},bool missing=false,std::optional<std::uint64_t> expected_revision={});
    std::uint64_t remove_tma_row(std::uint64_t image_id,std::size_t index,std::optional<std::uint64_t> expected_revision={});
    std::uint64_t remove_tma_column(std::uint64_t image_id,std::size_t index,std::optional<std::uint64_t> expected_revision={});
    // Preview only; use execute(qptma_grid_edits(result.grid), expected_revision)
    // for a separately authorized, bounded and atomic owner-side apply.
    std::uint64_t detect_tma_grid(std::uint64_t image_id,TmaDearrayOptions={},std::optional<std::uint64_t> expected_revision={});
    std::uint64_t select(std::uint64_t image_id,const std::vector<std::uint64_t>&);
    std::uint64_t activate(std::uint64_t image_id);
    std::uint64_t read_region(std::uint64_t image_id,RegionRequest);
    std::uint64_t inspect_raw(std::uint64_t image_id);
    std::uint64_t read_raw(std::uint64_t image_id,RawRegionRequest);
    std::uint64_t set_tiff_view(std::uint64_t image_id,TiffView);
    std::uint64_t analyze_with_sampling_policy(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,RoiSamplingPolicy,AnalysisKind=AnalysisKind::OpticalDensityMeans,bool nucleus=false);
    std::uint64_t analyze(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,AnalysisKind=AnalysisKind::OpticalDensityMeans,bool nucleus=false);
    std::uint64_t tissue(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,TissueParameters={},bool retain_result=false);
    std::uint64_t cells(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,CellParameters={},bool retain_result=false);
    std::uint64_t grid_cells(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,GridCellSampling={},GridCellParameters={},bool retain_result=false);
    // Bounds and parameters are copied at submission. The owner resolves units
    // against the live image/ROI when this queued command begins execution.
    std::uint64_t cells_with_nucleus_area_limits(std::uint64_t image_id,std::uint64_t object_id,const StainTransform&,
        std::optional<double> minimum_um2={},std::optional<double> maximum_um2={},CellParameters={},bool retain_result=false);
    std::uint64_t export_measurements(std::uint64_t image_id,MeasurementTableKind,const std::string&,bool require_current=false,const std::vector<std::uint64_t>& record_ids={});
    std::uint64_t export_cells(std::uint64_t image_id,const std::string&,bool require_current=false,const std::vector<std::uint64_t>& record_ids={});
    std::uint64_t analyze_raw(std::uint64_t image_id,RawRegionRequest,std::uint64_t object_id=0,bool nucleus=false,bool retain_result=false);
    std::uint64_t analyze_raw_grid(std::uint64_t image_id,std::uint64_t object_id,RawGridSampling={},ScientificRawOptions={},bool nucleus=false,bool retain_result=false);
    std::uint64_t verify_raw_analysis(std::uint64_t image_id,std::uint64_t record_id);
    std::uint64_t export_raw_measurements(std::uint64_t image_id,const std::string&,bool require_current=false,const std::vector<std::uint64_t>& record_ids={});
    std::uint64_t save(const std::string&);
    std::uint64_t close(bool discard_changes=false);
private:
    friend class detail::ViewerOwner;struct Impl;std::shared_ptr<Impl> impl_;
};
GuiReport run_viewer_session(std::shared_ptr<ViewerSession>,int frame_limit=0,const std::string& capture_path="",const GuiOptions& options={});
}
