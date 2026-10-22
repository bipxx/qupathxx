#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/source_identity.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/shared_ptr.h>
namespace nb=nanobind;using namespace nb::literals;
void bind_file_fingerprint(nb::module_& m) {
    using namespace qpx;
    nb::class_<FileFingerprintOptions>(m,"FileFingerprintOptions").def(nb::init<>()).def_rw("byte_limit",&FileFingerprintOptions::byte_limit).def_rw("buffer_bytes",&FileFingerprintOptions::buffer_bytes).def("validate",&FileFingerprintOptions::validate);
    nb::class_<FileFingerprint>(m,"FileFingerprint").def_ro("path",&FileFingerprint::path).def_ro("sha256",&FileFingerprint::sha256).def_ro("bytes",&FileFingerprint::bytes)
        .def_ro("identity_scheme",&FileFingerprint::identity_scheme).def_ro("identity",&FileFingerprint::identity).def_ro("stamp_scheme",&FileFingerprint::stamp_scheme)
        .def_ro("modified_token",&FileFingerprint::modified_token).def_ro("change_token",&FileFingerprint::change_token)
        .def_ro("started_unix_ms",&FileFingerprint::started_unix_ms).def_ro("finished_unix_ms",&FileFingerprint::finished_unix_ms).def_ro("elapsed_ms",&FileFingerprint::elapsed_ms);
    nb::class_<FileFingerprintStats>(m,"FileFingerprintStats").def_ro("state",&FileFingerprintStats::state).def_ro("path",&FileFingerprintStats::path).def_ro("output_path",&FileFingerprintStats::output_path)
        .def_ro("error",&FileFingerprintStats::error).def_ro("bytes",&FileFingerprintStats::bytes).def_ro("total_bytes",&FileFingerprintStats::total_bytes).def_ro("completed",&FileFingerprintStats::completed).def_ro("elapsed_ms",&FileFingerprintStats::elapsed_ms);
    m.def("fingerprint_file",[](const std::string& path,FileFingerprintOptions options) {nb::gil_scoped_release release;return fingerprint_file(path,options);},"path"_a,"options"_a=FileFingerprintOptions{});
    m.def("file_fingerprint_json",&file_fingerprint_json,"fingerprint"_a,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_file_fingerprint",&save_file_fingerprint,"fingerprint"_a,"path"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<FileFingerprintJob>(m,"FileFingerprintJob").def(nb::init<>()).def("request",&FileFingerprintJob::request,"path"_a,"output_path"_a="","options"_a=FileFingerprintOptions{})
        .def("poll",&FileFingerprintJob::poll).def("wait",&FileFingerprintJob::wait,nb::call_guard<nb::gil_scoped_release>()).def("cancel",&FileFingerprintJob::cancel)
        .def_prop_ro("busy",&FileFingerprintJob::busy).def_prop_ro("result",&FileFingerprintJob::result).def_prop_ro("association",&FileFingerprintJob::association)
        .def("request_source",&FileFingerprintJob::request_source,"source"_a,"output_path"_a="","options"_a=FileFingerprintOptions{});
    auto options=nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"));
    options.def_rw("file_fingerprint_associate_source",&GuiOptions::file_fingerprint_associate_source).def_rw("file_fingerprint_path",&GuiOptions::file_fingerprint_path).def_rw("file_fingerprint_output",&GuiOptions::file_fingerprint_output)
        .def_rw("file_fingerprint_byte_limit",&GuiOptions::file_fingerprint_byte_limit).def_rw("file_fingerprint_buffer_bytes",&GuiOptions::file_fingerprint_buffer_bytes);
    auto report=nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"));
    report.def_ro("file_fingerprint_association",&GuiReport::file_fingerprint_association).def_ro("file_fingerprint_state",&GuiReport::file_fingerprint_state).def_ro("file_fingerprint_error",&GuiReport::file_fingerprint_error)
        .def_ro("file_fingerprint_result",&GuiReport::file_fingerprint_result).def_ro("file_fingerprint_bytes",&GuiReport::file_fingerprint_bytes).def_ro("file_fingerprint_ms",&GuiReport::file_fingerprint_ms);
}
