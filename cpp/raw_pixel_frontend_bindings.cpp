#include "qupathxx/core.hpp"
#include "qupathxx/raw_pixel_frontend.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_raw_pixel_frontends(nb::module_& m) {
    using namespace qpx;
    nb::class_<RawPixelTrainingSettings>(m,"RawPixelTrainingSettings").def(nb::init<>())
        .def_rw("lattice",&RawPixelTrainingSettings::lattice).def_rw("selection",&RawPixelTrainingSettings::selection)
        .def_rw("features",&RawPixelTrainingSettings::features).def_rw("sampling",&RawPixelTrainingSettings::sampling)
        .def_rw("tiles",&RawPixelTrainingSettings::tiles).def_rw("forest",&RawPixelTrainingSettings::forest)
        .def_rw("physical_sigmas",&RawPixelTrainingSettings::physical_sigmas).def_rw("checkpoint_path",&RawPixelTrainingSettings::checkpoint_path)
        .def_rw("checkpoint",&RawPixelTrainingSettings::checkpoint).def("validate",&RawPixelTrainingSettings::validate);
    nb::class_<GuiRawPixelOptions>(m,"GuiRawPixelOptions").def(nb::init<>())
        .def_rw("open",&GuiRawPixelOptions::open).def_rw("selected_only",&GuiRawPixelOptions::selected_only).def_rw("show_overlay",&GuiRawPixelOptions::show_overlay)
        .def_rw("replace_model",&GuiRawPixelOptions::replace_model).def_rw("object_id",&GuiRawPixelOptions::object_id).def_rw("record_id",&GuiRawPixelOptions::record_id)
        .def_rw("read_index",&GuiRawPixelOptions::read_index).def_rw("preview_budget",&GuiRawPixelOptions::preview_budget)
        .def_rw("training",&GuiRawPixelOptions::training).def_rw("model",&GuiRawPixelOptions::model).def_rw("preview",&GuiRawPixelOptions::preview)
        .def_rw("stream",&GuiRawPixelOptions::stream).def_rw("save",&GuiRawPixelOptions::save).def_rw("read_limits",&GuiRawPixelOptions::read_limits)
        .def_rw("model_path",&GuiRawPixelOptions::model_path).def_rw("stream_path",&GuiRawPixelOptions::stream_path);
    nb::class_<GuiRawPixelReport>(m,"GuiRawPixelReport")
        .def_ro("state",&GuiRawPixelReport::state).def_ro("error",&GuiRawPixelReport::error).def_ro("model",&GuiRawPixelReport::model)
        .def_ro("historical_model",&GuiRawPixelReport::historical_model)
        .def_ro("training",&GuiRawPixelReport::training).def_ro("checkpoint",&GuiRawPixelReport::checkpoint)
        .def_ro("prediction",&GuiRawPixelReport::prediction).def_ro("historical",&GuiRawPixelReport::historical).def_ro("link",&GuiRawPixelReport::link)
        .def_ro("training_record",&GuiRawPixelReport::training_record).def_ro("stream_record",&GuiRawPixelReport::stream_record)
        .def_ro("preview_bytes",&GuiRawPixelReport::preview_bytes).def_ro("preview_ready",&GuiRawPixelReport::preview_ready);
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions")).def_prop_rw("raw_pixel",[](const GuiOptions& o)->std::optional<GuiRawPixelOptions> {if(o.raw_pixel) return *o.raw_pixel;return {};},[](GuiOptions& o,std::optional<GuiRawPixelOptions> value) {o.raw_pixel=value?std::make_shared<const GuiRawPixelOptions>(*value):nullptr;});
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport")).def_ro("raw_pixel",&GuiReport::raw_pixel);
}
