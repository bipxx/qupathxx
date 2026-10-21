#include "qupathxx/core.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <stdexcept>
namespace nb = nanobind;
using namespace nb::literals;
void bind_scheduler(nb::module_&);
NB_MODULE(_native, m) {
    using namespace qpx;
    nb::class_<Point>(m, "Point").def(nb::init<>()).def(nb::init<double, double>()).def_rw("x", &Point::x).def_rw("y", &Point::y);
    nb::class_<Rect>(m, "Rect").def(nb::init<>()).def(nb::init<double,double,double,double>())
        .def_rw("x", &Rect::x).def_rw("y", &Rect::y).def_rw("width", &Rect::width).def_rw("height", &Rect::height)
        .def_prop_ro("area", &Rect::area).def("contains", &Rect::contains);
    nb::enum_<RoiKind>(m,"RoiKind").value("RECTANGLE",RoiKind::Rectangle).value("ELLIPSE",RoiKind::Ellipse)
        .value("POLYGON",RoiKind::Polygon).value("MULTIPOLYGON",RoiKind::MultiPolygon).value("POLYLINE",RoiKind::Polyline).value("POINTS",RoiKind::Points);
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
        .def("translated",&Roi::translated).def("equals",&Roi::equals);
    m.attr("has_geos")=has_geos(); m.attr("geos_version")=geos_version();
    nb::class_<SpatialStats>(m,"SpatialStats").def_ro("builds",&SpatialStats::builds)
        .def_ro("base_entries",&SpatialStats::base_entries).def_ro("dirty_entries",&SpatialStats::dirty_entries);
    nb::enum_<ObjectKind>(m,"ObjectKind").value("ANNOTATION",ObjectKind::Annotation)
        .value("DETECTION",ObjectKind::Detection).value("CELL",ObjectKind::Cell);
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
    nb::class_<Document>(m, "Document").def(nb::init<Image>())
        .def(nb::init<std::shared_ptr<ImageServer>>())
        .def_prop_ro("image", &Document::image, nb::rv_policy::reference_internal)
        .def_prop_ro("server", &Document::server)
        .def_prop_ro("metadata", [](const Document& d) { return d.metadata(); })
        .def_prop_ro("annotations", [](const Document& d) { return d.annotations(); })
        .def("add_rectangle", &Document::add_rectangle, "roi"_a, "name"_a = "Annotation")
        .def("add_roi",&Document::add_roi,"roi"_a,"name"_a="Annotation").def("update_roi",&Document::update_roi)
        .def("add_detection",&Document::add_detection,"roi"_a,"name"_a="Detection","parent_id"_a=0,"class_name"_a="")
        .def("add_cell",&Document::add_cell,"roi"_a,"nucleus"_a=nb::none(),"name"_a="Cell","parent_id"_a=0,"class_name"_a="")
        .def("update_nucleus",&Document::update_nucleus,"id"_a,"nucleus"_a)
        .def("nucleus_area_um2",&Document::nucleus_area_um2).def("nucleus_mean_intensity",&Document::nucleus_mean_intensity)
        .def("remove_annotation", &Document::remove_annotation)
        .def("annotation",&Document::annotation)
        .def_prop_ro("object_count",&Document::object_count).def_prop_ro("revision",&Document::revision)
        .def("update_rectangle",&Document::update_rectangle).def("rename_annotation",&Document::rename_annotation)
        .def("classify_annotation",&Document::classify_annotation).def("reparent_annotation",&Document::reparent_annotation)
        .def("execute",&Document::execute,"edits"_a,"label"_a="Edit objects")
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
        .def("area_um2", &Document::area_um2).def("mean_intensity", &Document::mean_intensity);
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
        .value("POLYLINE_KEY",GuiInputKind::PolylineKey).value("POINTS_KEY",GuiInputKind::PointsKey).value("FINISH_KEY",GuiInputKind::FinishKey);
    nb::enum_<GuiInputTarget>(m,"GuiInputTarget").value("CANVAS",GuiInputTarget::Canvas).value("OVERVIEW",GuiInputTarget::Overview);
    nb::class_<GuiInput>(m,"GuiInput").def(nb::init<>()).def_rw("frame",&GuiInput::frame).def_rw("kind",&GuiInput::kind)
        .def_rw("target",&GuiInput::target).def_rw("x",&GuiInput::x).def_rw("y",&GuiInput::y).def_rw("wheel",&GuiInput::wheel);
    nb::class_<GuiOptions>(m,"GuiOptions").def(nb::init<>()).def_rw("width",&GuiOptions::width).def_rw("height",&GuiOptions::height)
        .def_rw("display",&GuiOptions::display).def_rw("inputs",&GuiOptions::inputs).def_rw("start_view",&GuiOptions::start_view);
    nb::class_<GuiReport>(m,"GuiReport").def_ro("frames",&GuiReport::frames)
        .def_ro("uploaded_tiles",&GuiReport::uploaded_tiles).def_ro("peak_gpu_bytes",&GuiReport::peak_gpu_bytes)
        .def_ro("generations",&GuiReport::generations).def_ro("cpu_frame_p95_ms",&GuiReport::cpu_frame_p95_ms)
        .def_ro("pending_tiles",&GuiReport::pending_tiles).def_ro("final_zoom",&GuiReport::final_zoom)
        .def_ro("final_origin",&GuiReport::final_origin).def_ro("framebuffer_width",&GuiReport::framebuffer_width)
        .def_ro("framebuffer_height",&GuiReport::framebuffer_height).def_ro("final_center",&GuiReport::final_center)
        .def_ro("gl_renderer",&GuiReport::gl_renderer).def_ro("gl_version",&GuiReport::gl_version).def_ro("visible_objects",&GuiReport::visible_objects);
#ifdef QUPATHXX_HAS_GUI
    m.attr("has_gui") = true;
    // Retain GIL: Document is single-threaded and shared with the UI.
    m.def("show", &run_gui, "document"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
#else
    m.attr("has_gui") = false;
    m.def("show", [](Document&, int, const std::string&, const GuiOptions&) { throw std::runtime_error("rebuild with -Ccmake.define.QUPATHXX_BUILD_GUI=ON"); }, "document"_a, "frame_limit"_a = 0, "capture_path"_a = "", "options"_a=GuiOptions{});
#endif
}
