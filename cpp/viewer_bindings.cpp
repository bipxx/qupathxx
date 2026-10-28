#include "qupathxx/viewer_session.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/project_batch.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/typing.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_viewer_session(nb::module_& m) {
    using namespace qpx;
    nb::class_<ViewerLimits>(m,"ViewerLimits").def(nb::init<>())
        .def_rw("tickets",&ViewerLimits::tickets).def_rw("queued_bytes",&ViewerLimits::queued_bytes).def_rw("result_bytes",&ViewerLimits::result_bytes)
        .def_rw("command_bytes",&ViewerLimits::command_bytes).def_rw("max_edits",&ViewerLimits::max_edits).def_rw("region_bytes",&ViewerLimits::region_bytes).def_rw("snapshot_bytes",&ViewerLimits::snapshot_bytes).def("validate",&ViewerLimits::validate);
    nb::class_<ViewerEvent>(m,"ViewerEvent")
        .def_ro("ticket",&ViewerEvent::ticket).def_ro("epoch",&ViewerEvent::epoch).def_ro("image_id",&ViewerEvent::image_id).def_ro("object_id",&ViewerEvent::object_id).def_ro("record_id",&ViewerEvent::record_id).def_ro("document_revision",&ViewerEvent::document_revision)
        .def_ro("action",&ViewerEvent::action).def_ro("state",&ViewerEvent::state).def_ro("error",&ViewerEvent::error).def_ro("path",&ViewerEvent::path).def_ro("committed",&ViewerEvent::committed).def_ro("verified",&ViewerEvent::verified).def_ro("elapsed_ms",&ViewerEvent::elapsed_ms)
        .def_ro("object_ids",&ViewerEvent::object_ids).def_ro("images",&ViewerEvent::images).def_ro("object",&ViewerEvent::object).def_ro("metadata",&ViewerEvent::metadata)
        .def_ro("spatial",&ViewerEvent::spatial).def_ro("analysis",&ViewerEvent::analysis).def_ro("tissue",&ViewerEvent::tissue).def_ro("cells",&ViewerEvent::cells).def_ro("pixels",&ViewerEvent::pixels)
        .def_prop_ro("tma_dearray",[](const ViewerEvent& e)->std::optional<std::shared_ptr<const TmaDearrayResult>> {if(e.tma_dearray)return e.tma_dearray;return {};})
        .def_ro("raw_analysis",&ViewerEvent::raw_analysis).def_ro("raw_pixels",&ViewerEvent::raw_pixels).def_ro("raw_metadata",&ViewerEvent::raw_metadata)
        .def_ro("raw_pixel_training",&ViewerEvent::raw_pixel_training).def_ro("raw_pixel_checkpoint",&ViewerEvent::raw_pixel_checkpoint)
        .def_ro("raw_pixel_prediction",&ViewerEvent::raw_pixel_prediction).def_ro("raw_pixel_link",&ViewerEvent::raw_pixel_link).def_ro("raw_pixel_contours",&ViewerEvent::raw_pixel_contours).def_ro("raw_pixel_stream_contours",&ViewerEvent::raw_pixel_stream_contours).def_ro("raw_pixel_file_contours",&ViewerEvent::raw_pixel_file_contours)
        .def_prop_ro("tiff_view",[](const ViewerEvent& event){return event.tiff_view;})
        .def_ro("samples",&ViewerEvent::samples).def_ro("components",&ViewerEvent::components).def_ro("rows",&ViewerEvent::rows).def_ro("bytes",&ViewerEvent::bytes);
    nb::class_<ViewerStats>(m,"ViewerStats")
        .def_ro("state",&ViewerStats::state).def_ro("error",&ViewerStats::error).def_ro("project_identity",&ViewerStats::project_identity)
        .def_ro("epoch",&ViewerStats::epoch).def_ro("frame",&ViewerStats::frame).def_ro("active_image",&ViewerStats::active_image).def_ro("document_revision",&ViewerStats::document_revision).def_ro("active_ticket",&ViewerStats::active_ticket).def_ro("completed",&ViewerStats::completed)
        .def_ro("objects",&ViewerStats::objects).def_ro("queued",&ViewerStats::queued).def_ro("queued_bytes",&ViewerStats::queued_bytes).def_ro("retained_bytes",&ViewerStats::retained_bytes).def_ro("tickets",&ViewerStats::tickets).def_ro("max_command_ms",&ViewerStats::max_command_ms);
    nb::class_<ViewerSession>(m,"ViewerSession").def(nb::init<std::string,ViewerLimits>(),"project_path"_a,"limits"_a=ViewerLimits{})
        .def_prop_ro("project_path",&ViewerSession::project_path).def_prop_ro("limits",&ViewerSession::limits).def_prop_ro("stats",&ViewerSession::stats)
        .def("poll",&ViewerSession::poll,"ticket"_a).def("wait",&ViewerSession::wait,"ticket"_a,"timeout_seconds"_a=30.,nb::call_guard<nb::gil_scoped_release>()).def("take",&ViewerSession::take,"ticket"_a).def("cancel",&ViewerSession::cancel,"ticket"_a)
        .def("catalog",&ViewerSession::catalog,"after_id"_a=0,"limit"_a=64).def("inspect",&ViewerSession::inspect,"image_id"_a,"object_id"_a=0)
        .def("execute",[](ViewerSession& s,std::uint64_t image,nb::typed<nb::sequence,ObjectEdit> values,std::string label,std::optional<std::uint64_t> revision){if(nb::len(values)>s.limits().max_edits) throw std::length_error("viewer edit count exceeds limit");std::vector<ObjectEdit> edits;edits.reserve(nb::len(values));for(auto item:values) edits.push_back(nb::cast<ObjectEdit>(item));return s.execute(image,edits,std::move(label),revision);},"image_id"_a,"edits"_a,"label"_a="Python edits","expected_revision"_a=nb::none())
        .def("spatial_distances",[](ViewerSession& s,std::uint64_t image,nb::typed<nb::sequence,std::uint64_t> a,nb::typed<nb::sequence,std::uint64_t> b,SpatialDistanceOptions options,bool keep,std::optional<std::uint64_t> revision) {
            if(!nb::len(a) || nb::len(a)>100000 || nb::len(b)>100000)throw std::length_error("spatial viewer cohort count exceeds limit");
            std::vector<std::uint64_t> sources,targets;sources.reserve(nb::len(a));targets.reserve(nb::len(b));for(auto v:a)sources.push_back(nb::cast<std::uint64_t>(v));for(auto v:b)targets.push_back(nb::cast<std::uint64_t>(v));
            return s.spatial_distances(image,sources,targets,options,keep,revision);
        },"image_id"_a,"sources"_a,"targets"_a,"options"_a=SpatialDistanceOptions{},"keep"_a=false,"expected_revision"_a=nb::none())
        .def("inspect_spatial",&ViewerSession::inspect_spatial,"image_id"_a,"record_id"_a)
        .def("verify_spatial",&ViewerSession::verify_spatial,"image_id"_a,"record_id"_a)
        .def("export_spatial",&ViewerSession::export_spatial,"image_id"_a,"record_id"_a,"path"_a,"byte_limit"_a=64u*1024*1024)
        .def("insert_tma_row",&ViewerSession::insert_tma_row,"image_id"_a,"index"_a,"label"_a="","bounds"_a=nb::none(),"missing"_a=false,"expected_revision"_a=nb::none())
        .def("insert_tma_column",&ViewerSession::insert_tma_column,"image_id"_a,"index"_a,"label"_a="","bounds"_a=nb::none(),"missing"_a=false,"expected_revision"_a=nb::none())
        .def("remove_tma_row",&ViewerSession::remove_tma_row,"image_id"_a,"index"_a,"expected_revision"_a=nb::none())
        .def("remove_tma_column",&ViewerSession::remove_tma_column,"image_id"_a,"index"_a,"expected_revision"_a=nb::none())
        .def("detect_tma_grid",&ViewerSession::detect_tma_grid,"image_id"_a,"options"_a=TmaDearrayOptions{},"expected_revision"_a=nb::none())
        .def("select",&ViewerSession::select,"image_id"_a,"object_ids"_a).def("activate",&ViewerSession::activate,"image_id"_a).def("read_region",&ViewerSession::read_region,"image_id"_a,"request"_a)
        .def("inspect_raw",&ViewerSession::inspect_raw,"image_id"_a).def("read_raw",&ViewerSession::read_raw,"image_id"_a,"request"_a)
        .def("set_tiff_view",&ViewerSession::set_tiff_view,"image_id"_a,"view"_a)
        .def("analyze_raw",&ViewerSession::analyze_raw,"image_id"_a,"request"_a,"object_id"_a=0,"nucleus"_a=false,"retain_result"_a=false)
        .def("analyze_raw_grid",&ViewerSession::analyze_raw_grid,"image_id"_a,"object_id"_a,"sampling"_a=RawGridSampling{},"options"_a=ScientificRawOptions{},"nucleus"_a=false,"retain_result"_a=false)
        .def("verify_raw_analysis",&ViewerSession::verify_raw_analysis,"image_id"_a,"record_id"_a)
        .def("export_raw_measurements",&ViewerSession::export_raw_measurements,"image_id"_a,"path"_a,"require_current"_a=false,"record_ids"_a=std::vector<std::uint64_t>{})
        .def("train_raw_pixels",&ViewerSession::train_raw_pixels,"image_id"_a,"annotation_ids"_a,"settings"_a=RawPixelTrainingSettings{},"retain_result"_a=true)
        .def("predict_raw_pixels",&ViewerSession::predict_raw_pixels,"image_id"_a,"model"_a,"window"_a,"object_id"_a=0,"min_vote_fraction"_a=0.,"work_bytes"_a=16u*1024*1024)
        .def("export_raw_pixels",&ViewerSession::export_raw_pixels,"image_id"_a,"model"_a,"lattice"_a,"path"_a,"options"_a=RawPixelStreamOptions{},"save_options"_a=RawPixelStreamSaveOptions{},"object_id"_a=0,"retain_result"_a=true)
        .def("contour_raw_pixels",&ViewerSession::contour_raw_pixels,"image_id"_a,"model"_a,"grid"_a,"options"_a=TissueContourOptions{},"object_id"_a=0,"apply"_a=false,"min_vote_fraction"_a=0,"work_bytes"_a=16u*1024*1024,"name_prefix"_a="Raw pixel","expected_revision"_a=nb::none())
        .def("contour_raw_pixel_file",&ViewerSession::contour_raw_pixel_file,"image_id"_a,"path"_a,"limits"_a=RawPixelStreamFileLimits{},"options"_a=RawPixelStreamContourOptions{},"record_id"_a=0,"replay"_a=false,"apply"_a=false,"name_prefix"_a="Raw pixel","expected_revision"_a=nb::none())
        .def("contour_raw_pixel_stream",&ViewerSession::contour_raw_pixel_stream,"image_id"_a,"model"_a,"lattice"_a,"stream"_a=RawPixelStreamOptions{},"options"_a=RawPixelStreamContourOptions{},"region"_a=nb::none(),"object_id"_a=0,"apply"_a=false,"name_prefix"_a="Raw pixel","expected_revision"_a=nb::none())
        .def("read_raw_pixel_stream",&ViewerSession::read_raw_pixel_stream,"image_id"_a,"path"_a,"index"_a=0,"limits"_a=RawPixelStreamFileLimits{},"record_id"_a=0,"retain_result"_a=false)
        .def("analyze_with_sampling_policy",&ViewerSession::analyze_with_sampling_policy,"image_id"_a,"object_id"_a,"transform"_a,"sampling_policy"_a,"kind"_a=AnalysisKind::OpticalDensityMeans,"nucleus"_a=false)
        .def("analyze",&ViewerSession::analyze,"image_id"_a,"object_id"_a,"transform"_a,"kind"_a=AnalysisKind::OpticalDensityMeans,"nucleus"_a=false)
        .def("tissue",&ViewerSession::tissue,"image_id"_a,"object_id"_a,"transform"_a,"parameters"_a=TissueParameters{},"retain_result"_a=false)
        .def("cells",&ViewerSession::cells,"image_id"_a,"object_id"_a,"transform"_a,"parameters"_a=CellParameters{},"retain_result"_a=false)
        .def("grid_cells",&ViewerSession::grid_cells,"image_id"_a,"object_id"_a,"transform"_a,"sampling"_a=GridCellSampling{},"parameters"_a=GridCellParameters{},"retain_result"_a=false)
        .def("cells_with_nucleus_area_limits",&ViewerSession::cells_with_nucleus_area_limits,"image_id"_a,"object_id"_a,"transform"_a,
            "minimum_um2"_a=nb::none(),"maximum_um2"_a=nb::none(),"parameters"_a=CellParameters{},"retain_result"_a=false)
        .def("export_measurements",&ViewerSession::export_measurements,"image_id"_a,"kind"_a,"path"_a,"require_current"_a=false,"record_ids"_a=std::vector<std::uint64_t>{})
        .def("export_cells",&ViewerSession::export_cells,"image_id"_a,"path"_a,"require_current"_a=false,"record_ids"_a=std::vector<std::uint64_t>{}).def("save",&ViewerSession::save,"path"_a).def("close",&ViewerSession::close,"discard_changes"_a=false);
#ifdef QUPATHXX_HAS_GUI
    m.def("show",[](std::shared_ptr<ViewerSession> session,int frames,const std::string& capture,GuiOptions options) {
        const auto threading=nb::module_::import_("threading");if(!threading.attr("current_thread")().is(threading.attr("main_thread")())) throw std::runtime_error("managed viewer must run on the Python main thread");
        if(options.project) throw std::invalid_argument("managed viewer owns its project; GuiOptions.project is not allowed");
        // Detach optional parameter instances from Python-owned shared_ptr deleters
        // before releasing the GIL. The endpoint argument stays owned here.
        if(options.cell_parameters) options.cell_parameters=std::make_shared<const CellParameters>(*options.cell_parameters);
        if(options.cell_grid) options.cell_grid=std::make_shared<const GridCellSampling>(*options.cell_grid);
        if(options.grid_cell_parameters) options.grid_cell_parameters=std::make_shared<const GridCellParameters>(*options.grid_cell_parameters);
        if(options.stain_estimate_parameters) options.stain_estimate_parameters=std::make_shared<const StainEstimateParameters>(*options.stain_estimate_parameters);
        if(options.batch_options) options.batch_options=std::make_shared<const ProjectBatchOptions>(*options.batch_options);
        if(options.spatial) options.spatial=std::make_shared<const GuiSpatialOptions>(*options.spatial);
        if(options.raw_pixel) options.raw_pixel=std::make_shared<const GuiRawPixelOptions>(*options.raw_pixel);
        GuiReport report;{nb::gil_scoped_release release;report=run_viewer_session(session,frames,capture,options);}return report;
    },"session"_a,"frame_limit"_a=0,"capture_path"_a="","options"_a=GuiOptions{});
#else
    m.def("show",[](std::shared_ptr<ViewerSession>,int,const std::string&,const GuiOptions&)->GuiReport {throw std::runtime_error("rebuild with QUPATHXX_BUILD_GUI=ON");},"session"_a,"frame_limit"_a=0,"capture_path"_a="","options"_a=GuiOptions{});
#endif
}
