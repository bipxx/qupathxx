#include "job_binding_utils.hpp"
#include "qupathxx/object_classifier_archive.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>
void bind_object_classifier_archives(nanobind::module_&);
namespace nb=nanobind;using namespace nb::literals;
void bind_object_classifiers(nb::module_& m) {
    using namespace qpx;
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def("apply_classifications",&Document::apply_classifications,"assignments"_a,"label"_a="Classify objects");
    nb::class_<ObjectFeatureOptions>(m,"ObjectFeatureOptions").def(nb::init<>())
        .def_static("from_model",&ObjectFeatureOptions::from_model,"model"_a)
        .def_rw("calibrated",&ObjectFeatureOptions::calibrated).def_rw("nucleus_shape",&ObjectFeatureOptions::nucleus_shape)
        .def_rw("cell_rgb",&ObjectFeatureOptions::cell_rgb).def_rw("cytoplasm_rgb",&ObjectFeatureOptions::cytoplasm_rgb)
        .def_rw("snapshot_bytes",&ObjectFeatureOptions::snapshot_bytes).def("validate",&ObjectFeatureOptions::validate);
    nb::class_<ObjectFeatureRequest>(m,"ObjectFeatureRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,ObjectFeatureOptions>(),"document"_a,"object_ids"_a,"options"_a=ObjectFeatureOptions{})
        .def("matches",&ObjectFeatureRequest::matches).def_prop_ro("row_count",&ObjectFeatureRequest::row_count)
        .def_prop_ro("snapshot_bytes",&ObjectFeatureRequest::snapshot_bytes).def_prop_ro("document_revision",&ObjectFeatureRequest::document_revision)
        .def_prop_ro("capture_ms",&ObjectFeatureRequest::capture_ms).def_prop_ro("options",&ObjectFeatureRequest::options)
        .def_prop_ro("source_anchor",&ObjectFeatureRequest::source_anchor);
    nb::class_<ObjectFeatureTable>(m,"ObjectFeatureTable")
        .def_prop_ro("object_ids",&ObjectFeatureTable::object_ids).def_prop_ro("object_revisions",&ObjectFeatureTable::object_revisions)
        .def_prop_ro("object_kinds",&ObjectFeatureTable::object_kinds).def_prop_ro("options",&ObjectFeatureTable::options)
        .def_prop_ro("class_names",&ObjectFeatureTable::class_names).def_prop_ro("features",[](const ObjectFeatureTable& table){return table.features();})
        .def_prop_ro("sha256",&ObjectFeatureTable::sha256).def_prop_ro("source_anchor",&ObjectFeatureTable::source_anchor)
        .def("numpy",[](nb::object self,bool copy)->nb::object {
            const auto& table=nb::cast<const ObjectFeatureTable&>(self);const auto rows=table.object_ids().size(),columns=table.features().size();
            if(!copy) return nb::ndarray<nb::numpy,const float>(table.values().data(),{rows,columns},self).cast();
            auto storage=std::make_unique<std::vector<float>>(table.values());auto* ptr=storage->data();
            nb::capsule owner(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<float>*>(p);});storage.release();
            return nb::ndarray<nb::numpy,float>(ptr,{rows,columns},owner).cast();
        },"copy"_a=false);
    nb::class_<ObjectClassificationResult>(m,"ObjectClassificationResult")
        .def_prop_ro("table",&ObjectClassificationResult::table).def_prop_ro("model",&ObjectClassificationResult::model)
        .def_prop_ro("predictions",&ObjectClassificationResult::predictions).def_prop_ro("model_sha256",&ObjectClassificationResult::model_sha256)
        .def_prop_ro("elapsed_ms",&ObjectClassificationResult::elapsed_ms);
    m.def("extract_object_features",[](const ObjectFeatureRequest& r){return extract_object_features(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("train_object_forest",[](const ObjectFeatureTable& table,ForestParameters p){return train_object_forest(table,p);},"table"_a,"parameters"_a=ForestParameters{},nb::call_guard<nb::gil_scoped_release>());
    m.def("classify_objects",[](const ObjectFeatureRequest& r,const ForestModel& model){return classify_objects(r,model);},"request"_a,"model"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("apply_object_classification",&apply_object_classification,"document"_a,"request"_a,"result"_a,"min_vote_fraction"_a=0);
    nb::class_<ObjectClassifierStats>(m,"ObjectClassifierStats").def_ro("state",&ObjectClassifierStats::state).def_ro("phase",&ObjectClassifierStats::phase)
        .def_ro("error",&ObjectClassifierStats::error).def_ro("generation",&ObjectClassifierStats::generation)
        .def_ro("completed_rows",&ObjectClassifierStats::completed_rows).def_ro("total_rows",&ObjectClassifierStats::total_rows).def_ro("elapsed_ms",&ObjectClassifierStats::elapsed_ms);
    using PythonObjectClassifierJob=qpx::bindings::OwnerJobHolder<ObjectClassifierJob>;
    nb::class_<PythonObjectClassifierJob>(m,"ObjectClassifierJob").def(nb::init<>())
        .def("train",qpx::bindings::owner_job_method<&ObjectClassifierJob::train>(),"request"_a,"parameters"_a=ForestParameters{})
        .def("predict",qpx::bindings::owner_job_method<&ObjectClassifierJob::predict>(),"request"_a,"model"_a)
        .def("poll",qpx::bindings::owner_job_method<&ObjectClassifierJob::poll>(),"document"_a).def("wait",[](PythonObjectClassifierJob& job,const Document& doc){return qpx::bindings::wait(job,doc);},"document"_a)
        .def("cancel",qpx::bindings::owner_job_method<&ObjectClassifierJob::cancel,false>()).def_prop_ro("busy",qpx::bindings::owner_job_method<&ObjectClassifierJob::busy>())
        .def_prop_ro("model",qpx::bindings::owner_job_method<&ObjectClassifierJob::model>()).def_prop_ro("table",qpx::bindings::owner_job_method<&ObjectClassifierJob::table>()).def_prop_ro("result",qpx::bindings::owner_job_method<&ObjectClassifierJob::result>())
        .def("apply",qpx::bindings::owner_job_method<&ObjectClassifierJob::apply>(),"document"_a,"min_vote_fraction"_a=0,"retain_record"_a=true)
        .def_prop_ro("archive",qpx::bindings::owner_job_method<&ObjectClassifierJob::archive>()).def("retain_training",qpx::bindings::owner_job_method<&ObjectClassifierJob::retain_training>(),"document"_a,"name"_a="Object classifier training");
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"))
        .def_rw("classifier_open",&GuiOptions::classifier_open).def_rw("classifier_selected_only",&GuiOptions::classifier_selected_only)
        .def_rw("classifier_unclassified_only",&GuiOptions::classifier_unclassified_only).def_rw("classifier_replace_model",&GuiOptions::classifier_replace_model)
        .def_rw("classifier_min_vote_fraction",&GuiOptions::classifier_min_vote_fraction).def_rw("classifier_model_path",&GuiOptions::classifier_model_path)
        .def_rw("classifier_features",&GuiOptions::classifier_features).def_rw("classifier_parameters",&GuiOptions::classifier_parameters).def_rw("classifier_model",&GuiOptions::classifier_model).def_rw("classifier_record_id",&GuiOptions::classifier_record_id);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"))
        .def_ro("classifier_state",&GuiReport::classifier_state).def_ro("classifier_error",&GuiReport::classifier_error)
        .def_ro("classifier_io_state",&GuiReport::classifier_io_state).def_ro("classifier_io_error",&GuiReport::classifier_io_error)
        .def_ro("classifier_model",&GuiReport::classifier_model).def_ro("classifier_result",&GuiReport::classifier_result)
        .def_ro("classified_objects",&GuiReport::classified_objects).def_ro("classifier_capture_ms",&GuiReport::classifier_capture_ms).def_ro("classifier_record_id",&GuiReport::classifier_record_id);
    bind_object_classifier_archives(m);
}
