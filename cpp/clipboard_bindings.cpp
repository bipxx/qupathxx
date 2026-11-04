#include "qupathxx/interchange.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_clipboard(nb::module_& m) {
    using namespace qpx;
    m.def("copy_objects",&copy_objects,"document"_a,"ids"_a,"include_descendants"_a=false,"allow_losses"_a=false,"max_bytes"_a=16*1024*1024);
    m.def("paste_objects",&paste_objects,"document"_a,"text"_a,"plane"_a=nb::none(),"allow_losses"_a=false,"max_bytes"_a=16*1024*1024);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("clipboard_copies",&GuiReport::clipboard_copies).def_ro("clipboard_pastes",&GuiReport::clipboard_pastes)
        .def_ro("clipboard_copied_objects",&GuiReport::clipboard_copied_objects).def_ro("clipboard_pasted_objects",&GuiReport::clipboard_pasted_objects)
        .def_ro("clipboard_error",&GuiReport::clipboard_error);
}
