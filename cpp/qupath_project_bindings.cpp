#include "qupathxx/qupath_project.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_qupath_project(nb::module_& m) {
    using namespace qpx;
    nb::class_<QuPathProjectImage>(m,"QuPathProjectImage")
        .def_ro("source_id",&QuPathProjectImage::source_id).def_ro("native_id",&QuPathProjectImage::native_id)
        .def_ro("name",&QuPathProjectImage::name).def_ro("source_path",&QuPathProjectImage::source_path)
        .def_ro("source_object_ids",&QuPathProjectImage::source_object_ids).def_ro("imported_object_ids",&QuPathProjectImage::imported_object_ids)
        .def_ro("reader",&QuPathProjectImage::reader).def_ro("objects",&QuPathProjectImage::objects).def_ro("available",&QuPathProjectImage::available);
    nb::class_<QuPathProjectPreview>(m,"QuPathProjectPreview")
        .def_prop_ro("source_path",&QuPathProjectPreview::source_path)
        .def_prop_ro("images",[](const QuPathProjectPreview& p){return p.images();})
        .def_prop_ro("issues",[](const QuPathProjectPreview& p){return p.issues();})
        .def_prop_ro("consumed",&QuPathProjectPreview::consumed)
        .def("import_new",[](const QuPathProjectPreview& p,const std::string& path,bool losses){nb::gil_scoped_release release;return p.import_new(path,losses);},"destination"_a,"allow_losses"_a=false);
    m.def("preview_qupath_project",[](const std::string& path,const std::map<std::string,std::string>& overrides,const std::string& directory){nb::gil_scoped_release release;return preview_qupath_project(path,overrides,directory);},"path"_a,"source_overrides"_a=std::map<std::string,std::string>{},"exchange_directory"_a="");
}
