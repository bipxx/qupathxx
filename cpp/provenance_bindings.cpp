#include "qupathxx/tissue.hpp"
#include "qupathxx/source_identity.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_provenance(nb::module_& m) {
    using namespace qpx;
    nb::class_<FileVersion>(m,"FileVersion").def_ro("path",&FileVersion::path).def_ro("bytes",&FileVersion::bytes)
        .def_ro("identity_scheme",&FileVersion::identity_scheme).def_ro("identity",&FileVersion::identity).def_ro("stamp_scheme",&FileVersion::stamp_scheme)
        .def_ro("modified_token",&FileVersion::modified_token).def_ro("change_token",&FileVersion::change_token).def_ro("observed_unix_ms",&FileVersion::observed_unix_ms);
    nb::class_<ReaderSource>(m,"ReaderSource").def_ro("generation",&ReaderSource::generation).def_ro("reader",&ReaderSource::reader).def_ro("locator",&ReaderSource::locator)
        .def_ro("scope",&ReaderSource::scope).def_ro("primary",&ReaderSource::primary).def_prop_ro("payload_bytes",&ReaderSource::payload_bytes);
    nb::class_<SourceFileAssociation>(m,"SourceFileAssociation").def_ro("source",&SourceFileAssociation::source).def_ro("fingerprint",&SourceFileAssociation::fingerprint).def_ro("status",&SourceFileAssociation::status);
    m.def("associate_source_file",&associate_source_file,"source"_a,"fingerprint"_a);
    m.def("reader_source_json",&reader_source_json,"source"_a);
    m.def("source_file_association_json",&source_file_association_json,"association"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_source_file_association",&save_source_file_association,"association"_a,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<BuildEnvironment>(m,"BuildEnvironment")
        .def_ro("app_version",&BuildEnvironment::app_version).def_ro("source_tree_sha256",&BuildEnvironment::source_tree_sha256).def_ro("git_revision",&BuildEnvironment::git_revision)
        .def_ro("compiler",&BuildEnvironment::compiler).def_ro("compiler_flags",&BuildEnvironment::compiler_flags).def_ro("configuration",&BuildEnvironment::configuration)
        .def_ro("platform",&BuildEnvironment::platform).def_ro("cmake_version",&BuildEnvironment::cmake_version)
        .def_ro("geos_version",&BuildEnvironment::geos_version).def_ro("openslide_version",&BuildEnvironment::openslide_version).def_ro("sqlite_version",&BuildEnvironment::sqlite_version)
        .def_ro("gui",&BuildEnvironment::gui).def_ro("python",&BuildEnvironment::python).def_ro("interchange",&BuildEnvironment::interchange);
    nb::class_<AnalysisProvenance>(m,"AnalysisProvenance").def_ro("object_id",&AnalysisProvenance::object_id).def_ro("object_revision",&AnalysisProvenance::object_revision)
        .def_ro("document_revision",&AnalysisProvenance::document_revision).def_ro("captured_unix_ms",&AnalysisProvenance::captured_unix_ms).def_ro("nucleus",&AnalysisProvenance::nucleus)
        .def_prop_ro("source",[](const AnalysisProvenance& p){return p.source;}).def_ro("reader",&AnalysisProvenance::reader).def_ro("locator",&AnalysisProvenance::locator)
        .def_ro("effective_pixel_width_um",&AnalysisProvenance::effective_pixel_width_um).def_ro("effective_pixel_height_um",&AnalysisProvenance::effective_pixel_height_um)
        .def_ro("reader_source",&AnalysisProvenance::reader_source).def_ro("environment",&AnalysisProvenance::environment).def_prop_ro("payload_bytes",&AnalysisProvenance::payload_bytes);
    m.def("build_environment",&build_environment);
    m.def("provenance_json",&provenance_json,"provenance"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_provenance",&save_provenance,"provenance"_a,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    auto result=nb::borrow<nb::class_<AnalysisResult>>(m.attr("AnalysisResult"));result.def_ro("provenance",&AnalysisResult::provenance);
    auto request=nb::borrow<nb::class_<AnalysisRequest>>(m.attr("AnalysisRequest"));request.def_prop_ro("provenance",&AnalysisRequest::provenance);
    auto tissue=nb::borrow<nb::class_<TissueResult>>(m.attr("TissueResult"));tissue.def_prop_ro("provenance",&TissueResult::provenance);
    auto source=nb::borrow<nb::class_<ImageServer>>(m.attr("ImageServer"));source.def_prop_ro("reader_id",&ImageServer::reader_id).def_prop_ro("source_locator",&ImageServer::source_locator).def_prop_ro("reader_source",&ImageServer::reader_source);
}
