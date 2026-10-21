#pragma once
#include "qupathxx/analysis.hpp"
namespace qpx {
enum class TissueFeature { OpticalDensitySum, Stain1, Stain2, Stain3 };
struct TissueParameters {
    TissueFeature feature=TissueFeature::OpticalDensitySum;
    double threshold=.3;
    int connectivity=8;
    std::int32_t level=-1; // -1 selects the finest available level fitting work_bytes.
    int open_radius=0,close_radius=0; // Square radii in sampled pixels, 0 disables.
    bool fill_holes=false;
    std::uint64_t max_hole_pixels=0; // 0 fills every enclosed eligible background component.
    std::uint64_t min_pixels=1,max_pixels=0;
    std::size_t tile_size=256,max_components=100000,work_bytes=64*1024*1024;
    void validate() const;
};
class TissueRequest {
public:
    TissueRequest(const Document&,std::uint64_t object_id,const StainTransform&,TissueParameters={},bool nucleus=false);
    bool matches(const Document& d,std::uint64_t id) const {return source_.matches(d,id);}
    std::uint64_t object_id() const {return source_.object_id();}
    const TissueParameters& parameters() const {return parameters_;}
    const Roi& roi() const {return source_.roi();}
    std::uint64_t total_tiles() const {return total_tiles_;}
    std::int32_t level() const {return level_;}
    double downsample() const {return downsample_;}
    Rect level_bounds() const {return {double(x0_),double(y0_),double(x1_-x0_),double(y1_-y0_)};}
    Rect bounds() const {const auto b=level_bounds();return {b.x*downsample_,b.y*downsample_,b.width*downsample_,b.height*downsample_};}
    std::size_t estimated_work_bytes() const {return estimated_work_bytes_;}
private:
    friend struct TissueKernel;
    AnalysisRequest source_;
    TissueParameters parameters_;
    std::size_t estimated_work_bytes_=0,component_capacity_=0;
    std::int32_t level_=0;
    double downsample_=1;
    std::int64_t x0_=0,y0_=0,x1_=0,y1_=0;
    std::uint64_t total_tiles_=0;
};
struct TissueComponent {
    std::uint32_t label=0;
    std::uint64_t pixels=0;
    Rect bounds;
    Point centroid;
};
class TissueResult {
public:
    TissueResult(const TissueResult&)=delete;
    const std::string& algorithm() const {return algorithm_;}
    const std::string& input_sha256() const {return input_sha256_;}
    Rect bounds() const {return bounds_;}
    Rect level_bounds() const {return level_bounds_;}
    std::int64_t width() const {return static_cast<std::int64_t>(level_bounds_.width);}
    std::int64_t height() const {return static_cast<std::int64_t>(level_bounds_.height);}
    std::int32_t level() const {return level_;}
    double downsample() const {return downsample_;}
    const char* input_format() const {return level_==0?"qpx.rgba-tiles/1":"qpx.rgba-level-tiles/1";}
    const Roi& roi() const {return roi_;}
    Rect image_bounds() const {return image_bounds_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<TissueComponent>& components() const {return components_;}
    const TissueParameters& parameters() const {return parameters_;}
    const ColorVector& white() const {return white_;}
    const std::array<ColorVector,3>& stains() const {return stains_;}
    std::uint64_t object_id() const {return object_id_;}
    std::uint64_t object_revision() const {return object_revision_;}
    std::uint64_t document_revision() const {return document_revision_;}
    bool nucleus() const {return nucleus_;}
    std::uint64_t samples() const {return samples_;}
    std::uint64_t threshold_pixels() const {return threshold_pixels_;}
    std::uint64_t cleaned_pixels() const {return cleaned_pixels_;}
    std::uint64_t filled_pixels() const {return filled_pixels_;}
    double cleanup_ms() const {return cleanup_ms_;}
    std::uint64_t foreground_pixels() const {return foreground_pixels_;}
    std::uint64_t raw_components() const {return raw_components_;}
    double elapsed_ms() const {return elapsed_ms_;}
    std::size_t estimated_work_bytes() const {return estimated_work_bytes_;}
    std::size_t retained_bytes() const {return sizeof(TissueResult)+labels_.capacity()*sizeof(std::uint32_t)+components_.capacity()*sizeof(TissueComponent);}
private:
    friend struct TissueKernel;
    TissueResult()=default;
    std::string algorithm_="qpx.threshold-components/1",input_sha256_;
    Rect bounds_,level_bounds_;
    Roi roi_;
    Rect image_bounds_;
    std::int32_t level_=0;
    double downsample_=1;
    std::vector<std::uint32_t> labels_;
    std::vector<TissueComponent> components_;
    TissueParameters parameters_;
    ColorVector white_{};
    std::array<ColorVector,3> stains_{};
    std::uint64_t object_id_=0,object_revision_=0,document_revision_=0,samples_=0,threshold_pixels_=0,foreground_pixels_=0,raw_components_=0;
    bool nucleus_=false;
    std::uint64_t cleaned_pixels_=0,filled_pixels_=0;
    double cleanup_ms_=0;
    double elapsed_ms_=0;
    std::size_t estimated_work_bytes_=0;
};
std::shared_ptr<const TissueResult> detect_tissue(const TissueRequest&,const std::atomic<bool>* cancelled=nullptr);
struct TissueContourOptions {
    bool clip_to_roi=true;
    std::size_t max_components=10000,max_edges=250000,max_vertices=250000,work_bytes=64*1024*1024;
    void validate() const;
};
struct TissueContour {
    std::uint32_t label=0;
    std::uint64_t pixels=0;
    Roi geometry;
};
class TissueContours {
public:
    TissueContours(const TissueContours&)=delete;
    const char* algorithm() const {return "qpx.pixel-contours/1";}
    const std::string& input_sha256() const {return input_sha256_;}
    const TissueContourOptions& options() const {return options_;}
    const std::vector<TissueContour>& components() const {return components_;}
    std::size_t edge_count() const {return edges_;}
    std::size_t vertex_count() const {return vertices_;}
    std::size_t estimated_work_bytes() const {return work_;}
    std::size_t retained_bytes() const {return retained_;}
    double elapsed_ms() const {return elapsed_ms_;}
    std::uint64_t object_id() const {return object_id_;}
private:
    friend struct TissueContourKernel;
    TissueContours()=default;
    std::string input_sha256_;
    TissueContourOptions options_;
    std::vector<TissueContour> components_;
    std::size_t edges_=0,vertices_=0,work_=0,retained_=0;
    double elapsed_ms_=0;
    std::uint64_t object_id_=0;
};
std::shared_ptr<const TissueContours> trace_tissue(const TissueResult&,TissueContourOptions={},const std::atomic<bool>* cancelled=nullptr);
struct TissueStats:AnalysisStats {std::string phase="idle";};
// Same owner-thread publication contract as AnalysisJob. Result buffers own only
// native labels/statistics; captured source ownership stays on the owner thread.
class TissueJob {
public:
    TissueJob();
    ~TissueJob();
    TissueJob(const TissueJob&)=delete;
    bool request(const TissueRequest&);
    TissueStats poll(const Document&,std::uint64_t selected_id);
    TissueStats wait(const Document&,std::uint64_t selected_id);
    void cancel();
    bool busy() const;
    std::shared_ptr<const TissueResult> result() const;
    bool request_contours(TissueContourOptions={});
    std::shared_ptr<const TissueContours> contours() const;
    bool contours_published() const;
    std::vector<std::uint64_t> keep_contours(Document&,std::uint64_t selected_id,std::string name_prefix="Tissue",std::string class_name="");
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
