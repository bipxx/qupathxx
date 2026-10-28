#pragma once
#include <nanobind/nanobind.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
namespace qpx::bindings {
namespace nb=nanobind;
// Python owns the controller, which owns a native job. Destroy native state
// without the GIL: a worker's shared_ptr caster can release a Python-owned file
// or server during shutdown. The native destructor cancels/joins on any thread.
template<class Job> struct JobHolder {
    std::unique_ptr<Job> value=std::make_unique<Job>();
    JobHolder()=default;JobHolder(const JobHolder&)=delete;
    ~JobHolder() {nb::gil_scoped_release release;value.reset();}
};
// Python entry points serialize mutable controller state with the GIL. An
// owner check prevents another thread from collecting/replacing a future while
// its owner waits without the GIL. Cancellation is the sole cross-thread call.
template<class Job> struct OwnerJobHolder : JobHolder<Job> {
    const std::thread::id owner=std::this_thread::get_id();
    void check_owner() const {
        if(owner!=std::this_thread::get_id()) throw std::runtime_error("job controller must run on its owner thread");
    }
};
template<class Method,template<class> class Holder=JobHolder> struct JobMethod;
template<template<class> class Holder,class R,class C,class... A> struct JobMethod<R(C::*)(A...),Holder> {
    template<R(C::*M)(A...),bool RequireOwner=true> static R call(Holder<C>& self,A... args) {if constexpr(RequireOwner && std::is_same_v<Holder<C>,OwnerJobHolder<C>>) self.check_owner();return ((*self.value).*M)(std::forward<A>(args)...);}
};
template<template<class> class Holder,class R,class C,class... A> struct JobMethod<R(C::*)(A...) const,Holder> {
    template<R(C::*M)(A...) const,bool RequireOwner=true> static R call(const Holder<C>& self,A... args) {if constexpr(RequireOwner && std::is_same_v<Holder<C>,OwnerJobHolder<C>>) self.check_owner();return ((*self.value).*M)(std::forward<A>(args)...);}
};
template<auto Method> constexpr auto job_method() {return &JobMethod<decltype(Method)>::template call<Method>;}
template<auto Method,bool RequireOwner=true> constexpr auto owner_job_method() {return &JobMethod<decltype(Method),OwnerJobHolder>::template call<Method,RequireOwner>;}
// Capture and delivery inspect the live Document with the GIL held. Only the
// native join releases it; workers use immutable captures and no Python APIs.
template<class Job,class Document,class... A> auto wait(Job& job,const Document& doc,A... args) {
    job.poll(doc,args...);{nb::gil_scoped_release release;job.await_completion();}return job.poll(doc,args...);
}
template<class Job,class Document,class... A> auto wait(JobHolder<Job>& job,const Document& doc,A... args) {return wait(*job.value,doc,args...);}
template<class Job,class Document,class... A> auto wait(OwnerJobHolder<Job>& job,const Document& doc,A... args) {job.check_owner();return wait(*job.value,doc,args...);}
// Document-free controllers still collect mutable future/result state under
// the GIL. Cross-thread cancel may run only during the readiness-only wait.
template<class Job> auto wait(OwnerJobHolder<Job>& job) {
    job.check_owner();job.value->poll();
    {nb::gil_scoped_release release;job.value->await_completion();}
    return job.value->poll();
}
}
