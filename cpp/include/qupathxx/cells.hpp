#pragma once
#include "qupathxx/tissue.hpp"
namespace qpx {
struct CellParameters {
    TissueFeature feature=TissueFeature::OpticalDensitySum;
    double threshold=1,sigma=1,min_peak_radius=1,min_seed_distance=4,cell_expansion=5;
    std::uint64_t min_nucleus_pixels=5,max_nucleus_pixels=0;
    int connectivity=8;
    std::int32_t level=-1;
    std::size_t tile_size=256,max_cells=10000,work_bytes=256*1024*1024;
    TissueContourOptions contours;
    CellParameters() {contours.work_bytes=32*1024*1024;}
    void validate() const;
};
class CellRequest {
public:
    CellRequest(const Document&,std::uint64_t,const StainTransform&,CellParameters={});
    bool matches(const Document& d,std::uint64_t id) const {return source_.matches(d,id);}
    const CellParameters& parameters() const {return parameters_;}
    std::uint64_t object_id() const {return source_.object_id();}
    std::int32_t level() const {return source_.level();}
    double downsample() const {return source_.downsample();}
    Rect bounds() const {return source_.bounds();}
    Rect level_bounds() const {return source_.level_bounds();}
    const Roi& roi() const {return source_.roi();}
    std::uint64_t total_tiles() const {return source_.total_tiles();}
    std::size_t estimated_work_bytes() const {return work_;}
    static CellRequest replay(const Document&,std::uint64_t record_id);
private:
    friend struct CellKernel;
    friend struct InputDigestKernel;
    friend class Document;
    std::shared_ptr<const char> ticket_=std::make_shared<const char>();
    TissueRequest source_;
    CellParameters parameters_;
    std::shared_ptr<ImageServer> reader_;
    StainTransform transform_;
    std::uint64_t object_revision_,document_revision_;
    std::size_t work_=0;
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
    const CellParameters& parameters() const {return parameters_;}
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
    friend struct CellArchiveCodec;
    friend class Document;
    std::shared_ptr<const char> ticket_;
    std::string algorithm_="qpx.distance-watershed/1";
    CellResult()=default;
    CellParameters parameters_;
    std::shared_ptr<const TissueResult> nuclei_,cells_;
    std::shared_ptr<const TissueContours> nucleus_contours_,cell_contours_;
    std::vector<CellFeature> features_;
    std::vector<CellMarker> markers_;
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
