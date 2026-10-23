#include "qupathxx/core.hpp"
#include "qupathxx/thumbnail.hpp"
#include "qupathxx/analysis.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/interchange.hpp"
#include "qupathxx/stains.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <stdexcept>
namespace nb = nanobind;
using namespace nb::literals;
void bind_scheduler(nb::module_&);
void bind_analysis(nb::module_&);
void bind_stain_estimation(nb::module_&);
void bind_project_batch(nb::module_&);
void bind_arrays(nb::module_&);
void bind_thumbnails(nb::module_&);
void bind_classifiers(nb::module_&);
void bind_viewer_session(nb::module_&);
void bind_tissue(nb::module_&);
void bind_cells(nb::module_&);
void bind_cell_tables(nb::module_&);
void bind_measurement_tables(nb::module_&);
void bind_provenance(nb::module_&);
void bind_file_fingerprint(nb::module_&);
NB_MODULE(_native, m) {
    using namespace qpx;
    m.def("optical_density",&optical_density,"intensity"_a,"white"_a=255.);
    nb::class_<FloatRgbImage>(m,"FloatRgbImage")
        .def_prop_ro("width",&FloatRgbImage::width).def_prop_ro("height",&FloatRgbImage::height)
        .def_prop_ro("row_stride",&FloatRgbImage::row_stride)
        .def_prop_ro("data",[](const FloatRgbImage& p) {
            return nb::bytes(reinterpret_cast<const char*>(p.values().data()),p.values().size()*sizeof(float));
        });
    nb::class_<StainTransform>(m,"StainTransform")
        .def(nb::init<const std::vector<ColorVector>&,ColorVector>(),"stains"_a,"white"_a=ColorVector{255,255,255})
        .def_prop_ro("stains",&StainTransform::stains).def_prop_ro("white",&StainTransform::white)
        .def("optical_densities",[](const StainTransform& t,const PixelBuffer& p){return t.optical_densities(p);},
            "pixels"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("deconvolve",[](const StainTransform& t,const PixelBuffer& p){return t.deconvolve(p);},
            "pixels"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<Point>(m, "Point").def(nb::init<>()).def(nb::init<double, double>()).def_rw("x", &Point::x).def_rw("y", &Point::y);
    nb::class_<Rect>(m, "Rect").def(nb::init<>()).def(nb::init<double,double,double,double>())
        .def_rw("x", &Rect::x).def_rw("y", &Rect::y).def_rw("width", &Rect::width).def_rw("height", &Rect::height)
        .def_prop_ro("area", &Rect::area).def("contains", &Rect::contains);
    nb::enum_<RoiKind>(m,"RoiKind").value("RECTANGLE",RoiKind::Rectangle).value("ELLIPSE",RoiKind::Ellipse)
        .value("POLYGON",RoiKind::Polygon).value("MULTIPOLYGON",RoiKind::MultiPolygon).value("POLYLINE",RoiKind::Polyline).value("POINTS",RoiKind::Points);
    nb::enum_<BooleanOp>(m,"BooleanOp").value("UNION",BooleanOp::Union).value("INTERSECTION",BooleanOp::Intersection)
        .value("DIFFERENCE",BooleanOp::Difference).value("SYMMETRIC_DIFFERENCE",BooleanOp::SymmetricDifference);
    nb::class_<PolygonPart>(m,"PolygonPart").def(nb::init<Ring,std::vector<Ring>>(),"shell"_a,"holes"_a=std::vector<Ring>{})
        .def_ro("shell",&PolygonPart::shell).def_ro("holes",&PolygonPart::holes);
    nb::class_<Roi>(m,"Roi").def_static("rectangle",&Roi::rectangle).def_static("ellipse",&Roi::ellipse)
        .def_static("polygon",&Roi::polygon,"shell"_a,"holes"_a=std::vector<Ring>{})
        .def_static("multipolygon",&Roi::multipolygon).def_static("polyline",&Roi::polyline).def_static("points",&Roi::points)
        .def_prop_ro("kind",&Roi::kind).def_prop_ro("bounds",&Roi::bounds)
        .def_prop_ro("area",&Roi::area).def_prop_ro("perimeter",&Roi::perimeter).def_prop_ro("is_area",&Roi::is_area)
        .def_prop_ro("wkt",&Roi::wkt).def_prop_ro("paths",&Roi::paths).def_prop_ro("polygon_parts",&Roi::polygon_parts)
        .def("contains",&Roi::contains,nb::call_guard<nb::gil_scoped_release>())
        .def("covers",&Roi::covers,nb::call_guard<nb::gil_scoped_release>())
        .def("distance",&Roi::distance,nb::call_guard<nb::gil_scoped_release>())
        .def("as_polygon",&Roi::as_polygon)
        .def("with_vertex",&Roi::with_vertex,"path"_a,"vertex"_a,"point"_a)
        .def("inserted_vertex",&Roi::inserted_vertex,"path"_a,"vertex"_a,"point"_a)
        .def("without_vertex",&Roi::without_vertex,"path"_a,"vertex"_a)
        .def("with_hole",&Roi::with_hole,"part"_a,"hole"_a).def("without_hole",&Roi::without_hole,"part"_a,"hole"_a)
        .def("boolean_operation",&Roi::boolean_operation,"other"_a,"operation"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("translated",&Roi::translated).def("equals",&Roi::equals);
    m.attr("has_geos")=has_geos(); m.attr("geos_version")=geos_version();
    nb::class_<SpatialStats>(m,"SpatialStats").def_ro("builds",&SpatialStats::builds)
        .def_ro("base_entries",&SpatialStats::base_entries).def_ro("dirty_entries",&SpatialStats::dirty_entries);
    nb::enum_<ObjectKind>(m,"ObjectKind").value("ANNOTATION",ObjectKind::Annotation)
        .value("DETECTION",ObjectKind::Detection).value("CELL",ObjectKind::Cell);
    nb::class_<ObjectClass>(m,"ObjectClass").def_ro("name",&ObjectClass::name).def_ro("color",&ObjectClass::color);
    nb::class_<Annotation>(m, "Annotation").def_ro("id", &Annotation::id).def_ro("name", &Annotation::name)
        .def_prop_ro("roi", [](const Annotation& a) { return a.geometry.bounds(); })
        .def_prop_ro("geometry",[](const Annotation& a){return a.geometry;})
        .def_ro("kind",&Annotation::kind).def_prop_ro("is_detection",&Annotation::is_detection)
        .def_prop_ro("nucleus",[](const Annotation& a){return a.nucleus;}).def_prop_ro("bounds",&Annotation::bounds)
        .def_ro("parent_id",&Annotation::parent_id).def_ro("class_name",&Annotation::class_name).def_ro("revision",&Annotation::revision);
    nb::class_<ObjectEdit>(m,"ObjectEdit")
        .def_static("create",&ObjectEdit::create,"roi"_a,"name"_a="Annotation","parent_id"_a=0,"class_name"_a="")
        .def_static("replace",&ObjectEdit::replace,"id"_a,"roi"_a,"name"_a,"parent_id"_a=0,"class_name"_a="")
        .def_static("create_geometry",&ObjectEdit::create_geometry,"geometry"_a,"name"_a="Annotation","parent_id"_a=0,"class_name"_a="","kind"_a=ObjectKind::Annotation,"nucleus"_a=nb::none())
        .def_static("replace_geometry",&ObjectEdit::replace_geometry,"id"_a,"geometry"_a,"name"_a,"parent_id"_a=0,"class_name"_a="","kind"_a=ObjectKind::Annotation,"nucleus"_a=nb::none())
        .def_static("remove",&ObjectEdit::remove);
    nb::class_<Image>(m, "Image").def(nb::init<int,int,std::vector<std::uint8_t>>(), "width"_a, "height"_a, "rgb"_a)
        .def_static("demo", &Image::demo).def_static("read_ppm", &Image::read_ppm)
        .def_prop_ro("width", &Image::width).def_prop_ro("height", &Image::height);
    nb::enum_<PixelType>(m, "PixelType").value("UINT8", PixelType::UInt8);
    nb::enum_<PixelLayout>(m, "PixelLayout").value("RGBA", PixelLayout::RGBA);
    nb::class_<ImageLevel>(m, "ImageLevel")
        .def_ro("width", &ImageLevel::width).def_ro("height", &ImageLevel::height).def_ro("downsample", &ImageLevel::downsample);
    nb::class_<ImageMetadata>(m, "ImageMetadata")
        .def_ro("width", &ImageMetadata::width).def_ro("height", &ImageMetadata::height)
        .def_prop_ro("levels", [](const ImageMetadata& v) { return v.levels; })
        .def_ro("pixel_width_um", &ImageMetadata::pixel_width_um).def_ro("pixel_height_um", &ImageMetadata::pixel_height_um)
        .def_ro("vendor", &ImageMetadata::vendor).def_ro("pixel_type", &ImageMetadata::pixel_type).def_ro("layout", &ImageMetadata::layout);
    nb::class_<RegionRequest>(m, "RegionRequest")
        .def(nb::init<std::int64_t,std::int64_t,std::int32_t,std::int64_t,std::int64_t>(), "x"_a, "y"_a, "level"_a, "width"_a, "height"_a)
        .def_ro("x", &RegionRequest::x).def_ro("y", &RegionRequest::y).def_ro("level", &RegionRequest::level)
        .def_ro("width", &RegionRequest::width).def_ro("height", &RegionRequest::height);
    nb::class_<PixelBuffer>(m, "PixelBuffer")
        .def_prop_ro("width", &PixelBuffer::width).def_prop_ro("height", &PixelBuffer::height)
        .def_prop_ro("row_stride", &PixelBuffer::row_stride)
        .def_prop_ro("pixel_type", [](const PixelBuffer&) { return PixelType::UInt8; })
        .def_prop_ro("layout", [](const PixelBuffer&) { return PixelLayout::RGBA; })
        .def_prop_ro("data", [](const PixelBuffer& p) { return nb::bytes(reinterpret_cast<const char*>(p.bytes().data()), p.bytes().size()); });
    nb::class_<ImageServer>(m, "ImageServer")
        .def_prop_ro("metadata", [](const ImageServer& s) { return s.metadata(); })
        .def("read_region", &ImageServer::read_region, "request"_a, nb::call_guard<nb::gil_scoped_release>())
        .def("close", &ImageServer::close, nb::call_guard<nb::gil_scoped_release>())
        .def_prop_ro("closed", &ImageServer::closed);
    m.def("dense_image_server", &dense_image_server);
    m.def("open_slide", &open_slide, "path"_a, nb::call_guard<nb::gil_scoped_release>());
    m.attr("has_openslide") = has_openslide();
    bind_scheduler(m);
    nb::class_<RoiMeasurements>(m,"RoiMeasurements")
        .def_ro("area_px2",&RoiMeasurements::area_px2).def_ro("length_px",&RoiMeasurements::length_px)
        .def_ro("area_um2",&RoiMeasurements::area_um2).def_ro("length_um",&RoiMeasurements::length_um);
    nb::class_<ShapeMeasurements>(m,"ShapeMeasurements")
        .def_ro("roi",&ShapeMeasurements::roi).def_ro("nucleus",&ShapeMeasurements::nucleus);
    nb::class_<FeatureCacheStats>(m,"FeatureCacheStats")
        .def_ro("entries",&FeatureCacheStats::entries).def_ro("capacity",&FeatureCacheStats::capacity)
        .def_ro("hits",&FeatureCacheStats::hits).def_ro("misses",&FeatureCacheStats::misses);
    nb::class_<Document>(m, "Document").def(nb::init<Image>())
        .def_prop_ro("analysis_records",&Document::analysis_records).def("analysis_record",&Document::analysis_record)
        .def_prop_ro("analysis_record_count",&Document::analysis_record_count)
        .def("latest_analysis",&Document::latest_analysis).def("analysis_status",&Document::analysis_status)
        .def("keep_analysis",&Document::keep_analysis).def("verify_analysis",&Document::verify_analysis).def("remove_analysis",&Document::remove_analysis)
        .def(nb::init<std::shared_ptr<ImageServer>>())
        .def_prop_ro("image", [](const Document& d) {return d.image();})
        .def_prop_ro("server", &Document::server)
        .def_prop_ro("metadata", [](const Document& d) { return d.metadata(); })
        .def_prop_ro("annotations", [](const Document& d) { return d.annotations(); })
        .def("add_rectangle", &Document::add_rectangle, "roi"_a, "name"_a = "Annotation")
        .def("add_roi",&Document::add_roi,"roi"_a,"name"_a="Annotation").def("update_roi",&Document::update_roi)
        .def("add_detection",&Document::add_detection,"roi"_a,"name"_a="Detection","parent_id"_a=0,"class_name"_a="")
        .def("add_cell",&Document::add_cell,"roi"_a,"nucleus"_a=nb::none(),"name"_a="Cell","parent_id"_a=0,"class_name"_a="")
        .def("update_nucleus",&Document::update_nucleus,"id"_a,"nucleus"_a)
        .def("nucleus_area_um2",&Document::nucleus_area_um2).def("nucleus_mean_intensity",&Document::nucleus_mean_intensity)
        .def("edit_vertex",&Document::edit_vertex,"id"_a,"path"_a,"vertex"_a,"point"_a)
        .def("insert_vertex",&Document::insert_vertex,"id"_a,"path"_a,"vertex"_a,"point"_a)
        .def("remove_vertex",&Document::remove_vertex,"id"_a,"path"_a,"vertex"_a)
        .def("add_hole",&Document::add_hole,"id"_a,"part"_a,"hole"_a)
        .def("remove_hole",&Document::remove_hole,"id"_a,"part"_a,"hole"_a)
        .def("apply_boolean",&Document::apply_boolean,"target"_a,"operands"_a,"operation"_a)
        .def("remove_annotation", &Document::remove_annotation).def("remove_objects",&Document::remove_objects)
        .def("annotation",&Document::annotation)
        .def_prop_ro("object_count",&Document::object_count).def_prop_ro("revision",&Document::revision)
        .def("update_rectangle",&Document::update_rectangle).def("rename_annotation",&Document::rename_annotation)
        .def("classify_annotation",&Document::classify_annotation).def("reparent_annotation",&Document::reparent_annotation)
        .def("execute",&Document::execute,"edits"_a,"label"_a="Edit objects")
        .def_prop_ro("classes",[](const Document& d) {
            std::vector<ObjectClass> result; result.reserve(d.classes().size());
            for(const auto& item:d.classes()) result.push_back(item.second);
            return result;
        })
        .def("object_class",&Document::object_class).def("class_count",&Document::class_count)
        .def("set_class",&Document::set_class,"name"_a,"color"_a)
        .def("remove_class",&Document::remove_class).def("rename_class",&Document::rename_class)
        .def("classify_objects",&Document::classify_objects,"ids"_a,"class_name"_a)
        .def("children",&Document::children)
        .def("query_region",&Document::query_region)
        .def("pick",&Document::pick,"point"_a,"tolerance"_a=0)
        .def_prop_ro("spatial_stats",&Document::spatial_stats)
        .def("undo",&Document::undo).def("redo",&Document::redo)
        .def_prop_ro("can_undo",&Document::can_undo).def_prop_ro("can_redo",&Document::can_redo)
        .def_prop_ro("undo_label",&Document::undo_label).def_prop_ro("redo_label",&Document::redo_label)
        .def("clear_history",&Document::clear_history).def("set_history_limits",&Document::set_history_limits)
        .def_prop_ro("history_bytes",&Document::history_bytes)
        .def("set_calibration", &Document::set_calibration)
        .def("clear_calibration", &Document::clear_calibration)
        .def_prop_ro("pixel_width_um", &Document::pixel_width_um).def_prop_ro("pixel_height_um", &Document::pixel_height_um)
        .def("shape_measurements",&Document::shape_measurements).def("feature_cache_stats",&Document::feature_cache_stats)
        .def("set_feature_cache_limit",&Document::set_feature_cache_limit).def("clear_feature_cache",&Document::clear_feature_cache)
        .def_prop_ro("calibration_revision",&Document::calibration_revision)
        .def("area_um2", &Document::area_um2).def("mean_intensity", &Document::mean_intensity);
    nb::class_<InterchangeIssue>(m,"InterchangeIssue").def_ro("code",&InterchangeIssue::code)
        .def_ro("message",&InterchangeIssue::message).def_ro("count",&InterchangeIssue::count);
    nb::class_<InterchangeReport>(m,"InterchangeReport").def_ro("text",&InterchangeReport::text)
        .def_ro("issues",&InterchangeReport::issues).def_ro("imported_ids",&InterchangeReport::imported_ids)
        .def_ro("source_ids",&InterchangeReport::source_ids).def_ro("features",&InterchangeReport::features);
    m.attr("has_interchange")=has_interchange();
    m.def("export_geojson",&export_geojson,"document"_a,"ids"_a=std::vector<std::uint64_t>{},"allow_losses"_a=false);
    m.def("import_geojson",&import_geojson,"document"_a,"text"_a,"assume_pixels"_a=false,"allow_losses"_a=false,"qupath_ellipse_precision"_a=false);
    m.def("save_geojson",&save_geojson,"document"_a,"path"_a,"ids"_a=std::vector<std::uint64_t>{},"allow_losses"_a=false);
    m.def("load_geojson",&load_geojson,"document"_a,"path"_a,"assume_pixels"_a=false,"allow_losses"_a=false,"qupath_ellipse_precision"_a=false);
    m.def("measurements_csv",&measurements_csv,"document"_a,"ids"_a=std::vector<std::uint64_t>{},"include_intensity"_a=false);
    m.def("save_measurements_csv",&save_measurements_csv,"document"_a,"path"_a,"ids"_a=std::vector<std::uint64_t>{},"include_intensity"_a=false);
    nb::class_<ProjectImage>(m,"ProjectImage")
        .def_ro("id",&ProjectImage::id).def_ro("name",&ProjectImage::name)
        .def_ro("source_path",&ProjectImage::source_path).def_ro("reader",&ProjectImage::reader)
        .def_ro("width",&ProjectImage::width).def_ro("height",&ProjectImage::height)
        .def_ro("embedded",&ProjectImage::embedded).def_ro("available",&ProjectImage::available);
    nb::class_<ProjectView>(m,"ProjectView").def(nb::init<>()).def_rw("center",&ProjectView::center)
        .def_rw("zoom",&ProjectView::zoom).def_rw("display",&ProjectView::display).def("validate",&ProjectView::validate);
    nb::class_<RecoveryInfo>(m,"RecoveryInfo").def_ro("path",&RecoveryInfo::path).def_ro("identity",&RecoveryInfo::identity)
        .def_ro("session",&RecoveryInfo::session).def_ro("base_path",&RecoveryInfo::base_path).def_ro("base_status",&RecoveryInfo::base_status)
        .def_ro("error",&RecoveryInfo::error).def_ro("base_generation",&RecoveryInfo::base_generation)
        .def_ro("created_unix_ms",&RecoveryInfo::created_unix_ms).def_ro("images",&RecoveryInfo::images).def_ro("objects",&RecoveryInfo::objects).def_ro("fingerprint",&RecoveryInfo::fingerprint).def_ro("associated_base_path",&RecoveryInfo::associated_base_path);
    nb::class_<RecoveryRetention>(m,"RecoveryRetention").def(nb::init<>()).def_rw("enabled",&RecoveryRetention::enabled)
        .def_rw("keep_latest",&RecoveryRetention::keep_latest).def_rw("max_age_days",&RecoveryRetention::max_age_days)
        .def_rw("max_deletions",&RecoveryRetention::max_deletions).def("validate",&RecoveryRetention::validate);
    nb::class_<RecoveryRetentionStats>(m,"RecoveryRetentionStats").def_ro("inspected",&RecoveryRetentionStats::inspected)
        .def_ro("removed",&RecoveryRetentionStats::removed).def_ro("failed",&RecoveryRetentionStats::failed)
        .def_ro("elapsed_ms",&RecoveryRetentionStats::elapsed_ms).def_ro("error",&RecoveryRetentionStats::error)
        .def_ro("cancelled",&RecoveryRetentionStats::cancelled).def_ro("metadata_ms",&RecoveryRetentionStats::metadata_ms)
        .def_ro("ownership_ms",&RecoveryRetentionStats::ownership_ms).def_ro("removal_ms",&RecoveryRetentionStats::removal_ms).def_ro("marker_prefetch_peak",&RecoveryRetentionStats::marker_prefetch_peak);
    nb::class_<AutosaveStats>(m,"AutosaveStats").def_ro("state",&AutosaveStats::state).def_ro("path",&AutosaveStats::path)
        .def_ro("error",&AutosaveStats::error).def_ro("completed",&AutosaveStats::completed).def_ro("snapshot_bytes",&AutosaveStats::snapshot_bytes)
        .def_ro("capture_ms",&AutosaveStats::capture_ms).def_ro("write_ms",&AutosaveStats::write_ms)
        .def_ro("work_directories_cleaned",&AutosaveStats::work_directories_cleaned).def_ro("work_cleanup_error",&AutosaveStats::work_cleanup_error)
        .def_ro("checkpoints_pruned",&AutosaveStats::checkpoints_pruned).def_ro("retention_ms",&AutosaveStats::retention_ms).def_ro("retention_error",&AutosaveStats::retention_error)
        .def_ro("checkpoint_published",&AutosaveStats::checkpoint_published).def_ro("cancel_requested",&AutosaveStats::cancel_requested)
        .def_ro("retention_cancelled",&AutosaveStats::retention_cancelled).def_ro("maintenance_phase",&AutosaveStats::maintenance_phase)
        .def_ro("retention_inspected",&AutosaveStats::retention_inspected).def_ro("retention_removed",&AutosaveStats::retention_removed)
        .def_ro("retention_metadata_ms",&AutosaveStats::retention_metadata_ms).def_ro("retention_ownership_ms",&AutosaveStats::retention_ownership_ms).def_ro("retention_removal_ms",&AutosaveStats::retention_removal_ms).def_ro("retention_prefetch_peak",&AutosaveStats::retention_prefetch_peak);
    nb::class_<ProjectAutosave>(m,"ProjectAutosave").def(nb::init<std::size_t>(),"snapshot_budget"_a=256*1024*1024)
        .def("request",&ProjectAutosave::request,"project"_a,"path"_a="").def("poll",&ProjectAutosave::poll).def("wait",&ProjectAutosave::wait)
        .def("cancel",&ProjectAutosave::cancel).def("clear",&ProjectAutosave::clear).def("default_path",&ProjectAutosave::default_path)
        .def_prop_rw("retention",&ProjectAutosave::retention,&ProjectAutosave::set_retention)
        .def_prop_ro("busy",&ProjectAutosave::busy).def_prop_ro("stats",&ProjectAutosave::stats).def_prop_ro("session",&ProjectAutosave::session);
    nb::class_<ProjectSaveStats>(m,"ProjectSaveStats").def_ro("state",&ProjectSaveStats::state).def_ro("path",&ProjectSaveStats::path)
        .def_ro("error",&ProjectSaveStats::error).def_ro("completed",&ProjectSaveStats::completed).def_ro("generation",&ProjectSaveStats::generation)
        .def_ro("applied",&ProjectSaveStats::applied).def_ro("snapshot_bytes",&ProjectSaveStats::snapshot_bytes)
        .def_ro("capture_ms",&ProjectSaveStats::capture_ms).def_ro("write_ms",&ProjectSaveStats::write_ms);
    nb::class_<ProjectSaveJob>(m,"ProjectSaveJob").def(nb::init<std::size_t>(),"snapshot_budget"_a=256*1024*1024)
        .def("request",&ProjectSaveJob::request,"project"_a,"path"_a).def("poll",&ProjectSaveJob::poll).def("wait",&ProjectSaveJob::wait)
        .def("cancel",&ProjectSaveJob::cancel).def_prop_ro("busy",&ProjectSaveJob::busy).def_prop_ro("stats",&ProjectSaveJob::stats);
    nb::class_<Project>(m,"Project").def(nb::init<>()).def_static("open",&Project::open,"path"_a)
        .def_static("recover",&Project::recover,"path"_a,"base_path"_a="").def_static("inspect_recovery",&Project::inspect_recovery,"path"_a)
        .def_static("find_recoveries",&Project::find_recoveries,"base_path"_a="","limit"_a=64)
        .def_static("discard_recovery",&Project::discard_recovery,"path"_a,"expected_identity"_a)
        .def_static("review_recovery",&Project::review_recovery,"path"_a)
        .def_static("discard_recovery_checked",&Project::discard_recovery_checked,"reviewed"_a)
        .def_static("cleanup_recovery_work",&Project::cleanup_recovery_work,"path"_a)
        .def_static("cleanup_abandoned_recovery_work",&Project::cleanup_abandoned_recovery_work,"current_path"_a,"limit"_a=32)
        .def_static("prune_recoveries",&Project::prune_recoveries,"current_path"_a,"policy"_a)
        .def("restore_recovery",&Project::restore_recovery,"path"_a,"base_path"_a="")
        .def("save",&Project::save,"path"_a)
        .def("add_image",&Project::add_image,"path"_a,"reader"_a="auto","name"_a="")
        .def("add_document",&Project::add_document,"document"_a,"name"_a="","source_path"_a="","reader"_a="auto")
        .def("document",&Project::document).def_prop_ro("images",&Project::images)
        .def("remove_image",&Project::remove_image).def("relink",&Project::relink).def("rename_image",&Project::rename_image)
        .def("view",&Project::view).def("set_view",&Project::set_view).def("clear_view",&Project::clear_view)
        .def_prop_rw("active_image",&Project::active_image,&Project::set_active_image)
        .def_prop_ro("dirty",&Project::dirty).def_prop_ro("path",&Project::path)
        .def_prop_ro("identity",&Project::identity).def_prop_ro("generation",&Project::generation).def_prop_ro("recovery_source",&Project::recovery_source);
    m.attr("has_project")=has_project(); m.attr("sqlite_version")=sqlite_version();
    nb::class_<ObjectSelection>(m,"ObjectSelection")
        .def(nb::init<const Document&>(),nb::keep_alive<1,2>())
        .def_prop_ro("ids",&ObjectSelection::ids).def_prop_ro("primary",&ObjectSelection::primary)
        .def_prop_ro("size",&ObjectSelection::size).def_prop_ro("revision",&ObjectSelection::revision).def("contains",&ObjectSelection::contains)
        .def("replace",&ObjectSelection::replace,"ids"_a,"primary"_a=0)
        .def("toggle",&ObjectSelection::toggle).def("clear",&ObjectSelection::clear)
        .def("select_all",&ObjectSelection::select_all)
        .def("select_range",&ObjectSelection::select_range,"anchor"_a,"target"_a,"additive"_a=false)
        .def("select_region",&ObjectSelection::select_region,"bounds"_a,"additive"_a=false);
    nb::class_<Viewport>(m, "Viewport").def(nb::init<>()).def_rw("origin", &Viewport::origin).def_rw("zoom", &Viewport::zoom)
        .def("image_to_screen", &Viewport::image_to_screen).def("screen_to_image", &Viewport::screen_to_image)
        .def("zoom_at", &Viewport::zoom_at).def("fit", &Viewport::fit).def("center_on",&Viewport::center_on);
    nb::enum_<DisplayMode>(m,"DisplayMode").value("RGB",DisplayMode::RGB).value("GRAY",DisplayMode::Gray)
        .value("RED",DisplayMode::Red).value("GREEN",DisplayMode::Green).value("BLUE",DisplayMode::Blue);
    nb::class_<DisplaySettings>(m,"DisplaySettings").def(nb::init<>()).def_rw("mode",&DisplaySettings::mode)
        .def_rw("black",&DisplaySettings::black).def_rw("white",&DisplaySettings::white).def_rw("gamma",&DisplaySettings::gamma)
        .def("validate",&DisplaySettings::validate);
    m.def("display_pixels",[](const PixelBuffer& p,const DisplaySettings& s) {
        auto data=display_pixels(p,s); return nb::bytes(reinterpret_cast<const char*>(data.data()),data.size());
    });
    nb::enum_<GuiInputKind>(m,"GuiInputKind").value("MOVE",GuiInputKind::Move)
        .value("LEFT_DOWN",GuiInputKind::LeftDown).value("LEFT_UP",GuiInputKind::LeftUp)
        .value("RIGHT_DOWN",GuiInputKind::RightDown).value("RIGHT_UP",GuiInputKind::RightUp)
        .value("WHEEL",GuiInputKind::Wheel).value("RECTANGLE_KEY",GuiInputKind::RectangleKey)
        .value("UNDO_KEY",GuiInputKind::UndoKey).value("REDO_KEY",GuiInputKind::RedoKey).value("DELETE_KEY",GuiInputKind::DeleteKey)
        .value("ELLIPSE_KEY",GuiInputKind::EllipseKey).value("POLYGON_KEY",GuiInputKind::PolygonKey)
        .value("POLYLINE_KEY",GuiInputKind::PolylineKey).value("POINTS_KEY",GuiInputKind::PointsKey).value("FINISH_KEY",GuiInputKind::FinishKey)
        .value("SELECT_ALL_KEY",GuiInputKind::SelectAllKey).value("ESCAPE_KEY",GuiInputKind::EscapeKey)
        .value("VERTEX_KEY",GuiInputKind::VertexKey).value("HOLE_KEY",GuiInputKind::HoleKey)
        .value("INSERT_KEY",GuiInputKind::InsertKey).value("BOOLEAN_KEY",GuiInputKind::BooleanKey).value("SAVE_KEY",GuiInputKind::SaveKey).value("EXPORT_KEY",GuiInputKind::ExportKey).value("IMPORT_KEY",GuiInputKind::ImportKey)
        .value("NEXT_IMAGE_KEY",GuiInputKind::NextImageKey).value("PREVIOUS_IMAGE_KEY",GuiInputKind::PreviousImageKey).value("ADD_IMAGE_KEY",GuiInputKind::AddImageKey).value("RELINK_IMAGE_KEY",GuiInputKind::RelinkImageKey)
        .value("AUTOSAVE_KEY",GuiInputKind::AutosaveKey).value("RECOVER_KEY",GuiInputKind::RecoverKey).value("CLOSE_WINDOW",GuiInputKind::CloseWindow).value("OPEN_PROJECT_KEY",GuiInputKind::OpenProjectKey).value("NEW_PROJECT_KEY",GuiInputKind::NewProjectKey).value("REMOVE_IMAGE_KEY",GuiInputKind::RemoveImageKey).value("REVIEW_RECOVERY_KEY",GuiInputKind::ReviewRecoveryKey).value("FIND_RECOVERIES_KEY",GuiInputKind::FindRecoveriesKey).value("STOP_RECOVERY_MAINTENANCE_KEY",GuiInputKind::StopRecoveryMaintenanceKey)
        .value("ANALYZE_KEY",GuiInputKind::AnalyzeKey).value("CANCEL_ANALYSIS_KEY",GuiInputKind::CancelAnalysisKey)
        .value("KEEP_ANALYSIS_KEY",GuiInputKind::KeepAnalysisKey).value("RECHECK_ANALYSIS_KEY",GuiInputKind::RecheckAnalysisKey)
        .value("DETECT_TISSUE_KEY",GuiInputKind::DetectTissueKey).value("CANCEL_TISSUE_KEY",GuiInputKind::CancelTissueKey)
        .value("TRACE_TISSUE_KEY",GuiInputKind::TraceTissueKey).value("KEEP_TISSUE_CONTOURS_KEY",GuiInputKind::KeepTissueContoursKey)
        .value("KEEP_TISSUE_KEY",GuiInputKind::KeepTissueKey).value("RECHECK_TISSUE_KEY",GuiInputKind::RecheckTissueKey)
        .value("DETECT_CELLS_KEY",GuiInputKind::DetectCellsKey).value("KEEP_CELLS_KEY",GuiInputKind::KeepCellsKey).value("CANCEL_CELLS_KEY",GuiInputKind::CancelCellsKey)
        .value("KEEP_CELL_RUN_KEY",GuiInputKind::KeepCellRunKey).value("RECHECK_CELL_RUN_KEY",GuiInputKind::RecheckCellRunKey)
        .value("EXPORT_CELL_TABLE_KEY",GuiInputKind::ExportCellTableKey).value("CANCEL_CELL_TABLE_KEY",GuiInputKind::CancelCellTableKey)
        .value("EXPORT_MEASUREMENT_TABLE_KEY",GuiInputKind::ExportMeasurementTableKey).value("CANCEL_MEASUREMENT_TABLE_KEY",GuiInputKind::CancelMeasurementTableKey)
        .value("ESTIMATE_STAINS_KEY",GuiInputKind::EstimateStainsKey).value("APPLY_STAIN_ESTIMATE_KEY",GuiInputKind::ApplyStainEstimateKey).value("CANCEL_STAIN_ESTIMATE_KEY",GuiInputKind::CancelStainEstimateKey)
        .value("RUN_BATCH_KEY",GuiInputKind::RunBatchKey).value("CANCEL_BATCH_KEY",GuiInputKind::CancelBatchKey)
        .value("RESIZE_WINDOW",GuiInputKind::ResizeWindow).value("MINIMIZE_WINDOW",GuiInputKind::MinimizeWindow).value("RESTORE_WINDOW",GuiInputKind::RestoreWindow).value("SET_UI_SCALE",GuiInputKind::SetUiScale).value("OBJECT_CLASSIFIERS_KEY",GuiInputKind::ObjectClassifierKey).value("PIXEL_CLASSIFIERS_KEY",GuiInputKind::PixelClassifierKey)
        .value("FINGERPRINT_FILE_KEY",GuiInputKind::FingerprintFileKey).value("CANCEL_FINGERPRINT_FILE_KEY",GuiInputKind::CancelFingerprintFileKey);
    nb::enum_<GuiInputTarget>(m,"GuiInputTarget").value("CANVAS",GuiInputTarget::Canvas).value("OVERVIEW",GuiInputTarget::Overview).value("OBJECT_LIST",GuiInputTarget::ObjectList).value("IMAGE",GuiInputTarget::Image).value("PROJECT_LIST",GuiInputTarget::ProjectList).value("DISPLAY_RESET",GuiInputTarget::DisplayReset)
        .value("DISCARD_RECOVERY_REPLACEMENT",GuiInputTarget::DiscardRecoveryReplacement).value("KEEP_RECOVERY_CLOSE",GuiInputTarget::KeepRecoveryClose).value("CANCEL_CLOSE",GuiInputTarget::CancelClose).value("DISCARD_CLOSE",GuiInputTarget::DiscardClose)
        .value("SAVE_CLOSE",GuiInputTarget::SaveClose).value("SAVE_RECOVERY_REPLACEMENT",GuiInputTarget::SaveRecoveryReplacement).value("CANCEL_RECOVERY_REPLACEMENT",GuiInputTarget::CancelRecoveryReplacement).value("CANCEL_PROJECT_LOAD",GuiInputTarget::CancelProjectLoad).value("CONFIRM_REMOVE_IMAGE",GuiInputTarget::ConfirmRemoveImage).value("CANCEL_REMOVE_IMAGE",GuiInputTarget::CancelRemoveImage).value("CONFIRM_DISCARD_CHECKPOINT",GuiInputTarget::ConfirmDiscardCheckpoint).value("CANCEL_DISCARD_CHECKPOINT",GuiInputTarget::CancelDiscardCheckpoint).value("WINDOW",GuiInputTarget::Window).value("RESET_LAYOUT",GuiInputTarget::ResetLayout).value("SAVE_PROJECT",GuiInputTarget::SaveProject)
        .value("CLASSIFIER_TRAIN",GuiInputTarget::TrainObjectClassifier).value("CLASSIFIER_PREDICT",GuiInputTarget::PredictObjectClasses)
        .value("CLASSIFIER_APPLY",GuiInputTarget::ApplyObjectClasses).value("CLASSIFIER_CANCEL",GuiInputTarget::CancelObjectClassifier)
        .value("CLASSIFIER_SAVE_MODEL",GuiInputTarget::SaveClassifierModel).value("CLASSIFIER_LOAD_MODEL",GuiInputTarget::LoadClassifierModel)
        .value("CLASSIFIER_KEEP_TRAINING",GuiInputTarget::KeepClassifierTraining).value("CLASSIFIER_USE_RECORD",GuiInputTarget::UseClassifierRecord)
        .value("PIXEL_TRAIN",GuiInputTarget::TrainPixelClassifier).value("PIXEL_PREDICT",GuiInputTarget::PredictPixelClasses).value("PIXEL_CANCEL",GuiInputTarget::CancelPixelClassifier)
        .value("PIXEL_SAVE_MODEL",GuiInputTarget::SavePixelModel).value("PIXEL_LOAD_MODEL",GuiInputTarget::LoadPixelModel).value("PIXEL_TOGGLE_OVERLAY",GuiInputTarget::TogglePixelOverlay).value("PIXEL_TRACE",GuiInputTarget::TracePixelContours).value("PIXEL_PUBLISH",GuiInputTarget::PublishPixelContours)
        .value("PIXEL_KEEP_TRAINING",GuiInputTarget::KeepPixelTraining).value("PIXEL_KEEP_PREDICTION",GuiInputTarget::KeepPixelPrediction).value("PIXEL_USE_RECORD_MODEL",GuiInputTarget::UsePixelRecordModel).value("PIXEL_PREVIEW_RECORD",GuiInputTarget::PreviewPixelRecord).value("PIXEL_REMOVE_RECORD",GuiInputTarget::RemovePixelRecord);
    nb::enum_<GuiInputWait>(m,"GuiInputWait").value("IMMEDIATE",GuiInputWait::Immediate).value("TARGET_READY",GuiInputWait::TargetReady).value("STARTUP_FINISHED",GuiInputWait::StartupFinished)
        .value("TISSUE_READY",GuiInputWait::TissueReady).value("TISSUE_CONTOURS_READY",GuiInputWait::TissueContoursReady).value("TISSUE_CONTOURS_PUBLISHED",GuiInputWait::TissueContoursPublished)
        .value("TISSUE_ARCHIVED",GuiInputWait::TissueArchived).value("TISSUE_VERIFIED",GuiInputWait::TissueVerified)
        .value("CELLS_READY",GuiInputWait::CellsReady).value("CELLS_PUBLISHED",GuiInputWait::CellsPublished)
        .value("CELLS_ARCHIVED",GuiInputWait::CellsArchived).value("CELLS_VERIFIED",GuiInputWait::CellsVerified).value("CELL_TABLE_SAVED",GuiInputWait::CellTableSaved).value("FILE_FINGERPRINT_READY",GuiInputWait::FileFingerprintReady).value("MEASUREMENT_TABLE_SAVED",GuiInputWait::MeasurementTableSaved).value("STAINS_ESTIMATED",GuiInputWait::StainsEstimated).value("STAINS_APPLIED",GuiInputWait::StainsApplied).value("BATCH_FINISHED",GuiInputWait::BatchFinished).value("WINDOW_READY",GuiInputWait::WindowReady)
        .value("CLASSIFIER_TRAINED",GuiInputWait::ObjectClassifierTrained).value("CLASSIFIER_PREDICTED",GuiInputWait::ObjectClassifierPredicted)
        .value("CLASSIFIER_APPLIED",GuiInputWait::ObjectClassifierApplied).value("CLASSIFIER_MODEL_SAVED",GuiInputWait::ClassifierModelSaved).value("CLASSIFIER_MODEL_LOADED",GuiInputWait::ClassifierModelLoaded)
        .value("CLASSIFIER_TRAINING_RETAINED",GuiInputWait::ClassifierTrainingRetained).value("CLASSIFIER_RECORD_LOADED",GuiInputWait::ClassifierRecordLoaded)
        .value("PIXEL_TRAINED",GuiInputWait::PixelClassifierTrained).value("PIXEL_PREDICTED",GuiInputWait::PixelClassifierPredicted).value("PIXEL_PREVIEW_READY",GuiInputWait::PixelPreviewReady)
        .value("PIXEL_MODEL_SAVED",GuiInputWait::PixelModelSaved).value("PIXEL_MODEL_LOADED",GuiInputWait::PixelModelLoaded).value("PIXEL_CONTOURS_READY",GuiInputWait::PixelContoursReady).value("PIXEL_CONTOURS_PUBLISHED",GuiInputWait::PixelContoursPublished)
        .value("PIXEL_RECORD_KEPT",GuiInputWait::PixelRecordKept).value("PIXEL_RECORD_MODEL_LOADED",GuiInputWait::PixelRecordModelLoaded).value("PIXEL_HISTORICAL_PREVIEW_READY",GuiInputWait::PixelHistoricalPreviewReady);
    nb::class_<GuiInput>(m,"GuiInput").def(nb::init<>()).def_rw("frame",&GuiInput::frame).def_rw("kind",&GuiInput::kind)
        .def_rw("target",&GuiInput::target).def_rw("x",&GuiInput::x).def_rw("y",&GuiInput::y).def_rw("wheel",&GuiInput::wheel).def_rw("ctrl",&GuiInput::ctrl).def_rw("shift",&GuiInput::shift).def_rw("wait_for",&GuiInput::wait_for);
    nb::class_<GuiOptions>(m,"GuiOptions").def(nb::init<>())
        .def_rw("ui_scale",&GuiOptions::ui_scale).def_rw("docked_workspace",&GuiOptions::docked_workspace).def_rw("layout_path",&GuiOptions::layout_path).def_rw("reset_layout",&GuiOptions::reset_layout).def_rw("width",&GuiOptions::width).def_rw("height",&GuiOptions::height)
        .def_rw("replay_timeout_seconds",&GuiOptions::replay_timeout_seconds).def_rw("settle_tiles",&GuiOptions::settle_tiles)
        .def_rw("interchange_assume_pixels",&GuiOptions::interchange_assume_pixels).def_rw("interchange_allow_losses",&GuiOptions::interchange_allow_losses).def_rw("qupath_ellipse_precision",&GuiOptions::qupath_ellipse_precision)
        .def_rw("overlay_cache_budget",&GuiOptions::overlay_cache_budget)
        .def_rw("display",&GuiOptions::display).def_rw("inputs",&GuiOptions::inputs).def_rw("start_view",&GuiOptions::start_view).def_rw("project",&GuiOptions::project).def_rw("interchange_path",&GuiOptions::interchange_path).def_rw("project_image_path",&GuiOptions::project_image_path)
        .def_rw("autosave_path",&GuiOptions::autosave_path).def_rw("recovery_path",&GuiOptions::recovery_path).def_rw("recovery_base_path",&GuiOptions::recovery_base_path).def_rw("project_open_path",&GuiOptions::project_open_path).def_rw("autosave_interval_seconds",&GuiOptions::autosave_interval_seconds).def_rw("autosave_snapshot_budget",&GuiOptions::autosave_snapshot_budget).def_rw("save_snapshot_budget",&GuiOptions::save_snapshot_budget)
        .def_rw("startup_path",&GuiOptions::startup_path).def_rw("startup_kind",&GuiOptions::startup_kind).def_rw("startup_image_id",&GuiOptions::startup_image_id)
        .def_rw("recovery_retention_enabled",&GuiOptions::recovery_retention_enabled).def_rw("recovery_keep_latest",&GuiOptions::recovery_keep_latest).def_rw("recovery_max_age_days",&GuiOptions::recovery_max_age_days)
        .def_rw("analysis_kind",&GuiOptions::analysis_kind).def_rw("analysis_nucleus",&GuiOptions::analysis_nucleus)
        .def_rw("analysis_vectors",&GuiOptions::analysis_vectors).def_rw("analysis_white",&GuiOptions::analysis_white)
        .def_rw("tissue_feature",&GuiOptions::tissue_feature).def_rw("tissue_threshold",&GuiOptions::tissue_threshold)
        .def_rw("tissue_connectivity",&GuiOptions::tissue_connectivity).def_rw("tissue_min_pixels",&GuiOptions::tissue_min_pixels).def_rw("tissue_max_pixels",&GuiOptions::tissue_max_pixels)
        .def_rw("tissue_level",&GuiOptions::tissue_level)
        .def_rw("tissue_open_radius",&GuiOptions::tissue_open_radius).def_rw("tissue_close_radius",&GuiOptions::tissue_close_radius)
        .def_rw("tissue_fill_holes",&GuiOptions::tissue_fill_holes).def_rw("tissue_max_hole_pixels",&GuiOptions::tissue_max_hole_pixels);
    nb::class_<GuiReport>(m,"GuiReport").def_ro("frames",&GuiReport::frames)
        .def_ro("docked_workspace",&GuiReport::docked_workspace).def_ro("layout_loaded",&GuiReport::layout_loaded).def_ro("layout_saved",&GuiReport::layout_saved).def_ro("layout_error",&GuiReport::layout_error)
        .def_ro("browser_bounds",&GuiReport::browser_bounds).def_ro("display_bounds",&GuiReport::display_bounds).def_ro("measurement_bounds",&GuiReport::measurement_bounds).def_ro("canvas_bounds",&GuiReport::canvas_bounds).def_ro("canvas_visible",&GuiReport::canvas_visible)
        .def_ro("monitor_scale",&GuiReport::monitor_scale).def_ro("framebuffer_scale",&GuiReport::framebuffer_scale).def_ro("ui_scale",&GuiReport::ui_scale).def_ro("font_rebuilds",&GuiReport::font_rebuilds)
        .def_ro("ui_stacked_fields",&GuiReport::ui_stacked_fields).def_ro("ui_wrapped_buttons",&GuiReport::ui_wrapped_buttons).def_ro("ui_overflow_items",&GuiReport::ui_overflow_items).def_ro("ui_max_overflow",&GuiReport::ui_max_overflow).def_ro("ui_overflow_label",&GuiReport::ui_overflow_label)
        .def_ro("overview_state",&GuiReport::overview_state).def_ro("overview_error",&GuiReport::overview_error)
        .def_ro("overview_result",&GuiReport::overview_result).def_ro("overview_gpu_bytes",&GuiReport::overview_gpu_bytes)
        .def_ro("overview_peak_gpu_bytes",&GuiReport::overview_peak_gpu_bytes)
        .def_ro("suspended_iterations",&GuiReport::suspended_iterations).def_ro("suspended_work_ms",&GuiReport::suspended_work_ms)
        .def_ro("suspended_wait_ms",&GuiReport::suspended_wait_ms).def_ro("window_iconified",&GuiReport::window_iconified)
        .def_ro("overlay_cache_bytes",&GuiReport::overlay_cache_bytes).def_ro("overlay_cache_peak_bytes",&GuiReport::overlay_cache_peak_bytes)
        .def_ro("overlay_cache_builds",&GuiReport::overlay_cache_builds).def_ro("overlay_cache_hits",&GuiReport::overlay_cache_hits).def_ro("overlay_cache_fallbacks",&GuiReport::overlay_cache_fallbacks)
        .def_ro("replay_frame",&GuiReport::replay_frame).def_ro("replay_wait_frames",&GuiReport::replay_wait_frames).def_ro("replay_physical_wheel_events_ignored",&GuiReport::replay_physical_wheel_events_ignored).def_ro("settle_frames",&GuiReport::settle_frames)
        .def_ro("replay_complete",&GuiReport::replay_complete).def_ro("tiles_settled",&GuiReport::tiles_settled)
        .def_ro("uploaded_tiles",&GuiReport::uploaded_tiles).def_ro("peak_gpu_bytes",&GuiReport::peak_gpu_bytes)
        .def_ro("generations",&GuiReport::generations).def_ro("cpu_frame_p95_ms",&GuiReport::cpu_frame_p95_ms)
        .def_ro("pending_tiles",&GuiReport::pending_tiles).def_ro("final_zoom",&GuiReport::final_zoom)
        .def_ro("final_origin",&GuiReport::final_origin).def_ro("framebuffer_width",&GuiReport::framebuffer_width)
        .def_ro("framebuffer_height",&GuiReport::framebuffer_height).def_ro("final_center",&GuiReport::final_center)
        .def_ro("gl_renderer",&GuiReport::gl_renderer).def_ro("gl_version",&GuiReport::gl_version).def_ro("visible_objects",&GuiReport::visible_objects)
        .def_ro("selected_ids",&GuiReport::selected_ids).def_ro("primary_selection",&GuiReport::primary_selection)
        .def_ro("active_image",&GuiReport::active_image).def_ro("image_switches",&GuiReport::image_switches).def_ro("max_image_switch_ms",&GuiReport::max_image_switch_ms).def_ro("project_error",&GuiReport::project_error)
        .def_ro("autosaves_completed",&GuiReport::autosaves_completed).def_ro("recovery_path",&GuiReport::recovery_path).def_ro("autosave_error",&GuiReport::autosave_error).def_ro("autosave_capture_ms",&GuiReport::autosave_capture_ms).def_ro("max_cpu_frame_ms",&GuiReport::max_cpu_frame_ms).def_ro("project_replacements",&GuiReport::project_replacements).def_ro("project_load_ms",&GuiReport::project_load_ms).def_ro("images_removed",&GuiReport::images_removed)
        .def_ro("saves_completed",&GuiReport::saves_completed).def_ro("save_capture_ms",&GuiReport::save_capture_ms).def_ro("save_write_ms",&GuiReport::save_write_ms).def_ro("recovery_load_ms",&GuiReport::recovery_load_ms).def_ro("checkpoints_deleted",&GuiReport::checkpoints_deleted).def_ro("recovery_maintenance_error",&GuiReport::recovery_maintenance_error).def_ro("retired_workspaces",&GuiReport::retired_workspaces).def_ro("max_retirement_ms",&GuiReport::max_retirement_ms)
        .def_ro("checkpoints_pruned",&GuiReport::checkpoints_pruned).def_ro("retention_ms",&GuiReport::retention_ms).def_ro("retention_error",&GuiReport::retention_error)
        .def_ro("retention_cancelled",&GuiReport::retention_cancelled).def_ro("retention_inspected",&GuiReport::retention_inspected)
        .def_ro("startup_succeeded",&GuiReport::startup_succeeded).def_ro("startup_cancelled",&GuiReport::startup_cancelled).def_ro("startup_frames",&GuiReport::startup_frames)
        .def_ro("startup_images",&GuiReport::startup_images).def_ro("startup_objects",&GuiReport::startup_objects).def_ro("startup_ms",&GuiReport::startup_ms)
        .def_ro("retention_metadata_ms",&GuiReport::retention_metadata_ms).def_ro("retention_ownership_ms",&GuiReport::retention_ownership_ms).def_ro("retention_removal_ms",&GuiReport::retention_removal_ms).def_ro("retention_prefetch_peak",&GuiReport::retention_prefetch_peak)
        .def_ro("analysis_state",&GuiReport::analysis_state).def_ro("analysis_error",&GuiReport::analysis_error)
        .def_ro("analysis_means",&GuiReport::analysis_means).def_ro("analysis_samples",&GuiReport::analysis_samples)
        .def_ro("analysis_object",&GuiReport::analysis_object).def_ro("analysis_tiles",&GuiReport::analysis_tiles).def_ro("analysis_ms",&GuiReport::analysis_ms)
        .def_ro("analysis_records",&GuiReport::analysis_records).def_ro("saved_analysis_status",&GuiReport::saved_analysis_status)
        .def_ro("tissue_state",&GuiReport::tissue_state).def_ro("tissue_error",&GuiReport::tissue_error).def_ro("tissue_input_sha256",&GuiReport::tissue_input_sha256)
        .def_ro("tissue_components",&GuiReport::tissue_components).def_ro("tissue_gpu_bytes",&GuiReport::tissue_gpu_bytes)
        .def_ro("tissue_samples",&GuiReport::tissue_samples).def_ro("tissue_foreground_pixels",&GuiReport::tissue_foreground_pixels)
        .def_ro("tissue_ms",&GuiReport::tissue_ms).def_ro("tissue_preview_ready",&GuiReport::tissue_preview_ready)
        .def_ro("tissue_level",&GuiReport::tissue_level).def_ro("tissue_downsample",&GuiReport::tissue_downsample)
        .def_ro("tissue_width",&GuiReport::tissue_width).def_ro("tissue_height",&GuiReport::tissue_height)
        .def_ro("tissue_cleaned_pixels",&GuiReport::tissue_cleaned_pixels).def_ro("tissue_filled_pixels",&GuiReport::tissue_filled_pixels)
        .def_ro("tissue_cleanup_ms",&GuiReport::tissue_cleanup_ms)
        .def_ro("tissue_contour_components",&GuiReport::tissue_contour_components).def_ro("tissue_contour_edges",&GuiReport::tissue_contour_edges)
        .def_ro("tissue_contour_vertices",&GuiReport::tissue_contour_vertices).def_ro("tissue_contour_ms",&GuiReport::tissue_contour_ms)
        .def_ro("tissue_annotations_added",&GuiReport::tissue_annotations_added).def_ro("tissue_contours_published",&GuiReport::tissue_contours_published)
        .def_ro("saved_tissue_id",&GuiReport::saved_tissue_id).def_ro("saved_tissue_status",&GuiReport::saved_tissue_status);
    bind_analysis(m);
    bind_stain_estimation(m);
    bind_tissue(m);
    bind_cells(m);
    bind_cell_tables(m);
    bind_measurement_tables(m);
    bind_provenance(m);
    bind_file_fingerprint(m);
    bind_project_batch(m);
    bind_arrays(m);
    bind_thumbnails(m);
    bind_classifiers(m);
    m.def("default_workspace_layout_path",&default_workspace_layout_path);
    bind_viewer_session(m);
#ifdef QUPATHXX_HAS_GUI
    m.attr("has_gui") = true;
    // Retain GIL: Document is single-threaded and shared with the UI.
    m.def("show", &run_gui, "document"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
    m.def("show", &run_project_gui, "project"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
#else
    m.attr("has_gui") = false;
    m.def("show", [](Document&, int, const std::string&, const GuiOptions&) -> GuiReport { throw std::runtime_error("rebuild with -Ccmake.define.QUPATHXX_BUILD_GUI=ON"); }, "document"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
    m.def("show", [](std::shared_ptr<Project>, int, const std::string&, const GuiOptions&) -> GuiReport { throw std::runtime_error("rebuild with -Ccmake.define.QUPATHXX_BUILD_GUI=ON"); }, "project"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
#endif
}
