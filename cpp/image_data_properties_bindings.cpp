#include "qupathxx/core.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <set>
namespace nb=nanobind;using namespace nb::literals;
namespace {
using V=qpx::ImageDataValue;using P=qpx::ImageDataProperties;
V from_python(nb::handle h,std::set<PyObject*>& active,std::size_t& nodes,std::size_t depth) {
    if(depth>P::max_depth || ++nodes>P::max_nodes)throw std::invalid_argument("ImageData properties exceed depth/node limit");
    if(h.is_none())return {};
    if(PyBool_Check(h.ptr()))return V{h.ptr()==Py_True};
    if(PyLong_Check(h.ptr())) {
        const auto value=PyLong_AsLongLong(h.ptr());if(PyErr_Occurred())throw nb::python_error();
        return V{std::int64_t(value)};
    }
    if(PyFloat_Check(h.ptr()))return V{nb::cast<double>(h)};
    if(PyUnicode_Check(h.ptr()))return V{nb::cast<std::string>(h)};
    if(!PyList_Check(h.ptr()) && !PyDict_Check(h.ptr()))throw std::invalid_argument("ImageData values require null/bool/int64/float/str/list/string-keyed dict");
    if(!active.insert(h.ptr()).second)throw std::invalid_argument("cyclic ImageData properties are not supported");
    struct Pop {std::set<PyObject*>& active;PyObject* p;~Pop(){active.erase(p);}} pop{active,h.ptr()};
    if(PyList_Check(h.ptr())) {
        auto list=nb::borrow<nb::list>(h);if(nb::len(list)>P::max_entries)throw std::invalid_argument("ImageData collection exceeds 4096 entries");
        V::List out;for(auto item:list)out.push_back(from_python(item,active,nodes,depth+1));return V{std::move(out)};
    }
    auto dict=nb::borrow<nb::dict>(h);if(nb::len(dict)>P::max_entries)throw std::invalid_argument("ImageData collection exceeds 4096 entries");
    V::Map out;for(auto [key,value]:dict) {if(!PyUnicode_Check(key.ptr()))throw std::invalid_argument("ImageData map keys must be strings");out.emplace(nb::cast<std::string>(key),from_python(value,active,nodes,depth+1));}return V{std::move(out)};
}
nb::object to_python(const V& v) {
    switch(v.value.index()) {
        case 0:return nb::none();case 1:return nb::bool_(std::get<bool>(v.value));
        case 2:return nb::int_(std::get<std::int64_t>(v.value));case 3:return nb::float_(std::get<double>(v.value));
        case 4:return nb::str(std::get<std::string>(v.value).c_str());
        case 5:{nb::list out;for(const auto& item:std::get<V::List>(v.value))out.append(to_python(item));return out;}
        case 6:{nb::dict out;for(const auto& [k,item]:std::get<V::Map>(v.value))out[nb::str(k.c_str())]=to_python(item);return out;}
        default:throw std::logic_error("invalid native ImageData value");
    }
}
}
void bind_image_data_properties(nb::module_& m) {
    using namespace qpx;
    nb::class_<ImageDataProperties>(m,"ImageDataProperties")
        .def(nb::new_([](nb::dict values) {std::set<PyObject*> active;std::size_t nodes=0;auto root=from_python(values,active,nodes,0);return std::make_shared<ImageDataProperties>(std::move(std::get<V::Map>(root.value)));}),"values"_a=nb::dict())
        .def_prop_ro("values",[](const P& p) {nb::dict out;for(const auto& [k,v]:p.values())out[nb::str(k.c_str())]=to_python(v);return out;})
        .def_prop_ro("payload_bytes",&P::payload_bytes);
    auto doc=nb::borrow<nb::class_<Document>>(m.attr("Document"));
    doc.def_prop_ro("image_data_properties",&Document::image_data_properties)
        .def("set_image_data_properties",&Document::set_image_data_properties,"properties"_a=nb::none());
}
