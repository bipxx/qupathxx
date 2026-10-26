#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include <set>
#include <list>
#include "qupathxx/image_server.hpp"
#include "qupathxx/display.hpp"
#include "qupathxx/geometry.hpp"
#include "qupathxx/spatial_index.hpp"
#include "qupathxx/analysis_types.hpp"

namespace qpx {
namespace detail {struct ClassificationEdit;class GuiPixelClassifier;class GuiTma;class GuiRaw;class ViewerOwner;struct TmaIndex;}
class RawAnalysisRequest;class RawAnalysisResult;struct RawAnalysisRecord;
struct TmaGridOptions;
struct TmaDearrayOptions;struct TmaDearrayResult;
class TmaGrid;
class TmaCoreInfo;
class Project;
struct FileFingerprint;
struct SourceFileAssociation;
struct StainEstimateResult;
struct StainEstimateParameters;
struct ObjectFeatureOptions;
struct ForestParameters;
class ForestModel;
class PixelClassifier;
class PixelTrainingTable;
class PixelPredictionResult;
class PixelTrainingRequest;
class PixelPredictionRequest;
class PixelClassifierArchive;
struct PixelClassifierRecord;
class RawPixelArchive;struct RawPixelRecord;class RawPixelTrainingRequest;class RawPixelPredictionRequest;
class PixelContours;
struct TissueContourOptions;
struct PixelTrainingOptions;
struct PixelPredictionOptions;
struct PixelStreamOptions;
struct PixelStreamExportStats;
struct PixelStreamReadStats;
class PixelStreamLink;
struct PixelStreamRecord;
class ObjectClassificationResult;
class ObjectClassifierArchive;
class ObjectFeatureRequest;
struct ObjectClassifierRecord;
struct CellParameters;
struct GridCellSampling;
struct RawGridSampling;
struct GridCellParameters;
struct ProjectBatchOptions;
struct ProjectBatchReport;
class CellResult;
class CellRequest;
struct CellRecord;
struct CellFeature;
struct TissueRecord;
class TissueRequest;
class TissueResult;
enum class ObjectKind { Annotation=0, Detection=1, Cell=2 };
struct RoiMeasurements {
    double area_px2=0,length_px=0;
    std::optional<double> area_um2,length_um;
};
struct ShapeMeasurements {
    RoiMeasurements roi;
    std::optional<RoiMeasurements> nucleus;
};
struct FeatureCacheStats {
    std::size_t entries=0,capacity=0;
    std::uint64_t hits=0,misses=0;
};
struct ObjectClass {
    std::string name;
    std::uint32_t color = 0x41e1be; // RGB, no alpha.
};
struct CellFeatureLink {
    std::uint64_t record_id=0;
    std::uint32_t label=0;
    bool operator==(const CellFeatureLink& other) const {return record_id==other.record_id && label==other.label;}
};
struct Annotation {
    std::uint64_t id;
    std::string name;
    Roi geometry;
    std::uint64_t parent_id = 0;
    std::string class_name;
    std::uint64_t revision = 0;
    ObjectKind kind = ObjectKind::Annotation;
    std::optional<Roi> nucleus;
    std::optional<CellFeatureLink> cell_feature_link;
    std::shared_ptr<const TmaCoreInfo> tma_core;
    Rect bounds() const;
    bool is_detection() const { return kind!=ObjectKind::Annotation; }
};
enum class ObjectEditKind { Create, Replace, Remove, TmaMetadata };
struct ObjectEdit {
    ObjectEditKind kind;
    Annotation value;
    static ObjectEdit create(Rect roi, std::string name="Annotation", std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit replace(std::uint64_t id, Rect roi, std::string name, std::uint64_t parent_id=0, std::string class_name="");
    static ObjectEdit remove(std::uint64_t id);
    static ObjectEdit tma_metadata(std::uint64_t id,std::shared_ptr<const TmaCoreInfo>);
    static ObjectEdit create_geometry(Roi geometry, std::string name="Annotation", std::uint64_t parent_id=0, std::string class_name="", ObjectKind kind=ObjectKind::Annotation, std::optional<Roi> nucleus=std::nullopt);
    static ObjectEdit replace_geometry(std::uint64_t id, Roi geometry, std::string name, std::uint64_t parent_id=0, std::string class_name="", ObjectKind kind=ObjectKind::Annotation, std::optional<Roi> nucleus=std::nullopt);
};
double mean_rgb(const ImageServer&, const Rect&, const std::atomic<bool>* cancelled = nullptr);
double mean_rgb(const ImageServer&, const Roi&, const std::atomic<bool>* cancelled = nullptr);
class Image {
public:
    Image(int width, int height, std::vector<std::uint8_t> rgb);
    static Image demo();
    static Image read_ppm(const std::string& path);
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<std::uint8_t>& pixels() const { return *rgb_; }
    std::shared_ptr<const std::vector<std::uint8_t>> pixels_owner() const { return rgb_; }
private:
    int width_, height_;
    std::shared_ptr<const std::vector<std::uint8_t>> rgb_;
};
// Coordinates are level-zero image pixels; calibration is micrometers/pixel.
class Document {
    friend class CellTableRequest;
    friend class MeasurementTableRequest;
    friend class ObjectFeatureRequest;
    friend class detail::GuiPixelClassifier;
    friend class detail::GuiTma;
    friend class detail::GuiRaw;
    friend class detail::ViewerOwner;
public:
    explicit Document(Image image);
    explicit Document(std::shared_ptr<ImageServer> server);
    const Image& image() const { return dense_image(*server_); }
    std::shared_ptr<ImageServer> server() const { return server_; }
    void set_tiff_view(const TiffView&);
    const ImageMetadata& metadata() const { return server_->metadata(); }
    std::vector<Annotation> annotations() const;
    const std::map<std::uint64_t,Annotation>& objects() const { return objects_; }
    Annotation annotation(std::uint64_t id) const { return find(id); }
    std::size_t object_count() const { return objects_.size()+object_snapshot_count_; }
    std::shared_ptr<const TmaGrid> tma_grid(std::size_t byte_limit=64*1024*1024) const;
    std::vector<std::uint64_t> tma_core_ids(bool include_missing=false) const;
    std::size_t tma_core_count() const;
    std::uint64_t tma_revision() const {return tma_revision_;}
    std::vector<std::uint64_t> create_tma_grid(const TmaGridOptions&);
    std::uint64_t add_tma_core(std::size_t row,std::size_t column,Rect bounds,std::string case_id="",bool missing=false);
    // No bounds: infer positions. Empty bounds: insert vacant slots. Otherwise
    // supply one rectangle per cross-axis slot. Existing geometry/IDs stay put.
    // Atomic structural corrections; retained cores keep geometry, IDs and case data.
    // Absent bounds infer neighboring centers. Empty bounds insert vacant slots;
    // otherwise provide one ellipse bound per cross-axis slot. Ambiguous spacing
    // rejects without mutation. Removal deletes selected cores and descendants,
    // protecting/reparenting cores in other axes and their descendant subtrees.
    std::vector<std::uint64_t> insert_tma_row(std::size_t index,std::string label="",std::optional<std::vector<Rect>> bounds={},bool missing=false);
    std::vector<std::uint64_t> insert_tma_column(std::size_t index,std::string label="",std::optional<std::vector<Rect>> bounds={},bool missing=false);
    void remove_tma_row(std::size_t index);
    void remove_tma_column(std::size_t index);
    void translate_tma_grid(double dx,double dy,bool include_descendants=true);
    void set_tma_missing(const std::vector<std::uint64_t>&,bool);
    void set_tma_case_ids(const std::map<std::uint64_t,std::string>&);
    void set_tma_labels(std::string name,const std::vector<std::string>& rows,const std::vector<std::string>& columns,bool rename_cores=true);
    void clear_tma_grid(bool delete_cores=false);
    std::uint64_t revision() const { return revision_; }
    std::uint64_t structure_revision() const { return structure_revision_; }
    // Geometry/class/calibration commands change this; archival records do not.
    std::uint64_t render_revision() const { return render_revision_; }
    const std::map<std::string,ObjectClass>& classes() const { return classes_; }
    ObjectClass object_class(const std::string& name) const;
    std::size_t class_count(const std::string& name) const;
    void set_class(std::string name, std::uint32_t color);
    void remove_class(const std::string& name);
    void rename_class(const std::string& old_name, std::string new_name);
    void classify_objects(const std::vector<std::uint64_t>& ids, std::string class_name);
    void apply_classifications(const std::vector<std::pair<std::uint64_t,std::string>>& assignments,std::string label="Classify objects");
    std::uint64_t add_rectangle(Rect roi, std::string name = "Annotation");
    std::uint64_t add_roi(Roi roi, std::string name="Annotation");
    std::uint64_t add_detection(Roi roi, std::string name="Detection", std::uint64_t parent_id=0, std::string class_name="");
    std::uint64_t add_cell(Roi roi, std::optional<Roi> nucleus=std::nullopt, std::string name="Cell", std::uint64_t parent_id=0, std::string class_name="");
    void update_nucleus(std::uint64_t id, std::optional<Roi> nucleus);
    bool remove_annotation(std::uint64_t id);
    void remove_objects(const std::vector<std::uint64_t>& ids);
    void update_rectangle(std::uint64_t id, Rect roi);
    void update_roi(std::uint64_t id, Roi roi);
    void edit_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point);
    void insert_vertex(std::uint64_t id,std::size_t path,std::size_t vertex,Point point);
    void remove_vertex(std::uint64_t id,std::size_t path,std::size_t vertex);
    void add_hole(std::uint64_t id,std::size_t part,Ring hole);
    void remove_hole(std::uint64_t id,std::size_t part,std::size_t hole);
    void apply_boolean(std::uint64_t target,const std::vector<std::uint64_t>& operands,BooleanOp operation);
    void rename_annotation(std::uint64_t id, std::string name);
    void classify_annotation(std::uint64_t id, std::string class_name);
    void reparent_annotation(std::uint64_t id, std::uint64_t parent_id);
    std::vector<std::uint64_t> execute(const std::vector<ObjectEdit>& edits, std::string label="Edit objects");
    // Import-local IDs/parents are remapped; existing native IDs are untouched.
    std::vector<std::uint64_t> append_objects(const std::vector<Annotation>& objects,const std::vector<ObjectClass>& classes);
    std::vector<std::uint64_t> children(std::uint64_t parent_id) const;
    std::vector<std::uint64_t> query_region(Rect bounds) const;
    std::vector<std::uint64_t> pick(Point point,double tolerance=0) const;
    SpatialStats spatial_stats() const;
    bool undo();
    bool redo();
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    std::string undo_label() const { return can_undo()?undo_.back().label:""; }
    std::string redo_label() const { return can_redo()?redo_.back().label:""; }
    void clear_history();
    void set_history_limits(std::size_t steps, std::size_t payload_bytes);
    std::size_t history_bytes() const { return history_bytes_; }
    void set_calibration(double pixel_width_um, double pixel_height_um);
    std::optional<double> pixel_width_um() const { return pixel_width_um_; }
    std::optional<double> pixel_height_um() const { return pixel_height_um_; }
    void clear_calibration();
    std::uint64_t calibration_revision() const { return calibration_revision_; }
    // Owner-thread APIs, like spatial queries. Workers use immutable ROI values.
    ShapeMeasurements shape_measurements(std::uint64_t id) const;
    FeatureCacheStats feature_cache_stats() const;
    void set_feature_cache_limit(std::size_t entries);
    void clear_feature_cache();
    double area_um2(std::uint64_t id) const;
    double mean_intensity(std::uint64_t id) const;
    double nucleus_area_um2(std::uint64_t id) const;
    double nucleus_mean_intensity(std::uint64_t id) const;
    std::vector<AnalysisRecord> analysis_records() const;
    std::size_t analysis_record_count() const {return analysis_.size()+analysis_snapshot_count_;}
    AnalysisRecord analysis_record(std::uint64_t id) const;
    std::optional<AnalysisRecord> latest_analysis(std::uint64_t object_id) const;
    std::string analysis_status(std::uint64_t id) const;
    std::uint64_t keep_analysis(const class AnalysisRequest&,const AnalysisResult&);
    bool verify_analysis(std::uint64_t id,const class AnalysisRequest&,const AnalysisResult&);
    bool remove_analysis(std::uint64_t id);
    std::vector<std::shared_ptr<const TissueRecord>> tissue_records() const;
    std::size_t tissue_record_count() const {return tissue_.size()+tissue_snapshot_count_;}
    std::shared_ptr<const TissueRecord> tissue_record(std::uint64_t id) const;
    std::shared_ptr<const TissueRecord> latest_tissue(std::uint64_t object_id) const;
    std::string tissue_status(std::uint64_t id) const;
    std::uint64_t keep_tissue(const TissueRequest&,std::shared_ptr<const TissueResult>);
    bool verify_tissue(std::uint64_t id,const TissueRequest&,const TissueResult&);
    bool remove_tissue(std::uint64_t id);
    std::vector<std::shared_ptr<const CellRecord>> cell_records() const;
    std::size_t cell_record_count() const {return cells_.size()+cells_snapshot_.size();}
    std::shared_ptr<const CellRecord> cell_record(std::uint64_t id) const;
    std::shared_ptr<const CellRecord> latest_cells(std::uint64_t object_id) const;
    std::string cell_status(std::uint64_t id) const;
    std::uint64_t keep_cells(const CellRequest&,std::shared_ptr<const CellResult>);
    std::vector<std::uint64_t> publish_cells(const CellRequest&,std::shared_ptr<const CellResult>,std::string name_prefix="Cell",std::string class_name="");
    std::vector<std::uint64_t> publish_cell_record(std::uint64_t record_id,std::string name_prefix="Cell",std::string class_name="");
    CellFeature cell_object_feature(std::uint64_t object_id) const;
    std::string cell_object_status(std::uint64_t object_id) const;
    bool verify_cells(std::uint64_t id,const CellRequest&,const CellResult&);
    bool remove_cells(std::uint64_t id);
    std::vector<std::shared_ptr<const ObjectClassifierRecord>> classifier_records() const;
    std::shared_ptr<const ObjectClassifierRecord> classifier_record(std::uint64_t id) const;
    std::size_t classifier_record_count() const {return classifiers_.size()+classifiers_snapshot_.size();}
    std::uint64_t keep_classifier_training(const ObjectFeatureRequest&,std::shared_ptr<const ObjectClassifierArchive>,std::string name="Object classifier training");
    bool remove_classifier_record(std::uint64_t id);
    std::vector<std::shared_ptr<const PixelClassifierRecord>> pixel_classifier_records() const;
    std::shared_ptr<const PixelClassifierRecord> pixel_classifier_record(std::uint64_t id) const;
    std::size_t pixel_classifier_record_count() const {return pixels_.size()+pixels_snapshot_.size();}
    std::uint64_t keep_pixel_training(const PixelTrainingRequest&,std::shared_ptr<const PixelClassifierArchive>,std::string name="Pixel classifier training");
    std::uint64_t keep_pixel_prediction(const PixelPredictionRequest&,std::shared_ptr<const PixelClassifierArchive>,std::string name="Pixel classifier prediction");
    bool remove_pixel_classifier_record(std::uint64_t id);
    std::vector<std::shared_ptr<const RawPixelRecord>> raw_pixel_records() const;
    std::shared_ptr<const RawPixelRecord> raw_pixel_record(std::uint64_t) const;
    std::size_t raw_pixel_record_count() const {return raw_pixels_.size()+raw_pixels_snapshot_.size();}
    std::uint64_t keep_raw_pixel_training(const RawPixelTrainingRequest&,std::shared_ptr<const RawPixelArchive>,std::string name="Raw pixel training");
    std::uint64_t keep_raw_pixel_prediction(const RawPixelPredictionRequest&,std::shared_ptr<const RawPixelArchive>,std::string name="Raw pixel prediction");
    bool remove_raw_pixel_record(std::uint64_t);
    std::vector<std::shared_ptr<const PixelStreamRecord>> pixel_stream_records() const;
    std::shared_ptr<const PixelStreamRecord> pixel_stream_record(std::uint64_t id) const;
    std::size_t pixel_stream_record_count() const {return streams_.size()+streams_snapshot_.size();}
    std::uint64_t keep_pixel_stream(std::shared_ptr<const PixelStreamLink>,std::string name="Pixel stream");
    bool remove_pixel_stream_record(std::uint64_t id);
    void relink_pixel_stream_record(std::uint64_t id,std::shared_ptr<const PixelStreamLink>);
    std::vector<std::shared_ptr<const RawAnalysisRecord>> raw_analysis_records() const;
    std::vector<std::shared_ptr<const RawAnalysisRecord>> raw_analysis_page(std::uint64_t after_id=0,std::size_t limit=64) const;
    std::shared_ptr<const RawAnalysisRecord> raw_analysis_record(std::uint64_t id) const;
    std::shared_ptr<const RawAnalysisRecord> latest_raw_analysis(std::uint64_t object_id) const;
    std::size_t raw_analysis_record_count() const {return raw_analysis_.size()+raw_analysis_snapshot_count_;}
    std::string raw_analysis_status(std::uint64_t id) const;
    std::uint64_t keep_raw_analysis(const RawAnalysisRequest&,std::shared_ptr<const RawAnalysisResult>);
    bool verify_raw_analysis(std::uint64_t id,const RawAnalysisRequest&,std::shared_ptr<const RawAnalysisResult>);
    bool remove_raw_analysis(std::uint64_t id);


private:
    friend struct ProjectBatchKernel;
    void adopt_tiff_source(std::shared_ptr<ImageServer>);
    friend class Project;
    friend struct ObjectClassifierKernel;
    friend class AnalysisRequest;
    friend class RawAnalysisRequest;
    // Copies/assignments are independent live authorities, including when a
    // copied document happens to share its immutable image source and IDs.
    struct AnalysisIdentity {
        std::shared_ptr<const char> token=std::make_shared<const char>();
        AnalysisIdentity()=default;
        AnalysisIdentity(const AnalysisIdentity&) {}
        AnalysisIdentity& operator=(const AnalysisIdentity&) {token=std::make_shared<const char>();return *this;}
    } analysis_identity_;
    struct StoredRawAnalysis {
        std::shared_ptr<const RawAnalysisRecord> value;
        std::shared_ptr<const char> identity;
        std::shared_ptr<const ReaderSource> observed_source;
        bool verified=false;
        std::size_t slot=0;
    };
    struct RawAnalysisBlock {
        std::array<std::shared_ptr<const RawAnalysisRecord>,256> records;
        bool sealed=false;
    };
    struct RawAnalysisBlocks {
        std::vector<std::shared_ptr<RawAnalysisBlock>> values;
        RawAnalysisBlocks()=default;
        RawAnalysisBlocks(const RawAnalysisBlocks& other):values(other.values) {for(const auto& block:values) if(!block->sealed) block->sealed=true;}
        RawAnalysisBlocks& operator=(const RawAnalysisBlocks& other) {if(this!=&other) {RawAnalysisBlocks copy(other);values.swap(copy.values);}return *this;}
        RawAnalysisBlocks(RawAnalysisBlocks&&)=default;RawAnalysisBlocks& operator=(RawAnalysisBlocks&&)=default;
    };
    std::map<std::uint64_t,StoredRawAnalysis> raw_analysis_;
    RawAnalysisBlocks raw_analysis_blocks_;
    std::vector<std::shared_ptr<const RawAnalysisBlock>> raw_analysis_snapshot_;
    std::vector<std::size_t> raw_analysis_free_slots_;
    std::size_t raw_analysis_next_slot_=0,raw_analysis_snapshot_count_=0,raw_analysis_bytes_=0;
    std::map<std::pair<std::uint64_t,std::uint64_t>,bool> raw_analysis_by_object_;
    void restore_raw_analysis(std::shared_ptr<const RawAnalysisRecord>);
    struct StoredAnalysis {
        std::shared_ptr<const AnalysisRecord> value;
        std::shared_ptr<const char> identity;
        std::weak_ptr<ImageServer> source;
        bool verified=false;
        std::size_t slot=0;
    };
    std::map<std::uint64_t,StoredAnalysis> analysis_;
    struct AnalysisBlock {
        std::array<std::shared_ptr<const AnalysisRecord>,256> records;
        bool sealed=false; // Once shared with another authority/worker, never unseal.
    };
    struct AnalysisBlocks {
        std::vector<std::shared_ptr<AnalysisBlock>> values;
        AnalysisBlocks()=default;
        AnalysisBlocks(const AnalysisBlocks& other):values(other.values) {
            for(const auto& block:values) if(!block->sealed) block->sealed=true;
        }
        AnalysisBlocks& operator=(const AnalysisBlocks& other) {
            if(this!=&other) {AnalysisBlocks copy(other);values.swap(copy.values);}return *this;
        }
        AnalysisBlocks(AnalysisBlocks&&)=default;
        AnalysisBlocks& operator=(AnalysisBlocks&&)=default;
    };
    // Snapshots share const views of these blocks. Live edits clone a shared
    // block before modifying its slots; proofs and lookup maps stay owner-only.
    AnalysisBlocks analysis_blocks_;
    std::vector<std::shared_ptr<const AnalysisBlock>> analysis_snapshot_;
    std::vector<std::size_t> analysis_free_slots_;
    std::size_t analysis_next_slot_=0,analysis_snapshot_count_=0;
    std::map<std::pair<std::uint64_t,std::uint64_t>,bool> analysis_by_object_;
    std::size_t analysis_bytes_=0;
    void restore_analysis(AnalysisRecord);
    struct StoredTissue {
        std::shared_ptr<const TissueRecord> value;
        std::shared_ptr<const char> identity;
        std::weak_ptr<ImageServer> source;
        bool verified=false;
        std::size_t slot=0;
    };
    struct TissueBlock {
        std::array<std::shared_ptr<const TissueRecord>,256> records;
        bool sealed=false;
    };
    struct TissueBlocks {
        std::vector<std::shared_ptr<TissueBlock>> values;
        TissueBlocks()=default;
        TissueBlocks(const TissueBlocks& other):values(other.values) {for(const auto& block:values) block->sealed=true;}
        TissueBlocks& operator=(const TissueBlocks& other) {if(this!=&other) {TissueBlocks copy(other);values.swap(copy.values);}return *this;}
        TissueBlocks(TissueBlocks&&)=default;
        TissueBlocks& operator=(TissueBlocks&&)=default;
    } tissue_blocks_;
    std::map<std::uint64_t,StoredTissue> tissue_;
    std::map<std::pair<std::uint64_t,std::uint64_t>,bool> tissue_by_object_;
    std::vector<std::shared_ptr<const TissueBlock>> tissue_snapshot_;
    std::vector<std::size_t> tissue_free_slots_;
    std::size_t tissue_next_slot_=0,tissue_snapshot_count_=0,tissue_bytes_=0;
    void restore_tissue(std::shared_ptr<const TissueRecord>);
    struct StoredCells {
        std::shared_ptr<const CellRecord> value;
        std::shared_ptr<const char> identity;
        std::weak_ptr<ImageServer> source;
        bool verified=false;
    };
    std::map<std::uint64_t,StoredCells> cells_;
    std::map<std::pair<std::uint64_t,std::uint64_t>,bool> cells_by_object_;
    std::vector<std::shared_ptr<const CellRecord>> cells_snapshot_;
    std::size_t cells_bytes_=0;
    void restore_cells(std::shared_ptr<const CellRecord>);
    std::map<std::uint64_t,std::shared_ptr<const ObjectClassifierRecord>> classifiers_;
    std::vector<std::shared_ptr<const ObjectClassifierRecord>> classifiers_snapshot_;
    std::size_t classifiers_bytes_=0;
    void restore_classifier(std::shared_ptr<const ObjectClassifierRecord>);
    std::map<std::uint64_t,std::shared_ptr<const PixelClassifierRecord>> pixels_;
    std::vector<std::shared_ptr<const PixelClassifierRecord>> pixels_snapshot_;
    std::size_t pixels_bytes_=0;
    void restore_pixel_classifier(std::shared_ptr<const PixelClassifierRecord>);
    std::map<std::uint64_t,std::shared_ptr<const RawPixelRecord>> raw_pixels_;
    std::vector<std::shared_ptr<const RawPixelRecord>> raw_pixels_snapshot_;
    std::size_t raw_pixels_bytes_=0;
    void restore_raw_pixel(std::shared_ptr<const RawPixelRecord>);
    std::map<std::uint64_t,std::shared_ptr<const PixelStreamRecord>> streams_;
    std::vector<std::shared_ptr<const PixelStreamRecord>> streams_snapshot_;
    std::size_t streams_bytes_=0;
    void restore_pixel_stream(std::shared_ptr<const PixelStreamRecord>);

    Document() = default; // Project's private save snapshots populate canonical fields only.
    static std::shared_ptr<Document> restore(std::shared_ptr<ImageServer> server,
        const std::vector<Annotation>& objects,const std::vector<ObjectClass>& classes,
        std::optional<double> pixel_width,std::optional<double> pixel_height,
        std::uint64_t next_id,std::uint64_t revision);
    const Annotation& find(std::uint64_t id) const;
    void publish_roi(std::uint64_t id,Roi geometry,std::string label);
    using ObjectPatch = std::map<std::uint64_t,std::optional<Annotation>>;
    using ClassPatch = std::map<std::string,std::optional<ObjectClass>>;
    struct Calibration { std::optional<double> x,y; };
    struct ClassificationHistory {
        std::shared_ptr<detail::ClassificationEdit> value;
        ClassificationHistory()=default;
        ClassificationHistory(const ClassificationHistory&);
        ClassificationHistory& operator=(const ClassificationHistory&);
        ClassificationHistory(ClassificationHistory&&) noexcept=default;
        ClassificationHistory& operator=(ClassificationHistory&&) noexcept=default;
    };
    struct EditRecord {
        ObjectPatch before,after;
        Calibration before_calibration,after_calibration;
        ClassPatch before_classes,after_classes;
        std::string label;
        std::size_t bytes = 0;
        ClassificationHistory classifications{};
    };
    struct Staging;
    std::shared_ptr<const detail::TmaIndex> tma_index_;
    std::uint64_t tma_revision_=0;
    std::shared_ptr<const detail::TmaIndex> prepare_tma(const ObjectPatch&,const ObjectPatch&) const;
    void restore_tma_index();
    void apply_record(const EditRecord&, bool forward);
    void toggle_classifications(detail::ClassificationEdit&);
    void commit(Staging&, Calibration);
    void save_edit(Staging&, Calibration, std::string label);
    void trim_history();
    std::vector<ObjectEdit> tma_insert_axis_edits(bool column,std::size_t index,std::string label,const std::optional<std::vector<Rect>>& bounds,bool missing,std::size_t edit_limit=100000) const;
    std::vector<ObjectEdit> tma_remove_axis_edits(bool column,std::size_t index,std::size_t edit_limit=100000) const;
    void refresh_spatial() const;
    std::shared_ptr<ImageServer> server_;
    std::map<std::uint64_t,Annotation> objects_;
    struct ObjectBlock;
    // Published blocks are immutable. Captures copy block handles, never live
    // map nodes; edits prepare replacements before committing canonical state.
    std::map<std::uint64_t,std::shared_ptr<const ObjectBlock>> object_blocks_;
    std::vector<std::shared_ptr<const ObjectBlock>> object_snapshot_;
    std::size_t object_snapshot_count_=0,object_bytes_=0;
    static std::size_t object_payload(const Annotation&);
    std::map<std::uint64_t,std::set<std::uint64_t>> children_;
    std::map<std::string,ObjectClass> classes_;
    std::map<std::string,std::size_t> class_counts_;
    std::vector<EditRecord> undo_,redo_;
    std::uint64_t next_id_ = 1;
    std::uint64_t revision_ = 0;
    std::uint64_t structure_revision_ = 0;
    std::uint64_t render_revision_ = 0;
    std::size_t history_steps_ = 128, history_budget_ = 64*1024*1024, history_bytes_ = 0;
    std::optional<double> pixel_width_um_, pixel_height_um_;
    std::uint64_t calibration_revision_=0;
    struct FeatureCache {
        struct Entry {
            std::uint64_t object_revision,calibration_revision;
            ShapeMeasurements value;
            std::list<std::uint64_t>::iterator position;
        };
        std::size_t capacity=4096;
        std::uint64_t hits=0,misses=0;
        std::list<std::uint64_t> lru;
        std::map<std::uint64_t,Entry> entries;
        FeatureCache()=default;
        // Derived entries contain list iterators; copies start with a cold cache.
        FeatureCache(const FeatureCache& other):capacity(other.capacity) {}
        FeatureCache& operator=(const FeatureCache& other) {
            if(this!=&other) {entries.clear(); lru.clear(); capacity=other.capacity; hits=misses=0;}
            return *this;
        }
        FeatureCache(FeatureCache&&)=default;
        FeatureCache& operator=(FeatureCache&&)=default;
    };
    mutable FeatureCache feature_cache_;
    mutable std::shared_ptr<const SpatialIndex> spatial_;
    mutable std::map<std::uint64_t,std::optional<Rect>> spatial_dirty_;
    mutable bool spatial_rebuild_=true;
    mutable std::uint64_t spatial_builds_=0;
};
// Presentation state, bound to a document that must outlive it. Selection is
// not journaled; deletion/undo pruning never silently selects restored objects.
class ObjectSelection {
public:
    explicit ObjectSelection(const Document& document) : document_(document) {}
    std::vector<std::uint64_t> ids() const;
    std::uint64_t primary() const;
    std::size_t size() const;
    std::uint64_t revision() const {sync();return revision_;}
    bool contains(std::uint64_t id) const;
    void replace(const std::vector<std::uint64_t>& ids,std::uint64_t primary=0);
    void toggle(std::uint64_t id);
    void clear();
    void select_all();
    void select_range(std::uint64_t anchor,std::uint64_t target,bool additive=false);
    void select_region(Rect bounds,bool additive=false);
private:
    void sync() const;
    void publish(std::set<std::uint64_t> ids,std::uint64_t primary);
    const Document& document_;
    mutable std::set<std::uint64_t> ids_;
    mutable std::uint64_t primary_=0;
    mutable std::uint64_t structure_revision_=0;
    mutable std::uint64_t revision_=0;
};
struct Viewport {
    Point origin;
    double zoom = 1;
    Point image_to_screen(Point p) const;
    Point screen_to_image(Point p) const;
    void zoom_at(Point screen, double factor);
    void fit(std::int64_t image_width, std::int64_t image_height, double canvas_width, double canvas_height);
    void center_on(Point image_point, double canvas_width, double canvas_height);
};
// Runs on the calling/main thread. frame_limit=0 is interactive.
struct ThumbnailResult;
struct GuiReport {
    int frames = 0;
    std::uint64_t uploaded_tiles = 0;
    std::size_t peak_gpu_bytes = 0;
    std::uint64_t generations = 0;
    double cpu_frame_p95_ms = 0;
    std::size_t pending_tiles = 0;
    double final_zoom = 0;
    Point final_origin;
    Point final_center;
    int framebuffer_width = 0, framebuffer_height = 0;
    std::string gl_renderer, gl_version;
    std::size_t visible_objects = 0;
    std::vector<std::uint64_t> selected_ids;
    std::uint64_t primary_selection = 0;
    std::uint64_t active_image = 0,image_switches = 0;
    double max_image_switch_ms = 0;
    std::string project_error;
    std::uint64_t autosaves_completed=0;
    std::string recovery_path,autosave_error;
    double autosave_capture_ms=0;
    double max_cpu_frame_ms=0;
    std::uint64_t project_replacements=0;
    double project_load_ms=0;
    std::uint64_t images_removed=0;
    std::uint64_t saves_completed=0;
    double save_capture_ms=0,save_write_ms=0;
    double recovery_load_ms=0;
    std::uint64_t checkpoints_deleted=0;
    std::string recovery_maintenance_error;
    std::uint64_t retired_workspaces=0;
    double max_retirement_ms=0;
    std::size_t checkpoints_pruned=0;
    double retention_ms=0;
    std::string retention_error;
    bool retention_cancelled=false;
    std::size_t retention_inspected=0;
    double retention_metadata_ms=0,retention_ownership_ms=0,retention_removal_ms=0;
    std::size_t retention_prefetch_peak=0;
    bool startup_succeeded=false,startup_cancelled=false;
    std::size_t startup_frames=0,startup_images=0,startup_objects=0;
    double startup_ms=0;
    std::string analysis_state,analysis_error;
    std::vector<double> analysis_means;
    std::uint64_t analysis_samples=0,analysis_object=0,analysis_tiles=0;
    double analysis_ms=0;
    std::size_t analysis_records=0;
    std::string saved_analysis_status;
    std::string tissue_state,tissue_error,tissue_input_sha256;
    std::size_t tissue_components=0,tissue_gpu_bytes=0;
    std::uint64_t tissue_samples=0,tissue_foreground_pixels=0;
    double tissue_ms=0;
    bool tissue_preview_ready=false;
    std::int32_t tissue_level=-1;
    double tissue_downsample=0;
    std::int64_t tissue_width=0,tissue_height=0;
    std::uint64_t tissue_cleaned_pixels=0,tissue_filled_pixels=0;
    double tissue_cleanup_ms=0;
    std::uint64_t replay_frame=0,replay_wait_frames=0,settle_frames=0;
    bool replay_complete=true,tiles_settled=false;
    std::size_t tissue_contour_components=0,tissue_contour_edges=0,tissue_contour_vertices=0,tissue_annotations_added=0;
    double tissue_contour_ms=0;
    bool tissue_contours_published=false;
    std::uint64_t saved_tissue_id=0;
    std::string saved_tissue_status;
    std::string cell_state="idle",cell_error;
    std::shared_ptr<const CellResult> cell_result;
    std::size_t cell_annotations_added=0,cell_gpu_bytes=0;
    bool cell_preview_ready=false,cells_published=false;
    std::size_t overlay_cache_bytes=0,overlay_cache_peak_bytes=0;
    std::uint64_t overlay_cache_builds=0,overlay_cache_hits=0,overlay_cache_fallbacks=0;
    std::uint64_t saved_cell_id=0;
    std::string saved_cell_status;
    std::string cell_table_state="idle",cell_table_error,cell_table_path;
    std::uint64_t cell_table_rows=0;
    std::size_t cell_table_bytes=0;
    double cell_table_capture_ms=0,cell_table_write_ms=0;
    std::string file_fingerprint_state="idle",file_fingerprint_error;
    std::shared_ptr<const FileFingerprint> file_fingerprint_result;
    std::uint64_t file_fingerprint_bytes=0;
    double file_fingerprint_ms=0;
    std::string measurement_table_state="idle",measurement_table_error;
    std::uint64_t measurement_table_rows=0;
    double measurement_table_capture_ms=0,measurement_table_write_ms=0;
    std::shared_ptr<const SourceFileAssociation> file_fingerprint_association;
    std::string stain_estimate_state="idle",stain_estimate_error;
    std::shared_ptr<const StainEstimateResult> stain_estimate_result;
    bool stain_estimate_applied=false;
    std::string batch_state="idle",batch_error;
    std::shared_ptr<const ProjectBatchReport> batch_result;
    std::uint64_t suspended_iterations=0;
    double suspended_work_ms=0,suspended_wait_ms=0;
    bool window_iconified=false;
    std::string overview_state="idle",overview_error;
    std::shared_ptr<const ThumbnailResult> overview_result;
    std::size_t overview_gpu_bytes=0,overview_peak_gpu_bytes=0;
    bool docked_workspace=false,layout_loaded=false,layout_saved=false;
    std::string layout_error;
    Rect browser_bounds{0,0,1,1},display_bounds{0,0,1,1},measurement_bounds{0,0,1,1},canvas_bounds{0,0,1,1};
    bool canvas_visible=true;
    double monitor_scale=1,framebuffer_scale=1,ui_scale=1;
    std::uint64_t font_rebuilds=0;
    std::uint64_t ui_stacked_fields=0,ui_wrapped_buttons=0,ui_overflow_items=0;
    double ui_max_overflow=0;
    std::string ui_overflow_label;
    std::string classifier_state="idle",classifier_error,classifier_io_state="idle",classifier_io_error;
    std::shared_ptr<const ForestModel> classifier_model;
    std::shared_ptr<const ObjectClassificationResult> classifier_result;
    std::size_t classified_objects=0;
    double classifier_capture_ms=0;
    std::uint64_t classifier_record_id=0;
    std::string pixel_training_state="idle",pixel_prediction_state="idle",pixel_error,pixel_io_state="idle",pixel_io_error;
    std::shared_ptr<const PixelClassifier> pixel_model;
    std::shared_ptr<const PixelTrainingTable> pixel_training_table;
    std::shared_ptr<const PixelPredictionResult> pixel_result;
    bool pixel_preview_ready=false,pixel_overlay_visible=false;
    std::size_t pixel_gpu_bytes=0,pixel_peak_gpu_bytes=0,pixel_uploaded_tiles=0;
    std::uint64_t pixel_preview_generation=0;
    double pixel_capture_ms=0;
    std::size_t pixel_catalog_inspected=0,pixel_catalog_max_step=0;
    bool pixel_catalog_complete=false;
    Rect canvas_content_bounds{0,0,1,1};
    std::string pixel_contour_state="idle";
    std::shared_ptr<const PixelContours> pixel_contours;
    std::vector<std::uint64_t> pixel_published_ids;
    bool pixel_contours_published=false;
    std::string pixel_archive_state="idle";
    std::uint64_t pixel_record_id=0,pixel_selected_record_id=0;
    std::shared_ptr<const PixelPredictionResult> pixel_historical_result;
    std::size_t pixel_record_catalog_max_step=0;
    bool pixel_record_catalog_complete=false;
    std::uint64_t replay_physical_wheel_events_ignored=0;
    std::shared_ptr<const PixelStreamExportStats> pixel_stream;
    std::shared_ptr<const PixelStreamReadStats> pixel_stream_read;
    std::size_t pixel_stream_read_gpu_bytes=0,pixel_stream_read_peak_gpu_bytes=0;
    bool pixel_stream_read_preview_ready=false;
    Rect pixel_stream_read_preview_bounds{0,0,0,0};
    std::uint64_t pixel_stream_record_id=0,pixel_stream_selected_record_id=0;
    std::size_t pixel_stream_record_catalog_max_step=0;
    bool pixel_stream_record_catalog_complete=false;
    std::string tma_error,tma_export_state,qptma_state;
    std::shared_ptr<const TmaDearrayResult> tma_dearray_result;
    std::size_t qptma_preview_cores=0,qptma_preview_missing=0,qptma_preview_max_step=0,qptma_export_bytes=0;
    std::vector<std::uint64_t> tma_created_ids;
    std::size_t tma_cores=0,tma_missing_cores=0,tma_catalog_max_step=0,tma_metadata_bytes=0;
    std::string tiff_state,raw_task_state,raw_error;
    std::shared_ptr<const RawRegionStatistics> raw_statistics;
    std::size_t tiff_view_changes=0,raw_export_bytes=0,raw_catalog_max_step=0;
    std::shared_ptr<const RawAnalysisResult> raw_analysis_result;
    std::string raw_analysis_state="idle",raw_csv_state="idle",raw_record_status;
    std::uint64_t raw_record_id=0,raw_csv_rows=0;
    std::size_t raw_records=0,raw_csv_bytes=0;
    std::uint64_t overview_source_pixel_limit=4*1024*1024;

    // Bounded recent rendered-loop samples, using the CPU timer's warmup
    // policy. Includes swap/capture and post-paint startup dispatch, while
    // suspended iterations and final shutdown are outside this measurement.
    double rendered_frame_wall_p95_ms=0,max_rendered_frame_wall_ms=0;
    std::size_t rendered_frame_wall_samples=0;

};
enum class GuiInputKind { Move, LeftDown, LeftUp, RightDown, RightUp, Wheel, RectangleKey, UndoKey, RedoKey, DeleteKey, EllipseKey, PolygonKey, PolylineKey, PointsKey, FinishKey, SelectAllKey, EscapeKey, VertexKey, HoleKey, InsertKey, BooleanKey, SaveKey, ExportKey, ImportKey, NextImageKey, PreviousImageKey, AddImageKey, RelinkImageKey, AutosaveKey, RecoverKey, CloseWindow, OpenProjectKey, NewProjectKey, RemoveImageKey, ReviewRecoveryKey, FindRecoveriesKey, StopRecoveryMaintenanceKey, AnalyzeKey, CancelAnalysisKey, KeepAnalysisKey, RecheckAnalysisKey, DetectTissueKey, CancelTissueKey, TraceTissueKey, KeepTissueContoursKey, KeepTissueKey, RecheckTissueKey, DetectCellsKey, KeepCellsKey, CancelCellsKey, KeepCellRunKey, RecheckCellRunKey, ExportCellTableKey, CancelCellTableKey, FingerprintFileKey, CancelFingerprintFileKey, ExportMeasurementTableKey, CancelMeasurementTableKey, EstimateStainsKey, ApplyStainEstimateKey, CancelStainEstimateKey, RunBatchKey, CancelBatchKey, ResizeWindow, MinimizeWindow, RestoreWindow, SetUiScale, ObjectClassifierKey, PixelClassifierKey, TmaKey, RawChannelsKey };
enum class GuiInputTarget { Canvas, Overview, ObjectList, Image, ProjectList, DisplayReset, DiscardRecoveryReplacement, KeepRecoveryClose, CancelClose, DiscardClose, SaveClose, SaveRecoveryReplacement, CancelRecoveryReplacement, CancelProjectLoad, ConfirmRemoveImage, CancelRemoveImage, ConfirmDiscardCheckpoint, CancelDiscardCheckpoint, Window, ResetLayout, SaveProject, TrainObjectClassifier, PredictObjectClasses, ApplyObjectClasses, CancelObjectClassifier, SaveClassifierModel, LoadClassifierModel, KeepClassifierTraining, UseClassifierRecord, TrainPixelClassifier, PredictPixelClasses, CancelPixelClassifier, SavePixelModel, LoadPixelModel, TogglePixelOverlay, TracePixelContours, PublishPixelContours, KeepPixelTraining, KeepPixelPrediction, UsePixelRecordModel, PreviewPixelRecord, RemovePixelRecord, ExportPixelStream, OpenPixelStreamBrowser, OpenPixelStreamFile, ReadPixelStreamBlock, PreviousPixelStreamBlock, NextPixelStreamBlock, VerifyPixelStreamFile, CancelPixelStreamRead, ClosePixelStreamFile, KeepPixelStreamLink, OpenPixelStreamRecord, RelinkPixelStreamRecord, RemovePixelStreamRecord, CreateTmaGrid, ToggleTmaMissing, SelectTmaPresent, ApplyTmaCase, ApplyTmaGeometry, TranslateTmaGrid, ClearTmaGrid, OpenTmaBatch, ExportTmaMetadata, CancelTmaMetadata, TmaCoreTab, TmaGridTab, TmaExportTab, ApplyTmaLabels, ApplyTiffView, AddTiffSeries, MeasureRawRegion, ExportRawRegion, CancelRawTask, RawDisplayTab, RawDataTab, RawResultsTab, KeepRawAnalysis, VerifyRawAnalysisRecord, RemoveRawAnalysisRecord, ExportRawMeasurements, CancelRawMeasurements, RawRecordsTab, OverviewBudgetIncrease, OverviewBudgetDecrease, LoadQptmaPreview, ApplyQptmaPreview, ExportQptma, CancelQptma, DiscardQptmaPreview, AcknowledgeQptmaLosses, TmaInsertRowBefore, TmaInsertRowAfter, TmaInsertColumnBefore, TmaInsertColumnAfter, TmaRemoveRow, TmaRemoveColumn, TmaDetectGrid };
enum class GuiInputWait { Immediate, TargetReady, StartupFinished, TissueReady, TissueContoursReady, TissueContoursPublished, TissueArchived, TissueVerified, CellsReady, CellsPublished, CellsArchived, CellsVerified, CellTableSaved, FileFingerprintReady, MeasurementTableSaved, StainsEstimated, StainsApplied, BatchFinished, WindowReady, ObjectClassifierTrained, ObjectClassifierPredicted, ObjectClassifierApplied, ClassifierModelSaved, ClassifierModelLoaded, ClassifierTrainingRetained, ClassifierRecordLoaded, PixelClassifierTrained, PixelClassifierPredicted, PixelPreviewReady, PixelModelSaved, PixelModelLoaded, PixelContoursReady, PixelContoursPublished, PixelRecordKept, PixelRecordModelLoaded, PixelHistoricalPreviewReady, PixelStreamFinished, PixelStreamReadReady, PixelStreamReadFinished, PixelStreamVerified, PixelStreamLinked , TmaGridReady, TmaMetadataSaved, TiffReady, TiffViewApplied, RawTaskFinished, RawNumpySaved, RawAnalysisKept, RawAnalysisVerified, RawMeasurementsSaved };
struct GuiInput {
    int frame = 0;
    GuiInputKind kind = GuiInputKind::Move;
    GuiInputTarget target = GuiInputTarget::Canvas;
    double x = .5, y = .5, wheel = 0;
    bool ctrl = false, shift = false;
    GuiInputWait wait_for=GuiInputWait::Immediate;
};
namespace detail {class ViewerOwner;}
struct GuiOptions {
    int width = 1440, height = 900;
    DisplaySettings display;
    std::vector<GuiInput> inputs;
    std::optional<Viewport> start_view;
    std::shared_ptr<Project> project;
    std::string interchange_path;
    std::string project_image_path;
    std::string autosave_path,recovery_path;
    std::string recovery_base_path;
    std::string project_open_path;
    // Deferred initial input. Requires an empty project; kind is image/project/recovery.
    std::string startup_path,startup_kind;
    std::uint64_t startup_image_id=0;
    double autosave_interval_seconds=60;
    std::size_t autosave_snapshot_budget=256*1024*1024;
    std::size_t save_snapshot_budget=256*1024*1024;
    bool recovery_retention_enabled=false;
    std::size_t recovery_keep_latest=10;
    double recovery_max_age_days=30;
    RoiSamplingPolicy roi_sampling_policy=RoiSamplingPolicy::GeometryCoverage,tissue_sampling_policy=RoiSamplingPolicy::GeometryCoverage;
    int analysis_kind=0; // 0: RGB optical-density means; 1: supplied-stain means.
    bool analysis_nucleus=false;
    std::vector<double> analysis_vectors={.65,.70,.29,.07,.99,.11};
    std::vector<double> analysis_white={255,255,255};
    int tissue_feature=0,tissue_connectivity=8;
    double tissue_threshold=.3;
    std::uint64_t tissue_min_pixels=1,tissue_max_pixels=0;
    int tissue_open_radius=0,tissue_close_radius=0;
    bool tissue_fill_holes=false;
    std::uint64_t tissue_max_hole_pixels=0;
    std::int32_t tissue_level=-1;
    double replay_timeout_seconds=5;
    bool settle_tiles=false;
    std::shared_ptr<const CellParameters> cell_parameters;
    std::shared_ptr<const GridCellSampling> cell_grid;
    std::shared_ptr<const GridCellParameters> grid_cell_parameters;
    std::optional<double> cell_min_nucleus_area_um2,cell_max_nucleus_area_um2;
    std::size_t overlay_cache_budget=64*1024*1024;
    std::string cell_table_path;
    bool cell_table_require_current=false;
    std::size_t cell_table_byte_limit=256*1024*1024,cell_table_snapshot_budget=256*1024*1024;
    bool cell_table_published_objects=false;
    bool cell_table_capture_calibration=false;
    std::string file_fingerprint_path,file_fingerprint_output;
    std::uint64_t file_fingerprint_byte_limit=std::uint64_t(1)<<40;
    std::size_t file_fingerprint_buffer_bytes=1024*1024;
    std::string measurement_table_path;
    int measurement_table_kind=0;
    bool measurement_table_require_current=false;
    std::size_t measurement_table_byte_limit=256*1024*1024,measurement_table_snapshot_budget=256*1024*1024;
    bool file_fingerprint_associate_source=false;
    std::shared_ptr<const StainEstimateParameters> stain_estimate_parameters;
    std::string batch_output_directory;
    std::shared_ptr<const ProjectBatchOptions> batch_options;
    bool batch_selected_objects=false;
    bool tma_open=false;
    bool raw_open=false;
    int raw_tab=0;
    std::string image_reader="auto",raw_numpy_path,raw_csv_path;
    std::uint64_t raw_record_id=0;
    bool raw_require_current=false;
    std::shared_ptr<const TiffOpenOptions> tiff_options;
    std::shared_ptr<const RawRegionRequest> raw_request;
    std::shared_ptr<const RawGridSampling> raw_grid;
    std::size_t raw_grid_work_bytes=256u*1024*1024;
    int tma_tab=0;
    std::shared_ptr<const TmaGridOptions> tma_options;
    std::shared_ptr<const TmaDearrayOptions> tma_dearray_options;
    std::string tma_case_id,tma_metadata_path,qptma_input_path,qptma_output_path;
    bool tma_case_summary=false,tma_allow_unversioned=false,tma_case_cohort=false;
    std::string tma_insert_label;bool tma_insert_cores=true,tma_insert_missing=false;
    std::string tma_measurements_path,tma_value_column,tma_weight_column;
    Point tma_translation;
    std::optional<Rect> tma_core_bounds;
    bool interchange_assume_pixels=false,interchange_allow_losses=false,qupath_ellipse_precision=false;
    bool interchange_nested_hierarchy=false;
    detail::ViewerOwner* viewer_owner=nullptr;
    bool docked_workspace=false,reset_layout=false;
    std::string layout_path;
    double ui_scale=1; // Multiplier of automatic monitor-aware UI size, 0.5..3.
    bool classifier_open=false,classifier_selected_only=false,classifier_unclassified_only=true;
    bool classifier_replace_model=false;
    double classifier_min_vote_fraction=.5;
    std::string classifier_model_path;
    std::uint64_t classifier_record_id=0;
    std::shared_ptr<const ObjectFeatureOptions> classifier_features;
    std::shared_ptr<const ForestParameters> classifier_parameters;
    std::shared_ptr<const ForestModel> classifier_model;
    bool pixel_open=false,pixel_selected_only=false,pixel_replace_model=false,pixel_show_overlay=true,pixel_viewport_only=false;
    std::uint64_t pixel_object_id=0;
    std::size_t pixel_preview_budget=32u*1024*1024;
    std::string pixel_model_path;
    std::shared_ptr<const PixelTrainingOptions> pixel_training_options;
    std::shared_ptr<const PixelPredictionOptions> pixel_prediction_options;
    std::shared_ptr<const TissueContourOptions> pixel_contour_options;
    std::shared_ptr<const ForestParameters> pixel_parameters;
    std::shared_ptr<const PixelClassifier> pixel_model;
    std::string pixel_stream_path;
    bool pixel_stream_replace=false;
    std::uint64_t pixel_stream_max_bytes=1ULL<<40;
    std::shared_ptr<const PixelStreamOptions> pixel_stream_options;
    bool pixel_stream_browser_open=false;
    std::string pixel_stream_read_path;
    std::uint64_t pixel_stream_read_index=0,pixel_stream_read_max_bytes=1ULL<<40;
    std::uint64_t overview_source_pixel_limit=4*1024*1024;
};
GuiReport run_gui(Document& document, int frame_limit = 0, const std::string& capture_path = "", const GuiOptions& options = {});
std::string default_workspace_layout_path();
GuiReport run_project_gui(std::shared_ptr<Project> project, int frame_limit = 0, const std::string& capture_path = "", const GuiOptions& options = {});
}
