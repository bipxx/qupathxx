#pragma once
#include "qupathxx/fluorescence_cells.hpp"
#include "qupathxx/geometry.hpp"
#include "qupathxx/provenance.hpp"
namespace qpx {
class Document;
class DarkFlatRequest;
class LinearUnmixRequest;
class FluorescenceCellRun;
class FluorescenceTiledRequest;
struct FluorescenceTiledReport;
struct FluorescenceCheckpointOptions;
class FluorescenceCellRequest {
public:
    // Explicit level-zero channel block/Z/T. An object selects its native area
    // ROI; otherwise the supplied core crop must lie inside the image. Context
    // is clipped to the source and must cover the Gaussian radius on each axis.
    FluorescenceCellRequest(const Document&,RawRegionRequest,std::uint64_t object_id=0,
        FluorescenceCellOptions={},std::array<std::uint32_t,2> context={32,32});
    // Explicit derived analysis. The correction recipe must already be retained
    // in this document; cells/runs reference it once instead of copying fields.
    FluorescenceCellRequest(const Document&,const DarkFlatRequest&,RawRegionRequest,
        std::uint64_t object_id=0,FluorescenceCellOptions={},std::array<std::uint32_t,2> context={32,32});
    // Output selection uses component/residual indices, not measured indices.
    // Requires a retained recipe and a channel-independent ROI plane (c=-1).
    FluorescenceCellRequest(const Document&,const LinearUnmixRequest&,RawRegionRequest,
        std::uint64_t object_id=0,FluorescenceCellOptions={},std::array<std::uint32_t,2> context={32,32});
    bool matches(const Document&) const;
    // Actual file versions and metadata are rechecked at this trust boundary;
    // ordinary matches/polls do not rescan pixel data.
    bool publication_matches(const Document&) const;
    const RawRegionRequest& region() const;
    const RawRegionRequest& core_region() const;
    const std::optional<Roi>& roi() const;
    const FluorescenceCellOptions& options() const;
    const RawImageMetadata& source_metadata() const;
    std::array<std::uint32_t,2> context() const;
    std::uint64_t object_id() const;
    std::size_t estimated_work_bytes() const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    friend struct FluorescenceCellKernel;
    friend class FluorescenceCellRun;
    friend class FluorescenceCellJob;
};
class FluorescenceCellRun {
public:
    const FluorescenceCellResult& raster() const;
    const RawRegionRequest& region() const;
    const RawRegionRequest& core_region() const;
    const RawImageMetadata& source_metadata() const;
    const std::optional<Roi>& roi() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    const std::string& input_sha256() const;
    const std::string& sha256() const;
    const std::string& measurement_domain() const;
    const std::string& correction_record() const;
    const std::string& unmix_record() const;
    double elapsed_ms() const;
    std::array<std::uint32_t,2> context() const;
    std::size_t payload_bytes() const;
    bool matches(const FluorescenceCellRequest&) const;
    Point pixel_center(std::int64_t x,std::int64_t y) const;
private:
    struct Data;std::shared_ptr<const Data> data_;
    explicit FluorescenceCellRun(std::shared_ptr<const Data>);
    friend struct FluorescenceCellKernel;
};
std::shared_ptr<const FluorescenceCellRun> detect_fluorescence_cells(const FluorescenceCellRequest&,
    const std::atomic<bool>* cancelled=nullptr);
void require_current_fluorescence_cells(const Document&,const FluorescenceCellRequest&,const FluorescenceCellRun&,
    const std::atomic<bool>* cancelled=nullptr);
enum class FluorescenceCellPhase { Idle, Reading, Segmenting, Ready };
struct FluorescenceCellStats {
    std::string state="idle",error;
    std::uint64_t generation=0;
    FluorescenceCellPhase phase=FluorescenceCellPhase::Idle;
    double elapsed_ms=0;
};
// Owner-thread delivery; a worker holds only native immutable request snapshots.
// Cancellation is thread-safe. Destruction cancels and joins before owner release.
class FluorescenceCellJob {
public:
    FluorescenceCellJob();~FluorescenceCellJob();
    FluorescenceCellJob(const FluorescenceCellJob&)=delete;
    bool request(const FluorescenceCellRequest&);
    bool request(const FluorescenceTiledRequest&);
    bool request(const FluorescenceTiledRequest&,const std::string& checkpoint_directory,const FluorescenceCheckpointOptions&);
    bool busy() const;
    void cancel();void await_completion();
    FluorescenceCellStats poll(const Document&);
    FluorescenceCellStats wait(const Document&);
    std::shared_ptr<const FluorescenceCellRun> result() const;
    std::shared_ptr<const FluorescenceTiledReport> tiled_result() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
