#include "qupathxx/spatial_analysis.hpp"
#include <nanobind/nanobind.h>
#include <algorithm>
#include <stdexcept>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_spatial(nb::module_& m) {
    using namespace qpx;
    nb::enum_<SpatialDistanceUnits>(m,"SpatialDistanceUnits")
        .value("PIXELS",SpatialDistanceUnits::Pixels).value("MICROMETRES",SpatialDistanceUnits::Micrometres);
    nb::class_<SpatialDistanceOptions>(m,"SpatialDistanceOptions").def(nb::init<>())
        .def_rw("units",&SpatialDistanceOptions::units).def_rw("prefer_nucleus",&SpatialDistanceOptions::prefer_nucleus)
        .def_rw("exclude_self",&SpatialDistanceOptions::exclude_self).def_rw("snapshot_bytes",&SpatialDistanceOptions::snapshot_bytes)
        .def_rw("work_bytes",&SpatialDistanceOptions::work_bytes).def_rw("max_distance_tests",&SpatialDistanceOptions::max_distance_tests)
        .def("validate",&SpatialDistanceOptions::validate);
    nb::class_<SpatialObjectSnapshot>(m,"SpatialObjectSnapshot")
        .def_ro("id",&SpatialObjectSnapshot::id).def_ro("revision",&SpatialObjectSnapshot::revision)
        .def_prop_ro("centroid",[](const SpatialObjectSnapshot& s){return s.centroid;})
        .def_ro("kind",&SpatialObjectSnapshot::kind).def_ro("nucleus",&SpatialObjectSnapshot::nucleus)
        .def_ro("geometry_sha256",&SpatialObjectSnapshot::geometry_sha256);
    nb::class_<SpatialDistanceRequest>(m,"SpatialDistanceRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,SpatialDistanceOptions>(),
            "document"_a,"sources"_a,"targets"_a,"options"_a=SpatialDistanceOptions{})
        .def_static("replay",&SpatialDistanceRequest::replay,"document"_a,"record_id"_a)
        .def("matches",&SpatialDistanceRequest::matches,"document"_a)
        .def_prop_ro("source_count",&SpatialDistanceRequest::source_count).def_prop_ro("target_count",&SpatialDistanceRequest::target_count)
        .def("source_object",[](const SpatialDistanceRequest& r,std::size_t i){return SpatialObjectSnapshot(r.source_object(i));},"index"_a)
        .def("target_object",[](const SpatialDistanceRequest& r,std::size_t i){return SpatialObjectSnapshot(r.target_object(i));},"index"_a)
        .def_prop_ro("snapshot_bytes",&SpatialDistanceRequest::snapshot_bytes).def_prop_ro("required_work_bytes",&SpatialDistanceRequest::required_work_bytes)
        .def_prop_ro("document_revision",&SpatialDistanceRequest::document_revision).def_prop_ro("capture_ms",&SpatialDistanceRequest::capture_ms)
        .def_prop_ro("x_scale",&SpatialDistanceRequest::x_scale).def_prop_ro("y_scale",&SpatialDistanceRequest::y_scale)
        .def_prop_ro("options",&SpatialDistanceRequest::options).def_prop_ro("input_sha256",&SpatialDistanceRequest::input_sha256)
        .def_prop_ro("provenance",&SpatialDistanceRequest::provenance);
    nb::class_<SpatialDistanceRow>(m,"SpatialDistanceRow")
        .def_ro("source_id",&SpatialDistanceRow::source_id).def_ro("source_revision",&SpatialDistanceRow::source_revision)
        .def_prop_ro("source_centroid",[](const SpatialDistanceRow& r){return r.source_centroid;})
        .def_ro("source_kind",&SpatialDistanceRow::source_kind).def_ro("source_nucleus",&SpatialDistanceRow::source_nucleus)
        .def_ro("target_id",&SpatialDistanceRow::target_id).def_ro("target_revision",&SpatialDistanceRow::target_revision)
        .def_prop_ro("target_centroid",[](const SpatialDistanceRow& r){return r.target_centroid;})
        .def_ro("target_kind",&SpatialDistanceRow::target_kind).def_ro("target_nucleus",&SpatialDistanceRow::target_nucleus)
        .def_ro("distance",&SpatialDistanceRow::distance);
    nb::class_<SpatialDistanceResult>(m,"SpatialDistanceResult")
        .def_prop_ro("row_count",[](const SpatialDistanceResult& r){return r.rows().size();})
        .def("row",[](const SpatialDistanceResult& r,std::size_t i){return SpatialDistanceRow(r.rows().at(i));},"index"_a)
        .def("rows",[](const SpatialDistanceResult& r,std::size_t start,std::size_t limit) {
            if(!limit || limit>4096) throw std::invalid_argument("spatial row page limit must be 1..4096");
            if(start>r.rows().size()) throw std::out_of_range("spatial row page starts after the table");
            const auto count=std::min(limit,r.rows().size()-start);
            return std::vector<SpatialDistanceRow>(r.rows().begin()+start,r.rows().begin()+start+count);
        },"start"_a=0,"limit"_a=256)
        .def_prop_ro("algorithm",&SpatialDistanceResult::algorithm).def_prop_ro("input_sha256",&SpatialDistanceResult::input_sha256)
        .def_prop_ro("sha256",&SpatialDistanceResult::sha256).def_prop_ro("options",&SpatialDistanceResult::options)
        .def_prop_ro("source_count",&SpatialDistanceResult::source_count).def_prop_ro("target_count",&SpatialDistanceResult::target_count)
        .def("source_object",[](const SpatialDistanceResult& r,std::size_t i){return SpatialObjectSnapshot(r.source_object(i));},"index"_a)
        .def("target_object",[](const SpatialDistanceResult& r,std::size_t i){return SpatialObjectSnapshot(r.target_object(i));},"index"_a)
        .def_prop_ro("x_scale",&SpatialDistanceResult::x_scale).def_prop_ro("y_scale",&SpatialDistanceResult::y_scale)
        .def_prop_ro("document_revision",&SpatialDistanceResult::document_revision).def_prop_ro("distance_tests",&SpatialDistanceResult::distance_tests)
        .def_prop_ro("payload_bytes",&SpatialDistanceResult::payload_bytes).def_prop_ro("elapsed_ms",&SpatialDistanceResult::elapsed_ms)
        .def_prop_ro("provenance",&SpatialDistanceResult::provenance).def("belongs_to",&SpatialDistanceResult::belongs_to,"request"_a);
    nb::class_<SpatialDistanceRecord>(m,"SpatialDistanceRecord")
        .def_ro("id",&SpatialDistanceRecord::id).def_ro("captured_next_id",&SpatialDistanceRecord::captured_next_id)
        .def_ro("name",&SpatialDistanceRecord::name).def_ro("producer",&SpatialDistanceRecord::producer)
        .def_ro("result",&SpatialDistanceRecord::result).def_prop_ro("payload_bytes",&SpatialDistanceRecord::payload_bytes);
    nb::class_<SpatialDistanceStats>(m,"SpatialDistanceStats")
        .def_ro("state",&SpatialDistanceStats::state).def_ro("error",&SpatialDistanceStats::error)
        .def_ro("generation",&SpatialDistanceStats::generation).def_ro("completed_rows",&SpatialDistanceStats::completed_rows)
        .def_ro("total_rows",&SpatialDistanceStats::total_rows).def_ro("distance_tests",&SpatialDistanceStats::distance_tests)
        .def_ro("snapshot_bytes",&SpatialDistanceStats::snapshot_bytes).def_ro("work_bytes",&SpatialDistanceStats::work_bytes)
        .def_ro("capture_ms",&SpatialDistanceStats::capture_ms).def_ro("elapsed_ms",&SpatialDistanceStats::elapsed_ms);
    nb::class_<SpatialDistanceJob>(m,"SpatialDistanceJob").def(nb::init<>())
        .def("request",&SpatialDistanceJob::request,"request"_a).def("poll",&SpatialDistanceJob::poll,"document"_a)
        .def("wait",[](SpatialDistanceJob& job,const Document& doc) {
            job.poll(doc);
            {nb::gil_scoped_release release;job.wait_work();}
            return job.poll(doc);
        },"document"_a)
        .def("cancel",&SpatialDistanceJob::cancel).def_prop_ro("busy",&SpatialDistanceJob::busy).def_prop_ro("result",&SpatialDistanceJob::result);
    m.def("measure_spatial_distances",[](const SpatialDistanceRequest& r){return measure_spatial_distances(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("spatial_distance_csv",[](const SpatialDistanceResult& r,std::size_t limit){return spatial_distance_csv(r,limit);},"result"_a,"byte_limit"_a=64u*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_spatial_distance_csv",[](const SpatialDistanceResult& r,const std::string& path,std::size_t limit,bool replace){return save_spatial_distance_csv(r,path,limit,replace);},"result"_a,"path"_a,"byte_limit"_a=64u*1024*1024,"replace"_a=false,nb::call_guard<nb::gil_scoped_release>());
}
