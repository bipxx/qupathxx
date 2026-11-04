#include "qupathxx/project_source.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_project_source(nb::module_& m) {
    using namespace qpx;
    nb::class_<ProjectSourceReview>(m,"ProjectSourceReview")
        .def_prop_ro("image_id",&ProjectSourceReview::image_id).def_prop_ro("path",&ProjectSourceReview::path)
        .def_prop_ro("comparison",&ProjectSourceReview::comparison).def_prop_ro("coverage",&ProjectSourceReview::coverage)
        .def_prop_ro("association",&ProjectSourceReview::association).def_prop_ro("consumed",&ProjectSourceReview::consumed);
    auto project=nb::borrow<nb::class_<Project>>(m.attr("Project"));
    project.def("source_verification",&Project::source_verification,"image_id"_a)
        .def("review_source_verification",&Project::review_source_verification,"image_id"_a,"fingerprint"_a)
        .def("apply_source_review",&Project::apply_source_review,"review"_a,"accept_changed"_a=false)
        .def("clear_source_verification",&Project::clear_source_verification,"image_id"_a);
    auto kinds=nb::borrow<nb::enum_<GuiInputKind>>(m.attr("GuiInputKind"));kinds.value("REVIEW_SOURCE_VERIFICATION_KEY",GuiInputKind::ReviewSourceVerificationKey);
    auto targets=nb::borrow<nb::enum_<GuiInputTarget>>(m.attr("GuiInputTarget"));
    targets.value("APPLY_SOURCE_VERIFICATION",GuiInputTarget::ApplySourceVerification).value("ACCEPT_SOURCE_REPLACEMENT",GuiInputTarget::AcceptSourceReplacement);
}
