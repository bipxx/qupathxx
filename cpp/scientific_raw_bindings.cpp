#include "qupathxx/scientific_raw_sampling.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
    auto storage=std::make_unique<std::vector<T>>(values);auto* data=storage->data();
    nb::capsule capsule(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});storage.release();
    return nb::ndarray<nb::numpy,T>(data,shape,capsule).cast();
}
}
void bind_scientific_raw(nb::module_& m) {
    using namespace qpx;
    nb::enum_<ScientificRawReason>(m,"ScientificRawReason")
        .value("VALID",ScientificRawReason::Valid).value("OUTSIDE_SOURCE",ScientificRawReason::OutsideSource)
        .value("INVALID_SOURCE",ScientificRawReason::InvalidSource).value("NONFINITE_CHANNEL",ScientificRawReason::NonfiniteChannel);
    nb::class_<ScientificRawOptions>(m,"ScientificRawOptions")
        .def(nb::init<>()).def_rw("first_channel",&ScientificRawOptions::first_channel).def_rw("channels",&ScientificRawOptions::channels)
        .def_rw("z",&ScientificRawOptions::z).def_rw("t",&ScientificRawOptions::t).def_rw("work_bytes",&ScientificRawOptions::work_bytes)
        .def("validate",&ScientificRawOptions::validate);
    nb::class_<ScientificRawGrid>(m,"ScientificRawGrid")
        .def_prop_ro("grid",[](const ScientificRawGrid& v){return v.grid();})
        .def_prop_ro("metadata",[](const ScientificRawGrid& v){return v.metadata();})
        .def_prop_ro("options",[](const ScientificRawGrid& v){return v.options();})
        .def_prop_ro("width",&ScientificRawGrid::width).def_prop_ro("height",&ScientificRawGrid::height).def_prop_ro("channels",&ScientificRawGrid::channels)
        .def_prop_ro("reader",&ScientificRawGrid::reader).def_prop_ro("decoded_source_sha256",&ScientificRawGrid::decoded_source_sha256)
        .def_prop_ro("input_sha256",&ScientificRawGrid::input_sha256).def_prop_ro("estimated_work_bytes",&ScientificRawGrid::estimated_work_bytes)
        .def("numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawGrid&>(self);return array(v.values(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawGrid&>(self);return array(v.valid(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false)
        .def("reasons_numpy",[](nb::object self,bool copy){const auto& v=nb::cast<const ScientificRawGrid&>(self);return array(v.reasons(),{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},"copy"_a=false);
    nb::class_<ScientificRawDigest>(m,"ScientificRawDigest")
        .def_prop_ro("metadata",[](const ScientificRawDigest& v){return v.metadata;})
        .def_ro("reader",&ScientificRawDigest::reader).def_ro("decoded_source_sha256",&ScientificRawDigest::decoded_source_sha256)
        .def_ro("input_sha256",&ScientificRawDigest::input_sha256).def_ro("estimated_work_bytes",&ScientificRawDigest::estimated_work_bytes);
    m.def("scientific_raw_work_bytes",&scientific_raw_work_bytes,"metadata"_a,"grid"_a,"options"_a=ScientificRawOptions{});
    m.def("read_scientific_raw",[](const std::shared_ptr<ImageServer>& server,const ScientificGrid& grid,ScientificRawOptions options){
        if(!server) throw std::invalid_argument("scientific raw sampling requires a server");
        return read_scientific_raw(*server,grid,options);
    },"server"_a,"grid"_a,"options"_a=ScientificRawOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("digest_scientific_raw",[](const std::shared_ptr<ImageServer>& server,const ScientificGrid& grid,ScientificRawOptions options){
        if(!server) throw std::invalid_argument("scientific raw digest requires a server");
        return digest_scientific_raw(*server,grid,options);
    },"server"_a,"grid"_a,"options"_a=ScientificRawOptions{},nb::call_guard<nb::gil_scoped_release>());
}
