#include "qupathxx/tma.hpp"
#include "qupathxx/qptma.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_tma(nb::module_& m) {
    using namespace qpx;
    nb::class_<QptmaCore>(m,"QptmaCore").def(nb::init<>())
        .def_rw("name",&QptmaCore::name).def_rw("case_id",&QptmaCore::case_id)
        .def_rw("bounds",&QptmaCore::bounds).def_rw("missing",&QptmaCore::missing);
    nb::class_<QptmaData>(m,"QptmaData").def(nb::init<>())
        .def_rw("source_hint",&QptmaData::source_hint).def_rw("image_name",&QptmaData::image_name)
        .def_rw("rows",&QptmaData::rows).def_rw("columns",&QptmaData::columns)
        .def_prop_rw("cores",[](const QptmaData& data){return data.cores;},[](QptmaData& data,std::vector<QptmaCore> cores){data.cores=std::move(cores);},nb::rv_policy::copy)
        .def("validate",&QptmaData::validate);
    nb::class_<QptmaExport>(m,"QptmaExport")
        .def_prop_ro("data",[](const QptmaExport& report){return report.data;})
        .def_prop_ro("omitted_metadata",[](const QptmaExport& report){return report.omitted_metadata;});
    m.def("parse_qptma",[](const std::string& text,std::size_t limit) {nb::gil_scoped_release release;return parse_qptma(text,limit);},
        "text"_a,"byte_limit"_a=8*1024*1024);
    m.def("qptma_text",[](const QptmaData& value,std::size_t limit) {auto data=value;nb::gil_scoped_release release;return qptma_text(data,limit);},
        "data"_a,"byte_limit"_a=8*1024*1024);
    m.def("load_qptma",[](const std::string& path,std::size_t limit) {nb::gil_scoped_release release;return load_qptma(path,limit);},
        "path"_a,"byte_limit"_a=8*1024*1024);
    m.def("save_qptma",[](const QptmaData& value,const std::string& path,std::size_t limit) {auto data=value;nb::gil_scoped_release release;return save_qptma(data,path,limit);},
        "data"_a,"path"_a,"byte_limit"_a=8*1024*1024);
    m.def("qptma_from_grid",&qptma_from_grid,"snapshot"_a,"source_hint"_a="","image_name"_a="",nb::call_guard<nb::gil_scoped_release>());
    m.def("qptma_grid_edits",[](const QptmaData& value,const std::string& name,const std::vector<std::string>& rows,const std::vector<std::string>& columns) {
        auto data=value;nb::gil_scoped_release release;return qptma_grid_edits(data,name,rows,columns);
    },"data"_a,"grid_name"_a="Imported TMA","row_labels"_a=std::vector<std::string>{},"column_labels"_a=std::vector<std::string>{});
    m.def("import_qptma",&import_qptma,"document"_a,"data"_a,"grid_name"_a="Imported TMA",
        "row_labels"_a=std::vector<std::string>{},"column_labels"_a=std::vector<std::string>{});
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
    m.def("tma_case_summary_csv",[](const TmaGrid& snapshot,const std::string& table,const std::string& value,const std::string& weight,std::size_t limit,bool unversioned) {
        return tma_case_summary_csv(snapshot,table,value,weight,limit,unversioned);
    },"snapshot"_a,"table"_a="","value_column"_a="","weight_column"_a="","byte_limit"_a=64*1024*1024,"allow_unversioned"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("tma_cohort_manifest_csv",[](const TmaCohort& snapshots,std::size_t limit) {
        return tma_cohort_manifest_csv(snapshots,limit);
    },"snapshots"_a,"byte_limit"_a=64*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("tma_cohort_case_summary_csv",[](const TmaCohort& snapshots,const std::string& table,const std::string& value,const std::string& weight,std::size_t limit,bool unversioned) {
        return tma_cohort_case_summary_csv(snapshots,table,value,weight,limit,unversioned);
    },"snapshots"_a,"table"_a="","value_column"_a="","weight_column"_a="","byte_limit"_a=64*1024*1024,"allow_unversioned"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_tma_cohort_case_summary_csv",[](const TmaCohort& snapshots,const std::string& output,const std::string& input,const std::string& value,const std::string& weight,std::size_t limit,bool unversioned) {
        return save_tma_cohort_case_summary_csv(snapshots,output,input,value,weight,limit,unversioned);
    },"snapshots"_a,"output_path"_a,"input_path"_a="","value_column"_a="","weight_column"_a="","byte_limit"_a=64*1024*1024,"allow_unversioned"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_tma_case_summary_csv",[](const TmaGrid& snapshot,const std::string& output,const std::string& input,const std::string& value,const std::string& weight,std::size_t limit,bool unversioned) {
        return save_tma_case_summary_csv(snapshot,output,input,value,weight,limit,unversioned);
    },"snapshot"_a,"output_path"_a,"input_path"_a="","value_column"_a="","weight_column"_a="","byte_limit"_a=64*1024*1024,"allow_unversioned"_a=false,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"))
        .def_rw("tma_open",&GuiOptions::tma_open).def_rw("tma_tab",&GuiOptions::tma_tab)
        .def_prop_rw("tma_options",[](const GuiOptions& o)->std::optional<std::shared_ptr<const TmaGridOptions>> {if(o.tma_options) return o.tma_options;return {};},[](GuiOptions& o,std::optional<std::shared_ptr<const TmaGridOptions>> v){o.tma_options=v?*v:nullptr;})
        .def_rw("tma_case_id",&GuiOptions::tma_case_id).def_rw("tma_metadata_path",&GuiOptions::tma_metadata_path)
        .def_rw("tma_case_cohort",&GuiOptions::tma_case_cohort)
        .def_rw("tma_case_summary",&GuiOptions::tma_case_summary).def_rw("tma_measurements_path",&GuiOptions::tma_measurements_path)
        .def_rw("tma_value_column",&GuiOptions::tma_value_column).def_rw("tma_weight_column",&GuiOptions::tma_weight_column)
        .def_rw("tma_allow_unversioned",&GuiOptions::tma_allow_unversioned)
        .def_rw("tma_translation",&GuiOptions::tma_translation).def_rw("tma_core_bounds",&GuiOptions::tma_core_bounds);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"))
        .def_ro("tma_error",&GuiReport::tma_error).def_ro("tma_export_state",&GuiReport::tma_export_state)
        .def_ro("tma_created_ids",&GuiReport::tma_created_ids).def_ro("tma_cores",&GuiReport::tma_cores)
        .def_ro("tma_missing_cores",&GuiReport::tma_missing_cores).def_ro("tma_catalog_max_step",&GuiReport::tma_catalog_max_step)
        .def_ro("tma_metadata_bytes",&GuiReport::tma_metadata_bytes);
}
