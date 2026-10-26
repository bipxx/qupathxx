#pragma once
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
// Python owns the controller, which owns a native job. Destroy native state
// without the GIL: a worker's shared_ptr caster can release a Python-owned file
// or server during shutdown. The native destructor cancels/joins on any thread.
template<class Job> struct JobHolder {
    std::unique_ptr<Job> value=std::make_unique<Job>();
    JobHolder()=default;JobHolder(const JobHolder&)=delete;
    ~JobHolder() {nb::gil_scoped_release release;value.reset();}
};
template<class Method> struct JobMethod;
template<class R,class C,class... A> struct JobMethod<R(C::*)(A...)> {
    template<R(C::*M)(A...)> static R call(JobHolder<C>& self,A... args) {return ((*self.value).*M)(std::forward<A>(args)...);}
};
template<class R,class C,class... A> struct JobMethod<R(C::*)(A...) const> {
    template<R(C::*M)(A...) const> static R call(const JobHolder<C>& self,A... args) {return ((*self.value).*M)(std::forward<A>(args)...);}
};
template<auto Method> constexpr auto job_method() {return &JobMethod<decltype(Method)>::template call<Method>;}
// Capture and delivery inspect the live Document with the GIL held. Only the
// native join releases it; workers use immutable captures and no Python APIs.
template<class Job,class Document> auto wait(Job& job,const Document& doc) {
    job.poll(doc);{nb::gil_scoped_release release;job.await_completion();}return job.poll(doc);
}
template<class Job,class Document> auto wait(JobHolder<Job>& job,const Document& doc) {return wait(*job.value,doc);}
}
