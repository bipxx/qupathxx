#include "qupathxx/tma.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_tma(nb::module_& m) {
    using namespace qpx;
    nb::class_<TmaGridOptions>(m,"TmaGridOptions").def(nb::init<>())
        .def_rw("name",&TmaGridOptions::name).def_rw("rows",&TmaGridOptions::rows).def_rw("columns",&TmaGridOptions::columns)
        .def_rw("origin",&TmaGridOptions::origin).def_rw("row_step",&TmaGridOptions::row_step).def_rw("column_step",&TmaGridOptions::column_step)
        .def_rw("width",&TmaGridOptions::width).def_rw("height",&TmaGridOptions::height)
        .def_rw("row_labels",&TmaGridOptions::row_labels).def_rw("column_labels",&TmaGridOptions::column_labels).def("validate",&TmaGridOptions::validate);
    nb::class_<TmaGridInfo>(m,"TmaGridInfo")
        .def_prop_ro("name",&TmaGridInfo::name).def_prop_ro("rows",&TmaGridInfo::rows).def_prop_ro("columns",&TmaGridInfo::columns)
        .def_prop_ro("row_labels",&TmaGridInfo::row_labels,nb::rv_policy::copy).def_prop_ro("column_labels",&TmaGridInfo::column_labels,nb::rv_policy::copy)
        .def_prop_ro("payload_bytes",&TmaGridInfo::payload_bytes).def("same_layout",&TmaGridInfo::same_layout);
    nb::class_<TmaCoreInfo>(m,"TmaCoreInfo")
        .def_prop_ro("grid_info",&TmaCoreInfo::grid_info).def_prop_ro("grid_name",&TmaCoreInfo::grid_name)
        .def_prop_ro("row",&TmaCoreInfo::row).def_prop_ro("column",&TmaCoreInfo::column)
        .def_prop_ro("row_label",&TmaCoreInfo::row_label).def_prop_ro("column_label",&TmaCoreInfo::column_label)
        .def_prop_ro("missing",&TmaCoreInfo::missing).def_prop_ro("case_id",&TmaCoreInfo::case_id)
        .def("with_missing",&TmaCoreInfo::with_missing).def("with_case_id",&TmaCoreInfo::with_case_id)
        .def_prop_ro("payload_bytes",&TmaCoreInfo::payload_bytes);
    nb::class_<TmaGrid>(m,"TmaGrid")
        .def_prop_ro("info",&TmaGrid::info).def_prop_ro("cores",&TmaGrid::cores,nb::rv_policy::copy)
        .def("core",&TmaGrid::core,"row"_a,"column"_a).def_prop_ro("payload_bytes",&TmaGrid::payload_bytes)
        .def_prop_ro("document_revision",&TmaGrid::document_revision);
    nb::borrow<nb::class_<Annotation>>(m.attr("Annotation"))
        .def_prop_ro("tma_core",[](const Annotation& a)->std::optional<std::shared_ptr<const TmaCoreInfo>> {if(a.tma_core) return a.tma_core;return {};});
    nb::borrow<nb::class_<ObjectEdit>>(m.attr("ObjectEdit"))
        .def_static("tma_metadata",[](std::uint64_t id,std::optional<std::shared_ptr<const TmaCoreInfo>> info){return ObjectEdit::tma_metadata(id,info?*info:nullptr);},"id"_a,"metadata"_a=nb::none());
    nb::borrow<nb::class_<Document>>(m.attr("Document"))
        .def("tma_grid",[](const Document& d,std::size_t limit)->std::optional<std::shared_ptr<const TmaGrid>> {const auto value=d.tma_grid(limit);if(value) return value;return {};},"byte_limit"_a=64*1024*1024)
        .def_prop_ro("tma_core_count",&Document::tma_core_count).def_prop_ro("tma_revision",&Document::tma_revision).def("tma_core_ids",&Document::tma_core_ids,"include_missing"_a=false)
        .def("create_tma_grid",&Document::create_tma_grid,"options"_a=TmaGridOptions{})
        .def("add_tma_core",&Document::add_tma_core,"row"_a,"column"_a,"bounds"_a,"case_id"_a="","missing"_a=false)
        .def("translate_tma_grid",&Document::translate_tma_grid,"dx"_a,"dy"_a,"include_descendants"_a=true)
        .def("set_tma_missing",&Document::set_tma_missing,"ids"_a,"missing"_a)
        .def("set_tma_case_ids",&Document::set_tma_case_ids,"values"_a)
        .def("set_tma_labels",&Document::set_tma_labels,"name"_a="","row_labels"_a=std::vector<std::string>{},"column_labels"_a=std::vector<std::string>{},"rename_cores"_a=true)
        .def("clear_tma_grid",&Document::clear_tma_grid,"delete_cores"_a=false);
    m.def("tma_grid_edits",&tma_grid_edits,"options"_a=TmaGridOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("tma_metadata_csv",&tma_metadata_csv,"snapshot"_a,"byte_limit"_a=64*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("tma_context_csv",&tma_context_csv,"snapshot"_a,"table"_a,"byte_limit"_a=64*1024*1024,"require_revision"_a=true,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"))
        .def_rw("tma_open",&GuiOptions::tma_open).def_rw("tma_tab",&GuiOptions::tma_tab)
        .def_prop_rw("tma_options",[](const GuiOptions& o)->std::optional<std::shared_ptr<const TmaGridOptions>> {if(o.tma_options) return o.tma_options;return {};},[](GuiOptions& o,std::optional<std::shared_ptr<const TmaGridOptions>> v){o.tma_options=v?*v:nullptr;})
        .def_rw("tma_case_id",&GuiOptions::tma_case_id).def_rw("tma_metadata_path",&GuiOptions::tma_metadata_path)
        .def_rw("tma_translation",&GuiOptions::tma_translation).def_rw("tma_core_bounds",&GuiOptions::tma_core_bounds);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"))
        .def_ro("tma_error",&GuiReport::tma_error).def_ro("tma_export_state",&GuiReport::tma_export_state)
        .def_ro("tma_created_ids",&GuiReport::tma_created_ids).def_ro("tma_cores",&GuiReport::tma_cores)
        .def_ro("tma_missing_cores",&GuiReport::tma_missing_cores).def_ro("tma_catalog_max_step",&GuiReport::tma_catalog_max_step)
        .def_ro("tma_metadata_bytes",&GuiReport::tma_metadata_bytes);
}
