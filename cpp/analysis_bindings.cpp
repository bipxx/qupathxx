#include "job_binding_utils.hpp"
#include "qupathxx/analysis.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
namespace nb=nanobind;
using namespace nb::literals;
void bind_analysis(nb::module_& m) {
    using namespace qpx;
    nb::enum_<AnalysisKind>(m,"AnalysisKind").value("OPTICAL_DENSITY_MEANS",AnalysisKind::OpticalDensityMeans).value("STAIN_MEANS",AnalysisKind::StainMeans);
    nb::class_<AnalysisRequest>(m,"AnalysisRequest")
        .def(nb::init<const Document&,std::uint64_t,const StainTransform&,AnalysisKind,bool,std::size_t,RoiSamplingPolicy>(),
            "document"_a,"object_id"_a,"transform"_a,"kind"_a=AnalysisKind::StainMeans,"nucleus"_a=false,"tile_size"_a=256,"sampling_policy"_a=RoiSamplingPolicy::GeometryCoverage)
        .def("matches",&AnalysisRequest::matches,"document"_a,"selected_id"_a)
        .def_prop_ro("object_id",&AnalysisRequest::object_id).def_prop_ro("object_revision",&AnalysisRequest::object_revision)
        .def_prop_ro("document_revision",&AnalysisRequest::document_revision).def_prop_ro("kind",&AnalysisRequest::kind)
        .def_prop_ro("sampling_policy",&AnalysisRequest::sampling_policy)
        .def_prop_ro("nucleus",&AnalysisRequest::nucleus).def_prop_ro("tile_size",&AnalysisRequest::tile_size)
        .def_prop_ro("total_tiles",&AnalysisRequest::total_tiles).def_prop_ro("roi",&AnalysisRequest::roi)
        .def_prop_ro("transform",&AnalysisRequest::transform,nb::rv_policy::copy)
        .def_static("replay",&AnalysisRequest::replay,"document"_a,"record_id"_a);
    nb::class_<AnalysisResult>(m,"AnalysisResult")
        .def_prop_ro("sampling_policy",&AnalysisResult::sampling_policy)
        .def_ro("algorithm",&AnalysisResult::algorithm).def_ro("input_sha256",&AnalysisResult::input_sha256).def_ro("object_id",&AnalysisResult::object_id)
        .def_ro("object_revision",&AnalysisResult::object_revision).def_ro("document_revision",&AnalysisResult::document_revision)
        .def_ro("kind",&AnalysisResult::kind).def_ro("nucleus",&AnalysisResult::nucleus)
        .def_ro("samples",&AnalysisResult::samples).def_ro("tiles",&AnalysisResult::tiles)
        .def_ro("means",&AnalysisResult::means).def_ro("white",&AnalysisResult::white).def_ro("stains",&AnalysisResult::stains)
        .def_ro("elapsed_ms",&AnalysisResult::elapsed_ms);
    m.def("analyze",[](const AnalysisRequest& r){return analyze(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<AnalysisStats>(m,"AnalysisStats")
        .def_ro("state",&AnalysisStats::state).def_ro("error",&AnalysisStats::error).def_ro("generation",&AnalysisStats::generation)
        .def_ro("completed_tiles",&AnalysisStats::completed_tiles).def_ro("total_tiles",&AnalysisStats::total_tiles).def_ro("elapsed_ms",&AnalysisStats::elapsed_ms);
    using PythonAnalysisJob=qpx::bindings::OwnerJobHolder<AnalysisJob>;
    nb::class_<PythonAnalysisJob>(m,"AnalysisJob").def(nb::init<>()).def("request",qpx::bindings::owner_job_method<&AnalysisJob::request>())
        .def("poll",qpx::bindings::owner_job_method<&AnalysisJob::poll>(),"document"_a,"selected_id"_a)
        .def("wait",[](PythonAnalysisJob& job,const Document& doc,std::uint64_t selected){return qpx::bindings::wait(job,doc,selected);},"document"_a,"selected_id"_a)
        .def("cancel",qpx::bindings::owner_job_method<&AnalysisJob::cancel,false>()).def_prop_ro("busy",qpx::bindings::owner_job_method<&AnalysisJob::busy>()).def_prop_ro("result",qpx::bindings::owner_job_method<&AnalysisJob::result>())
        .def("keep_result",qpx::bindings::owner_job_method<&AnalysisJob::keep_result>(),"document"_a,"selected_id"_a).def("verify_result",qpx::bindings::owner_job_method<&AnalysisJob::verify_result>(),"document"_a,"record_id"_a);
    nb::class_<AnalysisRecord>(m,"AnalysisRecord").def_ro("id",&AnalysisRecord::id).def_ro("result",&AnalysisRecord::result)
        .def_ro("roi",&AnalysisRecord::roi).def_ro("tile_size",&AnalysisRecord::tile_size);
}
