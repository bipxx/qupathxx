#pragma once
#include "qupathxx/core.hpp"
#include "qupathxx/stains.hpp"

namespace qpx {
enum class AnalysisKind { OpticalDensityMeans, StainMeans };

// Capture and matches are owner-thread operations. Workers only read the
// immutable snapshot. The source stays owned by the caller/controller thread.
class AnalysisRequest {
public:
    AnalysisRequest(const Document&,std::uint64_t object_id,const StainTransform&,
        AnalysisKind kind=AnalysisKind::StainMeans,bool nucleus=false,std::size_t tile_size=256);
    bool matches(const Document&,std::uint64_t selected_id) const;
    std::uint64_t object_id() const {return object_id_;}
    std::uint64_t object_revision() const {return object_revision_;}
    std::uint64_t document_revision() const {return document_revision_;}
    AnalysisKind kind() const {return kind_;}
    bool nucleus() const {return nucleus_;}
    std::size_t tile_size() const {return tile_size_;}
    std::uint64_t total_tiles() const {return total_tiles_;}
    const Roi& roi() const {return roi_;}
    const StainTransform& transform() const {return transform_;}
private:
    friend struct AnalysisKernel;
    std::shared_ptr<const char> identity_;
    std::shared_ptr<ImageServer> source_;
    std::uint64_t object_id_,object_revision_,document_revision_,total_tiles_;
    AnalysisKind kind_;
    bool nucleus_;
    std::size_t tile_size_;
    Roi roi_;
    StainTransform transform_;
    std::int64_t x0_,y0_,x1_,y1_;
};
struct AnalysisResult {
    std::string algorithm="qpx.roi-channel-means/1";
    std::uint64_t object_id=0,object_revision=0,document_revision=0,samples=0,tiles=0;
    AnalysisKind kind=AnalysisKind::OpticalDensityMeans;
    bool nucleus=false;
    ColorVector means{},white{};
    std::array<ColorVector,3> stains{};
    double elapsed_ms=0;
};
AnalysisResult analyze(const AnalysisRequest&,const std::atomic<bool>* cancelled=nullptr);

struct AnalysisStats {
    std::string state="idle",error;
    std::uint64_t generation=0,completed_tiles=0,total_tiles=0;
    double elapsed_ms=0;
};
// Owner-thread controller: one job, no backlog, bounded tiles and numeric-only
// results. Poll/wait reconcile against the live document before publication.
class AnalysisJob {
public:
    AnalysisJob();
    ~AnalysisJob();
    AnalysisJob(const AnalysisJob&)=delete;
    bool request(const AnalysisRequest&);
    AnalysisStats poll(const Document&,std::uint64_t selected_id);
    AnalysisStats wait(const Document&,std::uint64_t selected_id);
    void cancel();
    bool busy() const;
    std::optional<AnalysisResult> result() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
