#include "qupathxx/qupath_project.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include "reader_file_guard.hpp"
#include "filesystem_access.hpp"
#include "utf8_text.hpp"
#include "sha256.hpp"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>
#include <set>
#if defined(QUPATHXX_HAS_PROJECT) && defined(QUPATHXX_HAS_INTERCHANGE)
#include "strict_json.hpp"
#endif
namespace qpx {
namespace fs=std::filesystem;
namespace {
bool same(const FileVersion& a,const FileVersion& b) {
    return a.identity_scheme==b.identity_scheme && a.identity==b.identity && a.bytes==b.bytes &&
        a.stamp_scheme==b.stamp_scheme && a.modified_token==b.modified_token && a.change_token==b.change_token;
}
void check_cancel(const std::atomic<bool>* flag) {if(flag && flag->load()) throw std::runtime_error("QuPath import cancelled");}
}
struct QuPathProjectPreview::Impl {
    std::string path;std::vector<QuPathProjectImage> images;std::vector<InterchangeIssue> issues;
    std::shared_ptr<Project> project=std::make_shared<Project>();
    std::map<std::string,FileVersion> files;std::vector<std::string> missing;
    mutable std::mutex mutex;bool consumed=false;
    void record(const std::string& path,const FileVersion& version) {
        const auto old=files.find(path);if(old!=files.end() && !same(old->second,version)) throw std::runtime_error("QuPath import input changed: "+path);
        files.insert_or_assign(path,version);
    }
    void observe(const std::string& path,const FileVersion* expected=nullptr) {
        detail::ReaderFileGuard guard(path);const auto version=guard.version();
        if(expected && !same(*version,*expected)) throw std::runtime_error("QuPath import source changed: "+path);
        record(path,*version);guard.verify();
    }
    bool exists(const std::string& path) {
        if(fs::exists(detail::access_path(fs::u8path(path)))) return true;
        missing.push_back(path);return false;
    }
    std::string read(const std::string& path,std::size_t limit,const std::atomic<bool>* cancelled) {
        detail::ReaderFileGuard guard(path);const auto version=guard.version();
        if(version->bytes>limit) throw std::length_error("QuPath import metadata exceeds byte limit");
        std::ifstream stream(detail::access_path(fs::u8path(path)),std::ios::binary);
        if(!stream) throw std::runtime_error("cannot read QuPath import input: "+path);
        std::string out;out.resize(std::size_t(version->bytes));
        for(std::size_t offset=0;offset<out.size();) {
            check_cancel(cancelled);const auto n=std::min<std::size_t>(65536,out.size()-offset);
            if(!stream.read(out.data()+offset,std::streamsize(n))) throw std::runtime_error("incomplete QuPath import input");offset+=n;
        }
        guard.verify();record(path,*version);return out;
    }
    void verify(const std::atomic<bool>* cancelled) const {
        for(const auto& [path,version]:files) {check_cancel(cancelled);detail::ReaderFileGuard guard(path);if(!same(*guard.version(),version)) throw std::runtime_error("QuPath import input changed since review: "+path);guard.verify();}
        for(const auto& path:missing) if(fs::exists(detail::access_path(fs::u8path(path)))) throw std::runtime_error("QuPath import input appeared since review: "+path);
    }
    void issue(std::string code,std::string message,std::size_t count=1) {issues.push_back({std::move(code),std::move(message),count});}
};
const std::string& QuPathProjectPreview::source_path() const {return impl_->path;}
const std::vector<QuPathProjectImage>& QuPathProjectPreview::images() const {return impl_->images;}
const std::vector<InterchangeIssue>& QuPathProjectPreview::issues() const {return impl_->issues;}
bool QuPathProjectPreview::consumed() const {std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->consumed;}
std::shared_ptr<Project> QuPathProjectPreview::import_new(const std::string& destination,bool losses,const std::atomic<bool>* cancelled) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);check_cancel(cancelled);
    if(impl_->consumed) throw std::invalid_argument("QuPath import preview already consumed");
    if(!losses && !impl_->issues.empty()) throw std::invalid_argument("QuPath import has reported losses; review before allow_losses=True");
    if(impl_->project->images().empty()) throw std::invalid_argument("QuPath import has no available supported images");
    impl_->verify(cancelled);
    impl_->project->save_new(destination,512ull*1024*1024,1024ull*1024*1024,cancelled,[&]{impl_->verify(cancelled);});
    impl_->consumed=true;return impl_->project;
}
#if defined(QUPATHXX_HAS_PROJECT) && defined(QUPATHXX_HAS_INTERCHANGE)
namespace {
using Json=nlohmann::json;
Json parse(const std::string& bytes) {detail::StrictJson strict("QuPath project");Json::sax_parse(bytes,&strict);return Json::parse(bytes);}
std::string hash(const std::string& bytes) {Sha256 sha;sha.update(bytes.data(),bytes.size());return sha.finish();}
std::string absolute(const std::string& path) {detail::bounded_utf8(path,32768);if(path.empty()) throw std::invalid_argument("empty QuPath project path");return fs::absolute(fs::u8path(path)).lexically_normal().u8string();}
std::string text(const Json& value) {auto s=value.get<std::string>();detail::bounded_utf8(s,4096);return s;}
std::string entry_id(const Json& value) {if(!value.is_number_unsigned() || !value.get<std::uint64_t>()) throw std::invalid_argument("QuPath entryID must be a positive integer");return std::to_string(value.get<std::uint64_t>());}
std::string uri_path(std::string uri,const fs::path& base) {
    if(uri.find_first_of("?#")!=std::string::npos) throw std::invalid_argument("unsupported image URI query/fragment");
    if(uri.rfind("file:",0)==0) {
        uri=uri.substr(5);
        if(uri.rfind("//localhost/",0)==0) uri=uri.substr(11);
        else if(uri.rfind("///",0)==0) uri=uri.substr(2);
    } else if(uri.find(':')!=std::string::npos) throw std::invalid_argument("only local file image URIs are supported");
    std::string decoded;
    auto digit=[](char c)->int {if(c>='0' && c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;};
    for(std::size_t i=0;i<uri.size();++i) {
        if(uri[i]=='%') {if(i+2>=uri.size() || digit(uri[i+1])<0 || digit(uri[i+2])<0) throw std::invalid_argument("invalid image URI escape");decoded+=char(digit(uri[i+1])*16+digit(uri[i+2]));i+=2;}
        else decoded+=uri[i];
    }
#ifdef _WIN32
    if(decoded.size()>3 && decoded[0]=='/' && decoded[2]==':') decoded.erase(0,1);
#endif
    detail::bounded_utf8(decoded,32768);auto path=fs::u8path(decoded);
    return absolute((path.is_absolute()?path:base/path).u8string());
}
std::optional<double> physical(const Json& value) {if(value.is_null()) return {};const auto n=value.get<double>();if(!std::isfinite(n) || n<=0) throw std::invalid_argument("invalid exchange calibration");return n;}
}
std::shared_ptr<const QuPathProjectPreview> preview_qupath_project(const std::string& path,const std::map<std::string,std::string>& overrides,const std::string& exchange,const std::atomic<bool>* cancelled) {
    try {
    auto state=std::make_shared<QuPathProjectPreview::Impl>();state->path=absolute(path);const auto base=fs::u8path(state->path).parent_path();
    const auto bytes=state->read(state->path,16*1024*1024,cancelled);const auto root=parse(bytes);
    if(root.at("version")!="0.7.0" || !root.at("images").is_array() || root.at("images").size()>1024) throw std::invalid_argument("unsupported QuPath project version or image count");
    std::set<std::string> ids;
    for(const auto& image:root.at("images")) if(!ids.insert(entry_id(image.at("entryID"))).second) throw std::invalid_argument("duplicate QuPath project entryID");
    for(const auto& [id,path]:overrides) if(!ids.count(id)) throw std::invalid_argument("source override has unknown entry ID");
    auto index_path=(exchange.empty()?base/"qupathxx-exchange":fs::u8path(absolute(exchange)))/"index.json";
    std::map<std::string,Json> entries;Json bundle_files;
    if(state->exists(index_path.u8string())) {
        const auto index=parse(state->read(index_path.u8string(),4*1024*1024,cancelled));
        if(index.at("schema")!="qpx.qupath-project-exchange/1" || index.at("qupath_version")!="0.7.0" || index.at("project_sha256")!=hash(bytes)) throw std::invalid_argument("exchange does not match this QuPath project");
        bundle_files=index.at("files");if(!bundle_files.is_object() || bundle_files.size()!=2 || !bundle_files.contains("classifiers/classes.json") || !bundle_files.contains("data/metadata.json")) throw std::invalid_argument("invalid exchange metadata bindings");
        if(!index.at("images").is_array() || index.at("images").size()>1024) throw std::invalid_argument("invalid exchange image list");
        for(const auto& item:index.at("images")) {const auto id=text(item.at("entry_id"));if(!entries.emplace(id,item).second) throw std::invalid_argument("duplicate exchange entry ID");}
    }
    auto bound_read=[&](const std::string& relative,std::size_t limit)->std::optional<Json> {
        const auto name=(base/fs::u8path(relative)).u8string();const bool present=state->exists(name);
        if(!present) {if(!bundle_files.is_null() && !bundle_files.at(relative).is_null())throw std::invalid_argument("exchange project metadata is missing");return {};}
        const auto data=state->read(name,limit,cancelled);
        if(!bundle_files.is_null() && bundle_files.at(relative)!=hash(data))throw std::invalid_argument("exchange project metadata is stale");return parse(data);
    };
    std::vector<ObjectClass> classes;
    if(const auto class_data=bound_read("classifiers/classes.json",1024*1024)) {
        const auto& list=class_data->at("pathClasses");if(!list.is_array() || list.size()>65536) throw std::invalid_argument("invalid project class list");
        std::set<std::string> names;
        for(const auto& c:list) {for(auto it=c.begin();it!=c.end();++it) if(it.key()!="name" && it.key()!="color") state->issue("class_properties_dropped","Unsupported class property: "+it.key());const auto name=text(c.at("name"));const auto color=c.at("color").get<std::int64_t>();if(name.empty() || color<0 || color>0xffffff || !names.insert(name).second) throw std::invalid_argument("invalid project class");classes.push_back({name,std::uint32_t(color)});}
    }
    if(const auto metadata_value=bound_read("data/metadata.json",1024*1024)) {const auto& metadata=*metadata_value;if(!metadata.is_object()) throw std::invalid_argument("invalid project metadata");if(!metadata.empty()) state->issue("project_metadata_dropped","Project metadata is not represented in native projects",metadata.size());}
    for(auto it=root.begin();it!=root.end();++it) if(it.key()!="version" && it.key()!="createTimestamp" && it.key()!="modifyTimestamp" && it.key()!="uri" && it.key()!="lastID" && it.key()!="images") state->issue("project_fields_dropped","Unsupported project field: "+it.key());
    for(const auto& [id,item]:entries) if(!ids.count(id)) throw std::invalid_argument("exchange has unknown entry ID");
    for(const auto& image:root.at("images")) {
        check_cancel(cancelled);QuPathProjectImage info;info.source_id=entry_id(image.at("entryID"));info.name=text(image.at("imageName"));

        const auto data_path=(base/"data"/info.source_id/"data.qpdata").u8string();const bool has_data=state->exists(data_path);if(has_data) state->observe(data_path);
        if(entries.count(info.source_id) && !has_data) throw std::invalid_argument("exchange saved image data is missing");
        const auto& builder=image.at("serverBuilder");std::uint32_t series=0;
        try {
            const auto provider=text(builder.at("providerClassName"));
            for(auto it=builder.begin();it!=builder.end();++it) if(it.key()!="builderType" && it.key()!="providerClassName" && it.key()!="uri" && it.key()!="args" && it.key()!="metadata") throw std::invalid_argument("unsupported server builder field: "+it.key());
            if(builder.at("builderType")!="uri" || (provider!="qupath.lib.images.servers.bioformats.BioFormatsServerBuilder" && provider!="qupath.lib.images.servers.imageio.ImageIoImageServerBuilder" && provider!="qupath.ext.openslide.OpenSlideServerBuilder" && provider!="qupath.lib.images.servers.openslide.OpenSlideServerBuilder")) throw std::invalid_argument("unsupported image server builder");
            const auto args=builder.value("args",Json::array());if(!args.is_array() || (!args.empty() && (args.size()!=2 || args[0]!="--series"))) throw std::invalid_argument("unsupported image server arguments");
            if(!args.empty()) {const auto value=text(args[1]);std::size_t consumed=0;const auto n=std::stoull(value,&consumed);if(consumed!=value.size() || value.empty() || value.find_first_not_of("0123456789")!=std::string::npos || n>65535) throw std::invalid_argument("invalid series");series=std::uint32_t(n);}
            if(auto replacement=overrides.find(info.source_id);replacement!=overrides.end()) info.source_path=absolute(replacement->second);
            else {
                auto uri=builder.at("uri").get<std::string>();detail::bounded_utf8(uri,98304);info.source_path=uri_path(std::move(uri),base);
                if(!fs::exists(detail::access_path(fs::u8path(info.source_path))) && root.contains("uri")) {
                    const auto previous=fs::u8path(uri_path(root.at("uri").get<std::string>(),base)).parent_path();
                    const auto relative=fs::u8path(info.source_path).lexically_relative(previous);
                    if(!relative.empty()) {const auto relocated=absolute((base/relative).u8string());if(fs::exists(detail::access_path(fs::u8path(relocated)))) info.source_path=relocated;}
                }
            }
        } catch(const std::exception& e) {state->issue("unsupported_builder",info.name+": "+e.what());state->images.push_back(info);continue;}
        if(!state->exists(info.source_path)) {state->issue("missing_image",info.name+": source image is missing");state->images.push_back(info);continue;}
        std::shared_ptr<Document> doc;
        try {
            auto ext=fs::u8path(info.source_path).extension().u8string();detail::ascii_lower_in_place(ext);
            if(ext==".tif" || ext==".tiff" || is_ome_tiff_path(info.source_path)) {TiffOpenOptions options;options.view.series=series;info.native_id=state->project->add_tiff_image(info.source_path,options,info.name);info.reader="tiff";}
            else {if(series) throw std::invalid_argument("non-TIFF series selection unsupported");info.reader=ext==".ppm"?"ppm":"openslide";info.native_id=state->project->add_image(info.source_path,info.reader,info.name);}
            doc=state->project->document(info.native_id);const auto& metadata=doc->metadata();
            if(builder.contains("metadata")) {
                const auto& expected=builder["metadata"];const auto raw=doc->server()->raw_metadata(cancelled);
                if(expected.contains("sizeZ") && expected["sizeZ"]!=(raw?raw->z_planes:1)) throw std::invalid_argument("source Z count differs from project");
                if(expected.contains("sizeT") && expected["sizeT"]!=(raw?raw->timepoints:1)) throw std::invalid_argument("source time count differs from project");
                if(expected.contains("channels") && expected["channels"].size()!=(raw?raw->channels:3)) throw std::invalid_argument("source channel count differs from project");
                if(expected.contains("pixelType")) {auto declared=text(expected["pixelType"]);detail::ascii_lower_in_place(declared);if(declared!=(raw?sample_name(raw->sample_type):"uint8")) throw std::invalid_argument("source pixel type differs from project");}
            }
            if(builder.contains("metadata") && (builder["metadata"].at("width")!=metadata.width || builder["metadata"].at("height")!=metadata.height)) throw std::invalid_argument("image dimensions differ from the project");
            if(const auto source=doc->server()->reader_source()) {if(source->primary)state->observe(source->primary->path,source->primary.get());for(const auto& file:source->companions) state->observe(file.path,&file);}
            else state->observe(info.source_path);
        } catch(const std::exception& e) {if(info.native_id)state->project->remove_image(info.native_id);info.native_id=0;state->issue("unavailable_image",info.name+": "+e.what());state->images.push_back(info);continue;}
        for(const auto& c:classes) doc->set_class(c.name,c.color);
        if(builder.contains("metadata") && builder["metadata"].contains("pixelCalibration")) {
            const auto& calibration=builder["metadata"]["pixelCalibration"];
            auto axis=[&](const char* key)->std::optional<double> {if(!calibration.contains(key)) return {};const auto& q=calibration[key];if(q.at("unit")==u8"\u00b5m") return physical(q.at("value"));if(q.at("unit")!="px")state->issue("calibration_unit",info.name+": unsupported calibration unit");return {};};
            const auto x=axis("pixelWidth"),y=axis("pixelHeight");if(x && y)doc->set_calibration(*x,*y);else if(!x && !y)doc->clear_calibration();else state->issue("partial_calibration",info.name+": partial calibration cannot replace native reader calibration");
        }
        if(has_data) {
            const auto entry=entries.find(info.source_id);
            if(entry==entries.end()) state->issue("qpdata_not_imported",info.name+": saved objects/settings require the QuPath exchange export");
            else {
                FileFingerprintOptions options;options.byte_limit=512ull*1024*1024;
                const auto fingerprint=fingerprint_file(data_path,options,cancelled);
                FileVersion version{data_path,fingerprint->identity_scheme,fingerprint->identity,fingerprint->stamp_scheme,fingerprint->modified_token,fingerprint->change_token,fingerprint->bytes,fingerprint->finished_unix_ms};state->observe(data_path,&version);
                const auto& item=entry->second;if(item.at("qpdata_sha256")!=fingerprint->sha256) throw std::invalid_argument("exchange image data is stale");
                const auto filename=text(item.at("file"));if(filename.empty() || fs::u8path(filename).filename().u8string()!=filename || filename.find_first_of("\\/:")!=std::string::npos || filename=="." || filename=="..") throw std::invalid_argument("exchange filename must be a single component");
                const auto contents=state->read((index_path.parent_path()/fs::u8path(filename)).u8string(),256ull*1024*1024,cancelled);
                if(item.at("sha256")!=hash(contents)) throw std::invalid_argument("exchange annotation checksum mismatch");
                const auto imported=import_geojson(*doc,contents,true,true);
                info.source_object_ids=imported.source_ids;info.imported_object_ids=imported.imported_ids;
                for(auto issue:imported.issues) {issue.message=info.name+": "+issue.message;state->issues.push_back(std::move(issue));}
                auto x=physical(item.at("pixel_width_um")),y=physical(item.at("pixel_height_um"));
                if(x && y)doc->set_calibration(*x,*y);else if(!x && !y)doc->clear_calibration();else state->issue("partial_calibration",info.name+": partial calibration cannot replace native reader calibration");
                for(const auto& warning:item.at("issues")) state->issue("image_settings_dropped",info.name+": "+text(warning));
            }
        }
        for(const auto key:{"metadata","tags","description"}) if(image.contains(key) && !image[key].empty()) state->issue("image_metadata_dropped",info.name+": "+key+" is not represented");
        doc->clear_history();info.objects=doc->object_count();info.available=true;state->images.push_back(std::move(info));
    }
    state->verify(cancelled);return std::shared_ptr<const QuPathProjectPreview>(new QuPathProjectPreview(std::move(state)));
    } catch(const Json::exception& e) {throw std::invalid_argument(std::string("invalid QuPath project metadata: ")+e.what());}
}
#else
std::shared_ptr<const QuPathProjectPreview> preview_qupath_project(const std::string&,const std::map<std::string,std::string>&,const std::string&,const std::atomic<bool>*) {throw std::runtime_error("QuPath project import requires project and interchange support");}
#endif
}
