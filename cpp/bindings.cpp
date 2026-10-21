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
    nb::class_<Annotation>(m, "Annotation").def_ro("id", &Annotation::id).def_ro("name", &Annotation::name)
        .def_prop_ro("roi", [](const Annotation& a) { return a.roi; });
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
        .def("remove_annotation", &Document::remove_annotation)
        .def("set_calibration", &Document::set_calibration)
        .def("clear_calibration", &Document::clear_calibration)
        .def_prop_ro("pixel_width_um", &Document::pixel_width_um).def_prop_ro("pixel_height_um", &Document::pixel_height_um)
        .def("area_um2", &Document::area_um2).def("mean_intensity", &Document::mean_intensity);
    nb::class_<Viewport>(m, "Viewport").def(nb::init<>()).def_rw("origin", &Viewport::origin).def_rw("zoom", &Viewport::zoom)
        .def("image_to_screen", &Viewport::image_to_screen).def("screen_to_image", &Viewport::screen_to_image)
        .def("zoom_at", &Viewport::zoom_at).def("fit", &Viewport::fit);
#ifdef QUPATHXX_HAS_GUI
    m.attr("has_gui") = true;
    // Retain GIL: Document is single-threaded and shared with the UI.
    m.def("show", &run_gui, "document"_a, "frame_limit"_a = 0);
#else
    m.attr("has_gui") = false;
    m.def("show", [](Document&, int) { throw std::runtime_error("rebuild with -Ccmake.define.QUPATHXX_BUILD_GUI=ON"); }, "document"_a, "frame_limit"_a = 0);
#endif
}
