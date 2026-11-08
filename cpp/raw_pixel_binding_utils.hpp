#pragma once
#include "job_binding_utils.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace qpx::bindings {
namespace nb=nanobind;
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
    auto storage=std::make_unique<std::vector<T>>(values);auto* data=storage->data();
    nb::capsule capsule(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});storage.release();
    return nb::ndarray<nb::numpy,T>(data,shape,capsule).cast();
}
inline nb::bytes bytes(const std::vector<std::uint8_t>& v) {return nb::bytes(reinterpret_cast<const char*>(v.data()),v.size());}
inline std::vector<std::uint8_t> snapshot(nb::bytes b,std::size_t maximum) {
    if(b.size()<68 || b.size()>maximum) throw std::invalid_argument("raw encoding size exceeds its byte limit");
    const auto* p=reinterpret_cast<const std::uint8_t*>(b.c_str());return {p,p+b.size()};
}
}
