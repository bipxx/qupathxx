#include "qupathxx/core.hpp"
#include "qupathxx/raw_pixel_file_jobs.hpp"
#include "raw_pixel_binding_utils.hpp"
#include <type_traits>
namespace nb=nanobind;using namespace nb::literals;
template<class Job> using PythonJob=qpx::bindings::JobHolder<Job>;
namespace {
template<class Job> auto owner_job(nb::module_& m,const char* name) {
    return nb::class_<PythonJob<Job>>(m,name).def(nb::init<>()).def("request",qpx::bindings::job_method<&Job::request>(),"request"_a,"path"_a,"options"_a=typename std::conditional_t<std::is_same_v<Job,qpx::RawPixelStreamSaveJob>,qpx::RawPixelStreamSaveOptions,qpx::RawPixelTrainingCheckpointOptions>{})
        .def_prop_ro("busy",qpx::bindings::job_method<&Job::busy>()).def("cancel",qpx::bindings::job_method<&Job::cancel>()).def("await_completion",qpx::bindings::job_method<&Job::await_completion>(),nb::call_guard<nb::gil_scoped_release>())
        .def("poll",qpx::bindings::job_method<&Job::poll>(),"document"_a).def("wait",[](PythonJob<Job>& job,const qpx::Document& doc){return qpx::bindings::wait(job,doc);},"document"_a).def("take",qpx::bindings::job_method<&Job::take>(),"document"_a);
}
}
void bind_raw_pixel_files(nb::module_& m) {
    using namespace qpx;
    nb::class_<RawPixelStreamRegion>(m,"RawPixelStreamRegion").def(nb::init<>())
        .def("__init__",[](RawPixelStreamRegion* p,std::int64_t x,std::int64_t y,std::int64_t w,std::int64_t h){new(p) RawPixelStreamRegion{x,y,w,h};},"first_x"_a,"first_y"_a,"width"_a,"height"_a)
        .def_rw("first_x",&RawPixelStreamRegion::first_x).def_rw("first_y",&RawPixelStreamRegion::first_y).def_rw("width",&RawPixelStreamRegion::width).def_rw("height",&RawPixelStreamRegion::height);
    nb::class_<RawPixelStreamOptions>(m,"RawPixelStreamOptions").def(nb::init<>())
        .def_rw("tile_width",&RawPixelStreamOptions::tile_width).def_rw("tile_height",&RawPixelStreamOptions::tile_height)
        .def_rw("max_samples",&RawPixelStreamOptions::max_samples).def_rw("max_windows",&RawPixelStreamOptions::max_windows)
        .def_rw("work_bytes",&RawPixelStreamOptions::work_bytes).def_rw("min_vote_fraction",&RawPixelStreamOptions::min_vote_fraction).def("validate",&RawPixelStreamOptions::validate);
    nb::class_<RawPixelTiledPredictionRequest>(m,"RawPixelTiledPredictionRequest")
        .def(nb::init<const Document&,const RawPixelClassifier&,ScientificRawLattice,RawPixelStreamOptions,std::optional<RawPixelStreamRegion>,std::uint64_t>(),"document"_a,"model"_a,"lattice"_a,"options"_a=RawPixelStreamOptions{},"region"_a=nb::none(),"object_id"_a=0)
        .def("matches",&RawPixelTiledPredictionRequest::matches,"document"_a).def_prop_ro("lattice",[](const RawPixelTiledPredictionRequest& v){return v.lattice();})
        .def_prop_ro("region",[](const RawPixelTiledPredictionRequest& v){return v.region();}).def_prop_ro("options",[](const RawPixelTiledPredictionRequest& v){return v.options();})
        .def_prop_ro("model",[](const RawPixelTiledPredictionRequest& v){return v.model();}).def("window",&RawPixelTiledPredictionRequest::window,"index"_a)
        .def_prop_ro("total_windows",&RawPixelTiledPredictionRequest::total_windows).def_prop_ro("total_samples",&RawPixelTiledPredictionRequest::total_samples)
        .def_prop_ro("estimated_work_bytes",&RawPixelTiledPredictionRequest::estimated_work_bytes).def_prop_ro("sha256",&RawPixelTiledPredictionRequest::sha256)
        .def("input_sha256",[](const RawPixelTiledPredictionRequest& v,std::uint64_t i){return v.input_sha256(i);},"index"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelPredictionStream>(m,"RawPixelPredictionStream").def(nb::init<const RawPixelTiledPredictionRequest&>(),"request"_a)
        .def("next",&RawPixelPredictionStream::next,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&RawPixelPredictionStream::cancel)
        .def_prop_ro("busy",&RawPixelPredictionStream::busy).def("matches",&RawPixelPredictionStream::matches,"document"_a)
        .def_prop_ro("state",&RawPixelPredictionStream::state).def_prop_ro("total_windows",&RawPixelPredictionStream::total_windows)
        .def_prop_ro("completed_windows",&RawPixelPredictionStream::completed_windows).def_prop_ro("completed_samples",&RawPixelPredictionStream::completed_samples)
        .def_prop_ro("valid_samples",&RawPixelPredictionStream::valid_samples).def_prop_ro("classified_samples",&RawPixelPredictionStream::classified_samples)
        .def_prop_ro("class_counts",&RawPixelPredictionStream::class_counts).def_prop_ro("estimated_work_bytes",&RawPixelPredictionStream::estimated_work_bytes)
        .def_prop_ro("input_sha256",&RawPixelPredictionStream::input_sha256).def_prop_ro("sha256",&RawPixelPredictionStream::sha256)
        .def("__iter__",[](RawPixelPredictionStream& v)->RawPixelPredictionStream& {return v;},nb::rv_policy::reference)
        .def("__next__",[](RawPixelPredictionStream& v){std::shared_ptr<const RawPixelWindowClassification> b;{nb::gil_scoped_release release;b=v.next();}if(!b) throw nb::stop_iteration();return b;});
    nb::class_<RawPixelStreamJobStats>(m,"RawPixelStreamJobStats")
        .def_ro("state",&RawPixelStreamJobStats::state).def_ro("phase",&RawPixelStreamJobStats::phase).def_ro("error",&RawPixelStreamJobStats::error)
        .def_ro("generation",&RawPixelStreamJobStats::generation).def_ro("total_windows",&RawPixelStreamJobStats::total_windows).def_ro("produced_windows",&RawPixelStreamJobStats::produced_windows)
        .def_ro("delivered_windows",&RawPixelStreamJobStats::delivered_windows).def_ro("total_samples",&RawPixelStreamJobStats::total_samples).def_ro("delivered_samples",&RawPixelStreamJobStats::delivered_samples)
        .def_ro("valid_samples",&RawPixelStreamJobStats::valid_samples).def_ro("classified_samples",&RawPixelStreamJobStats::classified_samples).def_ro("class_counts",&RawPixelStreamJobStats::class_counts).def_ro("elapsed_ms",&RawPixelStreamJobStats::elapsed_ms);
    nb::class_<PythonJob<RawPixelPredictionStreamJob>>(m,"RawPixelPredictionStreamJob").def(nb::init<>()).def("request",qpx::bindings::job_method<&RawPixelPredictionStreamJob::request>(),"request"_a)
        .def_prop_ro("busy",qpx::bindings::job_method<&RawPixelPredictionStreamJob::busy>()).def("cancel",qpx::bindings::job_method<&RawPixelPredictionStreamJob::cancel>())
        .def("await_completion",qpx::bindings::job_method<&RawPixelPredictionStreamJob::await_completion>(),nb::call_guard<nb::gil_scoped_release>())
        .def("poll",qpx::bindings::job_method<&RawPixelPredictionStreamJob::poll>(),"document"_a).def("wait",[](PythonJob<RawPixelPredictionStreamJob>& job,const Document& doc){return qpx::bindings::wait(job,doc);},"document"_a)
        .def("take",qpx::bindings::job_method<&RawPixelPredictionStreamJob::take>(),"document"_a).def_prop_ro("input_sha256",qpx::bindings::job_method<&RawPixelPredictionStreamJob::input_sha256>()).def_prop_ro("sha256",qpx::bindings::job_method<&RawPixelPredictionStreamJob::sha256>());
    nb::class_<RawPixelStreamFileLimits>(m,"RawPixelStreamFileLimits").def(nb::init<>()).def_rw("max_bytes",&RawPixelStreamFileLimits::max_bytes).def_rw("work_bytes",&RawPixelStreamFileLimits::work_bytes).def("validate",&RawPixelStreamFileLimits::validate);
    nb::class_<RawPixelStreamFileSummary>(m,"RawPixelStreamFileSummary")
        .def_ro("bytes",&RawPixelStreamFileSummary::bytes).def_ro("total_windows",&RawPixelStreamFileSummary::total_windows).def_ro("completed_windows",&RawPixelStreamFileSummary::completed_windows)
        .def_ro("valid_samples",&RawPixelStreamFileSummary::valid_samples).def_ro("classified_samples",&RawPixelStreamFileSummary::classified_samples).def_ro("class_counts",&RawPixelStreamFileSummary::class_counts)
        .def_ro("complete",&RawPixelStreamFileSummary::complete).def_ro("input_sha256",&RawPixelStreamFileSummary::input_sha256).def_ro("sha256",&RawPixelStreamFileSummary::sha256).def_ro("content_sha256",&RawPixelStreamFileSummary::content_sha256).def_ro("reused_windows",&RawPixelStreamFileSummary::reused_windows);
    nb::class_<RawPixelStreamSaveOptions>(m,"RawPixelStreamSaveOptions").def(nb::init<>()).def_rw("limits",&RawPixelStreamSaveOptions::limits).def_rw("resume_path",&RawPixelStreamSaveOptions::resume_path)
        .def_rw("max_new_windows",&RawPixelStreamSaveOptions::max_new_windows).def_rw("replace_existing",&RawPixelStreamSaveOptions::replace_existing);
    nb::class_<RawPixelStreamFile>(m,"RawPixelStreamFile")
        .def(nb::init<const std::string&,RawPixelStreamFileLimits>(),"path"_a,"limits"_a=RawPixelStreamFileLimits{},nb::call_guard<nb::gil_scoped_release>())
        .def_prop_ro("declared_summary",[](const RawPixelStreamFile& v){return v.declared_summary();}).def_prop_ro("model",[](const RawPixelStreamFile& v){return v.model();})
        .def_prop_ro("lattice",[](const RawPixelStreamFile& v){return v.lattice();}).def_prop_ro("region",[](const RawPixelStreamFile& v){return v.region();}).def_prop_ro("options",[](const RawPixelStreamFile& v){return v.options();})
        .def_prop_ro("provenance",&RawPixelStreamFile::provenance).def_prop_ro("verified",&RawPixelStreamFile::verified)
        .def("read_block",[](const RawPixelStreamFile& v,std::uint64_t i){return v.read_block(i);},"index"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("verify",[](const RawPixelStreamFile& v){v.verify();},nb::call_guard<nb::gil_scoped_release>())
        .def("check_unchanged",&RawPixelStreamFile::check_unchanged,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&RawPixelStreamFile::cancel)
        .def("link",&RawPixelStreamFile::link,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelStreamLink>(m,"RawPixelStreamLink")
        .def_prop_ro("path",&RawPixelStreamLink::path).def_prop_ro("lattice",[](const RawPixelStreamLink& v){return v.lattice();})
        .def_prop_ro("region",[](const RawPixelStreamLink& v){return v.region();}).def_prop_ro("options",[](const RawPixelStreamLink& v){return v.options();}).def_prop_ro("summary",[](const RawPixelStreamLink& v){return v.summary();})
        .def_prop_ro("model_sha256",&RawPixelStreamLink::model_sha256).def_prop_ro("request_sha256",&RawPixelStreamLink::request_sha256).def_prop_ro("header_sha256",&RawPixelStreamLink::header_sha256)
        .def_prop_ro("object_id",&RawPixelStreamLink::object_id).def_prop_ro("document_revision",&RawPixelStreamLink::document_revision).def_prop_ro("payload_bytes",&RawPixelStreamLink::payload_bytes)
        .def("same_content",&RawPixelStreamLink::same_content,"other"_a);
    nb::class_<RawPixelStreamRecord>(m,"RawPixelStreamRecord").def_ro("id",&RawPixelStreamRecord::id).def_ro("name",&RawPixelStreamRecord::name).def_ro("link",&RawPixelStreamRecord::link).def_prop_ro("payload_bytes",&RawPixelStreamRecord::payload_bytes);
    m.def("prepare_raw_pixel_stream_link",[](const std::string& p,RawPixelStreamFileLimits l){return prepare_raw_pixel_stream_link(p,l);},"path"_a,"limits"_a=RawPixelStreamFileLimits{},nb::call_guard<nb::gil_scoped_release>());
    m.def("save_raw_pixel_stream",[](const RawPixelTiledPredictionRequest& r,const std::string& p,RawPixelStreamSaveOptions o){return save_raw_pixel_stream(r,p,std::move(o));},"request"_a,"path"_a,"options"_a=RawPixelStreamSaveOptions{},nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelTrainingCheckpointLimits>(m,"RawPixelTrainingCheckpointLimits").def(nb::init<>()).def_rw("max_bytes",&RawPixelTrainingCheckpointLimits::max_bytes).def_rw("work_bytes",&RawPixelTrainingCheckpointLimits::work_bytes).def("validate",&RawPixelTrainingCheckpointLimits::validate);
    nb::class_<RawPixelTrainingCheckpoint>(m,"RawPixelTrainingCheckpoint")
        .def_static("from_bytes",[](nb::bytes b,RawPixelTrainingCheckpointLimits l){l.validate();auto bytes=qpx::bindings::snapshot(b,l.max_bytes);nb::gil_scoped_release release;return RawPixelTrainingCheckpoint::from_bytes(bytes,l);},"data"_a,"limits"_a=RawPixelTrainingCheckpointLimits{})
        .def("to_bytes",[](const RawPixelTrainingCheckpoint& v){return qpx::bindings::bytes(v.bytes());})
        .def_prop_ro("completed_windows",&RawPixelTrainingCheckpoint::completed_windows).def_prop_ro("total_windows",&RawPixelTrainingCheckpoint::total_windows).def_prop_ro("complete",&RawPixelTrainingCheckpoint::complete)
        .def_prop_ro("request_sha256",&RawPixelTrainingCheckpoint::request_sha256).def_prop_ro("prefix_sha256",&RawPixelTrainingCheckpoint::prefix_sha256);
    nb::class_<RawPixelTrainingCheckpointOptions>(m,"RawPixelTrainingCheckpointOptions").def(nb::init<>()).def_rw("limits",&RawPixelTrainingCheckpointOptions::limits).def_rw("resume_path",&RawPixelTrainingCheckpointOptions::resume_path)
        .def_rw("max_new_windows",&RawPixelTrainingCheckpointOptions::max_new_windows).def_rw("replace_existing",&RawPixelTrainingCheckpointOptions::replace_existing);
    nb::class_<RawPixelTrainingCheckpointResult>(m,"RawPixelTrainingCheckpointResult").def_ro("checkpoint",&RawPixelTrainingCheckpointResult::checkpoint).def_ro("table",&RawPixelTrainingCheckpointResult::table).def_ro("replayed_windows",&RawPixelTrainingCheckpointResult::replayed_windows);
    m.def("load_raw_pixel_training_checkpoint",[](const std::string& p,RawPixelTrainingCheckpointLimits l){return load_raw_pixel_training_checkpoint(p,l);},"path"_a,"limits"_a=RawPixelTrainingCheckpointLimits{},nb::call_guard<nb::gil_scoped_release>());
    m.def("save_raw_pixel_training_checkpoint",[](const RawPixelTiledTrainingRequest& r,const std::string& p,RawPixelTrainingCheckpointOptions o){return save_raw_pixel_training_checkpoint(r,p,std::move(o));},"request"_a,"path"_a,"options"_a=RawPixelTrainingCheckpointOptions{},nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelFileJobStats>(m,"RawPixelFileJobStats").def_ro("state",&RawPixelFileJobStats::state).def_ro("phase",&RawPixelFileJobStats::phase).def_ro("error",&RawPixelFileJobStats::error)
        .def_ro("generation",&RawPixelFileJobStats::generation).def_ro("total_windows",&RawPixelFileJobStats::total_windows).def_ro("completed_windows",&RawPixelFileJobStats::completed_windows).def_ro("elapsed_ms",&RawPixelFileJobStats::elapsed_ms);
    owner_job<RawPixelStreamSaveJob>(m,"RawPixelStreamSaveJob").def("take_link",qpx::bindings::job_method<&RawPixelStreamSaveJob::take_link>(),"document"_a);
    owner_job<RawPixelTrainingCheckpointJob>(m,"RawPixelTrainingCheckpointJob");
    nb::class_<PythonJob<RawPixelStreamFileReadJob>>(m,"RawPixelStreamFileReadJob").def(nb::init<>())
        .def("open",qpx::bindings::job_method<&RawPixelStreamFileReadJob::open>(),"path"_a,"limits"_a=RawPixelStreamFileLimits{})
        .def("open_link",qpx::bindings::job_method<&RawPixelStreamFileReadJob::open_link>(),"link"_a,"limits"_a=RawPixelStreamFileLimits{})
        .def("read",qpx::bindings::job_method<&RawPixelStreamFileReadJob::read>(),"file"_a,"index"_a).def_prop_ro("busy",qpx::bindings::job_method<&RawPixelStreamFileReadJob::busy>()).def("cancel",qpx::bindings::job_method<&RawPixelStreamFileReadJob::cancel>())
        .def("await_completion",qpx::bindings::job_method<&RawPixelStreamFileReadJob::await_completion>(),nb::call_guard<nb::gil_scoped_release>())
        .def("poll",qpx::bindings::job_method<&RawPixelStreamFileReadJob::poll>()).def("wait",qpx::bindings::job_method<&RawPixelStreamFileReadJob::wait>(),nb::call_guard<nb::gil_scoped_release>())
        .def("take_file",qpx::bindings::job_method<&RawPixelStreamFileReadJob::take_file>()).def("take_link",qpx::bindings::job_method<&RawPixelStreamFileReadJob::take_link>()).def("take_block",qpx::bindings::job_method<&RawPixelStreamFileReadJob::take_block>());
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def_prop_ro("raw_pixel_stream_records",&Document::raw_pixel_stream_records).def_prop_ro("raw_pixel_stream_record_count",&Document::raw_pixel_stream_record_count)
        .def("raw_pixel_stream_record",&Document::raw_pixel_stream_record,"id"_a).def("keep_raw_pixel_stream",&Document::keep_raw_pixel_stream,"link"_a,"name"_a="Raw pixel stream")
        .def("remove_raw_pixel_stream_record",&Document::remove_raw_pixel_stream_record,"id"_a).def("relink_raw_pixel_stream_record",&Document::relink_raw_pixel_stream_record,"id"_a,"link"_a);
}
