#pragma once
#include "qupathxx/raw_pixel_file_jobs.hpp"
#include "qupathxx/raw_pixel_archive.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
#include "qupathxx/raw_pixel_stream_contours.hpp"
namespace qpx {
// Data-only settings copied by frontend submission; no live document or reader.
struct RawPixelTrainingSettings {
    std::optional<ScientificRawLattice> lattice;
    ScientificRawOptions selection;
    RawPixelFeatureOptions features;
    RawPixelTrainingOptions sampling;
    RawPixelTileOptions tiles;
    ForestParameters forest;
    bool physical_sigmas=false;
    std::string checkpoint_path;
    RawPixelTrainingCheckpointOptions checkpoint;
    void validate() const;
    std::size_t payload_bytes() const;
};
struct GuiRawPixelOptions {
    bool open=false,selected_only=false,show_overlay=true,replace_model=false;
    std::uint64_t object_id=0,record_id=0,read_index=0;
    std::size_t preview_budget=16u*1024*1024;
    RawPixelTrainingSettings training;
    std::optional<RawPixelClassifier> model;
    std::optional<ScientificRawWindow> preview;
    RawPixelStreamOptions stream;
    RawPixelStreamSaveOptions save;
    RawPixelStreamFileLimits read_limits;
    std::string model_path,stream_path;
    bool stream_contours=false;
    RawPixelStreamContourOptions stream_contour_options;
    TissueContourOptions contours;
    std::size_t contour_work_bytes=16u*1024*1024;
    std::string contour_name_prefix="Raw pixel";
};
struct GuiRawPixelReport {
    std::string state="idle",error;
    std::shared_ptr<const RawPixelClassifier> model,historical_model;
    std::shared_ptr<const RawPixelArchive> training;
    std::shared_ptr<const RawPixelTrainingCheckpoint> checkpoint;
    std::shared_ptr<const RawPixelWindowClassification> prediction,historical;
    std::shared_ptr<const RawPixelStreamLink> link;
    std::uint64_t training_record=0,stream_record=0;
    std::size_t preview_bytes=0;
    bool preview_ready=false;
    std::shared_ptr<const RawPixelContours> contours;
    std::shared_ptr<const RawPixelStreamContours> stream_contours;
    std::shared_ptr<const RawPixelFileContours> historical_contours;
    bool file_replay=false;
    std::vector<std::uint64_t> published_ids;
    bool contours_published=false;
    std::size_t contour_work_bytes=0,contour_preview_bytes=0;
    bool contour_preview_ready=false;
};
}
