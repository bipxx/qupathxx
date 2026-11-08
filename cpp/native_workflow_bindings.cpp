#include "qupathxx/density_workflow.hpp"
#include "qupathxx/nucls_import.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <filesystem>
#include <type_traits>
namespace nb=nanobind;
using namespace nb::literals;
namespace {
// Reject Python's bool-as-int coercion at the language conversion boundary.
template<class T> T number(nb::handle value) {
    if(PyBool_Check(value.ptr())) throw nb::type_error("Expected a number, not bool");
    if constexpr(std::is_integral_v<T>) {
        if(!PyLong_Check(value.ptr())) throw nb::type_error("Expected an integer");
    } else if(!PyLong_Check(value.ptr()) && !PyFloat_Check(value.ptr()))
        throw nb::type_error("Expected a number");
    try {return nb::cast<T>(value);}
    catch(const nb::cast_error&) {throw nb::value_error("Number is outside the native type range");}
}
template<class C,class T> void numeric(nb::class_<C>& c,const char* name,T C::* member) {
    c.def_prop_rw(name,[member](const C& v){return v.*member;},
        [member](C& v,nb::handle value){v.*member=number<T>(value);});
}
template<class C> void flag(nb::class_<C>& c,const char* name,bool C::* member) {
    c.def_prop_rw(name,[member](const C& v){return v.*member;},
        [member](C& v,nb::handle value){
            if(!PyBool_Check(value.ptr())) throw nb::type_error("Expected bool");
            v.*member=nb::cast<bool>(value);
        });
}
std::vector<std::uint64_t> ids(nb::handle values) {
    std::vector<std::uint64_t> out;
    for(nb::handle v:nb::borrow<nb::iterable>(values)) out.push_back(number<std::uint64_t>(v));
    return out;
}
}
void bind_native_workflows(nb::module_& m) {
    using namespace qpx;
    m.attr("has_native_workflows")=true;
    nb::exception<WorkflowInputChanged>(m,"WorkflowInputChanged",PyExc_RuntimeError);
    nb::exception<WorkflowOutputExists>(m,"WorkflowOutputExists",PyExc_FileExistsError);
    nb::exception<ProjectDensityCsvBudget>(m,"ProjectDensityCsvBudget",PyExc_ValueError);
    nb::exception<WorkflowCaptureLimit>(m,"WorkflowCaptureLimit",PyExc_ValueError);
    nb::exception<WorkflowCancelled>(m,"WorkflowCancelled",PyExc_RuntimeError);
    nb::exception<WorkflowControlError>(m,"WorkflowControlError",PyExc_RuntimeError);
    nb::register_exception_translator([](const std::exception_ptr& e,void*) {
        try {std::rethrow_exception(e);}
        catch(const std::filesystem::filesystem_error& error){PyErr_SetString(PyExc_OSError,error.what());}
    });
    auto selection=nb::class_<ProjectDensitySelection>(m,"ProjectDensitySelection").def(nb::init<>());
    numeric(selection,"image_id",&ProjectDensitySelection::image_id);
    numeric(selection,"record_id",&ProjectDensitySelection::record_id);
    selection.def_prop_rw("ids",[](const ProjectDensitySelection& v){return v.ids;},
        [](ProjectDensitySelection& v,nb::handle value){
            auto roles=nb::cast<std::array<nb::object,4>>(value);
            std::array<std::vector<std::uint64_t>,4> owned;
            for(std::size_t i=0;i<4;++i) owned[i]=ids(roles[i]);
            v.ids=std::move(owned);
        });
    auto recipe=nb::class_<ProjectDensityRecipe>(m,"ProjectDensityRecipe").def(nb::init<>());
    recipe.def_rw("selections",&ProjectDensityRecipe::selections)
        .def_rw("classes",&ProjectDensityRecipe::classes).def_rw("options",&ProjectDensityRecipe::options)
        .def_prop_rw("images",[](const ProjectDensityRecipe& v){return v.images;},
            [](ProjectDensityRecipe& v,nb::handle value){v.images=ids(value);})
        .def("validate",&ProjectDensityRecipe::validate).def("capture_bytes",&ProjectDensityRecipe::capture_bytes);
    flag(recipe,"all_images",&ProjectDensityRecipe::all_images);flag(recipe,"all_cells",&ProjectDensityRecipe::all_cells);
    nb::class_<ProjectDensityManifest>(m,"ProjectDensityManifest")
        .def_prop_ro("path",&ProjectDensityManifest::path).def_prop_ro("sha256",&ProjectDensityManifest::sha256)
        .def_prop_ro("project",&ProjectDensityManifest::project)
        .def_prop_ro("recipe",[](const ProjectDensityManifest& v){return v.recipe();})
        .def("verify",[](const ProjectDensityManifest& v,bool rehash){v.verify(rehash);},"rehash"_a=false,nb::call_guard<nb::gil_scoped_release>());
    m.def("load_project_density_manifest",[](const std::string& path){return load_project_density_manifest(path);},
        "path"_a,nb::call_guard<nb::gil_scoped_release>());
    auto image=nb::class_<ProjectDensityImage>(m,"ProjectDensityImage");
    image.def_ro("id",&ProjectDensityImage::id);
    image.def_ro("record",&ProjectDensityImage::record);
    image.def_ro("checkpoint_image",&ProjectDensityImage::checkpoint_image);
    image.def_ro("hint_record",&ProjectDensityImage::hint_record);
    image.def_ro("name",&ProjectDensityImage::name);
    image.def_ro("error",&ProjectDensityImage::error);
    image.def_ro("state",&ProjectDensityImage::state);
    image.def_ro("checkpoint",&ProjectDensityImage::checkpoint);
    image.def_ro("input_sha256",&ProjectDensityImage::input_sha256);
    image.def_ro("result_sha256",&ProjectDensityImage::result_sha256);
    image.def_ro("issues",&ProjectDensityImage::issues);
    image.def_ro("image_present",&ProjectDensityImage::image_present);
    image.def_ro("reuse",&ProjectDensityImage::reuse);
    image.def_ro("reuse_reason",&ProjectDensityImage::reuse_reason);
    image.def_ro("checkpoint_sha256",&ProjectDensityImage::checkpoint_sha256);
    image.def_ro("result_input_sha256",&ProjectDensityImage::result_input_sha256);
    image.def_ro("receipt_json",&ProjectDensityImage::receipt_json);
    image.def_ro("ids",&ProjectDensityImage::ids);
    image.def_ro("x",&ProjectDensityImage::x);
    image.def_ro("y",&ProjectDensityImage::y);
    image.def_ro("rows",&ProjectDensityImage::rows);
    auto report=nb::class_<ProjectDensityReport>(m,"ProjectDensityReport");
    report.def_ro("source",&ProjectDensityReport::source);
    report.def_ro("source_sha256",&ProjectDensityReport::source_sha256);
    report.def_ro("project_id",&ProjectDensityReport::project_id);
    report.def_ro("state",&ProjectDensityReport::state);
    report.def_ro("error",&ProjectDensityReport::error);
    report.def_ro("output",&ProjectDensityReport::output);
    report.def_ro("csv",&ProjectDensityReport::csv);
    report.def_ro("report",&ProjectDensityReport::report);
    report.def_ro("manifest",&ProjectDensityReport::manifest);
    report.def_ro("manifest_sha256",&ProjectDensityReport::manifest_sha256);
    report.def_ro("json",&ProjectDensityReport::json);
    report.def_ro("images",&ProjectDensityReport::images);
    report.def_ro("completed",&ProjectDensityReport::completed);
    report.def_ro("computed",&ProjectDensityReport::computed);
    report.def_ro("reused",&ProjectDensityReport::reused);
    report.def_ro("failed",&ProjectDensityReport::failed);
    report.def_ro("rows",&ProjectDensityReport::rows);
    report.def_ro("not_run",&ProjectDensityReport::not_run);
    report.def_prop_ro("recipe",[](const ProjectDensityReport& v){return v.recipe;})
        .def_prop_ro("valid",&ProjectDensityReport::valid).def("validate_preview",&ProjectDensityReport::validate_preview);
    nb::class_<ProjectDensityManifestReport>(m,"ProjectDensityManifestReport")
        .def_prop_ro("preview",[](const ProjectDensityManifestReport& v){return v.preview;})
        .def_ro("manifest_json",&ProjectDensityManifestReport::manifest_json).def_ro("json",&ProjectDensityManifestReport::json)
        .def_ro("output",&ProjectDensityManifestReport::output).def_ro("written",&ProjectDensityManifestReport::written);
    auto run=nb::class_<ProjectDensityRunOptions>(m,"ProjectDensityRunOptions").def(nb::init<>());
    run.def_rw("resume_from",&ProjectDensityRunOptions::resume_from).def("validate",&ProjectDensityRunOptions::validate);
    numeric(run,"csv_bytes",&ProjectDensityRunOptions::csv_bytes);
    numeric(run,"project_bytes",&ProjectDensityRunOptions::project_bytes);
    numeric(run,"checkpoint_bytes",&ProjectDensityRunOptions::checkpoint_bytes);
    flag(run,"recompute",&ProjectDensityRunOptions::recompute);
    flag(run,"stop_on_error",&ProjectDensityRunOptions::stop_on_error);
    flag(run,"pause_after_image",&ProjectDensityRunOptions::pause_after_image);
    auto density_progress=nb::class_<ProjectDensityProgress>(m,"ProjectDensityProgress");
    density_progress.def_prop_ro("completed",[](const ProjectDensityProgress& v){return v.completed.load();});
    density_progress.def_prop_ro("total",[](const ProjectDensityProgress& v){return v.total.load();});
    density_progress.def_prop_ro("kernel_calls",[](const ProjectDensityProgress& v){return v.kernel_calls.load();});
    density_progress.def_prop_ro("reused",[](const ProjectDensityProgress& v){return v.reused.load();});
    density_progress.def_prop_ro("image",[](const ProjectDensityProgress& v){return v.image.load();});
    density_progress.def_prop_ro("phase",[](const ProjectDensityProgress& v){return v.phase.load();});
    nb::class_<ProjectDensityJob>(m,"ProjectDensityJob").def(nb::init<>())
        .def("preview",&ProjectDensityJob::preview,"project"_a,"recipe"_a)
        .def("generate_manifest",&ProjectDensityJob::generate_manifest,"project"_a,"recipe"_a,"output"_a="")
        .def("run",nb::overload_cast<const ProjectDensityManifest&,const std::string&,const ProjectDensityRunOptions&>(&ProjectDensityJob::run),
            "manifest"_a,"output"_a,"options"_a=ProjectDensityRunOptions{})
        .def("run_preview",nb::overload_cast<const ProjectDensityReport&,const std::string&,const std::string&>(&ProjectDensityJob::run),
            "preview"_a,"output"_a,"resume_from"_a="")
        .def_prop_ro("busy",&ProjectDensityJob::busy).def_prop_ro("cancelling",&ProjectDensityJob::cancelling)
        .def("poll",&ProjectDensityJob::poll).def("cancel",&ProjectDensityJob::cancel)
        .def("finish",&ProjectDensityJob::finish,nb::call_guard<nb::gil_scoped_release>())
        .def("rethrow_error",&ProjectDensityJob::rethrow_error)
        .def("take_images",&ProjectDensityJob::take_images).def("continue_images",&ProjectDensityJob::continue_images)
        .def("abort_control",&ProjectDensityJob::abort_control,"error"_a)
        .def_prop_ro("result",&ProjectDensityJob::result).def_prop_ro("manifest_result",&ProjectDensityJob::manifest_result)
        .def_prop_ro("progress",&ProjectDensityJob::progress,nb::rv_policy::reference_internal)
        .def_prop_ro("error",&ProjectDensityJob::error);
    m.def("preview_project_density",[](const std::string& project,ProjectDensityRecipe recipe){
        nb::gil_scoped_release release;return preview_project_density(project,recipe);
    },"project"_a,"recipe"_a);
    m.def("generate_project_density_manifest",[](const std::string& project,ProjectDensityRecipe recipe,const std::string& output){
        nb::gil_scoped_release release;return generate_project_density_manifest(project,recipe,output);
    },"project"_a,"recipe"_a,"output"_a="");
    m.def("run_project_density",[](ProjectDensityManifest manifest,const std::string& output,ProjectDensityRunOptions options){
        nb::gil_scoped_release release;return run_project_density(manifest,output,options);
    },"manifest"_a,"output"_a,"options"_a=ProjectDensityRunOptions{});
    auto limits=nb::class_<NuclsImportLimits>(m,"NuclsImportLimits").def(nb::init<>());
    numeric(limits,"images",&NuclsImportLimits::images);
    numeric(limits,"pixels",&NuclsImportLimits::pixels);
    numeric(limits,"rows",&NuclsImportLimits::rows);
    numeric(limits,"vertices",&NuclsImportLimits::vertices);
    numeric(limits,"issues_per_image",&NuclsImportLimits::issues_per_image);
    numeric(limits,"png_bytes",&NuclsImportLimits::png_bytes);
    numeric(limits,"csv_bytes",&NuclsImportLimits::csv_bytes);
    numeric(limits,"report_bytes",&NuclsImportLimits::report_bytes);
    numeric(limits,"snapshot_bytes",&NuclsImportLimits::snapshot_bytes);
    numeric(limits,"capture_bytes",&NuclsImportLimits::capture_bytes);
    numeric(limits,"file_bytes",&NuclsImportLimits::file_bytes);
    limits.def("validate",&NuclsImportLimits::validate);
    auto options=nb::class_<NuclsImportOptions>(m,"NuclsImportOptions").def(nb::init<>());
    numeric(options,"mpp_x",&NuclsImportOptions::mpp_x);numeric(options,"mpp_y",&NuclsImportOptions::mpp_y);
    flag(options,"pause_after_pair",&NuclsImportOptions::pause_after_pair);
    options.def_rw("representation",&NuclsImportOptions::representation).def_rw("class_column",&NuclsImportOptions::class_column)
        .def_rw("names",&NuclsImportOptions::names).def_rw("limits",&NuclsImportOptions::limits).def("validate",&NuclsImportOptions::validate);
    auto pair=nb::class_<NuclsPairReport>(m,"NuclsPairReport");
    pair.def_ro("name",&NuclsPairReport::name);
    pair.def_ro("image",&NuclsPairReport::image);
    pair.def_ro("annotations",&NuclsPairReport::annotations);
    pair.def_ro("image_sha256",&NuclsPairReport::image_sha256);
    pair.def_ro("csv_sha256",&NuclsPairReport::csv_sha256);
    pair.def_ro("rows",&NuclsPairReport::rows);
    pair.def_ro("imported",&NuclsPairReport::imported);
    pair.def_ro("source_bounds_outside",&NuclsPairReport::source_bounds_outside);
    pair.def_ro("issue_count",&NuclsPairReport::issue_count);
    pair.def_ro("classes",&NuclsPairReport::classes);
    pair.def_ro("issues",&NuclsPairReport::issues);
    pair.def_ro("width",&NuclsPairReport::width);
    pair.def_ro("height",&NuclsPairReport::height);
    auto imported=nb::class_<NuclsImportReport>(m,"NuclsImportReport");
    imported.def_ro("valid",&NuclsImportReport::valid);
    imported.def_ro("written",&NuclsImportReport::written);
    imported.def_ro("cancelled",&NuclsImportReport::cancelled);
    imported.def_ro("selected_pairs",&NuclsImportReport::selected_pairs);
    imported.def_ro("images",&NuclsImportReport::images);
    imported.def_ro("output",&NuclsImportReport::output);
    imported.def_ro("json",&NuclsImportReport::json);
    imported.def_prop_ro("options",[](const NuclsImportReport& v){return v.options;});
    auto import_progress=nb::class_<NuclsImportProgress>(m,"NuclsImportProgress");
    import_progress.def_prop_ro("pairs",[](const NuclsImportProgress& v){return v.pairs.load();});
    import_progress.def_prop_ro("total",[](const NuclsImportProgress& v){return v.total.load();});
    import_progress.def_prop_ro("rows",[](const NuclsImportProgress& v){return v.rows.load();});
    import_progress.def_prop_ro("phase",[](const NuclsImportProgress& v){return v.phase.load();});
    nb::class_<NuclsImportJob>(m,"NuclsImportJob").def(nb::init<>())
        .def("run",&NuclsImportJob::run,"images"_a,"annotations"_a,"options"_a,"output"_a="")
        .def_prop_ro("busy",&NuclsImportJob::busy).def("poll",&NuclsImportJob::poll).def("cancel",&NuclsImportJob::cancel)
        .def("finish",&NuclsImportJob::finish,nb::call_guard<nb::gil_scoped_release>())
        .def("rethrow_error",&NuclsImportJob::rethrow_error)
        .def("take_pairs",&NuclsImportJob::take_pairs).def("continue_pairs",&NuclsImportJob::continue_pairs)
        .def("abort_control",&NuclsImportJob::abort_control,"error"_a)
        .def_prop_ro("result",&NuclsImportJob::result)
        .def_prop_ro("progress",&NuclsImportJob::progress,nb::rv_policy::reference_internal)
        .def_prop_ro("error",&NuclsImportJob::error);
    m.def("import_nucls",[](const std::string& images,const std::string& annotations,NuclsImportOptions options,const std::string& output){
        nb::gil_scoped_release release;return import_nucls(images,annotations,options,output);
    },"images"_a,"annotations"_a,"options"_a,"output"_a="");
}
