#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_object_properties(nb::module_& m) {
    using namespace qpx;
    nb::class_<ObjectProperties>(m,"ObjectProperties")
        .def(nb::new_([](std::map<std::string,std::string> metadata,std::map<std::string,double> measurements,bool locked,std::optional<std::uint32_t> color,std::optional<double> probability) {
            return std::make_shared<ObjectProperties>(std::move(metadata),std::move(measurements),locked,color,probability);
        }),"metadata"_a=std::map<std::string,std::string>{},"measurements"_a=std::map<std::string,double>{},"locked"_a=false,"color_rgb"_a=nb::none(),"class_probability"_a=nb::none())
        .def_prop_ro("metadata",[](const ObjectProperties& p){return p.metadata();}).def_prop_ro("measurements",[](const ObjectProperties& p){return p.measurements();})
        .def_prop_ro("payload_bytes",&ObjectProperties::payload_bytes).def_prop_ro("locked",&ObjectProperties::locked)
        .def_prop_ro("color_rgb",&ObjectProperties::color_rgb).def_prop_ro("class_probability",&ObjectProperties::class_probability)
        .def("with_edit_state",&ObjectProperties::with_edit_state,"locked"_a=false,"color_rgb"_a=nb::none(),"class_probability"_a=nb::none());
    auto annotation=nb::borrow<nb::class_<Annotation>>(m.attr("Annotation"));
    annotation.def_prop_ro("properties",[](const Annotation& a){return a.properties;})
        .def_prop_ro("locked",&Annotation::locked).def_prop_ro("color_rgb",&Annotation::color_rgb).def_prop_ro("class_probability",&Annotation::class_probability);
    auto edit=nb::borrow<nb::class_<ObjectEdit>>(m.attr("ObjectEdit"));edit.def_static("properties",&ObjectEdit::properties,"id"_a,"properties"_a=nb::none());
    auto document=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    document.def_prop_ro("image_properties",&Document::image_properties)
        .def("set_image_properties",&Document::set_image_properties,"properties"_a=nb::none());
    document.def("set_object_properties",&Document::set_object_properties,"id"_a,"properties"_a=nb::none())
        .def("set_object_edit_state",&Document::set_object_edit_state,"id"_a,"locked"_a,"color_rgb"_a=nb::none(),"class_probability"_a=nb::none());
}
