#include "qupathxx/project_batch.hpp"
#include "qupathxx/input_digest.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_project_batch(nb::module_& m) {
    using namespace qpx;
    nb::enum_<ProjectBatchKind>(m,"ProjectBatchKind").value("ROI",ProjectBatchKind::Roi).value("TISSUE",ProjectBatchKind::Tissue).value("CELLS",ProjectBatchKind::Cells).value("PIXELS",ProjectBatchKind::Pixels);
    nb::enum_<ProjectBatchFailure>(m,"ProjectBatchFailure").value("STOP",ProjectBatchFailure::Stop).value("CONTINUE",ProjectBatchFailure::Continue);
    nb::class_<ProjectBatchOptions>(m,"ProjectBatchOptions").def(nb::init<>())
        .def_rw("kind",&ProjectBatchOptions::kind).def_rw("failure",&ProjectBatchOptions::failure).def_rw("roi_kind",&ProjectBatchOptions::roi_kind).def_rw("object_kind",&ProjectBatchOptions::object_kind)
        .def_rw("nucleus",&ProjectBatchOptions::nucleus).def_rw("resume",&ProjectBatchOptions::resume).def_rw("publish_cells",&ProjectBatchOptions::publish_cells)
        .def_rw("roi_tile_size",&ProjectBatchOptions::roi_tile_size).def_rw("snapshot_budget",&ProjectBatchOptions::snapshot_budget).def_rw("materialize_budget",&ProjectBatchOptions::materialize_budget)
        .def_rw("result_budget",&ProjectBatchOptions::result_budget).def_rw("csv_limit",&ProjectBatchOptions::csv_limit).def_rw("checkpoint_limit",&ProjectBatchOptions::checkpoint_limit)
        .def_rw("pixel_model",&ProjectBatchOptions::pixel_model).def_rw("pixels",&ProjectBatchOptions::pixels).def_rw("pixel_file_limit",&ProjectBatchOptions::pixel_file_limit).def_rw("pixel_total_limit",&ProjectBatchOptions::pixel_total_limit)
        .def_rw("tissue",&ProjectBatchOptions::tissue).def_rw("cells",&ProjectBatchOptions::cells).def("validate",&ProjectBatchOptions::validate);
    nb::class_<ProjectBatchSelection>(m,"ProjectBatchSelection").def(nb::init<>()).def_rw("image_id",&ProjectBatchSelection::image_id).def_rw("object_ids",&ProjectBatchSelection::object_ids);
    nb::class_<ProjectBatchRequest>(m,"ProjectBatchRequest")
        .def(nb::init<const Project&,const StainTransform&,const std::vector<ProjectBatchSelection>&,ProjectBatchOptions>(),"project"_a,"transform"_a,"selection"_a=std::vector<ProjectBatchSelection>{},"options"_a=ProjectBatchOptions{})
        .def_prop_ro("image_count",&ProjectBatchRequest::image_count).def_prop_ro("object_count",&ProjectBatchRequest::object_count).def_prop_ro("snapshot_bytes",&ProjectBatchRequest::snapshot_bytes)
        .def_prop_ro("capture_ms",&ProjectBatchRequest::capture_ms).def_prop_ro("parameters_json",&ProjectBatchRequest::parameters_json).def_prop_ro("parameters_sha256",&ProjectBatchRequest::parameters_sha256);
    nb::class_<ProjectBatchImageResult>(m,"ProjectBatchImageResult")
        .def_ro("image_id",&ProjectBatchImageResult::image_id).def_ro("image_name",&ProjectBatchImageResult::image_name).def_ro("state",&ProjectBatchImageResult::state)
        .def_ro("path",&ProjectBatchImageResult::path).def_ro("error",&ProjectBatchImageResult::error).def_ro("input_sha256",&ProjectBatchImageResult::input_sha256).def_ro("pixel_sha256",&ProjectBatchImageResult::pixel_sha256)
        .def_ro("inferred_blocks",&ProjectBatchImageResult::inferred_blocks).def_ro("hashed_blocks",&ProjectBatchImageResult::hashed_blocks).def_ro("verified_blocks",&ProjectBatchImageResult::verified_blocks).def_ro("stream_bytes",&ProjectBatchImageResult::stream_bytes)
        .def_ro("objects",&ProjectBatchImageResult::objects).def_ro("records",&ProjectBatchImageResult::records).def_ro("elapsed_ms",&ProjectBatchImageResult::elapsed_ms);
    nb::class_<ProjectBatchReport>(m,"ProjectBatchReport")
        .def_ro("state",&ProjectBatchReport::state).def_ro("error",&ProjectBatchReport::error).def_ro("report_path",&ProjectBatchReport::report_path).def_ro("parameters_sha256",&ProjectBatchReport::parameters_sha256)
        .def_ro("source_identity",&ProjectBatchReport::source_identity).def_ro("source_generation",&ProjectBatchReport::source_generation).def_ro("source_revision",&ProjectBatchReport::source_revision)
        .def_ro("saved",&ProjectBatchReport::saved).def_ro("reused",&ProjectBatchReport::reused).def_ro("failed",&ProjectBatchReport::failed).def_ro("snapshot_bytes",&ProjectBatchReport::snapshot_bytes)
        .def_ro("elapsed_ms",&ProjectBatchReport::elapsed_ms).def_ro("capture_ms",&ProjectBatchReport::capture_ms).def_ro("images",&ProjectBatchReport::images);
    nb::class_<ProjectBatchStats>(m,"ProjectBatchStats")
        .def_ro("state",&ProjectBatchStats::state).def_ro("phase",&ProjectBatchStats::phase).def_ro("error",&ProjectBatchStats::error).def_ro("image_id",&ProjectBatchStats::image_id)
        .def_ro("completed_images",&ProjectBatchStats::completed_images).def_ro("total_images",&ProjectBatchStats::total_images).def_ro("completed_objects",&ProjectBatchStats::completed_objects).def_ro("total_objects",&ProjectBatchStats::total_objects)
        .def_ro("capture_ms",&ProjectBatchStats::capture_ms).def_ro("elapsed_ms",&ProjectBatchStats::elapsed_ms);
    nb::class_<ProjectBatchJob>(m,"ProjectBatchJob").def(nb::init<>()).def("request",&ProjectBatchJob::request).def("poll",&ProjectBatchJob::poll)
        // Keep owner-side request destruction under the GIL: workers borrow it.
        .def("wait",&ProjectBatchJob::wait).def("cancel",&ProjectBatchJob::cancel).def_prop_ro("busy",&ProjectBatchJob::busy).def_prop_ro("result",&ProjectBatchJob::result);
    m.def("run_project_batch",[](const ProjectBatchRequest& r,const std::string& out){return run_project_batch(r,out);},"request"_a,"output_directory"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("project_batch_image_json",&project_batch_image_json);
    m.def("project_batch_report_json",&project_batch_report_json);
    m.def("batch_checkpoint_csv",&batch_checkpoint_csv,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_batch_checkpoint_csv",&save_batch_checkpoint_csv,"checkpoint"_a,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("input_sha256",[](const AnalysisRequest& r){return input_sha256(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("input_sha256",[](const TissueRequest& r){return input_sha256(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions")).def_rw("batch_output_directory",&GuiOptions::batch_output_directory)
        .def_prop_rw("batch_options",[](const GuiOptions& o)->std::optional<std::shared_ptr<const ProjectBatchOptions>> {
            if(o.batch_options) return o.batch_options;return std::nullopt;
        },[](GuiOptions& o,std::optional<std::shared_ptr<const ProjectBatchOptions>> value){o.batch_options=value?*value:nullptr;})
        .def_rw("batch_selected_objects",&GuiOptions::batch_selected_objects);
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport")).def_ro("batch_state",&GuiReport::batch_state)
        .def_ro("batch_error",&GuiReport::batch_error).def_prop_ro("batch_result",[](const GuiReport& r)->std::optional<std::shared_ptr<const ProjectBatchReport>> {
            if(r.batch_result) return r.batch_result;return std::nullopt;
        });
    m.def("input_sha256",[](const CellRequest& r){return input_sha256(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
}
