#include "qupathxx/pixel_contours.hpp"
#include "qupathxx/pixel_stream.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();auto storage=std::make_unique<std::vector<T>>(values);auto* data=storage->data();nb::capsule capsule(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});storage.release();return nb::ndarray<nb::numpy,T>(data,shape,capsule).cast();
}
}
void bind_pixel_workflows(nb::module_& m) {
    using namespace qpx;const StainTransform identity({{1,0,0},{0,1,0}});
    nb::class_<PixelTrainingOptions>(m,"PixelTrainingOptions").def(nb::init<>())
        .def_rw("features",&PixelTrainingOptions::features).def_rw("level",&PixelTrainingOptions::level).def_rw("calibrated",&PixelTrainingOptions::calibrated).def_rw("balanced",&PixelTrainingOptions::balanced)
        .def_rw("tile_size",&PixelTrainingOptions::tile_size).def_rw("max_samples",&PixelTrainingOptions::max_samples).def_rw("max_samples_per_class",&PixelTrainingOptions::max_samples_per_class)
        .def_rw("work_bytes",&PixelTrainingOptions::work_bytes).def_rw("seed",&PixelTrainingOptions::seed).def_rw("max_scanned_pixels",&PixelTrainingOptions::max_scanned_pixels).def("validate",&PixelTrainingOptions::validate);
    nb::class_<PixelTrainingRequest>(m,"PixelTrainingRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,const StainTransform&,PixelTrainingOptions>(),"document"_a,"annotation_ids"_a,"transform"_a=identity,"options"_a=PixelTrainingOptions{})
        .def("matches",&PixelTrainingRequest::matches).def_prop_ro("options",&PixelTrainingRequest::options).def_prop_ro("resolution",&PixelTrainingRequest::resolution)
        .def_prop_ro("document_revision",&PixelTrainingRequest::document_revision).def_prop_ro("snapshot_bytes",&PixelTrainingRequest::snapshot_bytes).def_prop_ro("capture_ms",&PixelTrainingRequest::capture_ms).def_prop_ro("provenance",&PixelTrainingRequest::provenance);
    nb::class_<PixelTrainingTable>(m,"PixelTrainingTable")
        .def_prop_ro("algorithm",&PixelTrainingTable::algorithm).def_prop_ro("row_count",&PixelTrainingTable::row_count)
        .def_prop_ro("classes",[](const PixelTrainingTable& t){return t.classes();}).def_prop_ro("features",[](const PixelTrainingTable& t){return t.features();})
        .def_prop_ro("eligible_class_pixels",[](const PixelTrainingTable& t){return t.eligible_class_pixels();}).def_prop_ro("sampled_class_counts",[](const PixelTrainingTable& t){return t.sampled_class_counts();})
        .def_prop_ro("ambiguous_pixels",&PixelTrainingTable::ambiguous_pixels).def_prop_ro("scanned_pixels",&PixelTrainingTable::scanned_pixels).def_prop_ro("tiles",&PixelTrainingTable::tiles)
        .def_prop_ro("options",&PixelTrainingTable::options).def_prop_ro("resolution",&PixelTrainingTable::resolution).def_prop_ro("transform",&PixelTrainingTable::transform,nb::rv_policy::copy)
        .def_prop_ro("provenance",&PixelTrainingTable::provenance).def_prop_ro("sha256",&PixelTrainingTable::sha256).def_prop_ro("input_sha256",&PixelTrainingTable::input_sha256)
        .def_prop_ro("retained_bytes",&PixelTrainingTable::retained_bytes).def_prop_ro("estimated_work_bytes",&PixelTrainingTable::estimated_work_bytes).def_prop_ro("elapsed_ms",&PixelTrainingTable::elapsed_ms)
        .def("numpy",[](nb::object self,bool copy){const auto& t=nb::cast<const PixelTrainingTable&>(self);return array(t.values(),{t.row_count(),t.features().size()},self,copy);},"copy"_a=false)
        .def("labels_numpy",[](nb::object self,bool copy){const auto& t=nb::cast<const PixelTrainingTable&>(self);return array(t.labels(),{t.row_count()},self,copy);},"copy"_a=false)
        .def("coordinates_numpy",[](nb::object self,bool copy){const auto& t=nb::cast<const PixelTrainingTable&>(self);return array(t.coordinates(),{t.row_count(),2},self,copy);},"copy"_a=false)
        .def("annotation_ids_numpy",[](nb::object self,bool copy){const auto& t=nb::cast<const PixelTrainingTable&>(self);return array(t.annotation_ids(),{t.row_count()},self,copy);},"copy"_a=false);
    nb::class_<PixelTrainingStats>(m,"PixelTrainingStats").def_prop_ro("state",[](const PixelTrainingStats& s){return s.state;}).def_prop_ro("phase",[](const PixelTrainingStats& s){return s.phase;})
        .def_prop_ro("error",[](const PixelTrainingStats& s){return s.error;}).def_prop_ro("generation",[](const PixelTrainingStats& s){return s.generation;})
        .def_prop_ro("completed_tiles",[](const PixelTrainingStats& s){return s.completed_tiles;}).def_prop_ro("total_tiles",[](const PixelTrainingStats& s){return s.total_tiles;}).def_prop_ro("elapsed_ms",[](const PixelTrainingStats& s){return s.elapsed_ms;});
    nb::class_<PixelTrainingJob>(m,"PixelTrainingJob").def(nb::init<>()).def("train",&PixelTrainingJob::train,"request"_a,"parameters"_a=pixel_forest_parameters())
        .def_prop_ro("busy",&PixelTrainingJob::busy).def("cancel",&PixelTrainingJob::cancel).def("poll",&PixelTrainingJob::poll)
        .def("wait",&PixelTrainingJob::wait,nb::call_guard<nb::gil_scoped_release>()).def_prop_ro("table",&PixelTrainingJob::table).def_prop_ro("model",&PixelTrainingJob::model);
    m.def("pixel_forest_parameters",&pixel_forest_parameters);
    m.def("sample_pixel_training",[](const PixelTrainingRequest& r){return sample_pixel_training(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("train_pixel_classifier",[](const PixelTrainingTable& t,ForestParameters p){return train_pixel_classifier(t,p);},"table"_a,"parameters"_a=pixel_forest_parameters(),nb::call_guard<nb::gil_scoped_release>());
    m.def("pixel_classifier_level",&pixel_classifier_level,"document"_a,"classifier"_a);
    nb::class_<PixelPredictionOptions>(m,"PixelPredictionOptions").def(nb::init<>()).def_rw("tile_size",&PixelPredictionOptions::tile_size).def_rw("work_bytes",&PixelPredictionOptions::work_bytes)
        .def_rw("min_vote_fraction",&PixelPredictionOptions::min_vote_fraction).def("validate",&PixelPredictionOptions::validate);
    nb::class_<PixelPredictionRequest>(m,"PixelPredictionRequest")
        .def(nb::init<const Document&,std::uint64_t,const PixelClassifier&,PixelPredictionOptions,std::optional<Rect>>(),"document"_a,"object_id"_a,"classifier"_a,"options"_a=PixelPredictionOptions{},"window"_a=nb::none())
        .def("matches",&PixelPredictionRequest::matches).def_prop_ro("object_id",&PixelPredictionRequest::object_id).def_prop_ro("document_revision",&PixelPredictionRequest::document_revision)
        .def_prop_ro("level",&PixelPredictionRequest::level).def_prop_ro("downsample",&PixelPredictionRequest::downsample).def_prop_ro("level_bounds",&PixelPredictionRequest::level_bounds)
        .def_prop_ro("estimated_work_bytes",&PixelPredictionRequest::estimated_work_bytes).def_prop_ro("total_tiles",&PixelPredictionRequest::total_tiles).def_prop_ro("windowed",&PixelPredictionRequest::windowed);
    nb::class_<PixelPredictionResult>(m,"PixelPredictionResult")
        .def("matches",&PixelPredictionResult::matches).def_prop_ro("windowed",&PixelPredictionResult::windowed)
        .def_prop_ro("algorithm",&PixelPredictionResult::algorithm).def_prop_ro("width",&PixelPredictionResult::width).def_prop_ro("height",&PixelPredictionResult::height)
        .def_prop_ro("object_id",&PixelPredictionResult::object_id).def_prop_ro("level",&PixelPredictionResult::level).def_prop_ro("downsample",&PixelPredictionResult::downsample)
        .def_prop_ro("bounds",&PixelPredictionResult::bounds).def_prop_ro("level_bounds",&PixelPredictionResult::level_bounds).def_prop_ro("roi",&PixelPredictionResult::roi,nb::rv_policy::copy)
        .def_prop_ro("provenance",&PixelPredictionResult::provenance).def_prop_ro("model",&PixelPredictionResult::model,nb::rv_policy::copy).def_prop_ro("options",&PixelPredictionResult::options)
        .def_prop_ro("class_counts",[](const PixelPredictionResult& r){return r.class_counts();}).def_prop_ro("valid_pixels",&PixelPredictionResult::valid_pixels).def_prop_ro("classified_pixels",&PixelPredictionResult::classified_pixels)
        .def_prop_ro("estimated_work_bytes",&PixelPredictionResult::estimated_work_bytes).def_prop_ro("elapsed_ms",&PixelPredictionResult::elapsed_ms).def_prop_ro("sha256",&PixelPredictionResult::sha256).def_prop_ro("input_sha256",&PixelPredictionResult::input_sha256)
        .def("labels_numpy",[](nb::object self,bool copy){const auto& r=nb::cast<const PixelPredictionResult&>(self);return array(r.labels(),{std::size_t(r.height()),std::size_t(r.width())},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy){const auto& r=nb::cast<const PixelPredictionResult&>(self);return array(r.valid(),{std::size_t(r.height()),std::size_t(r.width())},self,copy);},"copy"_a=false)
        .def("vote_fractions_numpy",[](nb::object self,bool copy){const auto& r=nb::cast<const PixelPredictionResult&>(self);return array(r.vote_fractions(),{std::size_t(r.height()),std::size_t(r.width())},self,copy);},"copy"_a=false);
    m.def("predict_pixel_region",[](const PixelPredictionRequest& r){return predict_pixel_region(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<PixelContourJob>(m,"PixelContourJob").def(nb::init<>()).def("request",&PixelContourJob::request,"request"_a,"result"_a,"options"_a=TissueContourOptions{})
        .def_prop_ro("busy",&PixelContourJob::busy).def("cancel",&PixelContourJob::cancel).def("poll",&PixelContourJob::poll)
        .def("wait",&PixelContourJob::wait,nb::call_guard<nb::gil_scoped_release>()).def_prop_ro("result",&PixelContourJob::result);
    nb::class_<PixelContours>(m,"PixelContours").def_prop_ro("contours",&PixelContours::contours).def_prop_ro("prediction_sha256",&PixelContours::prediction_sha256);
    m.def("trace_pixel_prediction",[](std::shared_ptr<const PixelPredictionResult> result,TissueContourOptions options){return trace_pixel_prediction(std::move(result),options);},"result"_a,"options"_a=TissueContourOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("publish_pixel_contours",&publish_pixel_contours,"document"_a,"request"_a,"contours"_a,"name_prefix"_a="Pixel");
    nb::class_<PixelPredictionJob>(m,"PixelPredictionJob").def(nb::init<>()).def("request",&PixelPredictionJob::request).def_prop_ro("busy",&PixelPredictionJob::busy).def("cancel",&PixelPredictionJob::cancel)
        .def("poll",&PixelPredictionJob::poll).def("wait",&PixelPredictionJob::wait,nb::call_guard<nb::gil_scoped_release>()).def_prop_ro("result",&PixelPredictionJob::result);
    nb::class_<PixelStreamOptions>(m,"PixelStreamOptions").def(nb::init<>()).def_rw("prediction",&PixelStreamOptions::prediction).def_rw("block_size",&PixelStreamOptions::block_size)
        .def_rw("max_pixels",&PixelStreamOptions::max_pixels).def_rw("max_blocks",&PixelStreamOptions::max_blocks).def("validate",&PixelStreamOptions::validate);
    nb::class_<PixelPredictionStream>(m,"PixelPredictionStream")
        .def(nb::init<const Document&,std::uint64_t,const PixelClassifier&,PixelStreamOptions,std::optional<Rect>>(),"document"_a,"object_id"_a,"classifier"_a,"options"_a=PixelStreamOptions{},"window"_a=nb::none())
        .def("next",&PixelPredictionStream::next,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&PixelPredictionStream::cancel).def("matches",&PixelPredictionStream::matches)
        .def("__iter__",[](PixelPredictionStream& stream)->PixelPredictionStream& {return stream;},nb::rv_policy::reference)
        .def("__next__",[](PixelPredictionStream& stream) {std::shared_ptr<const PixelPredictionResult> result;{nb::gil_scoped_release release;result=stream.next();}if(!result) throw nb::stop_iteration();return result;})
        .def_prop_ro("state",&PixelPredictionStream::state).def_prop_ro("busy",&PixelPredictionStream::busy).def_prop_ro("level_bounds",&PixelPredictionStream::level_bounds)
        .def_prop_ro("level",&PixelPredictionStream::level).def_prop_ro("downsample",&PixelPredictionStream::downsample).def_prop_ro("windowed",&PixelPredictionStream::windowed)
        .def_prop_ro("object_id",&PixelPredictionStream::object_id).def_prop_ro("document_revision",&PixelPredictionStream::document_revision)
        .def_prop_ro("total_blocks",&PixelPredictionStream::total_blocks).def_prop_ro("completed_blocks",&PixelPredictionStream::completed_blocks)
        .def_prop_ro("valid_pixels",&PixelPredictionStream::valid_pixels).def_prop_ro("classified_pixels",&PixelPredictionStream::classified_pixels).def_prop_ro("class_counts",&PixelPredictionStream::class_counts)
        .def_prop_ro("estimated_work_bytes",&PixelPredictionStream::estimated_work_bytes);
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"))
        .def_rw("pixel_open",&GuiOptions::pixel_open).def_rw("pixel_selected_only",&GuiOptions::pixel_selected_only).def_rw("pixel_replace_model",&GuiOptions::pixel_replace_model).def_rw("pixel_show_overlay",&GuiOptions::pixel_show_overlay).def_rw("pixel_viewport_only",&GuiOptions::pixel_viewport_only)
        .def_rw("pixel_object_id",&GuiOptions::pixel_object_id).def_rw("pixel_preview_budget",&GuiOptions::pixel_preview_budget).def_rw("pixel_model_path",&GuiOptions::pixel_model_path)
        .def_rw("pixel_contour_options",&GuiOptions::pixel_contour_options).def_rw("pixel_training_options",&GuiOptions::pixel_training_options).def_rw("pixel_prediction_options",&GuiOptions::pixel_prediction_options).def_rw("pixel_parameters",&GuiOptions::pixel_parameters).def_rw("pixel_model",&GuiOptions::pixel_model);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"))
        .def_ro("pixel_contour_state",&GuiReport::pixel_contour_state).def_ro("pixel_contours",&GuiReport::pixel_contours).def_ro("pixel_published_ids",&GuiReport::pixel_published_ids).def_ro("pixel_contours_published",&GuiReport::pixel_contours_published)
        .def_ro("pixel_archive_state",&GuiReport::pixel_archive_state).def_ro("pixel_record_id",&GuiReport::pixel_record_id).def_ro("pixel_selected_record_id",&GuiReport::pixel_selected_record_id).def_ro("pixel_historical_result",&GuiReport::pixel_historical_result).def_ro("pixel_record_catalog_max_step",&GuiReport::pixel_record_catalog_max_step).def_ro("pixel_record_catalog_complete",&GuiReport::pixel_record_catalog_complete)
        .def_ro("pixel_training_state",&GuiReport::pixel_training_state).def_ro("pixel_prediction_state",&GuiReport::pixel_prediction_state).def_ro("pixel_error",&GuiReport::pixel_error).def_ro("pixel_io_state",&GuiReport::pixel_io_state).def_ro("pixel_io_error",&GuiReport::pixel_io_error)
        .def_ro("pixel_model",&GuiReport::pixel_model).def_ro("pixel_training_table",&GuiReport::pixel_training_table).def_ro("pixel_result",&GuiReport::pixel_result)
        .def_ro("pixel_preview_ready",&GuiReport::pixel_preview_ready).def_ro("pixel_overlay_visible",&GuiReport::pixel_overlay_visible).def_ro("pixel_gpu_bytes",&GuiReport::pixel_gpu_bytes).def_ro("pixel_peak_gpu_bytes",&GuiReport::pixel_peak_gpu_bytes)
        .def_ro("pixel_uploaded_tiles",&GuiReport::pixel_uploaded_tiles).def_ro("pixel_preview_generation",&GuiReport::pixel_preview_generation).def_ro("pixel_capture_ms",&GuiReport::pixel_capture_ms)
        .def_ro("pixel_catalog_inspected",&GuiReport::pixel_catalog_inspected).def_ro("pixel_catalog_max_step",&GuiReport::pixel_catalog_max_step).def_ro("pixel_catalog_complete",&GuiReport::pixel_catalog_complete).def_ro("canvas_content_bounds",&GuiReport::canvas_content_bounds);

}
