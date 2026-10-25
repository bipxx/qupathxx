#pragma once
#include "qupathxx/tissue.hpp"
namespace qpx {
enum class CellAlgorithm { DistanceWatershed=0, LogWatershed=1, LogWatershedBodies=2, SampledLogWatershedBodies=3, GridLogWatershedBodies=4 };
enum class GridCellUnits { LevelZeroPixels, Micrometres };
// Count-based frontend planning: enclose the original ROI bounds in an integer
// level-zero extent, retain the ROI itself, and resolve the actual axis steps.
struct GridCellSampling {
    std::uint32_t samples_x=512,samples_y=512,context_x=32,context_y=32;
    void validate() const;
};
// Distances and areas are in the selected units and their square, respectively;
// never in sampled-array pixels. Context belongs to the explicit grid.
struct GridCellParameters {
    GridCellUnits units=GridCellUnits::LevelZeroPixels;
    TissueFeature feature=TissueFeature::OpticalDensitySum;
    double threshold=1,sigma=1,log_prominence=.5,min_peak_radius=0,min_seed_distance=0,cell_expansion=5;
    double min_nucleus_area=5;
    std::optional<double> max_nucleus_area;
    RoiSamplingPolicy sampling_policy=RoiSamplingPolicy::GeometryCoverage;
    std::size_t max_cells=10000,work_bytes=256*1024*1024;
    TissueContourOptions contours;
    GridCellParameters() {contours.work_bytes=32*1024*1024;}
    void validate() const;
};
struct ResolvedGridCellParameters {
    double step_x=0,step_y=0,sigma_x=0,sigma_y=0;
    std::uint64_t min_nucleus_samples=0,max_nucleus_samples=0;
};
struct ScientificCellContext;
struct CellParameters {
    CellAlgorithm algorithm=CellAlgorithm::DistanceWatershed;
    double log_prominence=.5;
    std::uint32_t context_padding=32;
    TissueFeature feature=TissueFeature::OpticalDensitySum;
    double threshold=1,sigma=1,min_peak_radius=1,min_seed_distance=4,cell_expansion=5;
    std::uint64_t min_nucleus_pixels=5,max_nucleus_pixels=0;
    int connectivity=8;
    std::int32_t level=-1;
    std::size_t tile_size=256,max_cells=10000,work_bytes=256*1024*1024;
    TissueContourOptions contours;
    CellParameters() {contours.work_bytes=32*1024*1024;}
    explicit CellParameters(CellAlgorithm value):CellParameters() {
        algorithm=value;if(value==CellAlgorithm::SampledLogWatershedBodies) {min_peak_radius=0;min_seed_distance=0;}
    }
    void validate() const;
};
class CellRequest {
public:
    CellRequest(const Document&,std::uint64_t,const StainTransform&,CellParameters={});
    CellRequest(const Document&,std::uint64_t,const StainTransform&,ScientificGrid,GridCellParameters={});
    bool matches(const Document&,std::uint64_t) const;
    const CellParameters& parameters() const;
    const ScientificGrid* scientific_grid() const;
    const GridCellParameters* grid_parameters() const;
    const ResolvedGridCellParameters* resolved_grid_parameters() const;
    std::uint64_t object_id() const;
    std::int32_t level() const;
    double downsample() const;
    Rect bounds() const;
    Rect level_bounds() const;
    const Roi& roi() const;
    std::uint64_t total_tiles() const;
    std::size_t estimated_work_bytes() const {return work_;}
    static CellRequest replay(const Document&,std::uint64_t record_id);
    static CellRequest for_grid_roi(const Document&,std::uint64_t,const StainTransform&,GridCellSampling={},GridCellParameters={});
    // Resolve inclusive physical area bounds on the selected natural grid.
    // Returned parameters contain the frozen sampled-pixel cutoffs used by the
    // ordinary detector/archive/replay path; invocation arguments are not kept.
    static CellRequest with_nucleus_area_limits(const Document&,std::uint64_t,const StainTransform&,
        std::optional<double> minimum_um2={},std::optional<double> maximum_um2={},CellParameters={});
private:
    friend struct CellKernel;
    friend struct ScientificCellKernel;
    friend struct InputDigestKernel;
    friend class Document;
    std::shared_ptr<const char> ticket_=std::make_shared<const char>();
    std::shared_ptr<const TissueRequest> source_;
    std::shared_ptr<const ScientificCellContext> scientific_;
    CellParameters parameters_;
    std::shared_ptr<ImageServer> reader_;
    StainTransform transform_;
    std::uint64_t object_revision_,document_revision_;
    std::optional<std::uint64_t> physical_calibration_revision_;
    std::size_t work_=0;
    Rect processing_grid_;
};
struct CellMarker {
    std::uint32_t label=0,result_label=0,pixel_index=0;
    Point center;
    double radius=0;
};
struct CellFeature {
    std::uint32_t label=0;
    std::uint64_t nucleus_pixels=0,cell_pixels=0;
    ColorVector nucleus_rgb{},cell_rgb{},nucleus_channels{},cell_channels{};
    std::optional<ColorVector> cytoplasm_rgb,cytoplasm_channels;
};
struct CalibratedCellFeature {
    std::uint32_t label=0;
    std::optional<double> pixel_width_um,pixel_height_um;
    std::optional<double> nucleus_sample_area_um2,cell_sample_area_um2,cytoplasm_sample_area_um2;
    std::optional<Point> nucleus_centroid_um,cell_centroid_um;
};
class CellResult : public std::enable_shared_from_this<CellResult> {
public:
    CellResult(const CellResult&)=delete;
    const char* algorithm() const {return algorithm_.c_str();}
    const CellParameters& parameters() const {
        if(grid_parameters_) throw std::logic_error("explicit-grid cells require grid_parameters");
        return parameters_;
    }
    const GridCellParameters* grid_parameters() const {return grid_parameters_?&*grid_parameters_:nullptr;}
    const ResolvedGridCellParameters* resolved_grid_parameters() const {return grid_parameters_?&resolved_grid_parameters_:nullptr;}
    const std::shared_ptr<const TissueResult>& nuclei() const {return nuclei_;}
    const std::shared_ptr<const TissueResult>& cells() const {return cells_;}
    const std::shared_ptr<const TissueContours>& nucleus_contours() const {return nucleus_contours_;}
    const std::shared_ptr<const TissueContours>& cell_contours() const {return cell_contours_;}
    const std::vector<CellFeature>& features() const {return features_;}
    // O(1), capture-time sampling geometry; never consults a live Document.
    CalibratedCellFeature calibrated_feature(std::uint32_t label) const;
    const std::vector<CellMarker>& markers() const {return markers_;}
    std::size_t raw_peaks() const {return raw_peaks_;}
    double elapsed_ms() const {return elapsed_ms_;}
    std::size_t retained_bytes() const;
private:
    friend struct CellKernel;
    friend struct ScientificCellKernel;
    friend struct CellArchiveCodec;
    friend class Document;
    std::shared_ptr<const char> ticket_;
    std::string algorithm_="qpx.distance-watershed/1";
    CellResult()=default;
    CellParameters parameters_;
    std::optional<GridCellParameters> grid_parameters_;
    ResolvedGridCellParameters resolved_grid_parameters_;
    std::shared_ptr<const TissueResult> nuclei_,cells_;
    std::shared_ptr<const TissueContours> nucleus_contours_,cell_contours_;
    std::vector<CellFeature> features_;
    std::vector<CellMarker> markers_;
    std::vector<std::uint8_t> domain_bits_; // QCL4 historical opaque ROI samples, packed least-significant bit first.
    std::size_t raw_peaks_=0;
    double elapsed_ms_=0;
};
std::shared_ptr<const CellResult> detect_cells(const CellRequest&,const std::atomic<bool>* cancelled=nullptr);
struct CellRecord {
    std::uint64_t id=0;
    std::string producer="qupathxx/0.1.0";
    std::shared_ptr<const CellResult> result;
    std::size_t payload_bytes() const;
};
struct CellStats : AnalysisStats {std::string phase="idle";};
class CellJob {
public:
    CellJob();~CellJob();CellJob(const CellJob&)=delete;
    bool request(const CellRequest&);
    CellStats poll(const Document&,std::uint64_t selected_id);
    CellStats wait(const Document&,std::uint64_t selected_id);
    bool busy() const;void cancel();
    std::shared_ptr<const CellResult> result() const;
    bool published() const;
    std::vector<std::uint64_t> keep_result(Document&,std::uint64_t selected_id,std::string name_prefix="Cell",std::string class_name="");
    std::uint64_t keep_record(Document&,std::uint64_t selected_id);
    bool verify_record(Document&,std::uint64_t record_id);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
