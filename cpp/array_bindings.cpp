#include "qupathxx/core.hpp"
#include "qupathxx/stains.hpp"
#include "qupathxx/tissue.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <algorithm>
#include <climits>
#include <cstring>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::capsule vector_owner(std::unique_ptr<std::vector<T>> values) {
    auto* ptr=values.get();nb::capsule owner(ptr,[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});values.release();return owner;
}
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(copy) {
        auto storage=std::make_unique<std::vector<T>>(values);auto* ptr=storage->data();auto capsule=vector_owner(std::move(storage));
        return nb::ndarray<nb::numpy,T>(ptr,shape,capsule).cast();
    }
    return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
}
using Input=nb::ndarray<const std::uint8_t,nb::ndim<3>,nb::device::cpu>;
std::vector<std::uint8_t> import_pixels(const Input& input,std::size_t channels) {
    const auto h=input.shape(0),w=input.shape(1);
    if(!w || !h || w>INT_MAX || h>INT_MAX || input.shape(2)!=channels || w>256ULL*1024*1024/channels/h) throw std::invalid_argument("expected nonempty HWC uint8 pixels within 256 MiB and int32 dimensions");
    std::vector<std::uint8_t> result(w*h*channels);
    if(input.stride(2)==1 && input.stride(1)==static_cast<std::int64_t>(channels) && input.stride(0)==static_cast<std::int64_t>(w*channels)) std::memcpy(result.data(),input.data(),result.size());
    else {auto* out=result.data();for(std::size_t y=0;y<h;++y) for(std::size_t x=0;x<w;++x) for(std::size_t c=0;c<channels;++c) *out++=input(y,x,c);}
    return result;
}
}
void bind_arrays(nb::module_& m) {
    using namespace qpx;
    nb::borrow<nb::class_<Image>>(m.attr("Image"))
        .def_static("from_numpy",[](const Input& a) {auto values=import_pixels(a,3);return Image(static_cast<int>(a.shape(1)),static_cast<int>(a.shape(0)),std::move(values));},"array"_a.noconvert())
        .def("numpy",[](const Image& image,bool copy) {
            // The Image wrapper may refer into a relinkable Document. Retain
            // only its independent immutable backing, never that borrowed Image.
            auto storage=image.pixels_owner();auto retained=std::make_unique<decltype(storage)>(storage);
            nb::capsule owner(retained.get(),[](void* p) noexcept {delete static_cast<decltype(storage)*>(p);});retained.release();
            return array(*storage,{static_cast<std::size_t>(image.height()),static_cast<std::size_t>(image.width()),3},owner,copy);
        },"copy"_a=false);
    nb::borrow<nb::class_<PixelBuffer>>(m.attr("PixelBuffer"))
        .def_static("from_numpy",[](const Input& a) {auto bytes=import_pixels(a,4);return PixelBuffer(a.shape(1),a.shape(0),std::move(bytes));},"array"_a.noconvert())
        .def("numpy",[](nb::object self,bool copy) {const auto& p=nb::cast<const PixelBuffer&>(self);return array(p.bytes(),{static_cast<std::size_t>(p.height()),static_cast<std::size_t>(p.width()),4},self,copy);},"copy"_a=false);
    nb::borrow<nb::class_<FloatRgbImage>>(m.attr("FloatRgbImage"))
        .def("numpy",[](nb::object self,bool copy) {const auto& p=nb::cast<const FloatRgbImage&>(self);return array(p.values(),{static_cast<std::size_t>(p.height()),static_cast<std::size_t>(p.width()),3},self,copy);},"copy"_a=false);
    nb::borrow<nb::class_<TissueResult>>(m.attr("TissueResult"))
        .def("labels_numpy",[](nb::object self,bool copy) {const auto& p=nb::cast<const TissueResult&>(self);return array(p.labels(),{static_cast<std::size_t>(p.height()),static_cast<std::size_t>(p.width())},self,copy);},"copy"_a=false)
        .def("mask_numpy",[](const TissueResult& p) {
            auto values=std::make_unique<std::vector<std::uint8_t>>(p.labels().size());for(std::size_t i=0;i<values->size();++i) (*values)[i]=p.labels()[i]?255:0;
            auto* ptr=values->data();auto owner=vector_owner(std::move(values));return nb::ndarray<nb::numpy,std::uint8_t>(ptr,{static_cast<std::size_t>(p.height()),static_cast<std::size_t>(p.width())},owner);
        });
}
