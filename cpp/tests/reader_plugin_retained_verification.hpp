#pragma once
#include "../cell_archive.hpp"
#include "../tissue_archive.hpp"
#include "../reader_plugin_link.hpp"
#include "qupathxx/provenance.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace qpx_test {
namespace reader_plugin_retained_detail {
inline void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
inline qpx::StainTransform transform() {return qpx::StainTransform({{1,0,0},{0,1,0}});}
inline std::string provenance(const std::shared_ptr<const qpx::AnalysisProvenance>& value) {
    check(bool(value),"Retained plugin workflow omitted provenance");return qpx::provenance_json(*value);
}
inline std::string binding(const std::shared_ptr<const qpx::AnalysisProvenance>& value) {
    check(value && value->reader_source && qpx::detail::is_reader_plugin_binding(value->reader_source->reader),
        "Retained plugin workflow omitted its implementation binding");return value->reader_source->reader;
}
struct FrozenTissue {
    std::uint64_t id;
    std::string producer,geometry,source;
    std::vector<std::uint8_t> payload;
    std::vector<std::uint32_t> labels;
    explicit FrozenTissue(const qpx::TissueRecord& record):id(record.id),producer(record.producer),
        geometry(record.result->roi().wkt()),source(provenance(record.result->provenance())),
        payload(qpx::TissueArchiveCodec::encode(*record.result)),labels(record.result->labels()) {}
    void unchanged(const qpx::Document& document) const {
        const auto record=document.tissue_record(id);
        check(record->id==id && record->producer==producer && record->result->roi().wkt()==geometry &&
            provenance(record->result->provenance())==source && qpx::TissueArchiveCodec::encode(*record->result)==payload &&
            record->result->labels()==labels,"Plugin tissue verification rewrote archived producer, geometry, provenance or mask payload");
    }
};
struct FrozenCells {
    std::uint64_t id;
    std::string producer,geometry,source;
    std::vector<std::uint8_t> payload;
    std::vector<std::uint32_t> nuclei,bodies;
    explicit FrozenCells(const qpx::CellRecord& record):id(record.id),producer(record.producer),
        geometry(record.result->nuclei()->roi().wkt()),source(provenance(record.result->nuclei()->provenance())),
        payload(qpx::CellArchiveCodec::encode(*record.result)),nuclei(record.result->nuclei()->labels()),bodies(record.result->cells()->labels()) {}
    void unchanged(const qpx::Document& document) const {
        const auto record=document.cell_record(id);
        check(record->id==id && record->producer==producer && record->result->nuclei()->roi().wkt()==geometry &&
            provenance(record->result->nuclei()->provenance())==source && qpx::CellArchiveCodec::encode(*record->result)==payload &&
            record->result->nuclei()->labels()==nuclei && record->result->cells()->labels()==bodies,
            "Plugin cell verification rewrote archived producer, geometry, provenance or mask payload");
    }
};
template<class Action> void changed_implementation(Action action) {
    try {action();}
    catch(const std::invalid_argument& error) {
        check(std::string(error.what())=="reader implementation changed; recompute new result",
            "Retained plugin verification refused for a reason unrelated to implementation identity");return;
    }
    throw std::runtime_error("Identical pixels let an archived result adopt a different plugin implementation");
}
inline void check_source(const qpx::Document& document) {
    check(document.metadata().width==2 && document.metadata().height==2 &&
        document.annotation(1).geometry.equals(qpx::Roi::rectangle({0,0,2,2})),
        "Retained plugin verification fixture needs its existing 2x2 ROI1");
}
}
inline void retain_plugin_verification_examples(qpx::Document& document) {
    using namespace reader_plugin_retained_detail;check_source(document);
    check(document.tissue_record_count()==0 && document.cell_record_count()==0,"Retained plugin examples must be captured once");
    qpx::TissueParameters tissue;tissue.level=0;tissue.tile_size=16;tissue.threshold=0;tissue.max_components=16;
    qpx::TissueRequest tissue_request(document,1,transform(),tissue);const auto tissue_result=qpx::detect_tissue(tissue_request);
    check(tissue_result->samples()==4 && tissue_result->foreground_pixels()==4 &&
        tissue_result->labels()==std::vector<std::uint32_t>({1,1,1,1}),"Independent 2x2 tissue mask oracle changed");
    binding(tissue_result->provenance());const auto tissue_id=document.keep_tissue(tissue_request,tissue_result);
    const FrozenTissue frozen_tissue(*document.tissue_record(tissue_id));
    const auto tissue_replay=qpx::TissueRequest::replay(document,tissue_id);
    check(document.verify_tissue(tissue_id,tissue_replay,*qpx::detect_tissue(tissue_replay)) && document.tissue_status(tissue_id)=="current",
        "Same-implementation plugin tissue replay did not verify");frozen_tissue.unchanged(document);

    qpx::CellParameters cells;cells.level=0;cells.tile_size=16;cells.threshold=0;cells.sigma=0;
    cells.min_peak_radius=0;cells.min_seed_distance=0;cells.min_nucleus_pixels=1;cells.cell_expansion=0;cells.max_cells=16;
    qpx::CellRequest cell_request(document,1,transform(),cells);const auto cell_result=qpx::detect_cells(cell_request);
    check(cell_result->nuclei()->samples()==4 && cell_result->nuclei()->width()==2 && cell_result->nuclei()->height()==2 &&
        !cell_result->features().empty(),"Permissive native 2x2 cell fixture did not produce a retained run");
    binding(cell_result->nuclei()->provenance());const auto cell_id=document.keep_cells(cell_request,cell_result);
    const FrozenCells frozen_cells(*document.cell_record(cell_id));const auto cell_replay=qpx::CellRequest::replay(document,cell_id);
    check(document.verify_cells(cell_id,cell_replay,*qpx::detect_cells(cell_replay)) && document.cell_status(cell_id)=="current",
        "Same-implementation plugin cell replay did not verify");frozen_cells.unchanged(document);
}
inline void check_plugin_verification_after_adoption(qpx::Document& document) {
    using namespace reader_plugin_retained_detail;check_source(document);
    check(document.tissue_record_count()==1 && document.cell_record_count()==1,"Saved plugin examples were not retained");
    const auto old_tissue=document.tissue_records().front();const auto old_cells=document.cell_records().front();
    const FrozenTissue frozen_tissue(*old_tissue);const FrozenCells frozen_cells(*old_cells);
    const auto revision=document.revision();const auto tissue_status=document.tissue_status(old_tissue->id),cell_status=document.cell_status(old_cells->id);
    check(tissue_status!="current" && cell_status!="current","Source adoption left the old retained run current");

    const auto tissue_request=qpx::TissueRequest::replay(document,old_tissue->id);const auto tissue_result=qpx::detect_tissue(tissue_request);
    const auto cell_request=qpx::CellRequest::replay(document,old_cells->id);const auto cell_result=qpx::detect_cells(cell_request);
    check(binding(old_tissue->result->provenance())!=binding(tissue_result->provenance()) &&
        binding(old_cells->result->nuclei()->provenance())!=binding(cell_result->nuclei()->provenance()),
        "Implementation adoption failed to change retained workflow provenance");
    check(old_tissue->result->input_sha256()==tissue_result->input_sha256() && old_tissue->result->labels()==tissue_result->labels() &&
        old_cells->result->nuclei()->input_sha256()==cell_result->nuclei()->input_sha256() &&
        old_cells->result->nuclei()->labels()==cell_result->nuclei()->labels() && old_cells->result->cells()->labels()==cell_result->cells()->labels(),
        "Implementation-only adoption changed the existing pixel hashes or masks");
    changed_implementation([&]{document.verify_tissue(old_tissue->id,tissue_request,*tissue_result);});
    changed_implementation([&]{document.verify_cells(old_cells->id,cell_request,*cell_result);});
    check(document.revision()==revision && document.tissue_status(old_tissue->id)==tissue_status && document.cell_status(old_cells->id)==cell_status &&
        document.tissue_record_count()==1 && document.cell_record_count()==1,"Rejected implementation verification changed retained current state");
    frozen_tissue.unchanged(document);frozen_cells.unchanged(document);

    const auto new_tissue=document.keep_tissue(tissue_request,tissue_result);
    const auto fresh_cell_request=qpx::CellRequest::replay(document,old_cells->id);
    const auto fresh_cell_result=qpx::detect_cells(fresh_cell_request);
    check(binding(fresh_cell_result->nuclei()->provenance())==binding(cell_result->nuclei()->provenance()) &&
        fresh_cell_result->nuclei()->input_sha256()==cell_result->nuclei()->input_sha256() &&
        fresh_cell_result->nuclei()->labels()==cell_result->nuclei()->labels() && fresh_cell_result->cells()->labels()==cell_result->cells()->labels(),
        "Recapturing cells after tissue retention changed source identity or masks");
    const auto new_cells=document.keep_cells(fresh_cell_request,fresh_cell_result);
    const auto replay_tissue=qpx::TissueRequest::replay(document,new_tissue);const auto replay_cells=qpx::CellRequest::replay(document,new_cells);
    check(document.verify_tissue(new_tissue,replay_tissue,*qpx::detect_tissue(replay_tissue)) && document.tissue_status(new_tissue)=="current" &&
        document.verify_cells(new_cells,replay_cells,*qpx::detect_cells(replay_cells)) && document.cell_status(new_cells)=="current",
        "Recomputed plugin runs could not be retained and verified under the new implementation");
    frozen_tissue.unchanged(document);frozen_cells.unchanged(document);
}
}
