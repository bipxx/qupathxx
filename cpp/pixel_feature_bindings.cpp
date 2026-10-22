#include "qupathxx/pixel_classifier.hpp"
#include <cstring>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
    auto storage=std::make_unique<std::vector<T>>(values);auto* data=storage->data();
    nb::capsule capsule(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});storage.release();
    return nb::ndarray<nb::numpy,T>(data,shape,capsule).cast();
}
}
void bind_pixel_features(nb::module_& m) {
    using namespace qpx;
    nb::class_<PixelFeatureOptions>(m,"PixelFeatureOptions").def(nb::init<>())
        .def_rw("rgb",&PixelFeatureOptions::rgb).def_rw("optical_density",&PixelFeatureOptions::optical_density).def_rw("stains",&PixelFeatureOptions::stains)
        .def_rw("gradient_magnitude",&PixelFeatureOptions::gradient_magnitude).def_rw("laplacian",&PixelFeatureOptions::laplacian)
        .def_rw("sigmas",&PixelFeatureOptions::sigmas).def_rw("work_bytes",&PixelFeatureOptions::work_bytes)
        .def("validate",&PixelFeatureOptions::validate).def_prop_ro("feature_count",&PixelFeatureOptions::feature_count).def_prop_ro("halo",&PixelFeatureOptions::halo);
    nb::class_<PixelFeatureWindow>(m,"PixelFeatureWindow").def(nb::init<>())
        .def(nb::init<std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t>(),"image_width"_a,"image_height"_a,"x"_a,"y"_a,"width"_a,"height"_a,"input_x"_a=0,"input_y"_a=0)
        .def_rw("image_width",&PixelFeatureWindow::image_width).def_rw("image_height",&PixelFeatureWindow::image_height)
        .def_rw("x",&PixelFeatureWindow::x).def_rw("y",&PixelFeatureWindow::y).def_rw("width",&PixelFeatureWindow::width).def_rw("height",&PixelFeatureWindow::height)
        .def_rw("input_x",&PixelFeatureWindow::input_x).def_rw("input_y",&PixelFeatureWindow::input_y);
    nb::class_<PixelFeatureImage>(m,"PixelFeatureImage")
        .def_prop_ro("algorithm",&PixelFeatureImage::algorithm).def_prop_ro("width",&PixelFeatureImage::width).def_prop_ro("height",&PixelFeatureImage::height)
        .def_prop_ro("window",&PixelFeatureImage::window).def_prop_ro("options",&PixelFeatureImage::options)
        .def_prop_ro("features",[](const PixelFeatureImage& image){return image.features();})
        .def_prop_ro("white",&PixelFeatureImage::white).def_prop_ro("stains",&PixelFeatureImage::stains)
        .def_prop_ro("valid_pixels",&PixelFeatureImage::valid_pixels).def_prop_ro("estimated_work_bytes",&PixelFeatureImage::estimated_work_bytes)
        .def_prop_ro("retained_bytes",&PixelFeatureImage::retained_bytes).def_prop_ro("elapsed_ms",&PixelFeatureImage::elapsed_ms)
        .def_prop_ro("sha256",&PixelFeatureImage::sha256).def_prop_ro("input_sha256",&PixelFeatureImage::input_sha256)
        .def_prop_ro("context_x",&PixelFeatureImage::context_x).def_prop_ro("context_y",&PixelFeatureImage::context_y)
        .def_prop_ro("context_width",&PixelFeatureImage::context_width).def_prop_ro("context_height",&PixelFeatureImage::context_height)
        .def("numpy",[](nb::object self,bool copy) {const auto& v=nb::cast<const PixelFeatureImage&>(self);return array(v.values(),{std::size_t(v.height()),std::size_t(v.width()),v.features().size()},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy) {const auto& v=nb::cast<const PixelFeatureImage&>(self);return array(v.valid(),{std::size_t(v.height()),std::size_t(v.width())},self,copy);},"copy"_a=false);
    const StainTransform identity({{1,0,0},{0,1,0}});
    nb::class_<PixelResolution>(m,"PixelResolution").def(nb::init<>())
        .def_rw("downsample",&PixelResolution::downsample).def_rw("pixel_width_um",&PixelResolution::pixel_width_um).def_rw("pixel_height_um",&PixelResolution::pixel_height_um)
        .def_prop_ro("physical",&PixelResolution::physical).def("validate",&PixelResolution::validate);
    nb::class_<PixelClassifier>(m,"PixelClassifier")
        .def(nb::init<const ForestModel&,PixelFeatureOptions,const StainTransform&,std::optional<PixelResolution>>(),"forest"_a,"options"_a=PixelFeatureOptions{},"transform"_a=identity,"resolution"_a=nb::none())
        .def_prop_ro("resolution",&PixelClassifier::resolution)
        .def_prop_ro("forest",&PixelClassifier::forest,nb::rv_policy::copy)
        .def_prop_ro("options",&PixelClassifier::options).def_prop_ro("transform",&PixelClassifier::transform,nb::rv_policy::copy)
        .def_prop_ro("features",[](const PixelClassifier& v){return v.features();}).def_prop_ro("classes",[](const PixelClassifier& v){return v.classes();})
        .def_prop_ro("sha256",&PixelClassifier::sha256)
        .def("to_bytes",[](const PixelClassifier& v){return nb::bytes(reinterpret_cast<const char*>(v.bytes().data()),v.bytes().size());})
        .def_static("from_bytes",[](nb::bytes b){if(b.size()>64u*1024*1024+4096) throw std::length_error("pixel classifier recipe exceeds byte limit");std::vector<std::uint8_t> bytes(b.size());std::memcpy(bytes.data(),b.c_str(),b.size());nb::gil_scoped_release release;return PixelClassifier::from_bytes(bytes);});
    nb::class_<PixelClassification>(m,"PixelClassification")
        .def_prop_ro("algorithm",&PixelClassification::algorithm).def_prop_ro("width",&PixelClassification::width).def_prop_ro("height",&PixelClassification::height)
        .def_prop_ro("window",&PixelClassification::window).def_prop_ro("classes",[](const PixelClassification& v){return v.classes();})
        .def_prop_ro("class_counts",[](const PixelClassification& v){return v.class_counts();})
        .def_prop_ro("valid_pixels",&PixelClassification::valid_pixels).def_prop_ro("classified_pixels",&PixelClassification::classified_pixels)
        .def_prop_ro("min_vote_fraction",&PixelClassification::min_vote_fraction).def_prop_ro("elapsed_ms",&PixelClassification::elapsed_ms)
        .def_prop_ro("estimated_work_bytes",&PixelClassification::estimated_work_bytes).def_prop_ro("model_sha256",&PixelClassification::model_sha256)
        .def_prop_ro("feature_sha256",&PixelClassification::feature_sha256).def_prop_ro("input_sha256",&PixelClassification::input_sha256).def_prop_ro("sha256",&PixelClassification::sha256)
        .def("labels_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const PixelClassification&>(self);return array(v.labels(),{std::size_t(v.height()),std::size_t(v.width())},self,copy);},"copy"_a=false)
        .def("vote_fractions_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const PixelClassification&>(self);return array(v.vote_fractions(),{std::size_t(v.height()),std::size_t(v.width())},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const PixelClassification&>(self);return array(v.valid(),{std::size_t(v.height()),std::size_t(v.width())},self,copy);},"copy"_a=false);
    m.def("classify_pixels",[](const PixelBuffer& p,const PixelClassifier& c,double minimum,std::optional<PixelFeatureWindow> w,std::size_t budget){return classify_pixels(p,c,minimum,w,budget);},"pixels"_a,"classifier"_a,"min_vote_fraction"_a=0,"window"_a=nb::none(),"work_bytes"_a=0,nb::call_guard<nb::gil_scoped_release>());
    m.def("load_pixel_classifier",[](const std::string& path){return load_pixel_classifier(path);},"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_pixel_classifier",[](const PixelClassifier& c,const std::string& path,bool replace){save_pixel_classifier(c,path,replace);},"classifier"_a,"path"_a,"replace_existing"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("pixel_feature_schema",&pixel_feature_schema,"options"_a=PixelFeatureOptions{},"transform"_a=identity);
    m.def("pixel_feature_work_bytes",&pixel_feature_work_bytes,"input_width"_a,"input_height"_a,"options"_a=PixelFeatureOptions{},"window"_a=nb::none());
    m.def("extract_pixel_features",[](const PixelBuffer& p,PixelFeatureOptions o,const StainTransform& t,std::optional<PixelFeatureWindow> w){return extract_pixel_features(p,t,std::move(o),w);},"pixels"_a,"options"_a=PixelFeatureOptions{},"transform"_a=identity,"window"_a=nb::none(),nb::call_guard<nb::gil_scoped_release>());
}
