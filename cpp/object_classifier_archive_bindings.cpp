#include "qupathxx/object_classifier_archive.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_object_classifier_archives(nb::module_& m) {
    using namespace qpx;
    nb::class_<ObjectCellFeatureSource>(m,"ObjectCellFeatureSource")
        .def_ro("record_id",&ObjectCellFeatureSource::record_id).def_ro("object_id",&ObjectCellFeatureSource::object_id)
        .def_ro("object_revision",&ObjectCellFeatureSource::object_revision).def_ro("document_revision",&ObjectCellFeatureSource::document_revision).def_ro("cell_count",&ObjectCellFeatureSource::cell_count)
        .def_ro("producer",&ObjectCellFeatureSource::producer).def_ro("algorithm",&ObjectCellFeatureSource::algorithm).def_ro("input_sha256",&ObjectCellFeatureSource::input_sha256)
        .def_prop_ro("parameters",[](const ObjectCellFeatureSource& s){return s.parameters;})
        .def_ro("roi",&ObjectCellFeatureSource::roi).def_ro("level",&ObjectCellFeatureSource::level).def_ro("downsample",&ObjectCellFeatureSource::downsample)
        .def_ro("white",&ObjectCellFeatureSource::white).def_ro("stains",&ObjectCellFeatureSource::stains).def_ro("provenance",&ObjectCellFeatureSource::provenance);
    nb::class_<ObjectFeatureCellOrigin>(m,"ObjectFeatureCellOrigin")
        .def_ro("record_id",&ObjectFeatureCellOrigin::record_id).def_ro("label",&ObjectFeatureCellOrigin::label).def_ro("source",&ObjectFeatureCellOrigin::source);
    nb::borrow<nb::class_<ObjectFeatureTable>>(m.attr("ObjectFeatureTable"))
        .def("body_roi",&ObjectFeatureTable::body_roi,"row"_a,nb::rv_policy::copy)
        .def("nucleus_roi",&ObjectFeatureTable::nucleus_roi,"row"_a)
        .def("cell_origin",&ObjectFeatureTable::cell_origin,"row"_a).def("cell_feature",&ObjectFeatureTable::cell_feature,"row"_a);
    nb::class_<ObjectClassifierArchive>(m,"ObjectClassifierArchive")
        .def_prop_ro("table",&ObjectClassifierArchive::table).def_prop_ro("model",&ObjectClassifierArchive::model)
        .def_prop_ro("result",&ObjectClassifierArchive::result).def_prop_ro("purpose",&ObjectClassifierArchive::purpose)
        .def_prop_ro("sha256",&ObjectClassifierArchive::sha256).def_prop_ro("payload_bytes",&ObjectClassifierArchive::payload_bytes);
    nb::class_<ObjectClassifierRecord>(m,"ObjectClassifierRecord")
        .def_ro("id",&ObjectClassifierRecord::id).def_ro("name",&ObjectClassifierRecord::name).def_ro("archive",&ObjectClassifierRecord::archive)
        .def_ro("applied",&ObjectClassifierRecord::applied).def_ro("minimum_vote_fraction",&ObjectClassifierRecord::minimum_vote_fraction)
        .def_ro("changed_objects",&ObjectClassifierRecord::changed_objects).def_prop_ro("payload_bytes",&ObjectClassifierRecord::payload_bytes);
    m.def("archive_object_classifier",[](std::shared_ptr<const ObjectFeatureTable> table,const ForestModel& model,std::shared_ptr<const ObjectClassificationResult> result){return archive_object_classifier(std::move(table),model,std::move(result));},"table"_a,"model"_a,"result"_a=nb::none(),nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def_prop_ro("classifier_records",&Document::classifier_records).def_prop_ro("classifier_record_count",&Document::classifier_record_count)
        .def("classifier_record",&Document::classifier_record,"id"_a)
        .def("keep_classifier_training",&Document::keep_classifier_training,"request"_a,"archive"_a,"name"_a="Object classifier training")
        .def("remove_classifier_record",&Document::remove_classifier_record,"id"_a);
}
