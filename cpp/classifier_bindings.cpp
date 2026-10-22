#include "qupathxx/classifier.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <cstring>
namespace nb=nanobind;using namespace nb::literals;
void bind_object_classifiers(nb::module_&);
void bind_pixel_features(nb::module_&);
namespace {
using Samples=nb::ndarray<const float,nb::ndim<2>,nb::device::cpu>;
using Labels=nb::ndarray<const std::uint32_t,nb::ndim<1>,nb::device::cpu>;
std::vector<float> snapshot(const Samples& a,std::size_t columns) {
    if(!a.shape(0) || a.shape(0)>100000 || a.shape(1)!=columns || !columns || columns>128) throw std::invalid_argument("expected nonempty classifier float32 matrix with matching columns and <=100000 rows");
    std::vector<float> result(a.shape(0)*columns);
    if(a.stride(1)==1 && a.stride(0)==static_cast<std::int64_t>(columns)) std::memcpy(result.data(),a.data(),result.size()*sizeof(float));
    else for(std::size_t r=0;r<a.shape(0);++r) for(std::size_t c=0;c<columns;++c) result[r*columns+c]=a(r,c);
    return result;
}
}
void bind_classifiers(nb::module_& m) {
    using namespace qpx;
    nb::class_<ClassifierFeature>(m,"ClassifierFeature").def(nb::init<>()).def(nb::init<std::string,std::string>(),"name"_a,"unit"_a)
        .def_rw("name",&ClassifierFeature::name).def_rw("unit",&ClassifierFeature::unit);
    nb::class_<ForestParameters>(m,"ForestParameters").def(nb::init<>()).def_rw("trees",&ForestParameters::trees).def_rw("max_depth",&ForestParameters::max_depth)
        .def_rw("min_samples",&ForestParameters::min_samples).def_rw("active_features",&ForestParameters::active_features).def_rw("seed",&ForestParameters::seed).def_rw("work_bytes",&ForestParameters::work_bytes).def("validate",&ForestParameters::validate);
    nb::class_<ClassifierPrediction>(m,"ClassifierPrediction").def_ro("class_index",&ClassifierPrediction::class_index).def_ro("vote_fraction",&ClassifierPrediction::vote_fraction).def_ro("votes",&ClassifierPrediction::votes);
    nb::class_<ForestModel>(m,"ForestModel")
        .def_prop_ro("features",[](const ForestModel& f){return f.features();}).def_prop_ro("classes",[](const ForestModel& f){return f.classes();})
        .def_prop_ro("parameters",[](const ForestModel& f){return f.parameters();}).def_prop_ro("training_sha256",&ForestModel::training_sha256).def_prop_ro("backend",&ForestModel::backend)
        .def_prop_ro("training_rows",&ForestModel::training_rows).def_prop_ro("tree_count",&ForestModel::tree_count).def_prop_ro("node_count",&ForestModel::node_count)
        .def_prop_ro("sha256",&ForestModel::sha256,nb::call_guard<nb::gil_scoped_release>())
        .def("to_bytes",[](const ForestModel& f){std::vector<std::uint8_t> b;{nb::gil_scoped_release release;b=f.bytes();}return nb::bytes(b.data(),b.size());})
        .def_static("from_bytes",[](nb::bytes b){if(b.size()>64*1024*1024) throw std::length_error("classifier model exceeds byte limit");std::vector<std::uint8_t> snapshot(b.size());std::memcpy(snapshot.data(),b.c_str(),b.size());nb::gil_scoped_release release;return ForestModel::from_bytes(snapshot);},"data"_a)
        .def("predict",[](const ForestModel& f,const Samples& x,std::vector<ClassifierFeature> schema){auto samples=snapshot(x,schema.size());nb::gil_scoped_release release;return f.predict(samples,schema);},"samples"_a.noconvert(),"schema"_a);
    m.attr("has_classifier_training")=has_classifier_training();
    m.def("load_forest_model",[](const std::string& path){return load_forest_model(path);},"path"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_forest_model",[](const ForestModel& model,const std::string& path,bool replace){save_forest_model(model,path,replace);},"model"_a,"path"_a,"replace_existing"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("train_forest",[](const Samples& x,const Labels& y,std::vector<ClassifierFeature> schema,std::vector<std::string> classes,ForestParameters p){
        auto samples=snapshot(x,schema.size());if(y.shape(0)!=x.shape(0)) throw std::invalid_argument("classifier label count differs from rows");
        std::vector<std::uint32_t> labels(y.shape(0));for(std::size_t i=0;i<labels.size();++i) labels[i]=y(i);
        nb::gil_scoped_release release;return train_forest(samples,labels,schema,classes,p);
    },"samples"_a.noconvert(),"labels"_a.noconvert(),"schema"_a,"classes"_a,"parameters"_a=ForestParameters{});
    bind_object_classifiers(m);
    bind_pixel_features(m);
}
