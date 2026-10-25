#include "qupathxx/tissue.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;
using namespace nb::literals;
void bind_tissue(nb::module_& m) {
    using namespace qpx;
    nb::enum_<TissueFeature>(m,"TissueFeature").value("OPTICAL_DENSITY_SUM",TissueFeature::OpticalDensitySum)
        .value("STAIN_1",TissueFeature::Stain1).value("STAIN_2",TissueFeature::Stain2).value("STAIN_3",TissueFeature::Stain3);
    nb::class_<TissueParameters>(m,"TissueParameters").def(nb::init<>()).def_rw("feature",&TissueParameters::feature)
        .def_rw("sampling_policy",&TissueParameters::sampling_policy)
        .def_rw("threshold",&TissueParameters::threshold).def_rw("connectivity",&TissueParameters::connectivity)
        .def_rw("level",&TissueParameters::level)
        .def_rw("open_radius",&TissueParameters::open_radius).def_rw("close_radius",&TissueParameters::close_radius)
        .def_rw("fill_holes",&TissueParameters::fill_holes).def_rw("max_hole_pixels",&TissueParameters::max_hole_pixels)
        .def_rw("min_pixels",&TissueParameters::min_pixels).def_rw("max_pixels",&TissueParameters::max_pixels)
        .def_rw("tile_size",&TissueParameters::tile_size).def_rw("max_components",&TissueParameters::max_components).def_rw("work_bytes",&TissueParameters::work_bytes)
        .def("validate",&TissueParameters::validate);
    nb::class_<TissueRequest>(m,"TissueRequest")
        .def_static("replay",&TissueRequest::replay,"document"_a,"record_id"_a)
        .def(nb::init<const Document&,std::uint64_t,const StainTransform&,TissueParameters,bool>(),"document"_a,"object_id"_a,"transform"_a,"parameters"_a=TissueParameters{},"nucleus"_a=false)
        .def("matches",&TissueRequest::matches).def_prop_ro("object_id",&TissueRequest::object_id)
        .def_prop_ro("parameters",[](const TissueRequest& r){return r.parameters();})
        .def_prop_ro("level",&TissueRequest::level).def_prop_ro("downsample",&TissueRequest::downsample)
        .def_prop_ro("bounds",&TissueRequest::bounds).def_prop_ro("level_bounds",&TissueRequest::level_bounds)
        .def_prop_ro("roi",&TissueRequest::roi).def_prop_ro("total_tiles",&TissueRequest::total_tiles).def_prop_ro("estimated_work_bytes",&TissueRequest::estimated_work_bytes);
    nb::class_<TissueComponent>(m,"TissueComponent").def_ro("label",&TissueComponent::label).def_ro("pixels",&TissueComponent::pixels)
        .def_prop_ro("bounds",[](const TissueComponent& c){return c.bounds;}).def_prop_ro("centroid",[](const TissueComponent& c){return c.centroid;});
    nb::class_<TissueResult>(m,"TissueResult")
        .def_prop_ro("algorithm",&TissueResult::algorithm).def_prop_ro("input_sha256",&TissueResult::input_sha256)
        .def_prop_ro("bounds",&TissueResult::bounds).def_prop_ro("width",&TissueResult::width).def_prop_ro("height",&TissueResult::height)
        .def_prop_ro("level_bounds",&TissueResult::level_bounds).def_prop_ro("level",&TissueResult::level)
        .def_prop_ro("downsample",&TissueResult::downsample).def_prop_ro("input_format",&TissueResult::input_format)
        .def_prop_ro("downsample_x",&TissueResult::downsample_x).def_prop_ro("downsample_y",&TissueResult::downsample_y)
        .def_prop_ro("scientific_grid",[](const TissueResult& r){return r.scientific_grid()?std::optional<ScientificGrid>(*r.scientific_grid()):std::nullopt;})
        .def("sample_edge_x",&TissueResult::sample_edge_x,"index"_a).def("sample_edge_y",&TissueResult::sample_edge_y,"index"_a)
        .def_prop_ro("roi",&TissueResult::roi).def_prop_ro("image_bounds",&TissueResult::image_bounds)
        .def_prop_ro("labels",[](const TissueResult& r){return nb::bytes(reinterpret_cast<const char*>(r.labels().data()),r.labels().size()*sizeof(std::uint32_t));})
        .def_prop_ro("mask",[](const TissueResult& r){std::string bytes(r.labels().size(),'\0');for(std::size_t i=0;i<bytes.size();++i) bytes[i]=r.labels()[i]?static_cast<char>(255):0;return nb::bytes(bytes.data(),bytes.size());})
        .def_prop_ro("components",[](const TissueResult& r){return std::vector<TissueComponent>(r.components());})
        .def_prop_ro("component_count",[](const TissueResult& r){return r.components().size();})
        .def_prop_ro("parameters",[](const TissueResult& r){return r.parameters();})
        .def_prop_ro("white",&TissueResult::white).def_prop_ro("stains",&TissueResult::stains)
        .def_prop_ro("object_id",&TissueResult::object_id).def_prop_ro("object_revision",&TissueResult::object_revision).def_prop_ro("document_revision",&TissueResult::document_revision)
        .def_prop_ro("nucleus",&TissueResult::nucleus).def_prop_ro("samples",&TissueResult::samples)
        .def_prop_ro("threshold_pixels",&TissueResult::threshold_pixels).def_prop_ro("foreground_pixels",&TissueResult::foreground_pixels)
        .def_prop_ro("cleaned_pixels",&TissueResult::cleaned_pixels).def_prop_ro("filled_pixels",&TissueResult::filled_pixels).def_prop_ro("cleanup_ms",&TissueResult::cleanup_ms)
        .def_prop_ro("raw_components",&TissueResult::raw_components).def_prop_ro("elapsed_ms",&TissueResult::elapsed_ms)
        .def_prop_ro("estimated_work_bytes",&TissueResult::estimated_work_bytes).def_prop_ro("retained_bytes",&TissueResult::retained_bytes);
    m.def("detect_tissue",[](const TissueRequest& r){return detect_tissue(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<TissueContourOptions>(m,"TissueContourOptions").def(nb::init<>())
        .def_rw("clip_to_roi",&TissueContourOptions::clip_to_roi).def_rw("max_components",&TissueContourOptions::max_components)
        .def_rw("max_edges",&TissueContourOptions::max_edges).def_rw("max_vertices",&TissueContourOptions::max_vertices).def_rw("work_bytes",&TissueContourOptions::work_bytes).def("validate",&TissueContourOptions::validate);
    nb::class_<TissueContour>(m,"TissueContour").def_ro("label",&TissueContour::label).def_ro("pixels",&TissueContour::pixels).def_ro("geometry",&TissueContour::geometry);
    nb::class_<TissueContours>(m,"TissueContours")
        .def_prop_ro("algorithm",&TissueContours::algorithm).def_prop_ro("input_sha256",&TissueContours::input_sha256).def_prop_ro("object_id",&TissueContours::object_id)
        .def_prop_ro("options",[](const TissueContours& c){return c.options();}).def_prop_ro("components",[](const TissueContours& c){return std::vector<TissueContour>(c.components());})
        .def_prop_ro("component_count",[](const TissueContours& c){return c.components().size();}).def_prop_ro("edge_count",&TissueContours::edge_count).def_prop_ro("vertex_count",&TissueContours::vertex_count)
        .def_prop_ro("elapsed_ms",&TissueContours::elapsed_ms).def_prop_ro("estimated_work_bytes",&TissueContours::estimated_work_bytes).def_prop_ro("retained_bytes",&TissueContours::retained_bytes);
    m.def("trace_tissue",[](const TissueResult& mask,TissueContourOptions options){return trace_tissue(mask,options);},"result"_a,"options"_a=TissueContourOptions{},nb::call_guard<nb::gil_scoped_release>());
    nb::class_<TissueRecord>(m,"TissueRecord")
        .def_ro("id",&TissueRecord::id).def_ro("result",&TissueRecord::result).def_ro("producer",&TissueRecord::producer)
        .def_prop_ro("payload_bytes",&TissueRecord::payload_bytes);
    auto document=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    document.def("keep_tissue",&Document::keep_tissue,"request"_a,"result"_a)
        .def("verify_tissue",&Document::verify_tissue,"record_id"_a,"request"_a,"result"_a)
        .def("remove_tissue",&Document::remove_tissue,"record_id"_a)
        .def("tissue_record",&Document::tissue_record,"record_id"_a).def("latest_tissue",&Document::latest_tissue,"object_id"_a)
        .def("tissue_status",&Document::tissue_status,"record_id"_a)
        .def_prop_ro("tissue_records",&Document::tissue_records).def_prop_ro("tissue_record_count",&Document::tissue_record_count);
    nb::class_<TissueStats,AnalysisStats>(m,"TissueStats").def_ro("phase",&TissueStats::phase);
    nb::class_<TissueJob>(m,"TissueJob").def(nb::init<>()).def("request",&TissueJob::request)
        .def("poll",&TissueJob::poll,"document"_a,"selected_id"_a).def("wait",&TissueJob::wait,"document"_a,"selected_id"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("cancel",&TissueJob::cancel).def_prop_ro("busy",&TissueJob::busy).def_prop_ro("result",&TissueJob::result)
        .def("request_contours",&TissueJob::request_contours,"options"_a=TissueContourOptions{}).def_prop_ro("contours",&TissueJob::contours).def_prop_ro("contours_published",&TissueJob::contours_published)
        .def("keep_contours",&TissueJob::keep_contours,"document"_a,"selected_id"_a,"name_prefix"_a="Tissue","class_name"_a="");
    auto job=nb::borrow<nb::class_<TissueJob>>(m.attr("TissueJob"));
    job.def("keep_result",&TissueJob::keep_result,"document"_a,"selected_id"_a).def("verify_result",&TissueJob::verify_result,"document"_a,"record_id"_a);
}
