#include "qupathxx/scientific_channels.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/array.h>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
    auto storage=std::make_unique<std::vector<T>>(values);auto* data=storage->data();
    nb::capsule capsule(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});storage.release();
    return nb::ndarray<nb::numpy,T>(data,shape,capsule).cast();
}
}
void bind_scientific_grid(nb::module_& m) {
    using namespace qpx;
    nb::class_<GridFraction>(m,"GridFraction")
        .def(nb::init<std::int64_t,std::int64_t>(),"numerator"_a,"denominator"_a)
        .def_prop_ro("numerator",&GridFraction::numerator).def_prop_ro("denominator",&GridFraction::denominator)
        .def("floor",&GridFraction::floor).def("ceil",&GridFraction::ceil).def("__float__",&GridFraction::binary64);
    nb::class_<ScientificGridAxis>(m,"ScientificGridAxis")
        .def(nb::init<std::int64_t,std::int64_t,std::int64_t,std::int64_t>(),"origin"_a,"extent"_a,"samples"_a,"context"_a=0)
        .def_prop_ro("origin",&ScientificGridAxis::origin).def_prop_ro("extent",&ScientificGridAxis::extent)
        .def_prop_ro("samples",&ScientificGridAxis::samples).def_prop_ro("context",&ScientificGridAxis::context)
        .def_prop_ro("first_index",&ScientificGridAxis::first_index).def_prop_ro("stop_index",&ScientificGridAxis::stop_index)
        .def("edge",&ScientificGridAxis::edge,"index"_a).def("center",&ScientificGridAxis::center,"index"_a)
        .def("overlap",&ScientificGridAxis::overlap,"index"_a,"source_pixel"_a)
        .def_prop_ro("overlap_visits",&ScientificGridAxis::overlap_visits);
    nb::class_<GridReadBounds>(m,"GridReadBounds")
        .def_ro("x",&GridReadBounds::x).def_ro("y",&GridReadBounds::y)
        .def_ro("width",&GridReadBounds::width).def_ro("height",&GridReadBounds::height);
    nb::class_<ScientificGrid>(m,"ScientificGrid")
        .def(nb::init<std::int64_t,std::int64_t,ScientificGridAxis,ScientificGridAxis,std::uint64_t>(),
             "image_width"_a,"image_height"_a,"x"_a,"y"_a,"work_bytes"_a=256ULL*1024*1024)
        .def_prop_ro("x",[](const ScientificGrid& grid){return grid.x();})
        .def_prop_ro("y",[](const ScientificGrid& grid){return grid.y();})
        .def_prop_ro("image_width",&ScientificGrid::image_width).def_prop_ro("image_height",&ScientificGrid::image_height)
        .def_prop_ro("width",&ScientificGrid::width).def_prop_ro("height",&ScientificGrid::height)
        .def_prop_ro("read_bounds",&ScientificGrid::read_bounds).def_prop_ro("clipped_read_bounds",&ScientificGrid::clipped_read_bounds)
        .def_prop_ro("overlap_visits",&ScientificGrid::overlap_visits).def_prop_ro("sampling_buffer_bytes",&ScientificGrid::sampling_buffer_bytes);
    nb::class_<ScientificRgbGrid>(m,"ScientificRgbGrid")
        .def_prop_ro("decoded_source_sha256",&ScientificRgbGrid::decoded_source_sha256)
        .def_prop_ro("grid",[](const ScientificRgbGrid& value){return value.grid();})
        .def_prop_ro("width",&ScientificRgbGrid::width).def_prop_ro("height",&ScientificRgbGrid::height)
        .def("numpy",[](nb::object self,bool copy) {const auto& value=nb::cast<const ScientificRgbGrid&>(self);
            return array(value.values(),{std::size_t(value.height()),std::size_t(value.width()),3},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy) {const auto& value=nb::cast<const ScientificRgbGrid&>(self);
            return array(value.valid(),{std::size_t(value.height()),std::size_t(value.width())},self,copy);},"copy"_a=false);
    m.def("read_scientific_rgb",[](const std::shared_ptr<ImageServer>& server,const ScientificGrid& grid) {
        if(!server) throw std::invalid_argument("scientific RGB sampling requires a server");
        return read_scientific_rgb(*server,grid);
    },"server"_a,"grid"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::enum_<ScientificChannelMode>(m,"ScientificChannelMode")
        .value("OPTICAL_DENSITY",ScientificChannelMode::OpticalDensity).value("STAINS",ScientificChannelMode::Stains);
    nb::class_<ScientificChannels>(m,"ScientificChannels")
        .def_prop_ro("grid",[](const ScientificChannels& value){return value.grid();})
        .def_prop_ro("mode",&ScientificChannels::mode).def_prop_ro("algorithm",&ScientificChannels::algorithm)
        .def_prop_ro("white",[](const ScientificChannels& value){return value.white();})
        .def_prop_ro("stains",[](const ScientificChannels& value){return value.stains();})
        .def_prop_ro("width",&ScientificChannels::width).def_prop_ro("height",&ScientificChannels::height)
        .def_prop_ro("estimated_work_bytes",&ScientificChannels::estimated_work_bytes)
        .def("numpy",[](nb::object self,bool copy) {const auto& value=nb::cast<const ScientificChannels&>(self);
            return array(value.values(),{std::size_t(value.height()),std::size_t(value.width()),3},self,copy);},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy) {const auto& value=nb::cast<const ScientificChannels&>(self);
            return array(value.valid(),{std::size_t(value.height()),std::size_t(value.width())},self,copy);},"copy"_a=false);
    m.def("scientific_channel_work_bytes",[](const ScientificGrid& grid){return scientific_channel_work_bytes(grid);},"grid"_a);
    m.def("scientific_channel_work_bytes",[](const ScientificRgbGrid& input){return scientific_channel_work_bytes(input);},"input"_a);
    m.def("scientific_optical_densities",[](const ScientificRgbGrid& input,const StainTransform& transform,std::size_t work) {
        return scientific_optical_densities(input,transform,work);
    },"input"_a,"transform"_a,"work_bytes"_a=256ULL*1024*1024,nb::call_guard<nb::gil_scoped_release>());
    m.def("scientific_deconvolve",[](const ScientificRgbGrid& input,const StainTransform& transform,std::size_t work) {
        return scientific_deconvolve(input,transform,work);
    },"input"_a,"transform"_a,"work_bytes"_a=256ULL*1024*1024,nb::call_guard<nb::gil_scoped_release>());
}
