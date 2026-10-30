#include "job_binding_utils.hpp"
#include "qupathxx/cells.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_cells(nb::module_& m) {
    using namespace qpx;
    nb::enum_<CellAlgorithm>(m,"CellAlgorithm")
        .value("DISTANCE_WATERSHED",CellAlgorithm::DistanceWatershed).value("LOG_WATERSHED",CellAlgorithm::LogWatershed)
        .value("LOG_WATERSHED_BODIES",CellAlgorithm::LogWatershedBodies)
        .value("SAMPLED_LOG_WATERSHED_BODIES",CellAlgorithm::SampledLogWatershedBodies)
        .value("GRID_LOG_WATERSHED_BODIES",CellAlgorithm::GridLogWatershedBodies);
    nb::enum_<GridCellUnits>(m,"GridCellUnits").value("LEVEL_ZERO_PIXELS",GridCellUnits::LevelZeroPixels).value("MICROMETRES",GridCellUnits::Micrometres);
    nb::enum_<CellProcessingPolicy>(m,"CellProcessingPolicy").value("QUANTITATIVE",CellProcessingPolicy::Quantitative).value("IMAGEJ_154P",CellProcessingPolicy::ImageJ154p).value("QUPATH_070",CellProcessingPolicy::QuPath070).value("QUPATH_070_V2",CellProcessingPolicy::QuPath070V2);
    nb::class_<NominalCellRoiSampling>(m,"NominalCellRoiSampling").def(nb::init<>())
        .def_rw("downsample",&NominalCellRoiSampling::downsample).def_rw("context_x",&NominalCellRoiSampling::context_x)
        .def_rw("sampling",&NominalCellRoiSampling::sampling)
        .def_rw("context_y",&NominalCellRoiSampling::context_y).def_prop_ro("algorithm",&NominalCellRoiSampling::algorithm).def("validate",&NominalCellRoiSampling::validate);
    nb::class_<NominalCellSampling>(m,"NominalCellSampling")
        .def(nb::init<RgbNominalRequest,std::uint32_t,std::uint32_t>(),"request"_a,"context_x"_a=32,"context_y"_a=32)
        .def_prop_ro("source_request",[](const NominalCellSampling& n){return n.source_request();})
        .def_prop_ro("context_x",&NominalCellSampling::context_x).def_prop_ro("context_y",&NominalCellSampling::context_y)
        .def_prop_ro("core_width",&NominalCellSampling::core_width).def_prop_ro("core_height",&NominalCellSampling::core_height)
        .def_prop_ro("width",&NominalCellSampling::width).def_prop_ro("height",&NominalCellSampling::height)
        .def("edge_x",&NominalCellSampling::edge_x,"index"_a).def("edge_y",&NominalCellSampling::edge_y,"index"_a)
        .def("center_x",&NominalCellSampling::center_x,"index"_a).def("center_y",&NominalCellSampling::center_y,"index"_a);
    nb::class_<GridCellSampling>(m,"GridCellSampling").def(nb::init<>())
        .def_rw("samples_x",&GridCellSampling::samples_x).def_rw("samples_y",&GridCellSampling::samples_y)
        .def_rw("context_x",&GridCellSampling::context_x).def_rw("context_y",&GridCellSampling::context_y).def("validate",&GridCellSampling::validate);
    nb::class_<GridCellParameters>(m,"GridCellParameters").def(nb::init<>())
        .def_rw("processing",&GridCellParameters::processing)
        .def_rw("units",&GridCellParameters::units).def_rw("feature",&GridCellParameters::feature).def_rw("threshold",&GridCellParameters::threshold)
        .def_rw("sigma",&GridCellParameters::sigma).def_rw("log_prominence",&GridCellParameters::log_prominence)
        .def_rw("min_peak_radius",&GridCellParameters::min_peak_radius).def_rw("min_seed_distance",&GridCellParameters::min_seed_distance)
        .def_rw("cell_expansion",&GridCellParameters::cell_expansion).def_rw("min_nucleus_area",&GridCellParameters::min_nucleus_area).def_rw("max_nucleus_area",&GridCellParameters::max_nucleus_area)
        .def_rw("sampling_policy",&GridCellParameters::sampling_policy).def_rw("max_cells",&GridCellParameters::max_cells).def_rw("work_bytes",&GridCellParameters::work_bytes)
        .def_rw("contours",&GridCellParameters::contours).def("validate",&GridCellParameters::validate);
    nb::class_<ResolvedGridCellParameters>(m,"ResolvedGridCellParameters")
        .def_ro("step_x",&ResolvedGridCellParameters::step_x).def_ro("step_y",&ResolvedGridCellParameters::step_y)
        .def_ro("sigma_x",&ResolvedGridCellParameters::sigma_x).def_ro("sigma_y",&ResolvedGridCellParameters::sigma_y)
        .def_ro("min_nucleus_samples",&ResolvedGridCellParameters::min_nucleus_samples).def_ro("max_nucleus_samples",&ResolvedGridCellParameters::max_nucleus_samples);
    nb::class_<CellParameters>(m,"CellParameters").def(nb::init<>())
        .def(nb::init<CellAlgorithm>(),"algorithm"_a)
        .def_rw("algorithm",&CellParameters::algorithm).def_rw("log_prominence",&CellParameters::log_prominence).def_rw("context_padding",&CellParameters::context_padding)
        .def_rw("feature",&CellParameters::feature).def_rw("threshold",&CellParameters::threshold).def_rw("sigma",&CellParameters::sigma)
        .def_rw("min_peak_radius",&CellParameters::min_peak_radius).def_rw("min_seed_distance",&CellParameters::min_seed_distance).def_rw("cell_expansion",&CellParameters::cell_expansion)
        .def_rw("min_nucleus_pixels",&CellParameters::min_nucleus_pixels).def_rw("max_nucleus_pixels",&CellParameters::max_nucleus_pixels)
        .def_rw("connectivity",&CellParameters::connectivity).def_rw("level",&CellParameters::level).def_rw("tile_size",&CellParameters::tile_size)
        .def_rw("max_cells",&CellParameters::max_cells).def_rw("work_bytes",&CellParameters::work_bytes).def_rw("contours",&CellParameters::contours).def("validate",&CellParameters::validate);
    nb::class_<CellRequest>(m,"CellRequest").def(nb::init<const Document&,std::uint64_t,const StainTransform&,CellParameters>(),"document"_a,"object_id"_a,"transform"_a,"parameters"_a=CellParameters{})
        .def(nb::init<const Document&,std::uint64_t,const StainTransform&,ScientificGrid,GridCellParameters>(),"document"_a,"object_id"_a,"transform"_a,"grid"_a,"parameters"_a=GridCellParameters{})
        .def(nb::init<const Document&,std::uint64_t,const StainTransform&,NominalCellSampling,GridCellParameters>(),"document"_a,"object_id"_a,"transform"_a,"sampling"_a,"parameters"_a=GridCellParameters{})
        .def_static("for_nominal_roi",&CellRequest::for_nominal_roi,"document"_a,"object_id"_a,"transform"_a,"downsample"_a,
            "context_x"_a=32,"context_y"_a=32,"parameters"_a=GridCellParameters{},"sampling"_a=RgbNominalSampling::IntegerOrigin)
        .def_static("for_grid_roi",&CellRequest::for_grid_roi,"document"_a,"object_id"_a,"transform"_a,"sampling"_a=GridCellSampling{},"parameters"_a=GridCellParameters{})
        .def_prop_ro("scientific_grid",[](const CellRequest& r){return r.scientific_grid()?std::optional<ScientificGrid>(*r.scientific_grid()):std::nullopt;})
        .def_prop_ro("nominal_sampling",[](const CellRequest& r){return r.nominal_sampling()?std::optional<NominalCellSampling>(*r.nominal_sampling()):std::nullopt;})
        .def_prop_ro("grid_parameters",[](const CellRequest& r){return r.grid_parameters()?std::optional<GridCellParameters>(*r.grid_parameters()):std::nullopt;})
        .def_prop_ro("resolved_grid_parameters",[](const CellRequest& r){return r.resolved_grid_parameters()?std::optional<ResolvedGridCellParameters>(*r.resolved_grid_parameters()):std::nullopt;})
        .def_static("replay",&CellRequest::replay,"document"_a,"record_id"_a)
        .def_static("with_nucleus_area_limits",&CellRequest::with_nucleus_area_limits,"document"_a,"object_id"_a,"transform"_a,
            "minimum_um2"_a=nb::none(),"maximum_um2"_a=nb::none(),"parameters"_a=CellParameters{})
        .def("matches",&CellRequest::matches).def_prop_ro("parameters",[](const CellRequest& r){return r.parameters();})
        .def_prop_ro("object_id",&CellRequest::object_id).def_prop_ro("level",&CellRequest::level).def_prop_ro("downsample",&CellRequest::downsample)
        .def_prop_ro("bounds",&CellRequest::bounds).def_prop_ro("level_bounds",&CellRequest::level_bounds).def_prop_ro("roi",&CellRequest::roi)
        .def_prop_ro("total_tiles",&CellRequest::total_tiles).def_prop_ro("estimated_work_bytes",&CellRequest::estimated_work_bytes);
    nb::class_<CellMarker>(m,"CellMarker").def_ro("label",&CellMarker::label).def_ro("result_label",&CellMarker::result_label)
        .def_ro("pixel_index",&CellMarker::pixel_index).def_prop_ro("center",[](const CellMarker& m){return m.center;}).def_ro("radius",&CellMarker::radius);
    nb::class_<CellFeature>(m,"CellFeature").def_ro("label",&CellFeature::label).def_ro("nucleus_pixels",&CellFeature::nucleus_pixels).def_ro("cell_pixels",&CellFeature::cell_pixels)
        .def_ro("nucleus_rgb",&CellFeature::nucleus_rgb).def_ro("cell_rgb",&CellFeature::cell_rgb).def_ro("cytoplasm_rgb",&CellFeature::cytoplasm_rgb)
        .def_ro("nucleus_channels",&CellFeature::nucleus_channels).def_ro("cell_channels",&CellFeature::cell_channels).def_ro("cytoplasm_channels",&CellFeature::cytoplasm_channels);
    nb::class_<CalibratedCellFeature>(m,"CalibratedCellFeature").def_ro("label",&CalibratedCellFeature::label)
        .def_ro("pixel_width_um",&CalibratedCellFeature::pixel_width_um).def_ro("pixel_height_um",&CalibratedCellFeature::pixel_height_um)
        .def_ro("nucleus_sample_area_um2",&CalibratedCellFeature::nucleus_sample_area_um2)
        .def_ro("cell_sample_area_um2",&CalibratedCellFeature::cell_sample_area_um2)
        .def_ro("cytoplasm_sample_area_um2",&CalibratedCellFeature::cytoplasm_sample_area_um2)
        .def_prop_ro("nucleus_centroid_um",[](const CalibratedCellFeature& v){return v.nucleus_centroid_um;})
        .def_prop_ro("cell_centroid_um",[](const CalibratedCellFeature& v){return v.cell_centroid_um;});
    nb::class_<CellResult>(m,"CellResult").def_prop_ro("algorithm",&CellResult::algorithm).def_prop_ro("parameters",[](const CellResult& r){return r.parameters();})
        .def_prop_ro("scientific_grid",[](const CellResult& r){const auto* grid=r.nuclei()->scientific_grid();return grid?std::optional<ScientificGrid>(*grid):std::nullopt;})
        .def_prop_ro("nominal_sampling",[](const CellResult& r){const auto* n=r.nuclei()->nominal_sampling();return n?std::optional<NominalCellSampling>(*n):std::nullopt;})
        .def_prop_ro("grid_parameters",[](const CellResult& r){return r.grid_parameters()?std::optional<GridCellParameters>(*r.grid_parameters()):std::nullopt;})
        .def_prop_ro("resolved_grid_parameters",[](const CellResult& r){return r.resolved_grid_parameters()?std::optional<ResolvedGridCellParameters>(*r.resolved_grid_parameters()):std::nullopt;})
        .def("calibrated_feature",&CellResult::calibrated_feature,"label"_a)
        .def_prop_ro("nuclei",&CellResult::nuclei).def_prop_ro("cells",&CellResult::cells)
        .def_prop_ro("nucleus_contours",&CellResult::nucleus_contours).def_prop_ro("cell_contours",&CellResult::cell_contours)
        .def_prop_ro("features",[](const CellResult& r){return r.features();}).def_prop_ro("markers",[](const CellResult& r){return r.markers();})
        .def_prop_ro("cell_count",[](const CellResult& r){return r.features().size();}).def_prop_ro("raw_peaks",&CellResult::raw_peaks)
        .def_prop_ro("elapsed_ms",&CellResult::elapsed_ms).def_prop_ro("retained_bytes",&CellResult::retained_bytes);
    m.def("detect_cells",[](const CellRequest& r){return detect_cells(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<CellRecord>(m,"CellRecord").def_ro("id",&CellRecord::id).def_ro("producer",&CellRecord::producer)
        .def_ro("result",&CellRecord::result).def_prop_ro("payload_bytes",&CellRecord::payload_bytes);
    nb::class_<CellFeatureLink>(m,"CellFeatureLink").def_ro("record_id",&CellFeatureLink::record_id).def_ro("label",&CellFeatureLink::label);
    auto annotation=nb::borrow<nb::class_<Annotation>>(m.attr("Annotation"));
    annotation.def_ro("cell_feature_link",&Annotation::cell_feature_link);
    auto document=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    document.def_prop_ro("cell_records",&Document::cell_records).def_prop_ro("cell_record_count",&Document::cell_record_count)
        .def("cell_record",&Document::cell_record).def("latest_cells",&Document::latest_cells).def("cell_status",&Document::cell_status)
        .def("publish_cells",&Document::publish_cells,"request"_a,"result"_a,"name_prefix"_a="Cell","class_name"_a="")
        .def("publish_cell_record",&Document::publish_cell_record,"record_id"_a,"name_prefix"_a="Cell","class_name"_a="")
        .def("cell_object_feature",&Document::cell_object_feature).def("cell_object_status",&Document::cell_object_status)
        .def("keep_cells",&Document::keep_cells).def("verify_cells",&Document::verify_cells).def("remove_cells",&Document::remove_cells);
    nb::class_<CellStats,AnalysisStats>(m,"CellStats").def_ro("phase",&CellStats::phase);
    using PythonCellJob=qpx::bindings::OwnerJobHolder<CellJob>;
    nb::class_<PythonCellJob>(m,"CellJob").def(nb::init<>()).def("request",qpx::bindings::owner_job_method<&CellJob::request>())
        .def("poll",qpx::bindings::owner_job_method<&CellJob::poll>(),"document"_a,"selected_id"_a).def("wait",[](PythonCellJob& job,const Document& doc,std::uint64_t selected){return qpx::bindings::wait(job,doc,selected);},"document"_a,"selected_id"_a)
        .def("cancel",qpx::bindings::owner_job_method<&CellJob::cancel,false>()).def_prop_ro("busy",qpx::bindings::owner_job_method<&CellJob::busy>()).def_prop_ro("result",qpx::bindings::owner_job_method<&CellJob::result>()).def_prop_ro("published",qpx::bindings::owner_job_method<&CellJob::published>())
        .def("keep_result",qpx::bindings::owner_job_method<&CellJob::keep_result>(),"document"_a,"selected_id"_a,"name_prefix"_a="Cell","class_name"_a="")
        .def("keep_record",qpx::bindings::owner_job_method<&CellJob::keep_record>(),"document"_a,"selected_id"_a).def("verify_record",qpx::bindings::owner_job_method<&CellJob::verify_record>(),"document"_a,"record_id"_a);
    auto options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    options.def_prop_rw("cell_parameters",[](const GuiOptions& o){return o.cell_parameters?*o.cell_parameters:CellParameters{};},[](GuiOptions& o,CellParameters p){p.validate();o.cell_parameters=std::make_shared<const CellParameters>(std::move(p));})
        .def_prop_rw("cell_nominal",[](const GuiOptions& o){return o.cell_nominal?std::optional<NominalCellRoiSampling>(*o.cell_nominal):std::nullopt;},[](GuiOptions& o,std::optional<NominalCellRoiSampling> p){if(p) p->validate();o.cell_nominal=p?std::make_shared<const NominalCellRoiSampling>(*p):nullptr;})
        .def_prop_rw("cell_grid",[](const GuiOptions& o){return o.cell_grid?std::optional<GridCellSampling>(*o.cell_grid):std::nullopt;},[](GuiOptions& o,std::optional<GridCellSampling> p){if(p) p->validate();o.cell_grid=p?std::make_shared<const GridCellSampling>(*p):nullptr;})
        .def_prop_rw("grid_cell_parameters",[](const GuiOptions& o){return o.grid_cell_parameters?*o.grid_cell_parameters:GridCellParameters{};},[](GuiOptions& o,GridCellParameters p){p.validate();o.grid_cell_parameters=std::make_shared<const GridCellParameters>(std::move(p));})
        .def_rw("cell_min_nucleus_area_um2",&GuiOptions::cell_min_nucleus_area_um2)
        .def_rw("cell_max_nucleus_area_um2",&GuiOptions::cell_max_nucleus_area_um2);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("cell_state",&GuiReport::cell_state).def_ro("cell_error",&GuiReport::cell_error).def_ro("cell_result",&GuiReport::cell_result)
        .def_ro("cell_annotations_added",&GuiReport::cell_annotations_added).def_ro("cell_gpu_bytes",&GuiReport::cell_gpu_bytes)
        .def_ro("cell_preview_ready",&GuiReport::cell_preview_ready).def_ro("cells_published",&GuiReport::cells_published)
        .def_ro("saved_cell_id",&GuiReport::saved_cell_id).def_ro("saved_cell_status",&GuiReport::saved_cell_status);
}
