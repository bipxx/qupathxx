#include "qupathxx/tma.hpp"
#include "qupathxx/qptma.hpp"
#include "qupathxx/tma_dearray.hpp"
#include "qupathxx/project.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <stdexcept>
namespace nb=nanobind;using namespace nb::literals;
namespace {
std::string qptma_python_text(const nb::str& text,std::size_t limit) {
    if(nb::len(text)>limit)throw std::length_error("qptma text exceeds byte limit");
    Py_ssize_t bytes=0;const auto raw=PyUnicode_AsUTF8AndSize(text.ptr(),&bytes);if(!raw)throw nb::python_error();
    if(static_cast<std::size_t>(bytes)>limit)throw std::length_error("qptma text exceeds byte limit");
    return {raw,static_cast<std::size_t>(bytes)};
}
}

void bind_tma(nb::module_& m) {
    using namespace qpx;
    nb::enum_<TmaPolarity>(m,"TmaPolarity").value("DarkOnLight",TmaPolarity::DarkOnLight).value("LightOnDark",TmaPolarity::LightOnDark);
    nb::class_<TmaDearrayOptions>(m,"TmaDearrayOptions").def(nb::init<>())
        .def_rw("rows",&TmaDearrayOptions::rows).def_rw("columns",&TmaDearrayOptions::columns)
        .def_rw("core_width",&TmaDearrayOptions::core_width).def_rw("core_height",&TmaDearrayOptions::core_height)
        .def_rw("bounds_scale",&TmaDearrayOptions::bounds_scale).def_rw("missing_density",&TmaDearrayOptions::missing_density)
        .def_rw("min_valid_fraction",&TmaDearrayOptions::min_valid_fraction)
        .def_rw("min_component_area",&TmaDearrayOptions::min_component_area).def_rw("max_component_area",&TmaDearrayOptions::max_component_area)
        .def_rw("min_component_extent",&TmaDearrayOptions::min_component_extent).def_rw("max_fit_error",&TmaDearrayOptions::max_fit_error)
        .def_rw("max_rotation_degrees",&TmaDearrayOptions::max_rotation_degrees).def_rw("polarity",&TmaDearrayOptions::polarity)
        .def_rw("thumbnail",&TmaDearrayOptions::thumbnail).def_rw("max_candidates",&TmaDearrayOptions::max_candidates)
        .def_rw("work_bytes",&TmaDearrayOptions::work_bytes).def_rw("pair_checks",&TmaDearrayOptions::pair_checks)
        .def_rw("name",&TmaDearrayOptions::name).def("validate",&TmaDearrayOptions::validate);
    nb::class_<TmaDearrayCore>(m,"TmaDearrayCore")
        .def_prop_ro("center",[](const TmaDearrayCore& c){return c.center;})
        .def_ro("tissue_density",&TmaDearrayCore::tissue_density).def_ro("valid_fraction",&TmaDearrayCore::valid_fraction)
        .def_ro("observed",&TmaDearrayCore::observed).def_ro("fit_error",&TmaDearrayCore::fit_error);
    nb::class_<TmaDearrayResult>(m,"TmaDearrayResult")
        .def_prop_ro("grid",[](const TmaDearrayResult& r){return r.grid;})
        .def_prop_ro("cores",[](const TmaDearrayResult& r){return r.cores;})
        .def_ro("image_width",&TmaDearrayResult::image_width).def_ro("image_height",&TmaDearrayResult::image_height)
        .def_ro("preview_width",&TmaDearrayResult::preview_width).def_ro("preview_height",&TmaDearrayResult::preview_height)
        .def_ro("candidates",&TmaDearrayResult::candidates).def_ro("estimated_work_bytes",&TmaDearrayResult::estimated_work_bytes)
        .def_ro("lattice_rmse",&TmaDearrayResult::lattice_rmse).def_ro("threshold",&TmaDearrayResult::threshold)
        .def_ro("input_sha256",&TmaDearrayResult::input_sha256).def_ro("recipe_sha256",&TmaDearrayResult::recipe_sha256)
        .def_prop_ro("thumbnail_plan",[](const TmaDearrayResult& r){return r.thumbnail_plan;});
    m.def("tma_dearray_work_bytes",&tma_dearray_work_bytes,"preview_width"_a,"preview_height"_a,"options"_a);
    m.def("detect_tma_grid",[](const PixelBuffer& pixels,std::int64_t width,std::int64_t height,TmaDearrayOptions options) {
        nb::gil_scoped_release release;return detect_tma_grid(pixels,width,height,std::move(options));
    },"preview"_a,"image_width"_a,"image_height"_a,"options"_a=TmaDearrayOptions{});
    m.def("detect_tma_grid",[](const ImageServer& server,TmaDearrayOptions options) {
        nb::gil_scoped_release release;return detect_tma_grid(server,std::move(options));
    },"server"_a,"options"_a=TmaDearrayOptions{});
    nb::borrow<nb::class_<Project>>(m.attr("Project")).def_static("open_without_journal_recovery",&Project::open_without_journal_recovery,"path"_a).def("save_new",[](Project& project,const std::string& path,std::size_t snapshot_bytes,std::uint64_t file_bytes) {
        project.save_new(path,snapshot_bytes,file_bytes);
    },"path"_a,"snapshot_bytes"_a=512ull*1024*1024,"file_bytes"_a=1024ull*1024*1024);
    nb::class_<QptmaCore>(m,"QptmaCore").def(nb::init<>())
        .def_rw("name",&QptmaCore::name).def_rw("case_id",&QptmaCore::case_id)
        .def_rw("bounds",&QptmaCore::bounds).def_rw("missing",&QptmaCore::missing).def("validate",&QptmaCore::validate);
    nb::class_<QptmaData>(m,"QptmaData").def(nb::init<>())
        .def_rw("source_hint",&QptmaData::source_hint).def_rw("image_name",&QptmaData::image_name)
        .def_rw("rows",&QptmaData::rows).def_rw("columns",&QptmaData::columns)
        .def_prop_rw("cores",[](const QptmaData& data){return data.cores;},[](QptmaData& data,const nb::list& values){
            if(nb::len(values)>10000)throw std::length_error("qptma core limit exceeded");
            std::vector<QptmaCore> cores;cores.reserve(nb::len(values));
            for(auto value:values) {const auto& core=nb::cast<const QptmaCore&>(value);core.validate();cores.push_back(core);}data.cores=std::move(cores);
        },nb::rv_policy::copy)
        .def("validate",&QptmaData::validate);
    nb::class_<QptmaExport>(m,"QptmaExport")
        .def_prop_ro("data",[](const QptmaExport& report){return report.data;})
        .def_prop_ro("omitted_metadata",[](const QptmaExport& report){return report.omitted_metadata;});
    m.def("parse_qptma",[](const nb::str& text,std::size_t limit) {
        if(!limit || limit>64*1024*1024)throw std::invalid_argument("qptma byte limit must be 1..64 MiB");
        auto input=qptma_python_text(text,limit);nb::gil_scoped_release release;return parse_qptma(input,limit);
    },
        "text"_a,"byte_limit"_a=8*1024*1024);
    m.def("qptma_text",[](const QptmaData& value,std::size_t limit) {value.validate();auto data=value;nb::gil_scoped_release release;return qptma_text(data,limit);},
        "data"_a,"byte_limit"_a=8*1024*1024);
    m.def("load_qptma",[](const nb::str& path,std::size_t limit) {auto input=qptma_python_text(path,32768);nb::gil_scoped_release release;return load_qptma(input,limit);},
        "path"_a,"byte_limit"_a=8*1024*1024);
    m.def("save_qptma",[](const QptmaData& value,const nb::str& path,std::size_t limit) {value.validate();auto output=qptma_python_text(path,32768);auto data=value;nb::gil_scoped_release release;return save_qptma(data,output,limit);},
        "data"_a,"path"_a,"byte_limit"_a=8*1024*1024);
    m.def("qptma_from_grid",&qptma_from_grid,"snapshot"_a,"source_hint"_a="","image_name"_a="",nb::call_guard<nb::gil_scoped_release>());
    m.def("qptma_grid_edits",[](const QptmaData& value,const std::string& name,const std::vector<std::string>& rows,const std::vector<std::string>& columns) {
        value.validate();auto data=value;nb::gil_scoped_release release;return qptma_grid_edits(data,name,rows,columns);
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
        .def("insert_tma_row",&Document::insert_tma_row,"index"_a,"label"_a="","bounds"_a=nb::none(),"missing"_a=false)
        .def("insert_tma_column",&Document::insert_tma_column,"index"_a,"label"_a="","bounds"_a=nb::none(),"missing"_a=false)
        .def("remove_tma_row",&Document::remove_tma_row,"index"_a).def("remove_tma_column",&Document::remove_tma_column,"index"_a)
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
        .def_rw("qptma_input_path",&GuiOptions::qptma_input_path).def_rw("qptma_output_path",&GuiOptions::qptma_output_path)
        .def_rw("tma_open",&GuiOptions::tma_open).def_rw("tma_tab",&GuiOptions::tma_tab)
        .def_prop_rw("tma_options",[](const GuiOptions& o)->std::optional<std::shared_ptr<const TmaGridOptions>> {if(o.tma_options) return o.tma_options;return {};},[](GuiOptions& o,std::optional<std::shared_ptr<const TmaGridOptions>> v){o.tma_options=v?*v:nullptr;})
        .def_rw("tma_insert_label",&GuiOptions::tma_insert_label).def_rw("tma_insert_cores",&GuiOptions::tma_insert_cores).def_rw("tma_insert_missing",&GuiOptions::tma_insert_missing)
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
        .def_ro("tma_metadata_bytes",&GuiReport::tma_metadata_bytes)
        .def_ro("qptma_state",&GuiReport::qptma_state).def_ro("qptma_preview_cores",&GuiReport::qptma_preview_cores)
        .def_ro("qptma_preview_missing",&GuiReport::qptma_preview_missing).def_ro("qptma_preview_max_step",&GuiReport::qptma_preview_max_step)
        .def_ro("qptma_export_bytes",&GuiReport::qptma_export_bytes);
}
