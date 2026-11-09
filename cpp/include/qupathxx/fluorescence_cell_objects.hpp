#pragma once
#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/tissue.hpp"
namespace qpx {
struct Annotation;
// Verifies saved geometry/raw-fact integrity; does not remeasure or assert that
// the original image is current. Cosmetic class/name/edit state is excluded.
bool fluorescence_cell_observations_intact(const Annotation&);
class FluorescenceCellContours {
public:
    const std::shared_ptr<const FluorescenceCellRun>& run() const {return run_;}
    const std::shared_ptr<const TissueContours>& nuclei() const {return nuclei_;}
    const std::shared_ptr<const TissueContours>& cells() const {return cells_;}
    std::size_t payload_bytes() const;
private:
    std::shared_ptr<const FluorescenceCellRun> run_;
    std::shared_ptr<const TissueContours> nuclei_,cells_;
    FluorescenceCellContours()=default;
    friend std::shared_ptr<const FluorescenceCellContours> trace_fluorescence_cells(std::shared_ptr<const FluorescenceCellRun>,TissueContourOptions,const std::atomic<bool>*);
};
// Requires the existing optional GEOS capability. Each mask is independently
// traced and ROI-clipped; every label and nucleus-within-body must be preserved.
std::shared_ptr<const FluorescenceCellContours> trace_fluorescence_cells(
    std::shared_ptr<const FluorescenceCellRun>,TissueContourOptions={},const std::atomic<bool>* cancelled=nullptr);
// One undo command; all objects/properties are prepared before currentness and
// publication. Limits native property publication to128 selected channels and
// the existing per-object entry/payload bounds. Repeated publication refuses.
std::vector<std::uint64_t> publish_fluorescence_cells(Document&,const FluorescenceCellRequest&,
    const FluorescenceCellContours&,std::string name_prefix="Fluorescence cell",
    std::size_t work_bytes=256u*1024*1024,const std::atomic<bool>* cancelled=nullptr);
// Replay creates fresh live authority from an intact published recipe. An old
// observed cell is not automatically a current analysis. Malformed recipes refuse;
// verification returns false for changed published facts or a different result.
FluorescenceCellRequest replay_fluorescence_cell(const Document&,std::uint64_t published_cell);
bool verify_fluorescence_cell_publication(const Document&,std::uint64_t published_cell,
    const FluorescenceCellRequest&,const FluorescenceCellRun&,const std::atomic<bool>* cancelled=nullptr);
// Verify a complete cohort, each label exactly once, with one source-currentness
// check rather than one file/metadata check per published cell.
bool verify_fluorescence_cell_publications(const Document&,const std::vector<std::uint64_t>& published_cells,
    const FluorescenceCellRequest&,const FluorescenceCellRun&,const std::atomic<bool>* cancelled=nullptr);
// Compact native recipe/result-identity records, including zero-cell outcomes.
// Source pixels/masks are not duplicated; replay regenerates and checks the exact
// saved run digest. Keep/remove each use existing image-properties undo.
std::string keep_fluorescence_cell_run(Document&,const FluorescenceCellRequest&,const FluorescenceCellRun&,const std::atomic<bool>* cancelled=nullptr);
std::vector<std::string> fluorescence_cell_record_ids(const Document&);
FluorescenceCellRequest replay_fluorescence_cell_record(const Document&,const std::string& record_id);
bool verify_fluorescence_cell_record(const Document&,const std::string& record_id,const FluorescenceCellRequest&,const FluorescenceCellRun&,const std::atomic<bool>* cancelled=nullptr);
void remove_fluorescence_cell_record(Document&,const std::string& record_id);
}
