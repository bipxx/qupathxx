#include "job_binding_utils.hpp"
#include "qupathxx/thumbnail.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <stdexcept>
namespace nb=nanobind;using namespace nb::literals;
void bind_thumbnails(nb::module_& m) {
    using namespace qpx;
    nb::class_<ThumbnailOptions>(m,"ThumbnailOptions").def(nb::init<>())
        .def_rw("max_width",&ThumbnailOptions::max_width).def_rw("max_height",&ThumbnailOptions::max_height)
        .def_rw("source_pixel_limit",&ThumbnailOptions::source_pixel_limit).def_rw("output_pixel_limit",&ThumbnailOptions::output_pixel_limit)
        .def("validate",&ThumbnailOptions::validate);
    nb::class_<ThumbnailPlan>(m,"ThumbnailPlan")
        .def_ro("level",&ThumbnailPlan::level).def_ro("width",&ThumbnailPlan::width).def_ro("height",&ThumbnailPlan::height)
        .def_ro("source_width",&ThumbnailPlan::source_width).def_ro("source_height",&ThumbnailPlan::source_height)
        .def_ro("source_pixels",&ThumbnailPlan::source_pixels).def_ro("output_pixels",&ThumbnailPlan::output_pixels)
        .def_ro("level_downsample",&ThumbnailPlan::level_downsample).def_ro("downsample_x",&ThumbnailPlan::downsample_x).def_ro("downsample_y",&ThumbnailPlan::downsample_y);
    nb::class_<ThumbnailResult>(m,"ThumbnailResult")
        .def_ro("plan",&ThumbnailResult::plan).def_ro("elapsed_ms",&ThumbnailResult::elapsed_ms)
        .def_prop_ro("pixels",[](const ThumbnailResult& result)->const PixelBuffer& {return result.pixels;},nb::rv_policy::reference_internal);
    nb::class_<ThumbnailStats>(m,"ThumbnailStats")
        .def_ro("state",&ThumbnailStats::state).def_ro("error",&ThumbnailStats::error).def_ro("completed",&ThumbnailStats::completed)
        .def_ro("source_pixels",&ThumbnailStats::source_pixels).def_ro("output_pixels",&ThumbnailStats::output_pixels).def_ro("elapsed_ms",&ThumbnailStats::elapsed_ms);
    nb::class_<qpx::bindings::OwnerJobHolder<ThumbnailJob>>(m,"ThumbnailJob").def(nb::init<>())
        .def("request",qpx::bindings::owner_job_method<&ThumbnailJob::request>(),"server"_a,"options"_a=ThumbnailOptions{})
        .def("poll",qpx::bindings::owner_job_method<&ThumbnailJob::poll>()).def("wait",[](qpx::bindings::OwnerJobHolder<ThumbnailJob>& job){return qpx::bindings::wait(job);})
        .def("cancel",qpx::bindings::owner_job_method<&ThumbnailJob::cancel,false>()).def("busy",qpx::bindings::owner_job_method<&ThumbnailJob::busy>()).def("result",qpx::bindings::owner_job_method<&ThumbnailJob::result>());
    m.def("plan_thumbnail",&plan_thumbnail,"metadata"_a,"options"_a=ThumbnailOptions{});
    m.def("read_thumbnail",[](const std::shared_ptr<ImageServer>& server,ThumbnailOptions options) {
        if(!server) throw std::invalid_argument("thumbnail requires a server");return read_thumbnail(*server,options);
    },"server"_a,"options"_a=ThumbnailOptions{},nb::call_guard<nb::gil_scoped_release>());
}
