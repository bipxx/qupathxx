#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/project.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_measurement_tables(nb::module_& m) {
    using namespace qpx;
    nb::enum_<MeasurementTableKind>(m,"MeasurementTableKind").value("ROI",MeasurementTableKind::Roi).value("TISSUE",MeasurementTableKind::Tissue);
    nb::class_<MeasurementTableOptions>(m,"MeasurementTableOptions").def(nb::init<>()).def_rw("require_current",&MeasurementTableOptions::require_current)
        .def_rw("byte_limit",&MeasurementTableOptions::byte_limit).def_rw("snapshot_budget",&MeasurementTableOptions::snapshot_budget).def("validate",&MeasurementTableOptions::validate);
    nb::class_<MeasurementTableRequest>(m,"MeasurementTableRequest").def(nb::init<const Document&,MeasurementTableKind,const std::vector<std::uint64_t>&,MeasurementTableOptions>(),"document"_a,"kind"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{})
        .def(nb::init<const Project&,MeasurementTableKind,const std::vector<std::uint64_t>&,MeasurementTableOptions>(),"project"_a,"kind"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{})
        .def_prop_ro("kind",&MeasurementTableRequest::kind)
        .def_prop_ro("project_scope",&MeasurementTableRequest::project_scope).def_prop_ro("image_count",&MeasurementTableRequest::image_count)
        .def_prop_ro("project_identity",&MeasurementTableRequest::project_identity).def_prop_ro("project_generation",&MeasurementTableRequest::project_generation).def_prop_ro("project_revision",&MeasurementTableRequest::project_revision)
        .def_prop_ro("record_count",&MeasurementTableRequest::record_count).def_prop_ro("row_count",&MeasurementTableRequest::row_count)
        .def_prop_ro("snapshot_bytes",&MeasurementTableRequest::snapshot_bytes).def_prop_ro("capture_ms",&MeasurementTableRequest::capture_ms)
        .def_prop_ro("document_revision",&MeasurementTableRequest::document_revision).def_prop_ro("options",&MeasurementTableRequest::options);
    m.def("measurements_csv",[](const MeasurementTableRequest& request) {nb::gil_scoped_release release;return measurements_csv(request);},"request"_a);
    m.def("measurements_csv",[](const Document& doc,MeasurementTableKind kind,const std::vector<std::uint64_t>& ids,MeasurementTableOptions options) {
        MeasurementTableRequest request(doc,kind,ids,options);nb::gil_scoped_release release;return measurements_csv(request);
    },"document"_a,"kind"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{});
    m.def("save_measurements_csv",[](const MeasurementTableRequest& request,const std::string& path) {nb::gil_scoped_release release;save_measurements_csv(request,path);},"request"_a,"path"_a);
    m.def("save_measurements_csv",[](const Document& doc,MeasurementTableKind kind,const std::string& path,const std::vector<std::uint64_t>& ids,MeasurementTableOptions options) {
        MeasurementTableRequest request(doc,kind,ids,options);nb::gil_scoped_release release;save_measurements_csv(request,path);
    },"document"_a,"kind"_a,"path"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{});
    m.def("measurements_csv",[](const Project& project,MeasurementTableKind kind,const std::vector<std::uint64_t>& images,MeasurementTableOptions options) {
        MeasurementTableRequest request(project,kind,images,options);nb::gil_scoped_release release;return measurements_csv(request);
    },"project"_a,"kind"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{});
    m.def("save_measurements_csv",[](const Project& project,MeasurementTableKind kind,const std::string& path,const std::vector<std::uint64_t>& images,MeasurementTableOptions options) {
        MeasurementTableRequest request(project,kind,images,options);nb::gil_scoped_release release;save_measurements_csv(request,path);
    },"project"_a,"kind"_a,"path"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=MeasurementTableOptions{});
    nb::class_<MeasurementTableExportStats>(m,"MeasurementTableExportStats").def_ro("state",&MeasurementTableExportStats::state).def_ro("path",&MeasurementTableExportStats::path).def_ro("error",&MeasurementTableExportStats::error)
        .def_ro("rows",&MeasurementTableExportStats::rows).def_ro("total_rows",&MeasurementTableExportStats::total_rows).def_ro("bytes",&MeasurementTableExportStats::bytes)
        .def_ro("completed",&MeasurementTableExportStats::completed).def_ro("document_revision",&MeasurementTableExportStats::document_revision).def_ro("snapshot_bytes",&MeasurementTableExportStats::snapshot_bytes)
        .def_ro("capture_ms",&MeasurementTableExportStats::capture_ms).def_ro("write_ms",&MeasurementTableExportStats::write_ms);
    nb::class_<MeasurementTableExportJob>(m,"MeasurementTableExportJob").def(nb::init<>()).def("request",&MeasurementTableExportJob::request)
        .def("poll",&MeasurementTableExportJob::poll).def("wait",&MeasurementTableExportJob::wait,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&MeasurementTableExportJob::cancel).def_prop_ro("busy",&MeasurementTableExportJob::busy);
    auto options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    options.def_rw("measurement_table_path",&GuiOptions::measurement_table_path).def_rw("measurement_table_kind",&GuiOptions::measurement_table_kind)
        .def_rw("measurement_table_require_current",&GuiOptions::measurement_table_require_current)
        .def_rw("measurement_table_byte_limit",&GuiOptions::measurement_table_byte_limit).def_rw("measurement_table_snapshot_budget",&GuiOptions::measurement_table_snapshot_budget);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("measurement_table_state",&GuiReport::measurement_table_state).def_ro("measurement_table_error",&GuiReport::measurement_table_error)
        .def_ro("measurement_table_rows",&GuiReport::measurement_table_rows).def_ro("measurement_table_capture_ms",&GuiReport::measurement_table_capture_ms).def_ro("measurement_table_write_ms",&GuiReport::measurement_table_write_ms);
}
