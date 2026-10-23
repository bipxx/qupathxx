#include "qupathxx/project_batch.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/source_identity.hpp"
#include "batch_checkpoint.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <locale>
#include <random>
#include <sstream>
#ifndef _WIN32
#include <sys/file.h>
#endif
namespace qpx {
namespace {
namespace fs=std::filesystem;using Clock=std::chrono::steady_clock;using detail::batch_json_quote;
std::string token() {std::random_device r;constexpr char hex[]="0123456789abcdef";std::string s;for(int i=0;i<32;++i) s+=hex[r()&15];return s;}
std::string hash(const std::string& s) {Sha256 h;h.update(s.data(),s.size());return h.finish();}
std::string real(double n) {std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<n;return s.str();}
std::string vector(const ColorVector& v) {return '['+real(v[0])+','+real(v[1])+','+real(v[2])+']';}
std::string error_text(const std::exception& e) {std::string s=e.what();try {detail::bounded_utf8(s,32768);} catch(...) {return "batch operation failed (error text is not bounded UTF-8)";}return s;}
fs::path access_path(const fs::path& path) {
#ifdef _WIN32
    auto value=fs::absolute(path).lexically_normal().native();
    if(value.size()>240 && value.rfind(L"\\\\?\\",0)!=0) {
        if(value.rfind(L"\\\\",0)==0) value=L"\\\\?\\UNC\\"+value.substr(2);
        else value=L"\\\\?\\"+value;
    }
    return fs::path(value);
#else
    return path;
#endif
}
bool plain(const fs::path& path,bool directory) {
#ifdef _WIN32
    const auto flags=GetFileAttributesW(access_path(path).c_str());return flags!=INVALID_FILE_ATTRIBUTES && !(flags&FILE_ATTRIBUTE_REPARSE_POINT) && bool(flags&FILE_ATTRIBUTE_DIRECTORY)==directory;
#else
    const auto s=fs::symlink_status(path);return directory?fs::is_directory(s):fs::is_regular_file(s);
#endif
}
struct DirectoryLock {
    int fd=-1;
    explicit DirectoryLock(const fs::path& root) {const auto path=root/".qupathxx-batch.lock";
#ifdef _WIN32
        if(_wsopen_s(&fd,path.c_str(),_O_CREAT|_O_RDWR|_O_BINARY|_O_NOINHERIT,_SH_DENYRW,_S_IREAD|_S_IWRITE)!=0) fd=-1;
#else
        fd=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);if(fd>=0 && ::flock(fd,LOCK_EX|LOCK_NB)!=0) {::close(fd);fd=-1;}
#endif
        if(fd<0) throw std::runtime_error("batch output directory is locked or unavailable");
    }
    ~DirectoryLock() {
#ifdef _WIN32
        if(fd>=0) _close(fd);
#else
        if(fd>=0) ::close(fd);
#endif
    }
};
struct Work {
    fs::path path;std::string owner;
    explicit Work(const fs::path& root):owner(token()) {path=root/(".qupathxx-batch-work-"+owner);if(!fs::create_directory(path)) throw std::runtime_error("cannot create batch work directory");std::ofstream f(path/"owner",std::ios::binary);f<<"qupathxx batch work 1\n"<<owner;f.close();if(!f) {std::error_code e;fs::remove(path/"owner",e);fs::remove(path,e);throw std::runtime_error("cannot mark batch work directory");}}
    ~Work() {try {if(!plain(path,true) || !plain(path/"owner",false)) return;std::ifstream f(path/"owner",std::ios::binary);std::string found((std::istreambuf_iterator<char>(f)),{});f.close();if(found!="qupathxx batch work 1\n"+owner) return;
        std::error_code e;for(const auto* name:{"input.qpxproj","result.qpxproj"}) for(const auto* suffix:{"","-journal","-wal","-shm"}) {if(!plain(path,true)) return;fs::remove(path/(std::string(name)+suffix),e);if(e) return;}fs::remove(path/"owner",e);if(!e) fs::remove(path,e);
    } catch(...) {}}
};
void publish(const fs::path& source,const fs::path& destination,const std::atomic<bool>* c) {
    detail::export_checkpoint(c);
#ifdef _WIN32
    if(!MoveFileExW(access_path(source).c_str(),access_path(destination).c_str(),MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("cannot publish batch checkpoint without replacing an existing file");
#else
    if(::link(source.c_str(),destination.c_str())!=0) throw std::runtime_error("cannot publish batch checkpoint without replacing an existing file");::unlink(source.c_str());
#endif
}
std::string csv_quote(const std::string& s) {std::string out="\"";for(char c:s) {if(c=='"') out+='"';out+=c;}return out+'"';}
std::string with_context(const std::string& csv,const std::string& prefix,std::size_t limit) {
    std::string result;auto append=[&](const std::string& s) {if(s.size()>limit-result.size()) throw std::length_error("batch CSV exceeds byte limit");result+=s;};
    const auto first=csv.find("\r\n");if(first==std::string::npos) throw std::logic_error("invalid native CSV header");append("batch_project_identity,batch_project_generation,batch_project_revision,batch_image_id,batch_image_name,");append(csv.substr(0,first+2));
    std::size_t begin=first+2;bool quoted=false;for(std::size_t at=begin;at<csv.size();++at) {if(csv[at]=='"') {if(quoted && at+1<csv.size() && csv[at+1]=='"') ++at;else quoted=!quoted;}
        if(!quoted && csv[at]=='\n') {append(prefix);append(csv.substr(begin,at+1-begin));begin=at+1;}}
    if(quoted || begin!=csv.size()) throw std::logic_error("invalid native CSV rows");return result;
}
std::string parameters(const ProjectBatchOptions& o,const StainTransform& t) {
    const auto e=build_environment();const auto& p=o.tissue;const auto& c=o.cells;std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);
    s<<"{\"schema\":\"qpx.batch-parameters/1\",\"kind\":"<<int(o.kind)<<",\"roi_kind\":"<<int(o.roi_kind)<<",\"object_kind\":"<<int(o.object_kind)<<",\"nucleus\":"<<(o.nucleus?"true":"false")<<",\"publish_cells\":"<<(o.publish_cells?"true":"false")<<",\"roi_tile_size\":"<<o.roi_tile_size<<",\"model_sha256\":"<<batch_json_quote(stain_model_sha256(t))<<",\"white\":"<<vector(t.white())<<",\"stains\":["<<vector(t.stains()[0])<<','<<vector(t.stains()[1])<<','<<vector(t.stains()[2])<<']';
    s<<",\"tissue\":{\"feature\":"<<int(p.feature)<<",\"threshold\":"<<p.threshold<<",\"connectivity\":"<<p.connectivity<<",\"level\":"<<p.level<<",\"open_radius\":"<<p.open_radius<<",\"close_radius\":"<<p.close_radius<<",\"fill_holes\":"<<(p.fill_holes?"true":"false")<<",\"max_hole_pixels\":"<<p.max_hole_pixels<<",\"min_pixels\":"<<p.min_pixels<<",\"max_pixels\":"<<p.max_pixels<<",\"tile_size\":"<<p.tile_size<<",\"max_components\":"<<p.max_components<<",\"work_bytes\":"<<p.work_bytes<<'}';
    s<<",\"cells\":{\"feature\":"<<int(c.feature)<<",\"threshold\":"<<c.threshold<<",\"sigma\":"<<c.sigma<<",\"min_peak_radius\":"<<c.min_peak_radius<<",\"min_seed_distance\":"<<c.min_seed_distance<<",\"cell_expansion\":"<<c.cell_expansion<<",\"min_nucleus_pixels\":"<<c.min_nucleus_pixels<<",\"max_nucleus_pixels\":"<<c.max_nucleus_pixels<<",\"connectivity\":"<<c.connectivity<<",\"level\":"<<c.level<<",\"tile_size\":"<<c.tile_size<<",\"max_cells\":"<<c.max_cells<<",\"work_bytes\":"<<c.work_bytes<<",\"contours\":{\"clip_to_roi\":"<<(c.contours.clip_to_roi?"true":"false")<<",\"max_components\":"<<c.contours.max_components<<",\"max_edges\":"<<c.contours.max_edges<<",\"max_vertices\":"<<c.contours.max_vertices<<",\"work_bytes\":"<<c.contours.work_bytes<<"}}";
    s<<",\"environment\":{\"app_version\":"<<batch_json_quote(e->app_version)<<",\"source_tree_sha256\":"<<batch_json_quote(e->source_tree_sha256)<<",\"compiler\":"<<batch_json_quote(e->compiler)<<",\"compiler_flags\":"<<batch_json_quote(e->compiler_flags)<<",\"configuration\":"<<batch_json_quote(e->configuration)<<",\"platform\":"<<batch_json_quote(e->platform)<<",\"geos_version\":"<<batch_json_quote(e->geos_version)<<",\"openslide_version\":"<<batch_json_quote(e->openslide_version)<<",\"sqlite_version\":"<<batch_json_quote(e->sqlite_version)<<"}}";
    auto value=s.str();if(value.size()>512*1024) throw std::length_error("batch parameter manifest exceeds limit");return value;
}
struct Progress {std::atomic<int> phase{0};std::atomic<std::uint64_t> image{0};std::atomic<std::size_t> images{0},objects{0};};
const char* phase_name(int p) {static const char* names[]={"preparing","snapshotting","hashing","analyzing","saving","verifying","reporting"};return p>=0 && p<7?names[p]:"unknown";}
}
struct ProjectBatchRequest::Data {
    ProjectBatchOptions options;StainTransform transform;std::unique_ptr<Project> snapshot;std::vector<ProjectBatchSelection> selections;
    std::string json,sha;std::size_t bytes=0,objects=0;double capture=0;
    explicit Data(const StainTransform& t):transform(t) {}
};
void ProjectBatchOptions::validate() const {
    if(kind!=ProjectBatchKind::Roi && kind!=ProjectBatchKind::Tissue && kind!=ProjectBatchKind::Cells) throw std::invalid_argument("invalid batch kind");
    if(failure!=ProjectBatchFailure::Stop && failure!=ProjectBatchFailure::Continue) throw std::invalid_argument("invalid batch failure policy");
    if(roi_kind!=AnalysisKind::OpticalDensityMeans && roi_kind!=AnalysisKind::StainMeans) throw std::invalid_argument("invalid batch ROI kind");
    if(object_kind!=ObjectKind::Annotation && object_kind!=ObjectKind::Detection && object_kind!=ObjectKind::Cell) throw std::invalid_argument("invalid batch object kind");
    if(roi_tile_size<16 || roi_tile_size>1024 || !snapshot_budget || snapshot_budget>4ULL*1024*1024*1024 || !materialize_budget || materialize_budget>4ULL*1024*1024*1024 || !result_budget || result_budget>768ULL*1024*1024 || !csv_limit || csv_limit>256ULL*1024*1024 || checkpoint_limit<4096 || checkpoint_limit>4ULL*1024*1024*1024) throw std::invalid_argument("invalid batch resource limits");
    tissue.validate();cells.validate();if(kind==ProjectBatchKind::Cells && nucleus) throw std::invalid_argument("cell batch uses body source ROIs");if(publish_cells && (kind!=ProjectBatchKind::Cells || !has_geos())) throw std::invalid_argument("cell publication requires a cells batch and native geometry");
}
ProjectBatchRequest::ProjectBatchRequest(const Project& project,const StainTransform& transform,const std::vector<ProjectBatchSelection>& selection,ProjectBatchOptions options) {
    const auto start=Clock::now();if(!has_project()) throw std::runtime_error("project batch requires project storage");options.validate();if(selection.size()>10000 || (selection.empty() && project.entries_.size()>10000)) throw std::length_error("batch exceeds 10000 images");
    auto d=std::make_shared<Data>(transform);d->options=options;d->json=parameters(options,transform);d->sha=hash(d->json);auto chosen=selection;if(chosen.empty()) for(const auto& [id,e]:project.entries_) chosen.push_back({id,{}});
    std::sort(chosen.begin(),chosen.end(),[](const auto& a,const auto& b){return a.image_id<b.image_id;});std::set<std::uint64_t> ids;std::size_t charge=sizeof(Data)+65536+d->json.capacity()+d->sha.capacity();
    for(auto& item:chosen) {
        if(!ids.insert(item.image_id).second) throw std::invalid_argument("duplicate batch image selection");const auto& doc=*project.entry(item.image_id).document;
        if(item.object_ids.size()>100000) throw std::length_error("batch object selection exceeds limit");
        if(item.object_ids.empty()) {for(const auto& [id,a]:doc.objects()) if(a.kind==options.object_kind && a.geometry.is_area()) item.object_ids.push_back(id);}
        else {std::sort(item.object_ids.begin(),item.object_ids.end());item.object_ids.erase(std::unique(item.object_ids.begin(),item.object_ids.end()),item.object_ids.end());for(auto id:item.object_ids) if(!doc.annotation(id).geometry.is_area()) throw std::invalid_argument("batch source must have an area ROI");}
        if(item.object_ids.size()>100000-d->objects) throw std::length_error("batch exceeds 100000 source objects");d->objects+=item.object_ids.size();charge+=sizeof(ProjectBatchSelection)+item.object_ids.capacity()*sizeof(std::uint64_t);
    }
    if(charge>=options.snapshot_budget) throw std::length_error("batch capture exceeds snapshot budget");std::size_t bytes=0;d->snapshot=project.snapshot_for_save(options.snapshot_budget-charge,bytes,&ids);d->bytes=bytes+charge;d->selections=std::move(chosen);d->capture=std::chrono::duration<double,std::milli>(Clock::now()-start).count();data_=std::move(d);
}
std::size_t ProjectBatchRequest::image_count() const {return data_->selections.size();}
std::size_t ProjectBatchRequest::object_count() const {return data_->objects;}
std::size_t ProjectBatchRequest::snapshot_bytes() const {return data_->bytes;}
double ProjectBatchRequest::capture_ms() const {return data_->capture;}
std::string ProjectBatchRequest::parameters_json() const {return data_->json;}
std::string ProjectBatchRequest::parameters_sha256() const {return data_->sha;}
struct ProjectBatchKernel {
    static std::uint64_t cost(const Document& d,bool embedded) {
        auto value=65536+2ULL*(d.object_bytes_+d.analysis_bytes_+d.tissue_bytes_+d.cells_bytes_+d.classifiers_bytes_+d.pixels_bytes_+d.streams_bytes_)+(embedded?2ULL*d.image().pixels().size():65536);
        value+=4ULL*(d.metadata().levels.size()*sizeof(ImageLevel)+d.metadata().vendor.size()+d.server()->source_locator().size());
        for(const auto& item:d.classes_) value+=4ULL*(sizeof(ObjectClass)+128+item.first.size()+item.second.name.size());return value;
    }
    static ProjectBatchReport run(const ProjectBatchRequest& request,const std::string& directory,const std::atomic<bool>* c,Progress* progress,const std::function<void(const ProjectBatchImageResult&)>& sink) {
        const auto started=Clock::now();const auto& d=*request.data_;const auto& o=d.options;ProjectBatchReport report;report.parameters_sha256=d.sha;report.source_identity=d.snapshot->identity_;report.source_generation=d.snapshot->generation_;report.source_revision=d.snapshot->revision_;report.snapshot_bytes=d.bytes;report.capture_ms=d.capture;std::size_t report_bytes=65536+d.selections.size()*sizeof(ProjectBatchImageResult);report.images.reserve(d.selections.size());
        auto phase=[&](int p){if(progress) progress->phase.store(p);};auto check=[&]{detail::export_checkpoint(c);};fs::path root;
        try {
            check();root=fs::u8path(detail::export_path(directory));fs::create_directories(root);if(!plain(root,true)) throw std::runtime_error("batch output must be a plain directory");DirectoryLock lock(root);
            for(const auto& item:d.selections) {
                check();const auto image_start=Clock::now();const auto& input=d.snapshot->entry(item.image_id);ProjectBatchImageResult result;result.image_id=item.image_id;result.image_name=input.info.name;result.objects=item.object_ids.size();if(progress) progress->image.store(item.image_id);
                try {
                    phase(1);if(cost(*input.document,input.info.embedded)>o.materialize_budget) throw std::length_error("batch image exceeds materialization budget");Work work(root);
                    Project isolated;isolated.identity_=d.snapshot->identity_;isolated.next_id_=d.snapshot->next_id_;isolated.active_image_=item.image_id;isolated.entries_.emplace(item.image_id,input);const auto view=d.snapshot->views_.find(item.image_id);if(view!=d.snapshot->views_.end()) isolated.views_.emplace(*view);
                    isolated.save_impl((work.path/"input.qpxproj").u8string(),"",c);result.input_sha256=fingerprint_file((work.path/"input.qpxproj").u8string(),{},c)->sha256;
                    const auto spec=hash(d.sha+result.input_sha256+std::to_string(report.source_generation)+":"+std::to_string(report.source_revision));const auto folder=root/("image-"+std::to_string(item.image_id)+"-"+spec);fs::create_directories(folder);if(!plain(folder,true)) throw std::runtime_error("batch image output must be a plain directory");
                    Project::LoadProgress load;load.external_cancelled=c;auto project=Project::open_impl((work.path/"input.qpxproj").u8string(),false,"",&load);auto doc=project->document(item.image_id);doc->set_history_limits(0,0);check();
                    auto pixel_stream=[&](bool compute,std::vector<std::uint64_t>& records,std::uint64_t& generated) {
                        Sha256 pixels;const std::string tag="qpx.batch-pixels/1";pixels.update(tag.data(),tag.size());
                        for(auto id:item.object_ids) {check();std::string digest;std::uint64_t record=0,bytes=0;
                            if(o.kind==ProjectBatchKind::Roi) {AnalysisRequest r(*doc,id,d.transform,o.roi_kind,o.nucleus,o.roi_tile_size);if(compute) {auto value=analyze(r,c);digest=value.input_sha256;record=doc->keep_analysis(r,value);bytes=doc->analysis_record(record).payload_bytes();}else digest=input_sha256(r,c);}
                            else if(o.kind==ProjectBatchKind::Tissue) {TissueRequest r(*doc,id,d.transform,o.tissue,o.nucleus);if(compute) {auto value=detect_tissue(r,c);digest=value->input_sha256();record=doc->keep_tissue(r,value);bytes=doc->tissue_record(record)->payload_bytes();}else digest=input_sha256(r,c);}
                            else {CellRequest r(*doc,id,d.transform,o.cells);if(compute) {auto value=detect_cells(r,c);digest=value->nuclei()->input_sha256();record=doc->keep_cells(r,value);bytes=doc->cell_record(record)->payload_bytes();if(o.publish_cells) {const auto extra=4ULL*value->retained_bytes()+value->features().size()*512ULL;if(cost(*doc,input.info.embedded)>o.materialize_budget || extra>o.materialize_budget-cost(*doc,input.info.embedded)) throw std::length_error("cell publication exceeds batch materialization budget");doc->publish_cell_record(record);}}else digest=input_sha256(r,c);}
                            pixels.integer(id);pixels.update(digest.data(),digest.size());
                            if(compute) {if(bytes>o.result_budget-generated) throw std::length_error("batch image exceeds result budget");generated+=bytes;records.push_back(record);if(cost(*doc,input.info.embedded)>o.materialize_budget) throw std::length_error("batch results exceed materialization budget");if(progress) progress->objects.fetch_add(1);}
                        }check();return pixels.finish();
                    };
                    auto find=[&](const std::string& digest)->fs::path {
                        const auto preferred=folder/(digest+".qpxproj");if(fs::exists(preferred)) return preferred;
                        std::vector<fs::path> names;std::size_t count=0;for(const auto& e:fs::directory_iterator(folder)) {check();if(++count>100000) throw std::length_error("batch checkpoint directory exceeds entry limit");const auto n=e.path().filename().u8string();if(n.rfind(digest+"-",0)==0 && n.size()==105 && n.substr(n.size()-8)==".qpxproj" && plain(e.path(),false)) names.push_back(e.path());}
                        if(names.empty()) return {};std::sort(names.begin(),names.end());return names.front();
                    };
                    bool candidates=false;if(o.resume) {std::size_t count=0;for(const auto& e:fs::directory_iterator(folder)) {check();if(++count>100000) throw std::length_error("batch checkpoint directory exceeds entry limit");const auto n=e.path().filename().u8string();if((n.size()==72 || n.size()==105) && n.substr(n.size()-8)==".qpxproj") {candidates=true;break;}}}
                    if(candidates) {
                        phase(2);std::vector<std::uint64_t> unused;std::uint64_t bytes=0;const auto digest=pixel_stream(false,unused,bytes);const auto candidate=find(digest);
                        if(!candidate.empty()) {
                            phase(5);if(!plain(candidate,false) || fs::file_size(candidate)>o.checkpoint_limit) throw std::length_error("batch checkpoint is not plain or exceeds byte limit");detail::ReaderFileGuard guard(access_path(candidate).u8string());const auto meta=detail::read_batch_checkpoint(access_path(candidate).u8string(),c,false,o.csv_limit);guard.verify();
                            if(meta.parameters_sha256!=d.sha || meta.input_sha256!=result.input_sha256 || meta.pixel_sha256!=digest || meta.image_id!=std::to_string(item.image_id) || meta.source_identity!=report.source_identity || meta.source_generation!=report.source_generation || meta.source_revision!=report.source_revision || meta.objects!=item.object_ids.size() || meta.generated_bytes>o.result_budget) throw std::invalid_argument("batch checkpoint does not match this request");
                            result.pixel_sha256=digest;result.path=access_path(candidate).u8string();result.records=item.object_ids.size();result.state="reused";++report.reused;if(progress) progress->objects.fetch_add(item.object_ids.size());
                        }
                    }
                    if(result.state.empty()) {
                        phase(3);std::vector<std::uint64_t> records;std::uint64_t generated=0;result.pixel_sha256=pixel_stream(true,records,generated);result.records=records.size();detail::BatchCheckpoint meta;meta.parameters_sha256=d.sha;meta.input_sha256=result.input_sha256;meta.pixel_sha256=result.pixel_sha256;meta.source_identity=report.source_identity;meta.source_generation=report.source_generation;meta.source_revision=report.source_revision;meta.image_id=std::to_string(item.image_id);meta.image_name=input.info.name;meta.parameters_json=d.json;meta.objects=item.object_ids.size();meta.generated_bytes=generated;meta.max_file_bytes=o.checkpoint_limit;meta.source_locator=input.info.source_path;
                        if(const auto source=doc->server()->reader_source()) if(source->primary) meta.source_file_identity=source->primary->identity;
                        std::string table;Document empty(Image(1,1,{0,0,0}));const auto& table_doc=records.empty()?empty:*doc;
                        if(o.kind==ProjectBatchKind::Cells) {CellTableOptions options;options.require_current=true;options.byte_limit=o.csv_limit;options.snapshot_budget=o.materialize_budget>512*1024*1024?512*1024*1024:o.materialize_budget;table=cell_measurements_csv(CellTableRequest(table_doc,records,options),c);}
                        else {MeasurementTableOptions options;options.require_current=true;options.byte_limit=o.csv_limit;options.snapshot_budget=o.materialize_budget>512*1024*1024?512*1024*1024:o.materialize_budget;table=measurements_csv(MeasurementTableRequest(table_doc,o.kind==ProjectBatchKind::Roi?MeasurementTableKind::Roi:MeasurementTableKind::Tissue,records,options),c);}
                        const auto prefix=csv_quote(report.source_identity)+','+std::to_string(report.source_generation)+','+std::to_string(report.source_revision)+','+std::to_string(item.image_id)+','+csv_quote(input.info.name)+',';meta.csv=with_context(table,prefix,o.csv_limit);
                        phase(4);const auto file=work.path/"result.qpxproj";project->save_impl(file.u8string(),folder.u8string(),c,nullptr,&meta);check();auto destination=folder/(result.pixel_sha256+".qpxproj");if(fs::exists(destination)) destination=folder/(result.pixel_sha256+"-"+token()+".qpxproj");publish(file,destination,c);result.path=access_path(destination).u8string();result.state="saved";++report.saved;
                    }
                } catch(const std::exception& e) {if(c && c->load()) throw;result.state="failed";result.error=error_text(e);result.records=0;++report.failed;}
                result.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-image_start).count();const auto bytes=result.image_name.capacity()+result.path.capacity()+result.error.capacity()+result.input_sha256.capacity()+result.pixel_sha256.capacity()+result.state.capacity();if(bytes>64*1024*1024-report_bytes) throw std::length_error("batch report exceeds memory limit");report_bytes+=bytes;report.images.push_back(result);if(progress) progress->images.store(report.images.size());if(sink) sink(result);
                if(result.state=="failed" && o.failure==ProjectBatchFailure::Stop) {report.state="failed";report.error=result.error;break;}
            }
            if(report.state=="idle") report.state=report.failed?"partial":"succeeded";
        } catch(const std::exception& e) {report.state=c && c->load()?"cancelled":"failed";report.error=error_text(e);}
        report.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        // The terminal audit is intentionally written after cancellation. It
        // publishes no additional image result and records committed images.
        if(!root.empty() && plain(root,true)) {try {phase(6);report.report_path=(root/("run-"+token()+".json")).u8string();const auto text=project_batch_report_json(report);detail::AtomicExportFile file(report.report_path,64*1024*1024);file.write(text.data(),text.size());file.commit();} catch(const std::exception& e) {report.state="failed";report.error="cannot write batch report: "+error_text(e);report.report_path.clear();}}
        return report;
    }
};
ProjectBatchReport run_project_batch(const ProjectBatchRequest& request,const std::string& output,const std::atomic<bool>* c,const std::function<void(const ProjectBatchImageResult&)>& sink) {return ProjectBatchKernel::run(request,output,c,nullptr,sink);}
std::string project_batch_image_json(const ProjectBatchImageResult& r) {return "{\"schema\":\"qpx.batch-image/1\",\"image_id\":"+std::to_string(r.image_id)+",\"image_name\":"+batch_json_quote(r.image_name)+",\"state\":"+batch_json_quote(r.state)+",\"path\":"+batch_json_quote(r.path)+",\"error\":"+batch_json_quote(r.error)+",\"input_sha256\":"+batch_json_quote(r.input_sha256)+",\"pixel_sha256\":"+batch_json_quote(r.pixel_sha256)+",\"objects\":"+std::to_string(r.objects)+",\"records\":"+std::to_string(r.records)+",\"elapsed_ms\":"+real(r.elapsed_ms)+"}";}
std::string project_batch_report_json(const ProjectBatchReport& r) {
    std::string s="{\"schema\":\"qpx.project-batch/1\",\"state\":"+batch_json_quote(r.state)+",\"error\":"+batch_json_quote(r.error)+",\"report_path\":"+batch_json_quote(r.report_path)+",\"parameters_sha256\":"+batch_json_quote(r.parameters_sha256)+",\"source_identity\":"+batch_json_quote(r.source_identity)+",\"source_generation\":"+std::to_string(r.source_generation)+",\"source_revision\":"+std::to_string(r.source_revision)+",\"saved\":"+std::to_string(r.saved)+",\"reused\":"+std::to_string(r.reused)+",\"failed\":"+std::to_string(r.failed)+",\"snapshot_bytes\":"+std::to_string(r.snapshot_bytes)+",\"capture_ms\":"+real(r.capture_ms)+",\"elapsed_ms\":"+real(r.elapsed_ms)+",\"images\":[";
    bool first=true;for(const auto& image:r.images) {if(!first) s+=',';first=false;s+=project_batch_image_json(image);if(s.size()>64*1024*1024) throw std::length_error("batch report exceeds byte limit");}return s+"]}\n";
}
struct ProjectBatchJob::Impl {
    std::optional<ProjectBatchRequest> request;std::future<ProjectBatchReport> future;std::atomic<bool> cancelled{false};Progress progress;ProjectBatchStats stats;std::shared_ptr<const ProjectBatchReport> result;
};
ProjectBatchJob::ProjectBatchJob():impl_(std::make_unique<Impl>()) {}
ProjectBatchJob::~ProjectBatchJob() {cancel();try {wait();} catch(...) {}}
bool ProjectBatchJob::busy() const {return impl_->future.valid();}
void ProjectBatchJob::cancel() {impl_->cancelled.store(true);}
std::shared_ptr<const ProjectBatchReport> ProjectBatchJob::result() const {return impl_->result;}
bool ProjectBatchJob::request(const ProjectBatchRequest& request,const std::string& directory) {
    poll();if(busy()) return false;const auto output=detail::export_path(directory);detail::bounded_utf8(output,32768);impl_->request=request;impl_->cancelled.store(false);impl_->result.reset();impl_->progress.phase.store(0);impl_->progress.image.store(0);impl_->progress.images.store(0);impl_->progress.objects.store(0);impl_->stats={};impl_->stats.state="running";impl_->stats.total_images=request.image_count();impl_->stats.total_objects=request.object_count();impl_->stats.capture_ms=request.capture_ms();
    try {impl_->future=std::async(std::launch::async,[state=impl_.get(),output] {return ProjectBatchKernel::run(*state->request,output,&state->cancelled,&state->progress,{});});} catch(...) {impl_->request.reset();impl_->stats.state="failed";throw;}return true;
}
ProjectBatchStats ProjectBatchJob::poll() {
    impl_->stats.phase=phase_name(impl_->progress.phase.load());impl_->stats.image_id=impl_->progress.image.load();impl_->stats.completed_images=impl_->progress.images.load();impl_->stats.completed_objects=impl_->progress.objects.load();if(busy() && impl_->cancelled.load()) impl_->stats.state="cancelling";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto report=impl_->future.get();impl_->stats.state=report.state;impl_->stats.error=report.error;impl_->stats.elapsed_ms=report.elapsed_ms;impl_->result=std::make_shared<const ProjectBatchReport>(std::move(report));} catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=error_text(e);}impl_->request.reset();impl_->stats.completed_images=impl_->progress.images.load();impl_->stats.completed_objects=impl_->progress.objects.load();}
    return impl_->stats;
}
ProjectBatchStats ProjectBatchJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
