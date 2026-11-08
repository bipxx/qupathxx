#include "qupathxx/core.hpp"
#include "qupathxx/stains.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_image_analysis_settings(nb::module_& m) {
    using namespace qpx;
    nb::class_<ImageAnalysisSettings>(m,"ImageAnalysisSettings")
        .def(nb::init<std::string,const std::vector<ImageAnalysisSettings::Vector>&,ImageAnalysisSettings::Vector,std::string,std::vector<std::string>>(),"image_type"_a="UNSET","stains"_a=std::vector<ImageAnalysisSettings::Vector>{},"white"_a=ImageAnalysisSettings::Vector{255,255,255},"profile_name"_a="","stain_names"_a=std::vector<std::string>{})
        .def_prop_ro("image_type",&ImageAnalysisSettings::image_type).def_prop_ro("has_stains",&ImageAnalysisSettings::has_stains)
        .def_prop_ro("basis",&ImageAnalysisSettings::basis).def_prop_ro("white",&ImageAnalysisSettings::white).def_prop_ro("profile_name",&ImageAnalysisSettings::profile_name)
        .def_prop_ro("stain_names",&ImageAnalysisSettings::stain_names).def_prop_ro("residual_stain",&ImageAnalysisSettings::residual_stain)
        .def_prop_ro("rgb8_supported",&ImageAnalysisSettings::rgb8_supported).def_prop_ro("model_sha256",&ImageAnalysisSettings::model_sha256)
        .def_prop_ro("payload_bytes",&ImageAnalysisSettings::payload_bytes).def("transform",&ImageAnalysisSettings::transform);
    auto doc=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    doc.def_prop_ro("image_analysis_settings",[](const Document& d){return d.image_analysis_settings();})
        .def("set_image_analysis_settings",&Document::set_image_analysis_settings,"settings"_a=nb::none());
}
