#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/raw_image.hpp"

namespace qpx {
// Captured on the document owner; constructing a request never opens a decoder.
// Workers own native-only reader/ROI snapshots. Display changes are independent
// of raw source identity, axes and calibrated measurement provenance.
struct RawTmaContext {
    std::uint64_t core_id=0,core_revision=0;
    std::uint32_t row=0,column=0;
    bool missing=false;
    std::string grid_name,row_label,column_label,core_name,case_id;
};
struct RawObjectContext {
    std::string name,class_name;
    std::uint64_t parent_id=0;
    ObjectKind kind=ObjectKind::Annotation;
    std::optional<RawTmaContext> tma;
    std::size_t payload_bytes() const;
    bool same_metadata(const RawObjectContext&) const;
};
class RawAnalysisRequest {
public:
    RawAnalysisRequest(const Document&,RawRegionRequest,std::uint64_t object_id=0,
        bool nucleus=false,std::size_t tile_size=256,std::uint64_t max_pixels=1000000000);
    bool matches(const Document&) const;
    const RawRegionRequest& region() const {return region_;}
    std::uint64_t object_id() const {return object_id_;}
    std::uint64_t object_revision() const {return object_revision_;}
    std::uint64_t document_revision() const {return document_revision_;}
    bool nucleus() const {return nucleus_;}
    std::size_t tile_size() const {return tile_size_;}
    std::uint64_t max_pixels() const {return max_pixels_;}
    const std::optional<Roi>& mask() const {return mask_;}
    const RawObjectContext& object_context() const {return context_;}
    static RawAnalysisRequest replay(const Document&,std::uint64_t record_id);
private:
    friend struct RawAnalysisKernel;
    friend class RawAnalysisJob;
    friend class Document;
    static bool context_matches(const Document&,std::uint64_t,const RawObjectContext&);
    std::shared_ptr<const char> identity_,capture_=std::make_shared<const char>();
    std::shared_ptr<ImageServer> source_;
    std::shared_ptr<const ReaderSource> observed_;
    RawRegionRequest region_;
    std::optional<Roi> mask_;
    RawObjectContext context_;
    std::uint64_t object_id_=0,object_revision_=0,document_revision_=0,max_pixels_=0;
    bool nucleus_=false;
    std::size_t tile_size_=256;
    std::uint32_t series_=0;
    std::optional<double> pixel_width_um_,pixel_height_um_;
};

struct RawAnalysisMetadata {
    std::int64_t width=0,height=0,level_width=0,level_height=0;
    std::uint32_t channels=0,z_planes=0,timepoints=0,series=0;
    SampleType sample_type=SampleType::UInt8;
    double downsample=1;
    // Names of the selected channels only, in output order.
    std::vector<std::string> channel_names;
    std::optional<double> z_spacing_um,time_interval_seconds;
    std::size_t payload_bytes() const;
};
class RawAnalysisResult : public std::enable_shared_from_this<RawAnalysisResult> {
public:
    const RawRegionStatistics& statistics() const {return statistics_;}
    const RawAnalysisMetadata& metadata() const {return metadata_;}
    const RawRegionRequest& region() const {return region_;}
    std::uint64_t object_id() const {return object_id_;}
    std::uint64_t object_revision() const {return object_revision_;}
    std::uint64_t document_revision() const {return document_revision_;}
    bool nucleus() const {return nucleus_;}
    std::size_t tile_size() const {return tile_size_;}
    std::uint64_t tiles() const {return tiles_;}
    double elapsed_ms() const {return elapsed_ms_;}
    const std::string& input_sha256() const {return input_sha256_;}
    const std::string& algorithm() const {return algorithm_;}
    const std::optional<double>& pixel_width_um() const {return pixel_width_um_;}
    const std::optional<double>& pixel_height_um() const {return pixel_height_um_;}
    const std::shared_ptr<const ReaderSource>& source() const {return source_;}
    const RawObjectContext& object_context() const {return context_;}
    std::size_t payload_bytes() const;
private:
    friend struct RawAnalysisKernel;
    friend struct RawAnalysisCodec;
    friend class Document;
    RawAnalysisResult()=default;
    RawRegionStatistics statistics_;
    RawAnalysisMetadata metadata_;
    RawObjectContext context_;
    RawRegionRequest region_;
    std::uint64_t object_id_=0,object_revision_=0,document_revision_=0,tiles_=0;
    bool nucleus_=false;
    std::size_t tile_size_=256;
    double elapsed_ms_=0;
    std::string input_sha256_,algorithm_="qpx.raw-roi-statistics/1";
    std::optional<double> pixel_width_um_,pixel_height_um_;
    std::shared_ptr<const ReaderSource> source_;
    std::shared_ptr<const char> capture_;
};
std::shared_ptr<const RawAnalysisResult> analyze_raw(const RawAnalysisRequest&,const std::atomic<bool>* cancelled=nullptr);
struct RawInputDigest {
    std::string input_sha256;
    std::uint64_t tiles=0,raw_bytes=0;
};
// Identical logical input scan, without statistics or ROI containment arithmetic.
RawInputDigest raw_analysis_input_digest(const RawAnalysisRequest&,const std::atomic<bool>* cancelled=nullptr);


struct RawAnalysisRecord {
    std::uint64_t id=0;
    std::shared_ptr<const RawAnalysisResult> result;
    std::optional<Roi> mask;
    std::uint64_t max_pixels=1000000000;
    std::size_t requested_tile_size=256;
    std::size_t payload_bytes() const;
    void validate(const ImageMetadata&) const;
};
struct RawAnalysisStats {
    std::string state="idle",error;
    std::uint64_t generation=0,completed_tiles=0,total_tiles=0;
    double elapsed_ms=0;
};
class RawAnalysisJob {
public:
    RawAnalysisJob();~RawAnalysisJob();RawAnalysisJob(const RawAnalysisJob&)=delete;
    bool request(const RawAnalysisRequest&);
    RawAnalysisStats poll(const Document&);
    RawAnalysisStats wait(const Document&);
    void cancel();bool busy() const;
    void await_completion();
    std::shared_ptr<const RawAnalysisResult> result() const;
    std::uint64_t keep_result(Document&);
    bool verify_result(Document&,std::uint64_t record_id);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
