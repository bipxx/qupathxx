#include "qupathxx/spatial_analysis.hpp"
#include "qupathxx/spatial_frontend.hpp"
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
    nb::class_<SpatialNeighborhoodOptions,SpatialDistanceOptions>(m,"SpatialNeighborhoodOptions").def(nb::init<>())
        .def_rw("radius",&SpatialNeighborhoodOptions::radius).def("validate",&SpatialNeighborhoodOptions::validate);
    nb::class_<SpatialNeighborhoodRequest>(m,"SpatialNeighborhoodRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,SpatialNeighborhoodOptions>(),
            "document"_a,"sources"_a,"targets"_a,"options"_a=SpatialNeighborhoodOptions{})
        .def_static("replay",&SpatialNeighborhoodRequest::replay,"document"_a,"record_id"_a)
        .def("matches",&SpatialNeighborhoodRequest::matches,"document"_a)
        .def_prop_ro("source_count",&SpatialNeighborhoodRequest::source_count).def_prop_ro("target_count",&SpatialNeighborhoodRequest::target_count)
        .def("source_object",[](const SpatialNeighborhoodRequest& r,std::size_t i){return SpatialObjectSnapshot(r.source_object(i));},"index"_a)
        .def("target_object",[](const SpatialNeighborhoodRequest& r,std::size_t i){return SpatialObjectSnapshot(r.target_object(i));},"index"_a)
        .def_prop_ro("snapshot_bytes",&SpatialNeighborhoodRequest::snapshot_bytes).def_prop_ro("required_work_bytes",&SpatialNeighborhoodRequest::required_work_bytes)
        .def_prop_ro("document_revision",&SpatialNeighborhoodRequest::document_revision).def_prop_ro("capture_ms",&SpatialNeighborhoodRequest::capture_ms)
        .def_prop_ro("x_scale",&SpatialNeighborhoodRequest::x_scale).def_prop_ro("y_scale",&SpatialNeighborhoodRequest::y_scale)
        .def_prop_ro("options",&SpatialNeighborhoodRequest::options).def_prop_ro("input_sha256",&SpatialNeighborhoodRequest::input_sha256)
        .def_prop_ro("provenance",&SpatialNeighborhoodRequest::provenance);
    nb::class_<SpatialNeighborhoodRow>(m,"SpatialNeighborhoodRow")
        .def_ro("source_id",&SpatialNeighborhoodRow::source_id).def_ro("source_revision",&SpatialNeighborhoodRow::source_revision)
        .def_prop_ro("source_centroid",[](const SpatialNeighborhoodRow& r){return r.source_centroid;})
        .def_ro("source_kind",&SpatialNeighborhoodRow::source_kind).def_ro("source_nucleus",&SpatialNeighborhoodRow::source_nucleus)
        .def_ro("count",&SpatialNeighborhoodRow::count);
    nb::class_<SpatialNeighborhoodResult>(m,"SpatialNeighborhoodResult")
        .def_prop_ro("row_count",[](const SpatialNeighborhoodResult& r){return r.rows().size();})
        .def("row",[](const SpatialNeighborhoodResult& r,std::size_t i){return SpatialNeighborhoodRow(r.rows().at(i));},"index"_a)
        .def("rows",[](const SpatialNeighborhoodResult& r,std::size_t start,std::size_t limit) {
            if(!limit || limit>4096) throw std::invalid_argument("spatial row page limit must be 1..4096");
            if(start>r.rows().size()) throw std::out_of_range("spatial row page starts after the table");
            const auto count=std::min(limit,r.rows().size()-start);
            return std::vector<SpatialNeighborhoodRow>(r.rows().begin()+start,r.rows().begin()+start+count);
        },"start"_a=0,"limit"_a=256)
        .def_prop_ro("algorithm",&SpatialNeighborhoodResult::algorithm).def_prop_ro("input_sha256",&SpatialNeighborhoodResult::input_sha256)
        .def_prop_ro("sha256",&SpatialNeighborhoodResult::sha256).def_prop_ro("options",&SpatialNeighborhoodResult::options)
        .def_prop_ro("source_count",&SpatialNeighborhoodResult::source_count).def_prop_ro("target_count",&SpatialNeighborhoodResult::target_count)
        .def("source_object",[](const SpatialNeighborhoodResult& r,std::size_t i){return SpatialObjectSnapshot(r.source_object(i));},"index"_a)
        .def("target_object",[](const SpatialNeighborhoodResult& r,std::size_t i){return SpatialObjectSnapshot(r.target_object(i));},"index"_a)
        .def_prop_ro("x_scale",&SpatialNeighborhoodResult::x_scale).def_prop_ro("y_scale",&SpatialNeighborhoodResult::y_scale)
        .def_prop_ro("document_revision",&SpatialNeighborhoodResult::document_revision).def_prop_ro("distance_tests",&SpatialNeighborhoodResult::distance_tests)
        .def_prop_ro("payload_bytes",&SpatialNeighborhoodResult::payload_bytes).def_prop_ro("elapsed_ms",&SpatialNeighborhoodResult::elapsed_ms)
        .def_prop_ro("provenance",&SpatialNeighborhoodResult::provenance).def("belongs_to",&SpatialNeighborhoodResult::belongs_to,"request"_a);
    nb::class_<SpatialNeighborhoodRecord>(m,"SpatialNeighborhoodRecord")
        .def_ro("id",&SpatialNeighborhoodRecord::id).def_ro("captured_next_id",&SpatialNeighborhoodRecord::captured_next_id)
        .def_ro("name",&SpatialNeighborhoodRecord::name).def_ro("producer",&SpatialNeighborhoodRecord::producer)
        .def_ro("result",&SpatialNeighborhoodRecord::result).def_prop_ro("payload_bytes",&SpatialNeighborhoodRecord::payload_bytes);
    nb::class_<SpatialNeighborhoodJob>(m,"SpatialNeighborhoodJob").def(nb::init<>())
        .def("request",&SpatialNeighborhoodJob::request,"request"_a).def("poll",&SpatialNeighborhoodJob::poll,"document"_a)
        .def("wait",[](SpatialNeighborhoodJob& job,const Document& doc) {
            job.poll(doc);
            {nb::gil_scoped_release release;job.wait_work();}
            return job.poll(doc);
        },"document"_a)
        .def("cancel",&SpatialNeighborhoodJob::cancel).def_prop_ro("busy",&SpatialNeighborhoodJob::busy).def_prop_ro("result",&SpatialNeighborhoodJob::result);
    m.def("measure_spatial_neighborhoods",[](const SpatialNeighborhoodRequest& r){return measure_spatial_neighborhoods(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("spatial_neighborhood_csv",[](const SpatialNeighborhoodResult& r,std::size_t limit){return spatial_neighborhood_csv(r,limit);},"result"_a,"byte_limit"_a=64u*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_spatial_neighborhood_csv",[](const SpatialNeighborhoodResult& r,const std::string& path,std::size_t limit,bool replace){return save_spatial_neighborhood_csv(r,path,limit,replace);},"result"_a,"path"_a,"byte_limit"_a=64u*1024*1024,"replace"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.attr("SpatialNeighborhoodStats")=m.attr("SpatialDistanceStats");
    nb::class_<SpatialDensityOptions>(m,"SpatialDensityOptions").def(nb::init<>())
        .def_rw("prefer_nucleus",&SpatialDensityOptions::prefer_nucleus)
        .def_rw("polygonize_curves",&SpatialDensityOptions::polygonize_curves)
        .def_rw("snapshot_bytes",&SpatialDensityOptions::snapshot_bytes)
        .def_rw("work_bytes",&SpatialDensityOptions::work_bytes)
        .def_rw("max_membership_tests",&SpatialDensityOptions::max_membership_tests)
        .def_rw("max_geometry_operations",&SpatialDensityOptions::max_geometry_operations)
        .def_rw("max_geometry_vertices",&SpatialDensityOptions::max_geometry_vertices)
        .def("validate",&SpatialDensityOptions::validate);
    nb::class_<SpatialDensityAreaSnapshot>(m,"SpatialDensityAreaSnapshot")
        .def_ro("id",&SpatialDensityAreaSnapshot::id)
        .def_ro("revision",&SpatialDensityAreaSnapshot::revision)
        .def_ro("geometry_sha256",&SpatialDensityAreaSnapshot::geometry_sha256)
        .def_prop_ro("geometry",[](const SpatialDensityAreaSnapshot& a){return a.geometry;});
    nb::class_<SpatialDensityRequest>(m,"SpatialDensityRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,const std::vector<std::uint64_t>&,SpatialDensityOptions>(),
            "document"_a,"windows"_a,"tissue"_a,"excluded"_a,"cells"_a,"options"_a=SpatialDensityOptions{})
        .def_static("replay",&SpatialDensityRequest::replay,"document"_a,"record_id"_a)
        .def("matches",&SpatialDensityRequest::matches,"document"_a)
        .def_prop_ro("window_count",&SpatialDensityRequest::window_count)
        .def_prop_ro("tissue_count",&SpatialDensityRequest::tissue_count)
        .def_prop_ro("exclusion_count",&SpatialDensityRequest::exclusion_count)
        .def_prop_ro("cell_count",&SpatialDensityRequest::cell_count)
        .def_prop_ro("x_scale",&SpatialDensityRequest::x_scale)
        .def_prop_ro("y_scale",&SpatialDensityRequest::y_scale)
        .def_prop_ro("document_revision",&SpatialDensityRequest::document_revision)
        .def_prop_ro("options",&SpatialDensityRequest::options)
        .def_prop_ro("input_sha256",&SpatialDensityRequest::input_sha256)
        .def_prop_ro("provenance",&SpatialDensityRequest::provenance)
        .def_prop_ro("snapshot_bytes",&SpatialDensityRequest::snapshot_bytes)
        .def_prop_ro("required_work_bytes",&SpatialDensityRequest::required_work_bytes)
        .def_prop_ro("capture_ms",&SpatialDensityRequest::capture_ms)
        .def("window_object",[](const SpatialDensityRequest& r,std::size_t i){return SpatialDensityAreaSnapshot(r.window_object(i));},"index"_a)
        .def("tissue_object",[](const SpatialDensityRequest& r,std::size_t i){return SpatialDensityAreaSnapshot(r.tissue_object(i));},"index"_a)
        .def("exclusion_object",[](const SpatialDensityRequest& r,std::size_t i){return SpatialDensityAreaSnapshot(r.exclusion_object(i));},"index"_a)
        .def("cell_object",[](const SpatialDensityRequest& r,std::size_t i){return SpatialObjectSnapshot(r.cell_object(i));},"index"_a);
    nb::class_<SpatialDensityRow>(m,"SpatialDensityRow")
        .def_ro("window_id",&SpatialDensityRow::window_id)
        .def_ro("window_revision",&SpatialDensityRow::window_revision)
        .def_ro("count",&SpatialDensityRow::count)
        .def_ro("area_pixels",&SpatialDensityRow::area_pixels)
        .def_ro("area_mm2",&SpatialDensityRow::area_mm2)
        .def_ro("density_per_mm2",&SpatialDensityRow::density_per_mm2)
        .def_ro("geometry_sha256",&SpatialDensityRow::geometry_sha256)
        .def_prop_ro("footprint",[](const SpatialDensityRow& r){return r.footprint;});
    nb::class_<SpatialDensityResult>(m,"SpatialDensityResult")
        .def_prop_ro("row_count",[](const SpatialDensityResult& r){return r.rows().size();})
        .def("row",[](const SpatialDensityResult& r,std::size_t i){return SpatialDensityRow(r.rows().at(i));},"index"_a)
        .def("rows",[](const SpatialDensityResult& r,std::size_t start,std::size_t limit) {
            if(!limit || limit>4096) throw std::invalid_argument("density row page limit must be1..4096");
            if(start>r.rows().size()) throw std::out_of_range("density row page starts after the table");
            const auto count=std::min(limit,r.rows().size()-start);
            return std::vector<SpatialDensityRow>(r.rows().begin()+start,r.rows().begin()+start+count);
        },"start"_a=0,"limit"_a=256)
        .def_prop_ro("window_count",&SpatialDensityResult::window_count)
        .def_prop_ro("tissue_count",&SpatialDensityResult::tissue_count)
        .def_prop_ro("exclusion_count",&SpatialDensityResult::exclusion_count)
        .def_prop_ro("cell_count",&SpatialDensityResult::cell_count)
        .def_prop_ro("x_scale",&SpatialDensityResult::x_scale)
        .def_prop_ro("y_scale",&SpatialDensityResult::y_scale)
        .def_prop_ro("document_revision",&SpatialDensityResult::document_revision)
        .def_prop_ro("options",&SpatialDensityResult::options)
        .def_prop_ro("input_sha256",&SpatialDensityResult::input_sha256)
        .def_prop_ro("provenance",&SpatialDensityResult::provenance)
        .def_prop_ro("algorithm",&SpatialDensityResult::algorithm)
        .def_prop_ro("geometry_policy",&SpatialDensityResult::geometry_policy)
        .def_prop_ro("sha256",&SpatialDensityResult::sha256)
        .def_prop_ro("payload_bytes",&SpatialDensityResult::payload_bytes)
        .def_prop_ro("elapsed_ms",&SpatialDensityResult::elapsed_ms)
        .def_prop_ro("membership_tests",&SpatialDensityResult::membership_tests)
        .def_prop_ro("geometry_operations",&SpatialDensityResult::geometry_operations)
        .def("window_object",[](const SpatialDensityResult& r,std::size_t i){return SpatialDensityAreaSnapshot(r.window_object(i));},"index"_a)
        .def("tissue_object",[](const SpatialDensityResult& r,std::size_t i){return SpatialDensityAreaSnapshot(r.tissue_object(i));},"index"_a)
        .def("exclusion_object",[](const SpatialDensityResult& r,std::size_t i){return SpatialDensityAreaSnapshot(r.exclusion_object(i));},"index"_a)
        .def("cell_object",[](const SpatialDensityResult& r,std::size_t i){return SpatialObjectSnapshot(r.cell_object(i));},"index"_a)
        .def("belongs_to",&SpatialDensityResult::belongs_to,"request"_a);
    nb::class_<SpatialDensityRecord>(m,"SpatialDensityRecord")
        .def_ro("id",&SpatialDensityRecord::id)
        .def_ro("captured_next_id",&SpatialDensityRecord::captured_next_id)
        .def_ro("name",&SpatialDensityRecord::name)
        .def_ro("producer",&SpatialDensityRecord::producer)
        .def_ro("result",&SpatialDensityRecord::result)
        .def_prop_ro("payload_bytes",&SpatialDensityRecord::payload_bytes);
    nb::class_<SpatialDensityStats>(m,"SpatialDensityStats")
        .def_ro("state",&SpatialDensityStats::state)
        .def_ro("error",&SpatialDensityStats::error)
        .def_ro("generation",&SpatialDensityStats::generation)
        .def_ro("completed_rows",&SpatialDensityStats::completed_rows)
        .def_ro("total_rows",&SpatialDensityStats::total_rows)
        .def_ro("membership_tests",&SpatialDensityStats::membership_tests)
        .def_ro("geometry_operations",&SpatialDensityStats::geometry_operations)
        .def_ro("snapshot_bytes",&SpatialDensityStats::snapshot_bytes)
        .def_ro("work_bytes",&SpatialDensityStats::work_bytes)
        .def_ro("capture_ms",&SpatialDensityStats::capture_ms)
        .def_ro("elapsed_ms",&SpatialDensityStats::elapsed_ms);
    nb::class_<SpatialDensityJob>(m,"SpatialDensityJob").def(nb::init<>())
        .def("request",&SpatialDensityJob::request,"request"_a).def("poll",&SpatialDensityJob::poll,"document"_a)
        .def("wait",[](SpatialDensityJob& job,const Document& doc) {
            job.poll(doc);{nb::gil_scoped_release release;job.wait_work();}return job.poll(doc);
        },"document"_a)
        .def("cancel",&SpatialDensityJob::cancel).def_prop_ro("busy",&SpatialDensityJob::busy).def_prop_ro("result",&SpatialDensityJob::result);
    m.def("measure_spatial_density",[](const SpatialDensityRequest& r){return measure_spatial_density(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("spatial_density_csv",[](const SpatialDensityResult& r,std::size_t limit){return spatial_density_csv(r,limit);},"result"_a,"byte_limit"_a=64u*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_spatial_density_csv",[](const SpatialDensityResult& r,const std::string& path,std::size_t limit,bool replace){return save_spatial_density_csv(r,path,limit,replace);},"result"_a,"path"_a,"byte_limit"_a=64u*1024*1024,"replace"_a=false,nb::call_guard<nb::gil_scoped_release>());
    auto document=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    document.def_prop_ro("density_records",&Document::density_records).def_prop_ro("density_record_count",&Document::density_record_count)
        .def("density_record",&Document::density_record,"record_id"_a)
        .def("keep_density",&Document::keep_density,"request"_a,"result"_a,"name"_a="Tissue cell density")
        .def("verify_density",&Document::verify_density,"record_id"_a,"request"_a,"result"_a)
        .def("density_status",&Document::density_status,"record_id"_a).def("remove_density",&Document::remove_density,"record_id"_a);
    nb::enum_<SpatialOperation>(m,"SpatialOperation")
        .value("NEAREST_DISTANCE",SpatialOperation::NearestDistance).value("RADIUS_COUNT",SpatialOperation::RadiusCount).value("TISSUE_DENSITY",SpatialOperation::TissueDensity);
    nb::class_<GuiSpatialOptions>(m,"GuiSpatialOptions").def(nb::init<>())
        .def_rw("open",&GuiSpatialOptions::open).def_rw("run",&GuiSpatialOptions::run)
        .def_rw("sources",&GuiSpatialOptions::sources).def_rw("targets",&GuiSpatialOptions::targets)
        .def_rw("analysis",&GuiSpatialOptions::analysis).def_rw("operation",&GuiSpatialOptions::operation).def_rw("radius",&GuiSpatialOptions::radius).def_rw("record_id",&GuiSpatialOptions::record_id)
        .def_rw("tissue",&GuiSpatialOptions::tissue).def_rw("excluded",&GuiSpatialOptions::excluded).def_rw("density",&GuiSpatialOptions::density)
        .def_rw("csv_path",&GuiSpatialOptions::csv_path).def_rw("csv_bytes",&GuiSpatialOptions::csv_bytes).def("validate",&GuiSpatialOptions::validate);
    nb::class_<GuiSpatialReport>(m,"GuiSpatialReport")
        .def_ro("state",&GuiSpatialReport::state).def_ro("error",&GuiSpatialReport::error).def_ro("record_status",&GuiSpatialReport::record_status)
        .def_ro("busy",&GuiSpatialReport::busy).def_ro("record_id",&GuiSpatialReport::record_id).def_ro("rows",&GuiSpatialReport::rows)
        .def_ro("sources",&GuiSpatialReport::sources).def_ro("targets",&GuiSpatialReport::targets).def_ro("result",&GuiSpatialReport::result)
        .def_ro("operation",&GuiSpatialReport::operation).def_ro("neighborhood_result",&GuiSpatialReport::neighborhood_result)
        .def_ro("density_result",&GuiSpatialReport::density_result).def_ro("density_analysis",&GuiSpatialReport::density_analysis)
        .def_ro("analysis",&GuiSpatialReport::analysis).def_ro("export_state",&GuiSpatialReport::export_state).def_ro("export_error",&GuiSpatialReport::export_error)
        .def_ro("export_committed",&GuiSpatialReport::export_committed).def_ro("export_rows",&GuiSpatialReport::export_rows).def_ro("export_bytes",&GuiSpatialReport::export_bytes).def_ro("path",&GuiSpatialReport::path);
    auto gui_options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    gui_options.def_prop_rw("spatial",[](const GuiOptions& o)->std::optional<GuiSpatialOptions> {if(o.spatial)return *o.spatial;return {};},
        [](GuiOptions& o,std::optional<GuiSpatialOptions> value){o.spatial=value?std::make_shared<const GuiSpatialOptions>(*value):nullptr;});
    auto gui_report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));gui_report.def_ro("spatial",&GuiReport::spatial);
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
