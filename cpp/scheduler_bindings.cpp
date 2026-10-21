#include "qupathxx/tile_scheduler.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
namespace nb = nanobind;
using namespace nb::literals;
void bind_scheduler(nb::module_& m) {
    using namespace qpx;
    nb::enum_<TileStatus>(m,"TileStatus")
        .value("QUEUED",TileStatus::Queued).value("RUNNING",TileStatus::Running).value("READY",TileStatus::Ready)
        .value("CANCELLED",TileStatus::Cancelled).value("STALE",TileStatus::Stale).value("FAILED",TileStatus::Failed);
    nb::class_<SchedulerOptions>(m,"SchedulerOptions").def(nb::init<>())
        .def_rw("workers",&SchedulerOptions::workers).def_rw("cache_bytes",&SchedulerOptions::cache_bytes)
        .def_rw("work_bytes",&SchedulerOptions::work_bytes).def_rw("max_requests",&SchedulerOptions::max_requests)
        .def_rw("max_cache_entries",&SchedulerOptions::max_cache_entries);
    nb::class_<TileResult>(m,"TileResult")
        .def_ro("ticket",&TileResult::ticket).def_ro("generation",&TileResult::generation).def_ro("status",&TileResult::status)
        .def_ro("pixels",&TileResult::pixels).def_ro("error",&TileResult::error);
    nb::class_<SchedulerStats>(m,"SchedulerStats")
        .def_ro("cache_bytes",&SchedulerStats::cache_bytes).def_ro("reserved_bytes",&SchedulerStats::reserved_bytes)
        .def_ro("requests",&SchedulerStats::requests).def_ro("queued",&SchedulerStats::queued)
        .def_ro("running",&SchedulerStats::running).def_ro("ready",&SchedulerStats::ready)
        .def_ro("cache_entries",&SchedulerStats::cache_entries).def_ro("reads",&SchedulerStats::reads)
        .def_ro("cache_hits",&SchedulerStats::cache_hits).def_ro("stopped",&SchedulerStats::stopped);
    nb::class_<TileScheduler>(m,"TileScheduler")
        .def(nb::init<std::shared_ptr<ImageServer>,SchedulerOptions>(),"server"_a,"options"_a=SchedulerOptions{})
        .def("submit",&TileScheduler::submit,"request"_a,"priority"_a=0)
        .def("cancel",&TileScheduler::cancel)
        .def("take",&TileScheduler::take)
        .def("advance_generation",&TileScheduler::advance_generation)
        .def_prop_ro("generation",&TileScheduler::generation)
        .def_prop_ro("stats",&TileScheduler::stats)
        .def("clear_cache",&TileScheduler::clear_cache)
        .def("wait_idle",&TileScheduler::wait_idle,"timeout_seconds"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("shutdown",&TileScheduler::shutdown,nb::call_guard<nb::gil_scoped_release>());
}
