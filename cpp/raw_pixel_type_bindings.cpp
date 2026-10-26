#include "qupathxx/raw_pixel_classifier.hpp"
#include "raw_pixel_binding_utils.hpp"
namespace nb=nanobind;using namespace nb::literals;
void bind_raw_pixel_types(nb::module_& m) {
    using namespace qpx;using qpx::bindings::array;
    nb::class_<ScientificRawLattice>(m,"ScientificRawLattice")
        .def(nb::init<std::int64_t,std::int64_t,ScientificGridAxis,ScientificGridAxis>(),"image_width"_a,"image_height"_a,"x"_a,"y"_a)
        .def(nb::init<const ScientificGrid&>(),"grid"_a)
        .def_prop_ro("image_width",&ScientificRawLattice::image_width).def_prop_ro("image_height",&ScientificRawLattice::image_height)
        .def_prop_ro("x",[](const ScientificRawLattice& v){return v.x();}).def_prop_ro("y",[](const ScientificRawLattice& v){return v.y();})
        .def_prop_ro("core_samples",&ScientificRawLattice::core_samples)
        .def("window",&ScientificRawLattice::window,"first_x"_a,"first_y"_a,"width"_a,"height"_a);
    nb::class_<ScientificRawWindowAxis>(m,"ScientificRawWindowAxis")
        .def(nb::init<ScientificGridAxis,std::int64_t,std::int64_t>(),"lattice"_a,"first"_a,"count"_a)
        .def_prop_ro("lattice",[](const ScientificRawWindowAxis& v){return v.lattice();})
        .def_prop_ro("origin",&ScientificRawWindowAxis::origin).def_prop_ro("extent",&ScientificRawWindowAxis::extent)
        .def_prop_ro("samples",&ScientificRawWindowAxis::samples).def_prop_ro("context",&ScientificRawWindowAxis::context)
        .def_prop_ro("first_core_index",&ScientificRawWindowAxis::first_core_index).def_prop_ro("core_samples",&ScientificRawWindowAxis::core_samples)
        .def_prop_ro("first_index",&ScientificRawWindowAxis::first_index).def_prop_ro("stop_index",&ScientificRawWindowAxis::stop_index)
        .def("edge",&ScientificRawWindowAxis::edge).def("center",&ScientificRawWindowAxis::center).def("overlap",&ScientificRawWindowAxis::overlap)
        .def_prop_ro("overlap_visits",&ScientificRawWindowAxis::overlap_visits);
    nb::class_<ScientificRawWindow>(m,"ScientificRawWindow")
        .def(nb::init<const ScientificGrid&,std::int64_t,std::int64_t,std::int64_t,std::int64_t>(),"grid"_a,"first_x"_a,"first_y"_a,"width"_a,"height"_a)
        .def(nb::init<std::int64_t,std::int64_t,ScientificGridAxis,ScientificGridAxis,std::int64_t,std::int64_t,std::int64_t,std::int64_t>(),"image_width"_a,"image_height"_a,"x"_a,"y"_a,"first_x"_a,"first_y"_a,"width"_a,"height"_a)
        .def_prop_ro("x",[](const ScientificRawWindow& v){return v.x();}).def_prop_ro("y",[](const ScientificRawWindow& v){return v.y();})
        .def_prop_ro("image_width",&ScientificRawWindow::image_width).def_prop_ro("image_height",&ScientificRawWindow::image_height)
        .def_prop_ro("core_width",&ScientificRawWindow::core_width).def_prop_ro("core_height",&ScientificRawWindow::core_height)
        .def_prop_ro("width",&ScientificRawWindow::width).def_prop_ro("height",&ScientificRawWindow::height)
        .def_prop_ro("read_bounds",&ScientificRawWindow::read_bounds).def_prop_ro("clipped_read_bounds",&ScientificRawWindow::clipped_read_bounds)
        .def_prop_ro("overlap_visits",&ScientificRawWindow::overlap_visits);
    nb::class_<ScientificRawWindowData>(m,"ScientificRawWindowData")
        .def_prop_ro("window",[](const ScientificRawWindowData& v){return v.window();})
        .def_prop_ro("metadata",[](const ScientificRawWindowData& v){return v.metadata();})
        .def_prop_ro("options",[](const ScientificRawWindowData& v){return v.options();})
        .def_prop_ro("width",&ScientificRawWindowData::width).def_prop_ro("height",&ScientificRawWindowData::height).def_prop_ro("channels",&ScientificRawWindowData::channels)
        .def_prop_ro("reader",&ScientificRawWindowData::reader).def_prop_ro("decoded_source_sha256",&ScientificRawWindowData::decoded_source_sha256)
        .def_prop_ro("input_sha256",&ScientificRawWindowData::input_sha256).def_prop_ro("estimated_work_bytes",&ScientificRawWindowData::estimated_work_bytes)
        .def("numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawWindowData&>(self);return array(v.values(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawWindowData&>(self);return array(v.valid(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false)
        .def("reasons_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawWindowData&>(self);return array(v.reasons(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false);
    m.def("scientific_raw_window_work_bytes",&scientific_raw_window_work_bytes,"metadata"_a,"window"_a,"options"_a=ScientificRawOptions{});
    m.def("read_scientific_raw_window",[](const std::shared_ptr<ImageServer>& source,const ScientificRawWindow& w,ScientificRawOptions o){if(!source) throw std::invalid_argument("raw window requires a server");return read_scientific_raw_window(*source,w,o);},"server"_a,"window"_a,"options"_a=ScientificRawOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("digest_scientific_raw_window",[](const std::shared_ptr<ImageServer>& source,const ScientificRawWindow& w,ScientificRawOptions o){if(!source) throw std::invalid_argument("raw window digest requires a server");return digest_scientific_raw_window(*source,w,o);},"server"_a,"window"_a,"options"_a=ScientificRawOptions{},nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawPixelFeatureScale>(m,"RawPixelFeatureScale").def(nb::init<>())
        .def("__init__",[](RawPixelFeatureScale* p,double x,double y){new(p) RawPixelFeatureScale{x,y};},"x"_a,"y"_a)
        .def_rw("x",&RawPixelFeatureScale::x).def_rw("y",&RawPixelFeatureScale::y);
    nb::class_<RawPixelFeatureOptions>(m,"RawPixelFeatureOptions").def(nb::init<>())
        .def_rw("offsets",&RawPixelFeatureOptions::offsets).def_rw("scales",&RawPixelFeatureOptions::scales).def_rw("sigmas",&RawPixelFeatureOptions::sigmas)
        .def_rw("gradient_magnitude",&RawPixelFeatureOptions::gradient_magnitude).def_rw("laplacian",&RawPixelFeatureOptions::laplacian)
        .def_rw("work_bytes",&RawPixelFeatureOptions::work_bytes).def("validate",&RawPixelFeatureOptions::validate,"channels"_a);
    nb::class_<RawPixelRecipe>(m,"RawPixelRecipe")
        .def(nb::init<const RawImageMetadata&,const ScientificGrid&,ScientificRawOptions,RawPixelFeatureOptions,bool>(),"metadata"_a,"grid"_a,"selection"_a,"features"_a,"physical_sigmas"_a=false)
        .def(nb::init<const RawImageMetadata&,const ScientificRawLattice&,ScientificRawOptions,RawPixelFeatureOptions,bool>(),"metadata"_a,"lattice"_a,"selection"_a,"features"_a,"physical_sigmas"_a=false)
        .def(nb::init<const RawImageMetadata&,const ScientificRawWindow&,ScientificRawOptions,RawPixelFeatureOptions,bool>(),"metadata"_a,"window"_a,"selection"_a,"features"_a,"physical_sigmas"_a=false)
        .def_prop_ro("requested_features",[](const RawPixelRecipe& v){return v.requested_features();}).def_prop_ro("features",[](const RawPixelRecipe& v){return v.features();})
        .def_prop_ro("selection",[](const RawPixelRecipe& v){return v.selection();}).def_prop_ro("sample_type",&RawPixelRecipe::sample_type).def_prop_ro("series",&RawPixelRecipe::series)
        .def_prop_ro("channel_names",[](const RawPixelRecipe& v){return v.channel_names();}).def_prop_ro("step_x",&RawPixelRecipe::step_x).def_prop_ro("step_y",&RawPixelRecipe::step_y)
        .def_prop_ro("physical_sigmas",&RawPixelRecipe::physical_sigmas).def_prop_ro("sampled_pixel_width_um",&RawPixelRecipe::sampled_pixel_width_um).def_prop_ro("sampled_pixel_height_um",&RawPixelRecipe::sampled_pixel_height_um)
        .def_prop_ro("feature_schema",&RawPixelRecipe::feature_schema)
        .def("matches",[](const RawPixelRecipe& v,const RawImageMetadata& m,const ScientificGrid& g,const ScientificRawOptions& o){return v.matches(m,g,o);},"metadata"_a,"grid"_a,"selection"_a)
        .def("matches",[](const RawPixelRecipe& v,const RawImageMetadata& m,const ScientificRawLattice& g,const ScientificRawOptions& o){return v.matches(m,g,o);},"metadata"_a,"lattice"_a,"selection"_a)
        .def("matches",[](const RawPixelRecipe& v,const RawImageMetadata& m,const ScientificRawWindow& g,const ScientificRawOptions& o){return v.matches(m,g,o);},"metadata"_a,"window"_a,"selection"_a)
        .def("to_bytes",[](const RawPixelRecipe& v){return qpx::bindings::bytes(v.bytes());})
        .def_static("from_bytes",[](nb::bytes b){auto bytes=qpx::bindings::snapshot(b,128*1024);nb::gil_scoped_release release;return RawPixelRecipe::from_bytes(bytes);},"data"_a);
    nb::class_<RawPixelClassifier>(m,"RawPixelClassifier").def(nb::init<const ForestModel&,const RawPixelRecipe&>(),"forest"_a,"recipe"_a)
        .def_prop_ro("forest",[](const RawPixelClassifier& v){return v.forest();}).def_prop_ro("recipe",[](const RawPixelClassifier& v){return v.recipe();})
        .def_prop_ro("sha256",&RawPixelClassifier::sha256).def("to_bytes",[](const RawPixelClassifier& v){return qpx::bindings::bytes(v.bytes());})
        .def_static("from_bytes",[](nb::bytes b){auto bytes=qpx::bindings::snapshot(b,64u*1024*1024+128*1024);nb::gil_scoped_release release;return RawPixelClassifier::from_bytes(bytes);},"data"_a);
    m.def("load_raw_pixel_classifier",[](const std::string& path){return load_raw_pixel_classifier(path);},"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_raw_pixel_classifier",[](const RawPixelClassifier& v,const std::string& path,bool replace){save_raw_pixel_classifier(v,path,replace);},"model"_a,"path"_a,"replace_existing"_a=false,nb::call_guard<nb::gil_scoped_release>());
}
