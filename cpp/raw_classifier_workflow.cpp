#include "qupathxx/raw_classifier_workflow.hpp"
#include "qupathxx/training_cohort.hpp"
#include "qupathxx/raw_pixel_training_checkpoint.hpp"
#include "qupathxx/raw_pixel_archive.hpp"
#include "qupathxx/raw_pixel_stream_file.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <filesystem>
#ifdef QUPATHXX_HAS_PROJECT
#include <nlohmann/json.hpp>
#endif
namespace qpx {
std::string run_raw_classifier_workflow(const std::string& input,const std::string& output,
    std::uint64_t image_id,std::uint32_t channels,const std::atomic<bool>* cancelled) {
#ifndef QUPATHXX_HAS_PROJECT
    throw std::runtime_error("raw classifier workflow requires project support");
#else
    workflow_path_argument(input);workflow_path_argument(output);
    if(!image_id)throw std::invalid_argument("image ID must be positive");
    ScientificRawOptions selection;selection.channels=channels;selection.validate();
    if(!has_classifier_training())throw std::runtime_error("raw classifier workflow requires the native training backend");
    auto poll=[&] {if(cancelled && cancelled->load())throw WorkflowCancelled("raw classifier workflow cancelled");};
    poll();
    namespace fs=std::filesystem;
    const auto directory=fs::absolute(fs::u8path(output));
    if(fs::exists(directory))throw WorkflowOutputExists("Choose a new workflow output directory");
    auto project=Project::open(input);
    struct Close {
        std::shared_ptr<Project> project;
        ~Close() {for(const auto& image:project->images())try {project->document(image.id)->server()->close();}catch(...) {}}
    } close{project};
    const auto doc=project->document(image_id);const auto& metadata=doc->metadata();
    ScientificRawLattice lattice(metadata.width,metadata.height,
        ScientificGridAxis(0,metadata.width,std::min<std::int64_t>(metadata.width,256)),
        ScientificGridAxis(0,metadata.height,std::min<std::int64_t>(metadata.height,256)));
    RawPixelTileOptions tiles;
    tiles.width=std::min<std::int64_t>(64,std::max<std::int64_t>(1,lattice.x().samples()/2));
    tiles.height=std::min<std::int64_t>(64,std::max<std::int64_t>(1,lattice.y().samples()/2));
    RawPixelFeatureOptions features;features.offsets.assign(channels,0.);features.scales.assign(channels,1.);
    const auto request=capture_labelled_raw_pixel_training(*doc,lattice,selection,features,{},tiles);
    fs::create_directories(directory.parent_path());
    if(!fs::create_directory(directory))throw WorkflowOutputExists("Workflow output directory already exists");
    const auto path=[&](const char* name){return (directory/name).u8string();};
    // Full coverage is the normal workflow. Partial/resume remains in the
    // native checkpoint API/CLI, without an artificial one-window round trip.
    const auto sampled=save_raw_pixel_training_checkpoint(request,path("training.qtc"),{},cancelled);
    if(!sampled.table || !sampled.checkpoint->complete())throw std::runtime_error("native training coverage did not complete");
    ForestParameters parameters;parameters.trees=16;parameters.max_depth=8;parameters.work_bytes=64u*1024*1024;
    poll();auto model=train_raw_pixel_tiled_classifier(*sampled.table,parameters,cancelled);
    save_raw_pixel_classifier(model,path("model.qrm"),false,cancelled);
    if(!request.matches(*doc))throw WorkflowInputChanged("training input changed before retention");
    doc->keep_raw_pixel_tiled_training(request,archive_raw_pixel_tiled_training(sampled.table,std::make_shared<const RawPixelClassifier>(model),cancelled));
    RawPixelStreamOptions stream;stream.tile_width=tiles.width;stream.tile_height=tiles.height;
    RawPixelTiledPredictionRequest prediction(*doc,model,lattice,stream);
    const auto summary=save_raw_pixel_stream(prediction,path("prediction.qws"),{},cancelled);
    if(!summary.complete)throw std::runtime_error("native prediction coverage did not complete");
    auto link=prepare_raw_pixel_stream_link(path("prediction.qws"),{},cancelled);
    if(!prediction.matches(*doc))throw WorkflowInputChanged("prediction input changed before retention");
    doc->keep_raw_pixel_stream(std::move(link));poll();
    project->save_new(path("analysis.qpx"),512ull*1024*1024,1ull<<30,cancelled);
    return nlohmann::json{{"project",path("analysis.qpx")},{"rows",sampled.table->row_count()},
        {"windows",summary.completed_windows},{"model_sha256",model.sha256()},{"stream_sha256",summary.sha256}}.dump();
#endif
}
}
