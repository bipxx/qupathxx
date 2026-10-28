#include "job_binding_utils.hpp"
#include "qupathxx/stain_estimation.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_stain_estimation(nb::module_& m) {
    using namespace qpx;
    nb::class_<StainEstimateParameters>(m,"StainEstimateParameters").def(nb::init<>())
        .def_rw("min_od",&StainEstimateParameters::min_od).def_rw("trim_quantile",&StainEstimateParameters::trim_quantile).def_rw("min_angle_degrees",&StainEstimateParameters::min_angle_degrees).def_rw("max_residual_fraction",&StainEstimateParameters::max_residual_fraction)
        .def_rw("sample_limit",&StainEstimateParameters::sample_limit).def_rw("min_samples",&StainEstimateParameters::min_samples).def_rw("tile_size",&StainEstimateParameters::tile_size).def_rw("work_bytes",&StainEstimateParameters::work_bytes)
        .def_rw("seed",&StainEstimateParameters::seed).def_rw("max_read_pixels",&StainEstimateParameters::max_read_pixels).def_rw("level",&StainEstimateParameters::level).def("validate",&StainEstimateParameters::validate);
    nb::class_<StainEstimateRequest>(m,"StainEstimateRequest").def(nb::init<const Document&,std::uint64_t,StainEstimateParameters,ColorVector,bool>(),"document"_a,"object_id"_a,"parameters"_a=StainEstimateParameters{},"white"_a=ColorVector{255,255,255},"nucleus"_a=false)
        .def("matches",&StainEstimateRequest::matches).def_prop_ro("object_id",&StainEstimateRequest::object_id).def_prop_ro("total_tiles",&StainEstimateRequest::total_tiles)
        .def_prop_ro("parameters",&StainEstimateRequest::parameters,nb::rv_policy::copy).def_prop_ro("provenance",&StainEstimateRequest::provenance)
        .def_prop_ro("estimated_work_bytes",&StainEstimateRequest::estimated_work_bytes).def_prop_ro("level",&StainEstimateRequest::level).def_prop_ro("downsample",&StainEstimateRequest::downsample);
    nb::class_<StainEstimateResult>(m,"StainEstimateResult").def_ro("algorithm",&StainEstimateResult::algorithm).def_ro("input_sha256",&StainEstimateResult::input_sha256).def_ro("model_sha256",&StainEstimateResult::model_sha256)
        .def_ro("stains",&StainEstimateResult::stains).def_ro("white",&StainEstimateResult::white).def_ro("eigenvalues",&StainEstimateResult::eigenvalues).def_prop_ro("parameters",[](const StainEstimateResult& r){return r.parameters;})
        .def_ro("roi",&StainEstimateResult::roi).def_ro("provenance",&StainEstimateResult::provenance).def_ro("samples",&StainEstimateResult::samples).def_ro("eligible",&StainEstimateResult::eligible).def_ro("sampled",&StainEstimateResult::sampled).def_ro("tiles",&StainEstimateResult::tiles)
        .def_ro("estimated_work_bytes",&StainEstimateResult::estimated_work_bytes).def_ro("level",&StainEstimateResult::level).def_ro("downsample",&StainEstimateResult::downsample).def_ro("angle_degrees",&StainEstimateResult::angle_degrees).def_ro("residual_fraction",&StainEstimateResult::residual_fraction).def_ro("elapsed_ms",&StainEstimateResult::elapsed_ms)
        .def_prop_ro("transform",&StainEstimateResult::transform);
    m.def("estimate_stains",[](const StainEstimateRequest& r){return estimate_stains(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("stain_model_sha256",&stain_model_sha256,"transform"_a);
    m.def("stain_estimate_json",&stain_estimate_json,"result"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_stain_estimate",&save_stain_estimate,"result"_a,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<StainTransform>>(m.attr("StainTransform")).def_static("from_normalized",&StainTransform::from_normalized,"stains"_a,"white"_a=ColorVector{255,255,255});
    using PythonStainEstimateJob=qpx::bindings::OwnerJobHolder<StainEstimateJob>;
    nb::class_<PythonStainEstimateJob>(m,"StainEstimateJob").def(nb::init<>()).def("request",qpx::bindings::owner_job_method<&StainEstimateJob::request>()).def("poll",qpx::bindings::owner_job_method<&StainEstimateJob::poll>(),"document"_a,"selected_id"_a)
        .def("wait",[](PythonStainEstimateJob& job,const Document& doc,std::uint64_t selected){return qpx::bindings::wait(job,doc,selected);},"document"_a,"selected_id"_a).def("cancel",qpx::bindings::owner_job_method<&StainEstimateJob::cancel,false>()).def_prop_ro("busy",qpx::bindings::owner_job_method<&StainEstimateJob::busy>()).def_prop_ro("result",qpx::bindings::owner_job_method<&StainEstimateJob::result>());
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions")).def_rw("stain_estimate_parameters",&GuiOptions::stain_estimate_parameters);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport")).def_ro("stain_estimate_state",&GuiReport::stain_estimate_state).def_ro("stain_estimate_error",&GuiReport::stain_estimate_error)
        .def_ro("stain_estimate_result",&GuiReport::stain_estimate_result).def_ro("stain_estimate_applied",&GuiReport::stain_estimate_applied);
}
