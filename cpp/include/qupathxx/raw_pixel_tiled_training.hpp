#pragma once
#include "qupathxx/raw_pixel_workflows.hpp"

namespace qpx {
struct RawPixelTileOptions {
    std::int64_t width=64,height=64;
    std::size_t max_windows=100000;
    void validate() const;
};
// Captures only labelled annotations and a union of their candidate tile ranges.
// The lattice carries global phase; each window carries its local core shape.
class RawPixelTiledTrainingRequest {
public:
    struct Data;
    RawPixelTiledTrainingRequest(const Document&,const std::vector<std::uint64_t>& ids,
        ScientificRawLattice,ScientificRawOptions,RawPixelFeatureOptions,
        RawPixelTrainingOptions={},RawPixelTileOptions={},bool physical_sigmas=false);
    static RawPixelTiledTrainingRequest replay(const Document&,std::uint64_t record_id);
    bool matches(const Document&) const;
    const ScientificRawLattice& lattice() const;
    const RawPixelRecipe& recipe() const;
    const RawPixelTrainingOptions& options() const;
    const RawPixelTileOptions& tile_options() const;
    std::size_t window_count() const;
    std::uint64_t scanned_samples() const;
    std::size_t snapshot_bytes() const;
    // Admission ceiling, including fixed captures and one window's workspace.
    std::size_t estimated_work_bytes() const;
private:
    std::shared_ptr<const Data> data_;
    std::shared_ptr<ImageServer> source_;
    struct Authority;
    std::shared_ptr<const Authority> authority_;
    std::shared_ptr<const char> identity_;
    std::uint64_t revision_;
    bool publication_matches(const Document&) const;
    friend struct RawPixelTiledTrainingKernel;
    friend class RawPixelTiledTrainingJob;
    friend struct RawPixelArchiveCodec;
    friend struct RawPixelTrainingCheckpointCodec;
    friend class Document;
};
// Distinct from full-grid QXT1: global coordinates and sparse coverage must not
// acquire legacy archive semantics through an implicit conversion.
class RawPixelTiledTrainingTable {
public:
    const RawPixelRecipe& recipe() const;
    const ScientificRawLattice& lattice() const;
    const RawPixelTrainingOptions& options() const;
    const RawPixelTileOptions& tile_options() const;
    const std::vector<std::string>& classes() const;
    const std::vector<float>& values() const {return values_;}
    const std::vector<std::uint32_t>& labels() const {return labels_;}
    const std::vector<std::int64_t>& coordinates() const {return coordinates_;}
    const std::vector<std::uint64_t>& annotation_ids() const {return ids_;}
    const std::vector<std::uint64_t>& eligible_class_samples() const {return eligible_;}
    const std::vector<std::uint64_t>& sampled_class_counts() const {return counts_;}
    std::uint64_t ambiguous_samples() const {return ambiguous_;}
    std::size_t row_count() const {return labels_.size();}
    std::size_t retained_bytes() const;
    std::size_t estimated_work_bytes() const;
    const std::string& input_sha256() const {return input_sha_;}
    const std::string& sha256() const {return sha_;}
private:
    RawPixelTiledTrainingTable()=default;
    std::shared_ptr<const RawPixelTiledTrainingRequest::Data> snapshot_;
    std::vector<float> values_;
    std::vector<std::uint32_t> labels_;
    std::vector<std::int64_t> coordinates_;
    std::vector<std::uint64_t> ids_,eligible_,counts_;
    std::uint64_t ambiguous_=0;
    std::string input_sha_,sha_;
    friend struct RawPixelTiledTrainingKernel;
    friend struct RawPixelArchiveCodec;
    friend class RawPixelArchive;
    friend class RawPixelTiledTrainingRequest;
    friend class Document;
};
std::shared_ptr<const RawPixelTiledTrainingTable> sample_raw_pixel_tiled_training(const RawPixelTiledTrainingRequest&,const std::atomic<bool>* cancelled=nullptr);
RawPixelClassifier train_raw_pixel_tiled_classifier(const RawPixelTiledTrainingTable&,ForestParameters={},const std::atomic<bool>* cancelled=nullptr);
class RawPixelTiledTrainingJob {
public:
    RawPixelTiledTrainingJob();~RawPixelTiledTrainingJob();
    RawPixelTiledTrainingJob(const RawPixelTiledTrainingJob&)=delete;
    bool sample(const RawPixelTiledTrainingRequest&);
    bool train(const RawPixelTiledTrainingRequest&,ForestParameters={});
    bool busy() const;void cancel();void await_completion();
    RawPixelJobStats poll(const Document&);RawPixelJobStats wait(const Document&);
    std::shared_ptr<const RawPixelTiledTrainingTable> table() const;
    std::shared_ptr<const RawPixelClassifier> model() const;
    bool verify_result(const Document&,std::uint64_t record_id);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
    bool start(const RawPixelTiledTrainingRequest&,std::optional<ForestParameters>);
};
}
