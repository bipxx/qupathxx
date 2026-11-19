#pragma once
#include "qupathxx/label_geometry.hpp"
#include "qupathxx/label_measurements.hpp"
#include "qupathxx/superpixels.hpp"
#include "qupathxx/subcellular.hpp"
#include "qupathxx/label_mask.hpp"
#include "qupathxx/ngff_label_mask.hpp"
#include "qupathxx/provenance.hpp"
#include <variant>

namespace qpx {
enum class LabelAnalysisMethod { Superpixels, Subcellular, ExternalMask, NgffMask };
using LabelAnalysisOptions =
    std::variant<SuperpixelOptions, SubcellularOptions, ExternalLabelMaskOptions,
                 std::shared_ptr<const NgffLabelMaskOptions>>;
struct LabelAnalysisReport {
    std::uint64_t rejected_small = 0, rejected_large = 0, nonfinite_pixels = 0, invalid_pixels = 0;
};
struct LabelAnalysisLimits {
    std::size_t work_bytes = 256u * 1024 * 1024;
    std::uint64_t max_membership_tests = 100000000;
    TissueContourOptions contours;
    void validate() const;
};
class LabelAnalysisRequest {
  public:
    LabelAnalysisRequest(const Document&, std::uint64_t area, SuperpixelOptions = {},
                         LabelAnalysisLimits = {});
    static LabelAnalysisRequest subcellular(const Document&, std::uint64_t area,
                                            std::vector<std::uint64_t> cells,
                                            RawRegionRequest channel_block, SubcellularOptions = {},
                                            LabelAnalysisLimits = {});
    static LabelAnalysisRequest external_mask(const Document&, std::uint64_t area,
                                              RawRegionRequest channel_block,
                                              ExternalLabelMaskOptions, LabelAnalysisLimits = {});
    static LabelAnalysisRequest ngff_mask(const Document&, std::uint64_t area,
                                          RawRegionRequest channel_block,
                                          NgffLabelMaskOptions, LabelAnalysisLimits = {});
    // Owner-thread metadata check; does not rescan source files or pixels.
    bool matches(const Document&) const;
    LabelAnalysisMethod method() const;
    std::uint64_t object_id() const;
    const ScientificGrid& grid() const;
    const LabelAnalysisLimits& limits() const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit LabelAnalysisRequest(std::shared_ptr<const Data>);
    friend struct LabelAnalysisKernel;
    friend class LabelAnalysisRun;
    friend class LabelAnalysisJob;
    friend std::string keep_label_analysis(Document&, const LabelAnalysisRequest&,
                                           const class LabelAnalysisRun&, const std::atomic<bool>*);
    friend LabelAnalysisRequest replay_label_analysis(const Document&, const std::string&,
                                                      LabelAnalysisLimits);
};
// Exact native identity for the raw crop submitted to an offline producer. The
// channel block/plane, ROI, calibration and decoded raw bytes all participate.
struct LabelAnalysisInputInfo {
    std::string sha256;
    RawRegionRequest raw;
    SampleType sample_type;
};
LabelAnalysisInputInfo label_analysis_input_info(const Document&, std::uint64_t area,
                                                 RawRegionRequest channel_block,
                                                 LabelAnalysisLimits = {},
                                                 const std::atomic<bool>* cancelled = nullptr);
std::string label_analysis_raw_input_sha256(const Document&, std::uint64_t area,
                                            RawRegionRequest channel_block,
                                            LabelAnalysisLimits = {},
                                            const std::atomic<bool>* cancelled = nullptr);
class LabelAnalysisRun {
  public:
    LabelAnalysisMethod method() const;
    const LabelRaster& raster() const;
    const std::shared_ptr<const TissueContours>& contours() const;
    const std::vector<LabelRegionMeasurements>& measurements() const;
    const std::vector<SubcellularCellCount>& cell_counts() const;
    const std::vector<std::uint64_t>& parent_ids() const;
    const std::vector<std::uint32_t>& source_labels() const;
    // Dense index zero maps native label1. Exact signed/UInt64 values never
    // enter floating measurement slots. The legacy UInt32 accessor is retained.
    LabelCode source_code(std::size_t dense_index) const;
    const LabelCodeMap* exact_source_labels() const;
    const LabelMaskSourceDescription* label_source_description() const;
    const LabelAnalysisOptions& options() const;
    std::optional<RawRegionRequest> raw_channels() const;
    const LabelAnalysisReport& report() const;
    const ScientificGrid& grid() const;
    const std::string& input_sha256() const;
    const std::string& sha256() const;
    const char* measurement_domain() const;
    const std::shared_ptr<const AnalysisProvenance>& provenance() const;
    bool matches(const LabelAnalysisRequest&) const;

  private:
    struct Data;
    std::shared_ptr<const Data> data_;
    explicit LabelAnalysisRun(std::shared_ptr<const Data>);
    friend struct LabelAnalysisKernel;
};
std::shared_ptr<const LabelAnalysisRun>
run_label_analysis(const LabelAnalysisRequest&, const std::atomic<bool>* cancelled = nullptr);
void require_current_label_analysis_inputs(const LabelAnalysisRun&,
                                           const std::atomic<bool>* cancelled = nullptr);
// Recipe/result identity is kept through existing native ImageData properties
// (one undo); publish creates all detection objects atomically (one undo).
// Empty runs are retained too. Explicit replay recomputes and checks exact IDs.
std::string keep_label_analysis(Document&, const LabelAnalysisRequest&, const LabelAnalysisRun&,
                                const std::atomic<bool>* cancelled = nullptr);
std::vector<std::string> label_analysis_ids(const Document&);
LabelAnalysisRequest replay_label_analysis(const Document&, const std::string& id,
                                           LabelAnalysisLimits = {});
void remove_label_analysis(Document&, const std::string& id);
std::vector<std::uint64_t> remove_label_publication(Document&, const std::string& id);
std::vector<std::uint64_t> publish_label_analysis(Document&, const LabelAnalysisRequest&,
                                                  const LabelAnalysisRun&,
                                                  std::string name_prefix = "Label",
                                                  const std::atomic<bool>* cancelled = nullptr);
std::size_t save_label_analysis_csv(const LabelAnalysisRun&, const std::string& path,
                                    std::size_t csv_bytes = 256u * 1024 * 1024,
                                    const std::vector<std::string>& protected_paths = {},
                                    const std::atomic<bool>* cancelled = nullptr);
std::size_t save_subcellular_counts_csv(const LabelAnalysisRun&, const std::string& path,
                                        std::size_t csv_bytes = 256u * 1024 * 1024,
                                        const std::vector<std::string>& protected_paths = {},
                                        const std::atomic<bool>* cancelled = nullptr);
void save_label_analysis_raster(const LabelAnalysisRun&, const std::string& path,
                                const std::vector<std::string>& protected_paths = {},
                                const std::atomic<bool>* cancelled = nullptr);

struct LabelAnalysisStats {
    std::string state = "idle", error;
    std::uint64_t generation = 0;
};
class LabelAnalysisJob {
  public:
    LabelAnalysisJob();
    ~LabelAnalysisJob();
    LabelAnalysisJob(const LabelAnalysisJob&) = delete;
    bool request(const LabelAnalysisRequest&);
    bool busy() const;
    void cancel();
    void await_completion();
    LabelAnalysisStats poll(const Document&), wait(const Document&);
    std::shared_ptr<const LabelAnalysisRun> result() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace qpx
