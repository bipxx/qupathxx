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
        .value("LOG_WATERSHED_BODIES",CellAlgorithm::LogWatershedBodies);
    nb::class_<CellParameters>(m,"CellParameters").def(nb::init<>())
        .def_rw("algorithm",&CellParameters::algorithm).def_rw("log_prominence",&CellParameters::log_prominence).def_rw("context_padding",&CellParameters::context_padding)
        .def_rw("feature",&CellParameters::feature).def_rw("threshold",&CellParameters::threshold).def_rw("sigma",&CellParameters::sigma)
        .def_rw("min_peak_radius",&CellParameters::min_peak_radius).def_rw("min_seed_distance",&CellParameters::min_seed_distance).def_rw("cell_expansion",&CellParameters::cell_expansion)
        .def_rw("min_nucleus_pixels",&CellParameters::min_nucleus_pixels).def_rw("max_nucleus_pixels",&CellParameters::max_nucleus_pixels)
        .def_rw("connectivity",&CellParameters::connectivity).def_rw("level",&CellParameters::level).def_rw("tile_size",&CellParameters::tile_size)
        .def_rw("max_cells",&CellParameters::max_cells).def_rw("work_bytes",&CellParameters::work_bytes).def_rw("contours",&CellParameters::contours).def("validate",&CellParameters::validate);
    nb::class_<CellRequest>(m,"CellRequest").def(nb::init<const Document&,std::uint64_t,const StainTransform&,CellParameters>(),"document"_a,"object_id"_a,"transform"_a,"parameters"_a=CellParameters{})
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
    nb::class_<CellJob>(m,"CellJob").def(nb::init<>()).def("request",&CellJob::request)
        .def("poll",&CellJob::poll,"document"_a,"selected_id"_a).def("wait",&CellJob::wait,"document"_a,"selected_id"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("cancel",&CellJob::cancel).def_prop_ro("busy",&CellJob::busy).def_prop_ro("result",&CellJob::result).def_prop_ro("published",&CellJob::published)
        .def("keep_result",&CellJob::keep_result,"document"_a,"selected_id"_a,"name_prefix"_a="Cell","class_name"_a="")
        .def("keep_record",&CellJob::keep_record,"document"_a,"selected_id"_a).def("verify_record",&CellJob::verify_record,"document"_a,"record_id"_a);
    auto options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    options.def_prop_rw("cell_parameters",[](const GuiOptions& o){return o.cell_parameters?*o.cell_parameters:CellParameters{};},[](GuiOptions& o,CellParameters p){p.validate();o.cell_parameters=std::make_shared<const CellParameters>(std::move(p));})
        .def_rw("cell_min_nucleus_area_um2",&GuiOptions::cell_min_nucleus_area_um2)
        .def_rw("cell_max_nucleus_area_um2",&GuiOptions::cell_max_nucleus_area_um2);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("cell_state",&GuiReport::cell_state).def_ro("cell_error",&GuiReport::cell_error).def_ro("cell_result",&GuiReport::cell_result)
        .def_ro("cell_annotations_added",&GuiReport::cell_annotations_added).def_ro("cell_gpu_bytes",&GuiReport::cell_gpu_bytes)
        .def_ro("cell_preview_ready",&GuiReport::cell_preview_ready).def_ro("cells_published",&GuiReport::cells_published)
        .def_ro("saved_cell_id",&GuiReport::saved_cell_id).def_ro("saved_cell_status",&GuiReport::saved_cell_status);
}
