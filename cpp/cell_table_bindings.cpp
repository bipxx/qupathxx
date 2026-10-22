#include "qupathxx/cell_tables.hpp"
#include "qupathxx/project.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_cell_tables(nb::module_& m) {
    using namespace qpx;
    nb::class_<CellTableOptions>(m,"CellTableOptions").def(nb::init<>()).def_rw("require_current",&CellTableOptions::require_current)
        .def_rw("published_objects",&CellTableOptions::published_objects)
        .def_rw("byte_limit",&CellTableOptions::byte_limit).def_rw("snapshot_budget",&CellTableOptions::snapshot_budget).def("validate",&CellTableOptions::validate);
    nb::class_<CellTableRequest>(m,"CellTableRequest").def(nb::init<const Document&,const std::vector<std::uint64_t>&,CellTableOptions>(),"document"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{})
        .def(nb::init<const Project&,const std::vector<std::uint64_t>&,CellTableOptions>(),"project"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{})
        .def_prop_ro("project_scope",&CellTableRequest::project_scope).def_prop_ro("image_count",&CellTableRequest::image_count)
        .def_prop_ro("project_identity",&CellTableRequest::project_identity).def_prop_ro("project_generation",&CellTableRequest::project_generation).def_prop_ro("project_revision",&CellTableRequest::project_revision)
        .def_prop_ro("record_count",&CellTableRequest::record_count).def_prop_ro("row_count",&CellTableRequest::row_count)
        .def_prop_ro("snapshot_bytes",&CellTableRequest::snapshot_bytes).def_prop_ro("capture_ms",&CellTableRequest::capture_ms)
        .def_prop_ro("document_revision",&CellTableRequest::document_revision).def_prop_ro("options",&CellTableRequest::options);
    m.def("cell_measurements_csv",[](const CellTableRequest& request) {nb::gil_scoped_release release;return cell_measurements_csv(request);},"request"_a);
    m.def("cell_measurements_csv",[](const Document& doc,const std::vector<std::uint64_t>& ids,CellTableOptions options) {
        CellTableRequest request(doc,ids,options);nb::gil_scoped_release release;return cell_measurements_csv(request);
    },"document"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{});
    m.def("save_cell_measurements_csv",[](const CellTableRequest& request,const std::string& path) {nb::gil_scoped_release release;save_cell_measurements_csv(request,path);},"request"_a,"path"_a);
    m.def("save_cell_measurements_csv",[](const Document& doc,const std::string& path,const std::vector<std::uint64_t>& ids,CellTableOptions options) {
        CellTableRequest request(doc,ids,options);nb::gil_scoped_release release;save_cell_measurements_csv(request,path);
    },"document"_a,"path"_a,"record_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{});
    m.def("cell_measurements_csv",[](const Project& project,const std::vector<std::uint64_t>& images,CellTableOptions options) {
        CellTableRequest request(project,images,options);nb::gil_scoped_release release;return cell_measurements_csv(request);
    },"project"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{});
    m.def("save_cell_measurements_csv",[](const Project& project,const std::string& path,const std::vector<std::uint64_t>& images,CellTableOptions options) {
        CellTableRequest request(project,images,options);nb::gil_scoped_release release;save_cell_measurements_csv(request,path);
    },"project"_a,"path"_a,"image_ids"_a=std::vector<std::uint64_t>{},"options"_a=CellTableOptions{});
    nb::class_<CellTableExportStats>(m,"CellTableExportStats").def_ro("state",&CellTableExportStats::state).def_ro("path",&CellTableExportStats::path).def_ro("error",&CellTableExportStats::error)
        .def_ro("rows",&CellTableExportStats::rows).def_ro("total_rows",&CellTableExportStats::total_rows).def_ro("bytes",&CellTableExportStats::bytes)
        .def_ro("completed",&CellTableExportStats::completed).def_ro("document_revision",&CellTableExportStats::document_revision).def_ro("snapshot_bytes",&CellTableExportStats::snapshot_bytes)
        .def_ro("capture_ms",&CellTableExportStats::capture_ms).def_ro("write_ms",&CellTableExportStats::write_ms);
    nb::class_<CellTableExportJob>(m,"CellTableExportJob").def(nb::init<>()).def("request",&CellTableExportJob::request)
        .def("poll",&CellTableExportJob::poll).def("wait",&CellTableExportJob::wait,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&CellTableExportJob::cancel).def_prop_ro("busy",&CellTableExportJob::busy);
    auto options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    options.def_rw("cell_table_path",&GuiOptions::cell_table_path).def_rw("cell_table_require_current",&GuiOptions::cell_table_require_current)
        .def_rw("cell_table_published_objects",&GuiOptions::cell_table_published_objects)
        .def_rw("cell_table_byte_limit",&GuiOptions::cell_table_byte_limit).def_rw("cell_table_snapshot_budget",&GuiOptions::cell_table_snapshot_budget);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("cell_table_state",&GuiReport::cell_table_state).def_ro("cell_table_error",&GuiReport::cell_table_error).def_ro("cell_table_path",&GuiReport::cell_table_path)
        .def_ro("cell_table_rows",&GuiReport::cell_table_rows).def_ro("cell_table_bytes",&GuiReport::cell_table_bytes)
        .def_ro("cell_table_capture_ms",&GuiReport::cell_table_capture_ms).def_ro("cell_table_write_ms",&GuiReport::cell_table_write_ms);
}
