#pragma once
#include "qupathxx/object_classifier.hpp"
#include "qupathxx/cells.hpp"
namespace qpx {
struct ObjectCellFeatureSource {
    std::uint64_t record_id=0,object_id=0,object_revision=0,document_revision=0,cell_count=0;
    std::string producer,algorithm,input_sha256;
    CellParameters parameters;
    Roi roi;
    std::int32_t level=0;
    double downsample=1;
    ColorVector white;
    std::array<ColorVector,3> stains;
    std::shared_ptr<const AnalysisProvenance> provenance;
    // Legacy parameters/downsample describe natural grids only. Explicit grids
    // carry typed values here; scalar downsample is NaN and parameters.validate
    // rejects the grid algorithm rather than silently reinterpret its units.
    std::optional<ScientificGrid> scientific_grid;
    std::optional<GridCellParameters> grid_parameters;
    ResolvedGridCellParameters resolved_grid_parameters;
    std::size_t payload_bytes() const;
};
// Immutable worker-prepared artifact. Reopening never restores live request authority.
class ObjectClassifierArchive : public std::enable_shared_from_this<ObjectClassifierArchive> {
public:
    const std::shared_ptr<const ObjectFeatureTable>& table() const {return table_;}
    const std::shared_ptr<const ForestModel>& model() const {return model_;}
    const std::shared_ptr<const ObjectClassificationResult>& result() const {return result_;}
    std::string purpose() const {return result_?"prediction":"training";}
    const std::string& sha256() const {return sha256_;}
    std::size_t payload_bytes() const {return payload_bytes_;}
private:
    ObjectClassifierArchive()=default;
    std::shared_ptr<const ObjectFeatureTable> table_;
    std::shared_ptr<const ForestModel> model_;
    std::shared_ptr<const ObjectClassificationResult> result_;
    std::vector<std::uint8_t> bytes_;
    std::string sha256_;
    std::size_t payload_bytes_=0;
    friend struct ObjectClassifierArchiveCodec;
};
std::shared_ptr<const ObjectClassifierArchive> archive_object_classifier(std::shared_ptr<const ObjectFeatureTable>,const ForestModel&,
    std::shared_ptr<const ObjectClassificationResult> result=nullptr,const std::atomic<bool>* cancelled=nullptr);
struct ObjectClassifierRecord : public std::enable_shared_from_this<ObjectClassifierRecord> {
    std::uint64_t id=0;
    std::string name;
    std::shared_ptr<const ObjectClassifierArchive> archive;
    bool applied=false;
    double minimum_vote_fraction=0;
    std::vector<std::uint64_t> changed_objects;
    std::size_t payload_bytes() const;
};
}
