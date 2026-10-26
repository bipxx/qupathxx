#include "raw_pixel_frontend.hpp"
#include "qupathxx/project.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "utf8_text.hpp"
#include <future>
#include <thread>
namespace qpx {
void RawPixelTrainingSettings::validate() const {
    selection.validate();auto f=features;if(f.offsets.empty()) f.offsets.assign(selection.channels,0);if(f.scales.empty()) f.scales.assign(selection.channels,1);f.validate(selection.channels);
    sampling.validate();tiles.validate();forest.validate();checkpoint.limits.validate();detail::bounded_utf8(checkpoint_path,32768);detail::bounded_utf8(checkpoint.resume_path,32768);
}
std::size_t RawPixelTrainingSettings::payload_bytes() const {return sizeof(*this)+features.offsets.capacity()*8+features.scales.capacity()*8+features.sigmas.capacity()*sizeof(RawPixelFeatureScale)+checkpoint_path.capacity()+checkpoint.resume_path.capacity();}
namespace detail {
ScientificRawLattice raw_pixel_frontend_lattice(const Document& d,const std::optional<ScientificRawLattice>& lattice) {if(lattice) return *lattice;const auto& m=d.metadata();return {m.width,m.height,{0,m.width,m.width},{0,m.height,m.height}};}
void guard_raw_pixel_frontend_output(const Project* project,const std::string& value) {
    const auto path=export_path(value);if(!project) return;
    for(const auto& source:{project->path(),project->recovery_source()}) if(!source.empty()) for(const auto* suffix:{"","-journal","-wal","-shm"}) guard_file_output(export_path(source+suffix),"",path);
    for(const auto& image:project->images()) {
        const auto source=project->document(image.id)->server()->reader_source();if(!source) continue;
        if(source->primary) guard_file_output(source->primary->path,source->primary->identity,path);
        for(const auto& file:source->companions) guard_file_output(file.path,file.identity,path);
    }
}
struct RawPixelFrontend::Impl {
    enum class Stage {Idle,Training,Checkpoint,Archive,Prediction,Save,Open,Read,LoadModel,SaveModel,ContourPrediction,Contours,StreamContours};
    struct Io {std::shared_ptr<const RawPixelArchive> archive;std::shared_ptr<const RawPixelClassifier> model;};
    Stage stage=Stage::Idle;RawPixelTrainingSettings settings;
    std::optional<RawPixelTiledTrainingRequest> training_request;
    RawPixelTiledTrainingJob training;RawPixelTrainingCheckpointJob checkpoint;
    std::optional<RawPixelPredictionRequest> contour_request;TissueContourOptions contour_options;
    RawPixelPredictionJob contour_prediction;RawPixelContourJob contours;
    std::optional<RawPixelTiledPredictionRequest> stream_contour_request;RawPixelStreamContourJob stream_contours;
    void clear_contours() {stream_contours.cancel();stream_contour_request.reset();report.stream_contours.reset();contour_prediction.cancel();contours.cancel();contour_request.reset();report.contours.reset();report.published_ids.clear();report.contours_published=false;report.contour_work_bytes=0;}
    RawPixelWindowPredictionJob prediction;RawPixelStreamSaveJob save;RawPixelStreamFileReadJob reader;
    std::shared_ptr<const RawPixelStreamFile> file;std::uint64_t index=0;
    std::future<Io> io;std::atomic<bool> cancelled{false};GuiRawPixelReport report;
    void start(Stage s) {if(stage!=Stage::Idle) throw std::logic_error("Wait for or cancel the current raw classifier operation");cancelled.store(false);clear_contours();stage=s;report.state="running";report.error.clear();}
    void fail(const std::string& state,const std::string& error) {stage=Stage::Idle;report.state=state;report.error=error;}
};
RawPixelFrontend::RawPixelFrontend():impl_(std::make_unique<Impl>()) {}
RawPixelFrontend::~RawPixelFrontend() {cancel();auto& s=*impl_;s.training.await_completion();s.checkpoint.await_completion();s.contour_prediction.await_completion();s.contours.await_completion();s.stream_contours.await_completion();s.prediction.await_completion();s.save.await_completion();s.reader.await_completion();if(s.io.valid()) s.io.wait();}
bool RawPixelFrontend::busy() const {return impl_->stage!=Impl::Stage::Idle;}
void RawPixelFrontend::cancel() {auto& s=*impl_;s.cancelled.store(true);s.training.cancel();s.checkpoint.cancel();s.contour_prediction.cancel();s.contours.cancel();s.stream_contours.cancel();s.prediction.cancel();s.save.cancel();s.reader.cancel();}
void RawPixelFrontend::wait(const Document& doc) {while(busy()) {poll(doc);if(busy()) std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
bool RawPixelFrontend::can_keep_training(const Document& doc) const {return impl_->training_request && impl_->report.training && impl_->training_request->matches(doc);}
void RawPixelFrontend::reset_records() {impl_->report.training_record=0;impl_->report.stream_record=0;impl_->clear_contours();}
GuiRawPixelReport RawPixelFrontend::report() const {return impl_->report;}
void RawPixelFrontend::discard_contours() {if(busy()) throw std::logic_error("Wait for the current operation");auto& s=*impl_;s.clear_contours();if(s.report.state=="contours_ready" || s.report.state=="applied") s.report.state="idle";}
void RawPixelFrontend::adopt(RawPixelClassifier model) {if(busy()) throw std::logic_error("raw classifier operation is busy");auto& s=*impl_;s.clear_contours();s.prediction.cancel();s.report.prediction.reset();s.training_request.reset();s.report.training.reset();s.report.model=std::make_shared<const RawPixelClassifier>(std::move(model));s.report.state="loaded";s.report.error.clear();}
void RawPixelFrontend::train(const Document& doc,const std::vector<std::uint64_t>& ids,RawPixelTrainingSettings settings) {
    settings.validate();if(!has_classifier_training()) throw std::runtime_error("Raw training requires the optional OpenCV backend");if(busy()) throw std::logic_error("raw classifier operation is busy");
    auto features=settings.features;if(features.offsets.empty()) features.offsets.assign(settings.selection.channels,0);if(features.scales.empty()) features.scales.assign(settings.selection.channels,1);
    RawPixelTiledTrainingRequest request(doc,ids,raw_pixel_frontend_lattice(doc,settings.lattice),settings.selection,features,settings.sampling,settings.tiles,settings.physical_sigmas);
    auto& s=*impl_;s.settings=std::move(settings);s.training_request=request;s.report.training.reset();s.report.checkpoint.reset();s.report.training_record=0;
    s.start(s.settings.checkpoint_path.empty()?Impl::Stage::Training:Impl::Stage::Checkpoint);
    try {if(s.stage==Impl::Stage::Training) s.training.train(request,s.settings.forest);else s.checkpoint.request(request,s.settings.checkpoint_path,s.settings.checkpoint);}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
void RawPixelFrontend::predict(const Document& doc,const RawPixelClassifier& model,ScientificRawWindow window,std::uint64_t object,double vote,std::size_t work) {
    RawPixelWindowPredictionRequest request(doc,model,std::move(window),object,vote,work);auto& s=*impl_;s.start(Impl::Stage::Prediction);s.report.prediction.reset();try {s.prediction.request(request);}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
void RawPixelFrontend::trace(const Document& doc,const RawPixelClassifier& model,ScientificGrid grid,TissueContourOptions options,std::uint64_t object,double vote,std::size_t work) {
    options.validate();if(!has_geos()) throw std::runtime_error("Raw contours require QUPATHXX_BUILD_GEOS=ON");
    RawPixelPredictionRequest request(doc,model,std::move(grid),object,vote,work);auto& s=*impl_;s.start(Impl::Stage::ContourPrediction);
    s.contour_request=request;s.contour_options=options;s.report.contour_work_bytes=request.estimated_work_bytes();
    try {s.contour_prediction.request(request);}catch(...) {s.fail("failed","Raw contour prediction could not start");throw;}
}
void RawPixelFrontend::trace_stream(const Document& doc,const RawPixelClassifier& model,ScientificRawLattice lattice,RawPixelStreamOptions stream,RawPixelStreamContourOptions options,std::optional<RawPixelStreamRegion> region,std::uint64_t object) {
    options.validate();if(!has_geos()) throw std::runtime_error("Raw streamed contours require QUPATHXX_BUILD_GEOS=ON");
    RawPixelTiledPredictionRequest request(doc,model,std::move(lattice),stream,region,object);auto& s=*impl_;s.start(Impl::Stage::StreamContours);
    s.stream_contour_request=request;s.report.contour_work_bytes=options.work_bytes;
    try {s.stream_contours.request(request,options);}catch(...) {s.stream_contour_request.reset();s.fail("failed","Raw streamed contours could not start");throw;}
}
const std::vector<std::uint64_t>& RawPixelFrontend::publish(Document& doc,const std::string& prefix) {
    auto& s=*impl_;if(busy() || s.report.contours_published) throw std::logic_error("Trace fresh raw contours before applying");
    std::vector<std::uint64_t> ids;
    if(s.report.stream_contours) {
        if(!s.stream_contour_request || s.stream_contours.poll(doc).state!="ready") throw std::logic_error("Trace fresh raw streamed contours before applying");
        ids=publish_raw_pixel_stream_contours(doc,*s.stream_contour_request,*s.report.stream_contours,prefix);
    } else {
        if(!s.contour_request || s.contours.poll(doc).state!="ready" || !s.report.contours) throw std::logic_error("Trace fresh raw contours before applying");
        ids=publish_raw_pixel_contours(doc,*s.contour_request,*s.report.contours,prefix);
    }
    s.report.published_ids=std::move(ids);s.report.contours_published=true;s.report.state="applied";s.report.error.clear();s.contours.cancel();s.stream_contours.cancel();return s.report.published_ids;
}
void RawPixelFrontend::save(const Document& doc,const RawPixelClassifier& model,ScientificRawLattice lattice,const std::string& path,RawPixelStreamOptions options,RawPixelStreamSaveOptions save,std::uint64_t object) {
    RawPixelTiledPredictionRequest request(doc,model,std::move(lattice),options,{},object);auto& s=*impl_;s.start(Impl::Stage::Save);s.report.link.reset();s.report.historical.reset();s.report.historical_model.reset();s.file.reset();s.report.stream_record=0;try {s.save.request(request,path,save);}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
void RawPixelFrontend::open(const std::string& path,RawPixelStreamFileLimits limits,std::uint64_t index,std::shared_ptr<const RawPixelStreamLink> link) {
    auto& s=*impl_;s.start(Impl::Stage::Open);s.index=index;s.file.reset();s.report.historical.reset();s.report.historical_model.reset();s.report.link.reset();s.report.stream_record=0;try {if(link) s.reader.open_link(std::move(link),limits);else s.reader.open(path,limits);}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
void RawPixelFrontend::load_model(const std::string& path) {
    auto& s=*impl_;s.start(Impl::Stage::LoadModel);try {s.io=std::async(std::launch::async,[path,&s] {Impl::Io out;out.model=std::make_shared<const RawPixelClassifier>(load_raw_pixel_classifier(path,&s.cancelled));return out;});}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
void RawPixelFrontend::save_model(const std::string& path,bool replace) {
    auto& s=*impl_;if(!s.report.model) throw std::logic_error("Train or load a raw model first");s.start(Impl::Stage::SaveModel);try {s.io=std::async(std::launch::async,[path,replace,model=s.report.model,&s] {save_raw_pixel_classifier(*model,path,replace,&s.cancelled);return Impl::Io{};});}catch(...) {s.fail("failed","Raw operation could not start");throw;}
}
std::uint64_t RawPixelFrontend::keep_training(Document& doc) {auto& s=*impl_;if(!s.training_request || !s.report.training) throw std::logic_error("No fresh raw training result");s.report.training_record=doc.keep_raw_pixel_tiled_training(*s.training_request,s.report.training);return s.report.training_record;}
std::uint64_t RawPixelFrontend::keep_link(Document& doc) {auto& s=*impl_;if(!s.report.link) throw std::logic_error("Verify a raw prediction file first");s.report.stream_record=doc.keep_raw_pixel_stream(s.report.link);return s.report.stream_record;}
void RawPixelFrontend::poll(const Document& doc) {
    auto& s=*impl_;using Stage=Impl::Stage;
    try {
        // Freshness continues after a result becomes ready, including previews
        // held while a workspace dialog temporarily blocks owner publication.
        const auto predicted=s.prediction.poll(doc);
        if(s.report.prediction && predicted.state!="ready") {s.report.prediction.reset();if(s.stage==Stage::Idle) s.fail(predicted.state,predicted.error);}
        s.contour_prediction.poll(doc);
        const auto traced=s.contours.poll(doc);
        if(s.report.contours && !s.report.contours_published && traced.state!="ready") {s.report.contours.reset();if(s.stage==Stage::Idle) s.fail(traced.state,traced.error);}
        const auto streamed=s.stream_contours.poll(doc);
        if(s.report.stream_contours && !s.report.contours_published && streamed.state!="ready") {s.report.stream_contours.reset();if(s.stage==Stage::Idle) s.fail(streamed.state,streamed.error);}
        if(s.stage==Stage::Idle) return;
        if(s.stage==Stage::StreamContours) {
            if(s.stream_contours.busy()) return;s.report.stream_contours=s.stream_contours.result();
            s.fail(s.report.stream_contours?"contours_ready":streamed.state,streamed.error);return;
        }
        if(s.stage==Stage::ContourPrediction) {
            const auto status=s.contour_prediction.poll(doc);if(s.contour_prediction.busy()) return;
            const auto prediction=s.contour_prediction.result();if(!prediction) {s.fail(status.state,status.error);return;}
            s.contours.request(*s.contour_request,prediction,s.contour_options);s.stage=Stage::Contours;
        }
        if(s.stage==Stage::Contours) {
            const auto status=s.contours.poll(doc);if(s.contours.busy()) return;s.report.contours=s.contours.result();
            s.fail(s.report.contours?"contours_ready":status.state,status.error);return;
        }
        if(s.stage==Stage::Checkpoint) {
            auto status=s.checkpoint.poll(doc);if(s.checkpoint.busy()) return;auto value=s.checkpoint.take(doc);if(!value) {status=s.checkpoint.poll(doc);s.fail(status.state,status.error);return;}
            s.report.checkpoint=value->checkpoint;if(!value->table) {s.fail("partial","");return;}
            s.training.train_table(*s.training_request,value->table,s.settings.forest);s.stage=Stage::Training;
        }
        if(s.stage==Stage::Training) {
            const auto status=s.training.poll(doc);if(s.training.busy()) return;if(status.state!="ready") {s.fail(status.state,status.error);return;}
            auto table=s.training.table();auto model=s.training.model();if(!table || !model) throw std::runtime_error("Raw training completed without a model");
            s.io=std::async(std::launch::async,[table,model,&s] {Impl::Io out;out.archive=archive_raw_pixel_tiled_training(table,model,&s.cancelled);out.model=model;return out;});s.stage=Stage::Archive;
        }
        if(s.stage==Stage::Archive || s.stage==Stage::LoadModel || s.stage==Stage::SaveModel) {
            if(s.io.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;auto out=s.io.get();if(s.cancelled.load()) {s.fail("cancelled","Raw classifier operation cancelled; committed files may remain");return;}
            if(s.stage==Stage::Archive && s.training.poll(doc).state!="ready") {s.fail("stale","Raw training source changed before delivery");return;}
            if(out.model) {s.report.model=std::move(out.model);s.report.prediction.reset();if(s.stage==Stage::LoadModel) {s.training_request.reset();s.report.training.reset();}}
            if(out.archive) s.report.training=std::move(out.archive);s.fail(s.stage==Stage::LoadModel?"loaded":s.stage==Stage::SaveModel?"saved":"trained","");return;
        }
        if(s.stage==Stage::Prediction) {const auto status=s.prediction.poll(doc);if(s.prediction.busy()) return;s.report.prediction=s.prediction.result();s.fail(s.report.prediction?"predicted":status.state,status.error);return;}
        if(s.stage==Stage::Save) {auto status=s.save.poll(doc);if(s.save.busy()) return;s.report.link=s.save.take_link(doc);if(!s.report.link) status=s.save.poll(doc);s.fail(s.report.link?(s.report.link->summary().complete?"saved":"partial"):status.state,status.error);return;}
        if(s.stage==Stage::Open || s.stage==Stage::Read) {
            auto status=s.reader.poll();if(s.reader.busy()) return;
            if(s.stage==Stage::Open) {s.file=s.reader.take_file();if(!s.file) {status=s.reader.poll();s.fail(status.state,status.error);return;}s.report.link=s.file->link();s.report.historical_model=std::make_shared<const RawPixelClassifier>(s.file->model());if(!s.report.link->summary().completed_windows) {s.fail("verified","");return;}s.reader.read(s.file,s.index);s.stage=Stage::Read;return;}
            s.report.historical=s.reader.take_block();if(!s.report.historical) status=s.reader.poll();s.fail(s.report.historical?"read":status.state,status.error);return;
        }
    }catch(const std::exception& e) {std::string message;try {message=e.what();bounded_utf8(message,8192);}catch(...) {message="Raw classifier operation failed";}s.fail(s.cancelled.load()?"cancelled":"failed",message);}
}
}
}
