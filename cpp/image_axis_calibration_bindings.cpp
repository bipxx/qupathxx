#include "qupathxx/core.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_image_axis_calibration(nb::module_& m) {
    using namespace qpx;
    nb::class_<ImageAxisCalibration>(m,"ImageAxisCalibration")
        .def(nb::init<std::optional<double>,std::vector<double>>(),"z_spacing_um"_a=nb::none(),"timepoints_seconds"_a=std::vector<double>{})
        .def_prop_ro("z_spacing_um",&ImageAxisCalibration::z_spacing_um)
        .def_prop_ro("timepoints_seconds",&ImageAxisCalibration::timepoints_seconds)
        .def_prop_ro("payload_bytes",&ImageAxisCalibration::payload_bytes)
        .def("timepoint_seconds",&ImageAxisCalibration::timepoint_seconds,"index"_a);
    auto doc=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    doc.def_prop_ro("image_axis_calibration",[](const Document& d){return d.image_axis_calibration();})
        .def("set_image_axis_calibration",&Document::set_image_axis_calibration,"value"_a=nb::none())
        .def_prop_ro("z_spacing_um",&Document::z_spacing_um)
        .def("z_position_um",&Document::z_position_um,"z"_a)
        .def("timepoint_seconds",&Document::timepoint_seconds,"t"_a);
}
