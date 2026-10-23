#include "qupathxx/pixel_classifier_archive.hpp"
#include <nanobind/nanobind.h>
#include <stdexcept>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_pixel_archives(nb::module_& m) {
    using namespace qpx;
    nb::class_<PixelAnnotationSnapshot>(m,"PixelAnnotationSnapshot")
        .def_ro("id",&PixelAnnotationSnapshot::id).def_ro("revision",&PixelAnnotationSnapshot::revision).def_ro("kind",&PixelAnnotationSnapshot::kind)
        .def_ro("class_name",&PixelAnnotationSnapshot::class_name).def_ro("geometry",&PixelAnnotationSnapshot::geometry);
    nb::class_<PixelClassifierArchive>(m,"PixelClassifierArchive")
        .def_prop_ro("table",&PixelClassifierArchive::table).def_prop_ro("result",&PixelClassifierArchive::result).def_prop_ro("model",&PixelClassifierArchive::model)
        .def_prop_ro("annotations",[](const PixelClassifierArchive& a){return a.annotations();}).def_prop_ro("provenance",&PixelClassifierArchive::provenance)
        .def_prop_ro("purpose",&PixelClassifierArchive::purpose).def_prop_ro("sha256",&PixelClassifierArchive::sha256).def_prop_ro("payload_bytes",&PixelClassifierArchive::payload_bytes)
        .def("to_bytes",[](const PixelClassifierArchive& a){const auto& b=a.bytes();return nb::bytes(reinterpret_cast<const char*>(b.data()),b.size());})
        .def_static("from_bytes",[](nb::bytes value){if(value.size()<68 || value.size()>128u*1024*1024) throw std::invalid_argument("pixel archive encoding must be 68 bytes..128 MiB");const auto* p=reinterpret_cast<const std::uint8_t*>(value.c_str());std::vector<std::uint8_t> bytes(p,p+value.size());nb::gil_scoped_release release;return PixelClassifierArchive::from_bytes(bytes);},"data"_a);
    nb::class_<PixelClassifierRecord>(m,"PixelClassifierRecord")
        .def_ro("id",&PixelClassifierRecord::id).def_ro("captured_next_id",&PixelClassifierRecord::captured_next_id).def_ro("name",&PixelClassifierRecord::name).def_ro("archive",&PixelClassifierRecord::archive).def_prop_ro("payload_bytes",&PixelClassifierRecord::payload_bytes);
    m.def("archive_pixel_training",[](std::shared_ptr<const PixelTrainingTable> t,const PixelClassifier& model){return archive_pixel_training(std::move(t),model);},"table"_a,"model"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("archive_pixel_prediction",[](std::shared_ptr<const PixelPredictionResult> r){return archive_pixel_prediction(std::move(r));},"result"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def_prop_ro("pixel_classifier_records",&Document::pixel_classifier_records).def_prop_ro("pixel_classifier_record_count",&Document::pixel_classifier_record_count)
        .def("pixel_classifier_record",&Document::pixel_classifier_record,"id"_a)
        .def("keep_pixel_training",&Document::keep_pixel_training,"request"_a,"archive"_a,"name"_a="Pixel classifier training")
        .def("keep_pixel_prediction",&Document::keep_pixel_prediction,"request"_a,"archive"_a,"name"_a="Pixel classifier prediction")
        .def("remove_pixel_classifier_record",&Document::remove_pixel_classifier_record,"id"_a);
}
