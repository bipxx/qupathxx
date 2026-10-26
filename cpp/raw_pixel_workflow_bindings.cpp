#include "qupathxx/core.hpp"
#include "qupathxx/raw_pixel_archive.hpp"
#include "qupathxx/raw_pixel_tiled_training.hpp"
#include "qupathxx/raw_pixel_window_workflows.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
#include "raw_pixel_binding_utils.hpp"
#include <type_traits>
namespace nb=nanobind;using namespace nb::literals;
namespace {
using namespace qpx;using qpx::bindings::array;
template<class Result> void classification(nb::module_& m,const char* name) {
    auto c=nb::class_<Result>(m,name);
    c.def_prop_ro("class_counts",[](const Result& v){return v.class_counts();})
        .def_prop_ro("model_sha256",&Result::model_sha256).def_prop_ro("feature_sha256",&Result::feature_sha256)
        .def_prop_ro("input_sha256",&Result::input_sha256).def_prop_ro("sha256",&Result::sha256).def_prop_ro("estimated_work_bytes",&Result::estimated_work_bytes);
    if constexpr(std::is_same_v<Result,RawPixelClassification>) c.def_prop_ro("grid",[](const Result& v){return v.grid();});
    else c.def_prop_ro("window",[](const Result& v){return v.window();});
    auto shape=[](const Result& v) {if constexpr(std::is_same_v<Result,RawPixelClassification>) return std::pair<std::size_t,std::size_t>{v.grid().y().samples(),v.grid().x().samples()};else return std::pair<std::size_t,std::size_t>{v.window().core_height(),v.window().core_width()};};
    c.def("labels_numpy",[shape](nb::object self,bool copy){const auto& v=nb::cast<const Result&>(self);const auto s=shape(v);return array(v.labels(),{s.first,s.second},self,copy);},"copy"_a=false)
        .def("valid_numpy",[shape](nb::object self,bool copy){const auto& v=nb::cast<const Result&>(self);const auto s=shape(v);return array(v.valid(),{s.first,s.second},self,copy);},"copy"_a=false)
        .def("vote_fractions_numpy",[shape](nb::object self,bool copy){const auto& v=nb::cast<const Result&>(self);const auto s=shape(v);return array(v.vote_fractions(),{s.first,s.second},self,copy);},"copy"_a=false);
}
template<class Table> void training_table(nb::module_& m,const char* name) {
    auto c=nb::class_<Table>(m,name);
    c.def_prop_ro("recipe",[](const Table& v){return v.recipe();}).def_prop_ro("options",[](const Table& v){return v.options();})
        .def_prop_ro("classes",[](const Table& v){return v.classes();}).def_prop_ro("eligible_class_samples",[](const Table& v){return v.eligible_class_samples();})
        .def_prop_ro("sampled_class_counts",[](const Table& v){return v.sampled_class_counts();}).def_prop_ro("ambiguous_samples",&Table::ambiguous_samples)
        .def_prop_ro("row_count",&Table::row_count).def_prop_ro("estimated_work_bytes",&Table::estimated_work_bytes).def_prop_ro("retained_bytes",&Table::retained_bytes)
        .def_prop_ro("input_sha256",&Table::input_sha256).def_prop_ro("sha256",&Table::sha256)
        .def("values_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const Table&>(self);return array(v.values(),{v.row_count(),v.recipe().feature_schema().size()},self,copy);},"copy"_a=false)
        .def("labels_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const Table&>(self);return array(v.labels(),{v.row_count()},self,copy);},"copy"_a=false)
        .def("coordinates_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const Table&>(self);return array(v.coordinates(),{v.row_count(),2},self,copy);},"copy"_a=false)
        .def("annotation_ids_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const Table&>(self);return array(v.annotation_ids(),{v.row_count()},self,copy);},"copy"_a=false);
    if constexpr(std::is_same_v<Table,RawPixelTrainingTable>) c.def_prop_ro("grid",[](const Table& v){return v.grid();});
    else c.def_prop_ro("lattice",[](const Table& v){return v.lattice();}).def_prop_ro("tile_options",[](const Table& v){return v.tile_options();});
}
template<class Request> void training_request(nb::module_& m,const char* name) {
    auto c=nb::class_<Request>(m,name);
    if constexpr(std::is_same_v<Request,RawPixelTrainingRequest>) {
        c.def(nb::init<const Document&,const std::vector<std::uint64_t>&,ScientificGrid,ScientificRawOptions,RawPixelFeatureOptions,RawPixelTrainingOptions,bool>(),"document"_a,"annotation_ids"_a,"grid"_a,"selection"_a,"features"_a,"options"_a=RawPixelTrainingOptions{},"physical_sigmas"_a=false)
            .def_prop_ro("grid",[](const Request& v){return v.grid();});
    }else {
        c.def(nb::init<const Document&,const std::vector<std::uint64_t>&,ScientificRawLattice,ScientificRawOptions,RawPixelFeatureOptions,RawPixelTrainingOptions,RawPixelTileOptions,bool>(),"document"_a,"annotation_ids"_a,"lattice"_a,"selection"_a,"features"_a,"options"_a=RawPixelTrainingOptions{},"tiles"_a=RawPixelTileOptions{},"physical_sigmas"_a=false)
            .def_prop_ro("lattice",[](const Request& v){return v.lattice();}).def_prop_ro("tile_options",[](const Request& v){return v.tile_options();})
            .def_prop_ro("window_count",&Request::window_count).def_prop_ro("scanned_samples",&Request::scanned_samples);
    }
    c.def("matches",&Request::matches,"document"_a).def_static("replay",&Request::replay,"document"_a,"record_id"_a)
        .def_prop_ro("recipe",[](const Request& v){return v.recipe();}).def_prop_ro("options",[](const Request& v){return v.options();})
        .def_prop_ro("snapshot_bytes",&Request::snapshot_bytes).def_prop_ro("estimated_work_bytes",&Request::estimated_work_bytes);
}
template<class Job> void training_job(nb::module_& m,const char* name) {
    nb::class_<Job>(m,name).def(nb::init<>()).def("sample",&Job::sample,"request"_a).def("train",&Job::train,"request"_a,"parameters"_a=ForestParameters{})
        .def_prop_ro("busy",&Job::busy).def("cancel",&Job::cancel)
        .def("await_completion",&Job::await_completion,nb::call_guard<nb::gil_scoped_release>())
        .def("poll",&Job::poll,"document"_a).def("wait",[](Job& job,const Document& doc){return qpx::bindings::wait(job,doc);},"document"_a)
        .def_prop_ro("table",&Job::table).def_prop_ro("model",&Job::model).def("verify_result",&Job::verify_result,"document"_a,"record_id"_a);
}
template<class Job> void prediction_job(nb::module_& m,const char* name) {
    nb::class_<Job>(m,name).def(nb::init<>()).def("request",&Job::request,"request"_a).def_prop_ro("busy",&Job::busy).def("cancel",&Job::cancel)
        .def("await_completion",&Job::await_completion,nb::call_guard<nb::gil_scoped_release>())
        .def("poll",&Job::poll,"document"_a).def("wait",[](Job& job,const Document& doc){return qpx::bindings::wait(job,doc);},"document"_a)
        .def_prop_ro("result",&Job::result);
}
}
void bind_raw_pixel_workflows(nb::module_& m) {
    using namespace qpx;
    nb::class_<RawPixelTrainingOptions>(m,"RawPixelTrainingOptions").def(nb::init<>())
        .def_rw("max_samples",&RawPixelTrainingOptions::max_samples).def_rw("max_samples_per_class",&RawPixelTrainingOptions::max_samples_per_class)
        .def_rw("work_bytes",&RawPixelTrainingOptions::work_bytes).def_rw("seed",&RawPixelTrainingOptions::seed)
        .def_rw("max_scanned_samples",&RawPixelTrainingOptions::max_scanned_samples).def_rw("balanced",&RawPixelTrainingOptions::balanced).def("validate",&RawPixelTrainingOptions::validate);
    nb::class_<RawPixelTileOptions>(m,"RawPixelTileOptions").def(nb::init<>()).def_rw("width",&RawPixelTileOptions::width).def_rw("height",&RawPixelTileOptions::height).def_rw("max_windows",&RawPixelTileOptions::max_windows).def("validate",&RawPixelTileOptions::validate);
    nb::class_<RawPixelJobStats>(m,"RawPixelJobStats").def_ro("state",&RawPixelJobStats::state).def_ro("phase",&RawPixelJobStats::phase).def_ro("error",&RawPixelJobStats::error)
        .def_ro("generation",&RawPixelJobStats::generation).def_ro("completed_samples",&RawPixelJobStats::completed_samples).def_ro("total_samples",&RawPixelJobStats::total_samples).def_ro("elapsed_ms",&RawPixelJobStats::elapsed_ms);
    classification<RawPixelClassification>(m,"RawPixelClassification");classification<RawPixelWindowClassification>(m,"RawPixelWindowClassification");
    training_table<RawPixelTrainingTable>(m,"RawPixelTrainingTable");training_table<RawPixelTiledTrainingTable>(m,"RawPixelTiledTrainingTable");
    training_request<RawPixelTrainingRequest>(m,"RawPixelTrainingRequest");training_request<RawPixelTiledTrainingRequest>(m,"RawPixelTiledTrainingRequest");
    nb::class_<RawPixelPredictionRequest>(m,"RawPixelPredictionRequest")
        .def(nb::init<const Document&,const RawPixelClassifier&,ScientificGrid,std::uint64_t,double,std::size_t>(),"document"_a,"model"_a,"grid"_a,"object_id"_a=0,"min_vote_fraction"_a=0.,"work_bytes"_a=0)
        .def_static("replay",&RawPixelPredictionRequest::replay,"document"_a,"record_id"_a).def("matches",&RawPixelPredictionRequest::matches,"document"_a)
        .def_prop_ro("grid",[](const RawPixelPredictionRequest& v){return v.grid();}).def_prop_ro("model",[](const RawPixelPredictionRequest& v){return v.model();}).def_prop_ro("estimated_work_bytes",&RawPixelPredictionRequest::estimated_work_bytes);
    nb::class_<RawPixelWindowPredictionRequest>(m,"RawPixelWindowPredictionRequest")
        .def(nb::init<const Document&,const RawPixelClassifier&,ScientificRawWindow,std::uint64_t,double,std::size_t>(),"document"_a,"model"_a,"window"_a,"object_id"_a=0,"min_vote_fraction"_a=0.,"work_bytes"_a=0)
        .def("matches",&RawPixelWindowPredictionRequest::matches,"document"_a).def_prop_ro("window",[](const RawPixelWindowPredictionRequest& v){return v.window();})
        .def_prop_ro("model",[](const RawPixelWindowPredictionRequest& v){return v.model();}).def_prop_ro("estimated_work_bytes",&RawPixelWindowPredictionRequest::estimated_work_bytes);
    training_job<RawPixelTrainingJob>(m,"RawPixelTrainingJob");training_job<RawPixelTiledTrainingJob>(m,"RawPixelTiledTrainingJob");
    prediction_job<RawPixelPredictionJob>(m,"RawPixelPredictionJob");prediction_job<RawPixelWindowPredictionJob>(m,"RawPixelWindowPredictionJob");
    nb::borrow<nb::class_<RawPixelPredictionJob>>(m.attr("RawPixelPredictionJob")).def("verify_result",&RawPixelPredictionJob::verify_result,"document"_a,"record_id"_a);
    m.def("sample_raw_pixel_training",[](const RawPixelTrainingRequest& r){return sample_raw_pixel_training(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("sample_raw_pixel_tiled_training",[](const RawPixelTiledTrainingRequest& r){return sample_raw_pixel_tiled_training(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("train_raw_pixel_classifier",[](const RawPixelTrainingTable& t,ForestParameters p){return train_raw_pixel_classifier(t,p);},"table"_a,"parameters"_a=ForestParameters{},nb::call_guard<nb::gil_scoped_release>());
    m.def("train_raw_pixel_tiled_classifier",[](const RawPixelTiledTrainingTable& t,ForestParameters p){return train_raw_pixel_tiled_classifier(t,p);},"table"_a,"parameters"_a=ForestParameters{},nb::call_guard<nb::gil_scoped_release>());
    m.def("predict_raw_pixel_region",[](const RawPixelPredictionRequest& r){return predict_raw_pixel_region(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("predict_raw_pixel_window",[](const RawPixelWindowPredictionRequest& r){return predict_raw_pixel_window(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("classify_raw_pixels",[](const ScientificRawGrid& g,const RawPixelClassifier& c,double minimum,std::size_t work){return classify_raw_pixels(g,c,minimum,work);},"grid"_a,"model"_a,"min_vote_fraction"_a=0.,"work_bytes"_a=0,nb::call_guard<nb::gil_scoped_release>());
    m.def("classify_raw_pixel_window",[](const ScientificRawWindowData& g,const RawPixelClassifier& c,double minimum,std::size_t work){return classify_raw_pixel_window(g,c,minimum,work);},"data"_a,"model"_a,"min_vote_fraction"_a=0.,"work_bytes"_a=0,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelContours>(m,"RawPixelContours").def_prop_ro("contours",&RawPixelContours::contours).def_prop_ro("prediction_sha256",&RawPixelContours::prediction_sha256);
    nb::class_<RawPixelContourJob>(m,"RawPixelContourJob").def(nb::init<>()).def("request",&RawPixelContourJob::request,"request"_a,"result"_a,"options"_a=TissueContourOptions{})
        .def_prop_ro("busy",&RawPixelContourJob::busy).def("cancel",&RawPixelContourJob::cancel).def("await_completion",&RawPixelContourJob::await_completion,nb::call_guard<nb::gil_scoped_release>())
        .def("poll",&RawPixelContourJob::poll,"document"_a).def("wait",[](RawPixelContourJob& job,const Document& doc){return qpx::bindings::wait(job,doc);},"document"_a).def_prop_ro("result",&RawPixelContourJob::result);
    m.def("trace_raw_pixel_prediction",[](const RawPixelPredictionRequest& r,std::shared_ptr<const RawPixelClassification> p,TissueContourOptions o){return trace_raw_pixel_prediction(r,std::move(p),o);},"request"_a,"result"_a,"options"_a=TissueContourOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("publish_raw_pixel_contours",&publish_raw_pixel_contours,"document"_a,"request"_a,"contours"_a,"name_prefix"_a="Raw pixel");
    nb::class_<RawPixelArchive>(m,"RawPixelArchive")
        .def_prop_ro("table",&RawPixelArchive::table).def_prop_ro("tiled_table",&RawPixelArchive::tiled_table).def_prop_ro("result",&RawPixelArchive::result).def_prop_ro("model",&RawPixelArchive::model)
        .def_prop_ro("provenance",&RawPixelArchive::provenance).def_prop_ro("grid",[](const RawPixelArchive& v){return v.grid();}).def_prop_ro("lattice",&RawPixelArchive::lattice)
        .def_prop_ro("purpose",&RawPixelArchive::purpose).def_prop_ro("sha256",&RawPixelArchive::sha256).def_prop_ro("payload_bytes",&RawPixelArchive::payload_bytes)
        .def("to_bytes",[](const RawPixelArchive& v){return qpx::bindings::bytes(v.bytes());})
        .def_static("from_bytes",[](nb::bytes b){auto bytes=qpx::bindings::snapshot(b,128u*1024*1024);nb::gil_scoped_release release;return RawPixelArchive::from_bytes(bytes);},"data"_a);
    nb::class_<RawPixelRecord>(m,"RawPixelRecord").def_ro("id",&RawPixelRecord::id).def_ro("captured_next_id",&RawPixelRecord::captured_next_id).def_ro("name",&RawPixelRecord::name).def_ro("archive",&RawPixelRecord::archive).def_prop_ro("payload_bytes",&RawPixelRecord::payload_bytes);
    m.def("archive_raw_pixel_training",[](std::shared_ptr<const RawPixelTrainingTable> t,std::shared_ptr<const RawPixelClassifier> c){return archive_raw_pixel_training(std::move(t),std::move(c));},"table"_a,"model"_a=nb::none(),nb::call_guard<nb::gil_scoped_release>());
    m.def("archive_raw_pixel_tiled_training",[](std::shared_ptr<const RawPixelTiledTrainingTable> t,std::shared_ptr<const RawPixelClassifier> c){return archive_raw_pixel_tiled_training(std::move(t),std::move(c));},"table"_a,"model"_a=nb::none(),nb::call_guard<nb::gil_scoped_release>());
    m.def("archive_raw_pixel_prediction",[](const RawPixelPredictionRequest& r,std::shared_ptr<const RawPixelClassification> p){return archive_raw_pixel_prediction(r,std::move(p));},"request"_a,"result"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("load_raw_pixel_archive",[](const std::string& p){return load_raw_pixel_archive(p);},"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_raw_pixel_archive",[](const RawPixelArchive& v,const std::string& p,bool replace){save_raw_pixel_archive(v,p,replace);},"archive"_a,"path"_a,"replace_existing"_a=false,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def_prop_ro("raw_pixel_records",&Document::raw_pixel_records).def_prop_ro("raw_pixel_record_count",&Document::raw_pixel_record_count).def("raw_pixel_record",&Document::raw_pixel_record,"id"_a)
        .def("keep_raw_pixel_training",&Document::keep_raw_pixel_training,"request"_a,"archive"_a,"name"_a="Raw pixel training")
        .def("keep_raw_pixel_tiled_training",&Document::keep_raw_pixel_tiled_training,"request"_a,"archive"_a,"name"_a="Raw tiled pixel training")
        .def("keep_raw_pixel_prediction",&Document::keep_raw_pixel_prediction,"request"_a,"archive"_a,"name"_a="Raw pixel prediction")
        .def("remove_raw_pixel_record",&Document::remove_raw_pixel_record,"id"_a)
        .def("verify_raw_pixel_training",&Document::verify_raw_pixel_training,"id"_a,"request"_a,"table"_a,"model"_a=nb::none())
        .def("verify_raw_pixel_tiled_training",&Document::verify_raw_pixel_tiled_training,"id"_a,"request"_a,"table"_a,"model"_a=nb::none())
        .def("verify_raw_pixel_prediction",&Document::verify_raw_pixel_prediction,"id"_a,"request"_a,"result"_a);
}
