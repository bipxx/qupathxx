#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/raw_tables.hpp"
#include "qupathxx/source_identity.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_raw_analysis(nb::module_& module) {
    using namespace qpx;
    nb::class_<RawTmaContext>(module,"RawTmaContext")
        .def_ro("core_id",&RawTmaContext::core_id).def_ro("core_revision",&RawTmaContext::core_revision).def_ro("row",&RawTmaContext::row).def_ro("column",&RawTmaContext::column)
        .def_ro("missing",&RawTmaContext::missing).def_ro("grid_name",&RawTmaContext::grid_name).def_ro("row_label",&RawTmaContext::row_label).def_ro("column_label",&RawTmaContext::column_label).def_ro("core_name",&RawTmaContext::core_name).def_ro("case_id",&RawTmaContext::case_id);
    nb::class_<RawObjectContext>(module,"RawObjectContext")
        .def_ro("name",&RawObjectContext::name).def_ro("class_name",&RawObjectContext::class_name).def_ro("parent_id",&RawObjectContext::parent_id).def_ro("kind",&RawObjectContext::kind).def_ro("tma",&RawObjectContext::tma)
        .def_prop_ro("payload_bytes",&RawObjectContext::payload_bytes).def("same_metadata",&RawObjectContext::same_metadata,"other"_a);
    nb::class_<RawAnalysisRequest>(module,"RawAnalysisRequest")
        .def(nb::init<const Document&,RawRegionRequest,std::uint64_t,bool,std::size_t,std::uint64_t>(),"document"_a,"request"_a,"object_id"_a=0,"nucleus"_a=false,"tile_size"_a=256,"max_pixels"_a=1000000000)
        .def("matches",&RawAnalysisRequest::matches,"document"_a).def_static("replay",&RawAnalysisRequest::replay,"document"_a,"record_id"_a)
        .def_prop_ro("region",[](const RawAnalysisRequest& request){return request.region();})
        .def_prop_ro("object_id",&RawAnalysisRequest::object_id).def_prop_ro("object_revision",&RawAnalysisRequest::object_revision).def_prop_ro("document_revision",&RawAnalysisRequest::document_revision)
        .def_prop_ro("nucleus",&RawAnalysisRequest::nucleus).def_prop_ro("tile_size",&RawAnalysisRequest::tile_size).def_prop_ro("max_pixels",&RawAnalysisRequest::max_pixels).def_prop_ro("mask",&RawAnalysisRequest::mask)
        .def_prop_ro("object_context",&RawAnalysisRequest::object_context,nb::rv_policy::reference_internal);
    nb::class_<RawAnalysisMetadata>(module,"RawAnalysisMetadata")
        .def_ro("width",&RawAnalysisMetadata::width).def_ro("height",&RawAnalysisMetadata::height).def_ro("level_width",&RawAnalysisMetadata::level_width).def_ro("level_height",&RawAnalysisMetadata::level_height)
        .def_ro("channels",&RawAnalysisMetadata::channels).def_ro("z_planes",&RawAnalysisMetadata::z_planes).def_ro("timepoints",&RawAnalysisMetadata::timepoints).def_ro("series",&RawAnalysisMetadata::series)
        .def_ro("sample_type",&RawAnalysisMetadata::sample_type).def_ro("downsample",&RawAnalysisMetadata::downsample).def_ro("channel_names",&RawAnalysisMetadata::channel_names)
        .def_ro("z_spacing_um",&RawAnalysisMetadata::z_spacing_um).def_ro("time_interval_seconds",&RawAnalysisMetadata::time_interval_seconds).def_prop_ro("payload_bytes",&RawAnalysisMetadata::payload_bytes);
    nb::class_<RawAnalysisResult>(module,"RawAnalysisResult")
        .def_prop_ro("statistics",&RawAnalysisResult::statistics,nb::rv_policy::reference_internal)
        .def_prop_ro("metadata",&RawAnalysisResult::metadata,nb::rv_policy::reference_internal)
        .def_prop_ro("object_context",&RawAnalysisResult::object_context,nb::rv_policy::reference_internal)
        .def_prop_ro("region",[](const RawAnalysisResult& result){return result.region();})
        .def_prop_ro("object_id",&RawAnalysisResult::object_id).def_prop_ro("object_revision",&RawAnalysisResult::object_revision).def_prop_ro("document_revision",&RawAnalysisResult::document_revision)
        .def_prop_ro("nucleus",&RawAnalysisResult::nucleus).def_prop_ro("tile_size",&RawAnalysisResult::tile_size).def_prop_ro("tiles",&RawAnalysisResult::tiles).def_prop_ro("elapsed_ms",&RawAnalysisResult::elapsed_ms)
        .def_prop_ro("input_sha256",&RawAnalysisResult::input_sha256).def_prop_ro("algorithm",&RawAnalysisResult::algorithm)
        .def_prop_ro("pixel_width_um",&RawAnalysisResult::pixel_width_um).def_prop_ro("pixel_height_um",&RawAnalysisResult::pixel_height_um).def_prop_ro("source",&RawAnalysisResult::source).def_prop_ro("payload_bytes",&RawAnalysisResult::payload_bytes);
    nb::class_<RawAnalysisRecord>(module,"RawAnalysisRecord")
        .def_ro("id",&RawAnalysisRecord::id).def_ro("result",&RawAnalysisRecord::result).def_ro("mask",&RawAnalysisRecord::mask).def_ro("max_pixels",&RawAnalysisRecord::max_pixels).def_ro("requested_tile_size",&RawAnalysisRecord::requested_tile_size).def_prop_ro("payload_bytes",&RawAnalysisRecord::payload_bytes);
    nb::class_<RawInputDigest>(module,"RawInputDigest")
        .def_ro("input_sha256",&RawInputDigest::input_sha256).def_ro("tiles",&RawInputDigest::tiles).def_ro("raw_bytes",&RawInputDigest::raw_bytes);
    nb::class_<RawAnalysisStats>(module,"RawAnalysisStats")
        .def_ro("state",&RawAnalysisStats::state).def_ro("error",&RawAnalysisStats::error).def_ro("generation",&RawAnalysisStats::generation).def_ro("completed_tiles",&RawAnalysisStats::completed_tiles).def_ro("total_tiles",&RawAnalysisStats::total_tiles).def_ro("elapsed_ms",&RawAnalysisStats::elapsed_ms);
    nb::class_<RawAnalysisJob>(module,"RawAnalysisJob").def(nb::init<>())
        .def("request",&RawAnalysisJob::request,"request"_a).def("poll",&RawAnalysisJob::poll,"document"_a)
        .def("wait",[](RawAnalysisJob& job,const Document& document){ {nb::gil_scoped_release release;job.await_completion();}return job.poll(document);},"document"_a)
        .def("cancel",&RawAnalysisJob::cancel).def_prop_ro("busy",&RawAnalysisJob::busy).def_prop_ro("result",&RawAnalysisJob::result)
        .def("keep_result",&RawAnalysisJob::keep_result,"document"_a).def("verify_result",&RawAnalysisJob::verify_result,"document"_a,"record_id"_a);
    module.def("analyze_raw",[](const RawAnalysisRequest& request){return analyze_raw(request);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    module.def("raw_analysis_input_digest",[](const RawAnalysisRequest& request){return raw_analysis_input_digest(request);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<Document>>(module.attr("Document"))
        .def_prop_ro("raw_analysis_record_count",&Document::raw_analysis_record_count).def_prop_ro("raw_analysis_records",&Document::raw_analysis_records)
        .def("raw_analysis_page",&Document::raw_analysis_page,"after_id"_a=0,"limit"_a=64).def("raw_analysis_record",&Document::raw_analysis_record,"record_id"_a).def("latest_raw_analysis",&Document::latest_raw_analysis,"object_id"_a)
        .def("raw_analysis_status",&Document::raw_analysis_status,"record_id"_a).def("keep_raw_analysis",&Document::keep_raw_analysis,"request"_a,"result"_a)
        .def("verify_raw_analysis",&Document::verify_raw_analysis,"record_id"_a,"request"_a,"result"_a).def("remove_raw_analysis",&Document::remove_raw_analysis,"record_id"_a);
    nb::class_<RawTableRequest>(module,"RawTableRequest")
        .def(nb::init<const Document&,const std::vector<std::uint64_t>&,bool,std::size_t,const std::vector<std::string>&>(),"document"_a,"record_ids"_a=std::vector<std::uint64_t>{},"require_current"_a=false,"snapshot_bytes"_a=128u*1024*1024,"protected_paths"_a=std::vector<std::string>{})
        .def_prop_ro("rows",&RawTableRequest::rows).def_prop_ro("records",&RawTableRequest::records).def_prop_ro("payload_bytes",&RawTableRequest::payload_bytes);
    nb::class_<RawTableStats>(module,"RawTableStats")
        .def_ro("state",&RawTableStats::state).def_ro("error",&RawTableStats::error).def_ro("path",&RawTableStats::path).def_ro("generation",&RawTableStats::generation)
        .def_ro("completed_rows",&RawTableStats::completed_rows).def_ro("total_rows",&RawTableStats::total_rows).def_ro("bytes",&RawTableStats::bytes).def_ro("elapsed_ms",&RawTableStats::elapsed_ms);
    nb::class_<RawTableJob>(module,"RawTableJob").def(nb::init<>())
        .def("request",&RawTableJob::request,"request"_a,"path"_a,"replace_existing"_a=false,"max_bytes"_a=1024u*1024*1024).def("poll",&RawTableJob::poll)
        .def("wait",[](RawTableJob& job){ {nb::gil_scoped_release release;job.await_completion();}return job.poll();})
        .def("cancel",&RawTableJob::cancel).def_prop_ro("busy",&RawTableJob::busy);
    module.def("raw_analysis_csv",[](const RawTableRequest& request,std::size_t maximum){return raw_analysis_csv(request,maximum);},"request"_a,"max_bytes"_a=64u*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    module.def("save_raw_analysis_csv",[](const RawTableRequest& request,const std::string& path,bool replace,std::size_t maximum){return save_raw_analysis_csv(request,path,replace,maximum);},"request"_a,"path"_a,"replace_existing"_a=false,"max_bytes"_a=1024u*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<GuiOptions>>(module.attr("GuiOptions")).def_rw("raw_csv_path",&GuiOptions::raw_csv_path).def_rw("raw_record_id",&GuiOptions::raw_record_id).def_rw("raw_require_current",&GuiOptions::raw_require_current);
    nb::borrow<nb::class_<GuiReport>>(module.attr("GuiReport"))
        .def_ro("raw_analysis_result",&GuiReport::raw_analysis_result).def_ro("raw_analysis_state",&GuiReport::raw_analysis_state).def_ro("raw_record_id",&GuiReport::raw_record_id).def_ro("raw_record_status",&GuiReport::raw_record_status)
        .def_ro("raw_records",&GuiReport::raw_records).def_ro("raw_csv_state",&GuiReport::raw_csv_state).def_ro("raw_csv_rows",&GuiReport::raw_csv_rows).def_ro("raw_csv_bytes",&GuiReport::raw_csv_bytes);
}
