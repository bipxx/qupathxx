#include "qupathxx/raw_image.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
namespace nb=nanobind;using namespace nb::literals;
namespace {
template<class T> nb::object array(const std::vector<T>& values,std::initializer_list<std::size_t> shape,nb::handle owner,bool copy) {
    if(!copy) return nb::ndarray<nb::numpy,const T>(values.data(),shape,owner).cast();
    auto storage=std::make_unique<std::vector<T>>(values);nb::capsule retained(storage.get(),[](void* p) noexcept {delete static_cast<std::vector<T>*>(p);});auto* data=storage->data();storage.release();
    return nb::ndarray<nb::numpy,T>(data,shape,retained).cast();
}
}
void bind_raw_images(nb::module_& m) {
    using namespace qpx;
    nb::enum_<SampleType>(m,"SampleType")
        .value("UINT8",SampleType::UInt8).value("UINT16",SampleType::UInt16).value("INT8",SampleType::Int8).value("INT16",SampleType::Int16)
        .value("UINT32",SampleType::UInt32).value("INT32",SampleType::Int32).value("UINT64",SampleType::UInt64).value("INT64",SampleType::Int64).value("FLOAT32",SampleType::Float32).value("FLOAT64",SampleType::Float64);
    nb::class_<RawImageMetadata>(m,"RawImageMetadata")
        .def_ro("width",&RawImageMetadata::width).def_ro("height",&RawImageMetadata::height).def_ro("channels",&RawImageMetadata::channels)
        .def_ro("z_planes",&RawImageMetadata::z_planes).def_ro("timepoints",&RawImageMetadata::timepoints).def_ro("series",&RawImageMetadata::series).def_ro("series_count",&RawImageMetadata::series_count)
        .def_ro("sample_type",&RawImageMetadata::sample_type).def_ro("dimension_order",&RawImageMetadata::dimension_order)
        .def_prop_ro("payload_bytes",&RawImageMetadata::payload_bytes)
        .def_prop_ro("levels",[](const RawImageMetadata& v){return v.levels;}).def_prop_ro("channel_names",[](const RawImageMetadata& v){return v.channel_names;})
        .def_ro("pixel_width_um",&RawImageMetadata::pixel_width_um).def_ro("pixel_height_um",&RawImageMetadata::pixel_height_um).def_ro("z_spacing_um",&RawImageMetadata::z_spacing_um).def_ro("time_interval_seconds",&RawImageMetadata::time_interval_seconds);
    nb::class_<RawRegionRequest>(m,"RawRegionRequest").def(nb::init<>())
        .def_rw("region",&RawRegionRequest::region).def_rw("first_channel",&RawRegionRequest::first_channel).def_rw("channels",&RawRegionRequest::channels).def_rw("z",&RawRegionRequest::z).def_rw("t",&RawRegionRequest::t)
        .def("validate",&RawRegionRequest::validate,"metadata"_a);
    nb::class_<RawPixelBuffer>(m,"RawPixelBuffer")
        .def_prop_ro("width",&RawPixelBuffer::width).def_prop_ro("height",&RawPixelBuffer::height).def_prop_ro("channels",&RawPixelBuffer::channels).def_prop_ro("sample_type",&RawPixelBuffer::sample_type).def_prop_ro("payload_bytes",&RawPixelBuffer::payload_bytes)
        .def("numpy",[](nb::object self,bool copy) {const auto& v=nb::cast<const RawPixelBuffer&>(self);return std::visit([&](const auto& values){return array(values,{std::size_t(v.height()),std::size_t(v.width()),v.channels()},self,copy);},v.storage());},"copy"_a=false)
        .def("valid_numpy",[](nb::object self,bool copy) {const auto& v=nb::cast<const RawPixelBuffer&>(self);return array(v.valid(),{std::size_t(v.height()),std::size_t(v.width())},self,copy);},"copy"_a=false)
        .def_prop_ro("data",[](const RawPixelBuffer& v){return std::visit([](const auto& values){return nb::bytes(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(typename std::decay_t<decltype(values)>::value_type));},v.storage());});
    nb::class_<TiffView>(m,"TiffView").def(nb::init<>())
        .def_rw("series",&TiffView::series).def_rw("z",&TiffView::z).def_rw("t",&TiffView::t).def_rw("channels",&TiffView::channels).def_rw("lower",&TiffView::lower).def_rw("upper",&TiffView::upper)
        .def("validate",[](const TiffView& v){v.validate();});
    nb::class_<TiffOpenOptions>(m,"TiffOpenOptions").def(nb::init<>()).def_rw("view",&TiffOpenOptions::view).def_rw("decode_bytes",&TiffOpenOptions::decode_bytes).def_rw("cache_bytes",&TiffOpenOptions::cache_bytes).def("validate",&TiffOpenOptions::validate);
    nb::class_<TiffReadStats>(m,"TiffReadStats").def_ro("decoded_blocks",&TiffReadStats::decoded_blocks).def_ro("cache_hits",&TiffReadStats::cache_hits).def_ro("decoded_bytes",&TiffReadStats::decoded_bytes).def_ro("directory_switches",&TiffReadStats::directory_switches).def_ro("cache_bytes",&TiffReadStats::cache_bytes).def_ro("peak_cache_bytes",&TiffReadStats::peak_cache_bytes);
    nb::borrow<nb::class_<ImageServer>>(m.attr("ImageServer"))
        .def_prop_ro("raw_metadata",[](const ImageServer& s)->std::optional<std::shared_ptr<const RawImageMetadata>> {auto value=s.raw_metadata();if(value) return value;return {};},nb::call_guard<nb::gil_scoped_release>())
        .def_prop_ro("tiff_view",[](const ImageServer& s)->std::optional<TiffView> {auto value=s.tiff_view();if(value) return *value;return {};})
        .def_prop_ro("tiff_options",[](const ImageServer& s)->std::optional<TiffOpenOptions> {auto value=s.tiff_options();if(value) return *value;return {};})
        .def_prop_ro("tiff_read_stats",[](const ImageServer& s)->std::optional<std::shared_ptr<const TiffReadStats>> {auto value=s.tiff_read_stats();if(value) return value;return {};},nb::call_guard<nb::gil_scoped_release>())
        .def("with_tiff_view",[](const ImageServer& s,TiffView view){return s.with_tiff_view(view);},"view"_a,nb::call_guard<nb::gil_scoped_release>())
        .def("read_raw",[](const ImageServer& s,RawRegionRequest r){return s.read_raw(r);},"request"_a,nb::call_guard<nb::gil_scoped_release>());
    nb::class_<RawRegionStatistics>(m,"RawRegionStatistics").def_ro("pixels",&RawRegionStatistics::pixels).def_ro("valid_pixels",&RawRegionStatistics::valid_pixels)
        .def_ro("finite_samples",&RawRegionStatistics::finite_samples).def_ro("nonfinite_samples",&RawRegionStatistics::nonfinite_samples).def_ro("means",&RawRegionStatistics::means).def_ro("minima",&RawRegionStatistics::minima).def_ro("maxima",&RawRegionStatistics::maxima);
    m.def("open_tiff",[](const std::string& path,TiffOpenOptions options){return open_tiff(path,std::move(options));},"path"_a,"options"_a=TiffOpenOptions{},nb::call_guard<nb::gil_scoped_release>());
    m.def("raw_region_statistics",[](const ImageServer& server,RawRegionRequest request,std::size_t tile,std::optional<Roi> mask,std::uint64_t maximum){return raw_region_statistics(server,request,tile,mask?&*mask:nullptr,maximum);},"server"_a,"request"_a,"tile_size"_a=256,"mask"_a=nb::none(),"max_pixels"_a=1000000000,nb::call_guard<nb::gil_scoped_release>());
    m.def("save_raw_numpy",[](const RawPixelBuffer& buffer,const std::string& path,bool replace){save_raw_numpy(buffer,path,replace);},"buffer"_a,"path"_a,"replace_existing"_a=false,nb::call_guard<nb::gil_scoped_release>());
    nb::borrow<nb::class_<Document>>(m.attr("Document")).def("set_tiff_view",[](Document& doc,TiffView view){doc.set_tiff_view(view);},"view"_a);
    nb::borrow<nb::class_<Project>>(m.attr("Project"))
        .def("add_tiff_image",[](Project& p,const std::string& path,TiffOpenOptions options,const std::string& name){return p.add_tiff_image(path,options,name);},"path"_a,"options"_a=TiffOpenOptions{},"name"_a="")
        .def("set_tiff_view",[](Project& p,std::uint64_t id,TiffView view){p.set_tiff_view(id,view);},"image_id"_a,"view"_a);
    nb::borrow<nb::class_<GuiOptions>>(m.attr("GuiOptions"))
        .def_rw("raw_open",&GuiOptions::raw_open).def_rw("raw_tab",&GuiOptions::raw_tab).def_rw("image_reader",&GuiOptions::image_reader).def_rw("raw_numpy_path",&GuiOptions::raw_numpy_path)
        .def_prop_rw("tiff_options",[](const GuiOptions& o)->std::optional<TiffOpenOptions>{if(o.tiff_options) return *o.tiff_options;return {};},[](GuiOptions& o,std::optional<TiffOpenOptions> v){o.tiff_options=v?std::make_shared<const TiffOpenOptions>(*v):nullptr;})
        .def_prop_rw("raw_request",[](const GuiOptions& o)->std::optional<RawRegionRequest>{if(o.raw_request) return *o.raw_request;return {};},[](GuiOptions& o,std::optional<RawRegionRequest> v){o.raw_request=v?std::make_shared<const RawRegionRequest>(*v):nullptr;});
    nb::borrow<nb::class_<GuiReport>>(m.attr("GuiReport"))
        .def_ro("tiff_state",&GuiReport::tiff_state).def_ro("raw_task_state",&GuiReport::raw_task_state).def_ro("raw_error",&GuiReport::raw_error)
        .def_prop_ro("raw_statistics",[](const GuiReport& r)->std::optional<std::shared_ptr<const RawRegionStatistics>>{if(r.raw_statistics) return r.raw_statistics;return {};})
        .def_ro("tiff_view_changes",&GuiReport::tiff_view_changes).def_ro("raw_export_bytes",&GuiReport::raw_export_bytes).def_ro("raw_catalog_max_step",&GuiReport::raw_catalog_max_step);
    m.attr("has_tiff")=has_tiff();m.def("tiff_library_version",&tiff_library_version);
}
