#include "density_reuse.hpp"
#include "density_preview.hpp"
#include "json_string.hpp"
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "strict_json.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include <fstream>
namespace qpx {
namespace {
using detail::ReaderFileGuard;
using detail::AtomicExportFile;
using detail::StrictJson;
using detail::json_string;
using detail::export_checkpoint;
using detail::export_path;
using detail::density_reuse_compatible;
constexpr std::size_t mib=1024*1024;
struct CsvLimit:std::length_error {using std::length_error::length_error;};
const std::string csv_header="project_id,image_id,image_name,record_id,reuse,request_input_sha256,checkpoint,window_id,window_revision,count,area_pixels,area_mm2,density_per_mm2,status,units,cell_count,tissue_count,exclusion_count,algorithm,geometry_policy,footprint_sha256,input_sha256,result_sha256,pixel_width_um,pixel_height_um,prefer_nucleus,polygonize_curves\n";
void checkpoint(const std::atomic<bool>* c) {export_checkpoint(c);}
void no_sidecars(const std::string& path) {
    for(const auto* suffix:{"-journal","-wal","-shm"})
        if(std::filesystem::exists(std::filesystem::u8path(path+suffix)))
            throw std::runtime_error("Close/recover the source project; SQLite sidecar present");
}
std::string fingerprint(const std::string& path,const std::atomic<bool>* c) {
    FileFingerprintOptions options;options.byte_limit=1024ull*mib;
    return fingerprint_file(path,options,c)->sha256;
}
void verify_source(const std::string& path,const ReaderFileGuard& guard) {no_sidecars(path);guard.verify();}
std::string quote_csv(const std::string& text) {
    std::string out="\"";for(auto c:text) {out+=c;if(c=='"')out+='"';}return out+'"';
}
std::string report_json(const ProjectDensityReport& report) {
    std::string out="{\"schema\":\"qpx.gui-project-density/1\",\"state\":"+json_string(report.state)+",\"error\":"+json_string(report.error)+
        ",\"source\":"+json_string(report.source)+",\"source_sha256\":"+json_string(report.source_sha256)+",\"project_id\":"+json_string(report.project_id)+
        ",\"completed\":"+std::to_string(report.completed)+",\"computed\":"+std::to_string(report.computed)+",\"reused\":"+std::to_string(report.reused)+",\"failed\":"+std::to_string(report.failed)+",\"rows\":"+std::to_string(report.rows)+",\"images\":[";
    bool first=true;for(const auto& row:report.images) {
        if(!first)out+=',';first=false;
        out+="{\"image_id\":"+std::to_string(row.id)+",\"image_name\":"+json_string(row.name)+",\"state\":"+json_string(row.state)+",\"error\":"+json_string(row.error)+
            ",\"checkpoint\":"+json_string(row.checkpoint)+",\"checkpoint_image_id\":"+std::to_string(row.checkpoint_image)+",\"record_id\":"+std::to_string(row.record)+
            ",\"input_sha256\":"+json_string(row.input_sha256)+",\"result_sha256\":"+json_string(row.result_sha256)+
            ",\"reuse\":"+json_string(row.reuse)+",\"reuse_reason\":"+json_string(row.reuse_reason)+",\"checkpoint_sha256\":"+json_string(row.checkpoint_sha256)+",\"result_input_sha256\":"+json_string(row.result_input_sha256)+"}";
    }return out+"]}\n";
}
void write_new(const std::string& path,const std::string& data,std::size_t limit) {
    AtomicExportFile file(path,limit);file.write(data.data(),data.size());file.commit(false);
}
using Json=nlohmann::json;
Json engine(const BuildEnvironment& e) {
    return {{"source_tree_sha256",e.source_tree_sha256},{"compiler",e.compiler},{"compiler_flags",e.compiler_flags},
        {"configuration",e.configuration},{"platform",e.platform},{"geos_version",e.geos_version}};
}

struct ReusedFile {
    std::string path,sha;
    std::unique_ptr<ReaderFileGuard> guard;
};
std::uint64_t positive_id(const Json& value) {
    if(!value.is_number_unsigned() && !value.is_number_integer())throw std::invalid_argument("Checkpoint requires integer IDs");
    if(value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>()<=0)throw std::invalid_argument("Checkpoint requires positive IDs");
    const auto id=value.get<std::uint64_t>();if(!id)throw std::invalid_argument("Checkpoint requires positive IDs");return id;
}
std::shared_ptr<const SpatialDensityResult> reuse_result(const std::filesystem::path& directory,const std::string& project_id,
    ProjectDensityImage& row,const SpatialDensityRequest& request,const std::atomic<bool>* c,std::vector<ReusedFile>& held) {
    const auto path=directory/("image-"+std::to_string(row.id)+".json");
    if(!std::filesystem::exists(path)){row.reuse_reason="No complete checkpoint receipt";return {};}
    ReaderFileGuard receipt_guard(path.u8string());
    if(std::filesystem::file_size(path)>mib)throw std::length_error("Checkpoint receipt exceeds 1 MiB");
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read checkpoint receipt");
    std::string data(mib+1,'\0');in.read(data.data(),data.size());data.resize(static_cast<std::size_t>(in.gcount()));
    if(in.bad() || data.size()>mib)throw std::length_error("Cannot read bounded checkpoint receipt");
    StrictJson strict("density checkpoint");Json::sax_parse(data,&strict);const auto value=Json::parse(data);receipt_guard.verify();
    for(const auto* key:{"schema","project_id","image_id","engine","checkpoint","checkpoint_image_id","checkpoint_sha256","record_id","result_sha256","result_input_sha256"})
        if(!value.is_object() || !value.contains(key))throw std::invalid_argument("Checkpoint receipt is incomplete");
    if(!value.is_object() || value.at("schema")!="qpx.gui-density-checkpoint/1" || value.at("project_id")!=project_id || positive_id(value.at("image_id"))!=row.id)
        throw std::invalid_argument("Checkpoint schema/source image identity differs");
    const auto& env=value.at("engine");
    for(const auto* name:{"source_tree_sha256","compiler","compiler_flags","configuration","platform","geos_version"})if(!env.at(name).is_string())throw std::invalid_argument("Invalid checkpoint engine");
    if(env!=engine(*build_environment())){row.reuse_reason="Engine identity changed";return {};}
    const auto checkpoint_path=std::filesystem::canonical(std::filesystem::u8path(value.at("checkpoint").get<std::string>())).u8string();no_sidecars(checkpoint_path);
    auto guard=std::make_unique<ReaderFileGuard>(checkpoint_path);const auto sha=fingerprint(checkpoint_path,c);
    if(sha!=value.at("checkpoint_sha256"))throw std::invalid_argument("Checkpoint checksum differs; previous file preserved");
    const auto image=positive_id(value.at("checkpoint_image_id")),record=positive_id(value.at("record_id"));
    auto saved=Project::open_without_journal_recovery(checkpoint_path);auto doc=saved->document(image);auto stored=doc->density_record(record)->result;
    SpatialDensityRequest::replay(*doc,record);
    if(stored->sha256()!=value.at("result_sha256") || stored->input_sha256()!=value.at("result_input_sha256"))throw std::invalid_argument("Saved result identity differs");
    verify_source(checkpoint_path,*guard);receipt_guard.verify();checkpoint(c);
    if(engine(*stored->provenance()->environment)!=env)throw std::invalid_argument("Saved result engine differs from receipt");
    if(!density_reuse_compatible(request,*stored)){row.reuse_reason="Captured inputs, policy or admission limits changed";return {};}
    row.reuse="reused";row.reuse_reason="Unchanged native capture; saved record replayed (not recomputed)";
    row.checkpoint=checkpoint_path;row.checkpoint_sha256=sha;row.checkpoint_image=image;row.record=record;
    held.push_back({checkpoint_path,sha,std::move(guard)});return stored;
}
void checkpoint_receipt(const ProjectDensityReport& report,const ProjectDensityImage& row,const SpatialDensityResult& result) {
    const Json value={{"schema","qpx.gui-density-checkpoint/1"},{"project_id",report.project_id},{"image_id",row.id},
        {"engine",engine(*result.provenance()->environment)},{"checkpoint",row.checkpoint},{"checkpoint_image_id",row.checkpoint_image},
        {"checkpoint_sha256",row.checkpoint_sha256},{"record_id",row.record},{"result_sha256",result.sha256()},{"result_input_sha256",result.input_sha256()}};
    write_new((std::filesystem::u8path(report.output)/("image-"+std::to_string(row.id)+".json")).u8string(),value.dump(2),mib);
}
}
void ProjectDensityRecipe::validate() const {
    options.validate();
    if(!selections.empty()) {
        if(all_images || !images.empty() || std::any_of(classes.begin(),classes.end(),[](const auto& v){return !v.empty();}))
            throw std::invalid_argument("Choose explicit cohorts or class selectors, not both");
        if(selections.size()>1000) throw std::invalid_argument("Select 1..1000 distinct project images");
        std::set<std::uint64_t> image_ids;
        for(const auto& selected:selections) {
            if(!selected.image_id || !image_ids.insert(selected.image_id).second) throw std::invalid_argument("Image IDs must be positive and distinct");
            for(std::size_t role=0;role<4;++role) {
                const auto& ids=selected.ids[role];const auto limit=role==3?100000u:10000u;
                if(ids.size()>limit || (role<2 && ids.empty()) || std::find(ids.begin(),ids.end(),0)!=ids.end() || std::set<std::uint64_t>(ids.begin(),ids.end()).size()!=ids.size())
                    throw std::invalid_argument("Explicit cohort IDs are empty, duplicated, zero or exceed limits");
            }
        }
        return;
    }
    if((all_images && !images.empty()) || (!all_images && images.empty()) || images.size()>1000 || std::set<std::uint64_t>(images.begin(),images.end()).size()!=images.size() || std::find(images.begin(),images.end(),0)!=images.end())
        throw std::invalid_argument("Select 1..1000 distinct project images");
    if(classes[0].empty() || classes[1].empty()) throw std::invalid_argument("Choose window and tissue classes");
    if(all_cells==!classes[3].empty()) throw std::invalid_argument("Choose Cell classes or all Cells");
    for(const auto& role:classes) {
        if(role.size()>1000) throw std::invalid_argument("Too many class selectors");
        std::set<std::string> seen;
        for(const auto& name:role) if(name.empty() || name.size()>4096 || !seen.insert(name).second)
            throw std::invalid_argument("Class names must be nonempty, exact and distinct");
    }
}
bool ProjectDensityReport::valid() const {
    return state=="preview" && !images.empty() && error.empty() &&
        std::all_of(images.begin(),images.end(),[](const auto& row){return row.error.empty();});
}
ProjectDensityReport preview_project_density(const std::string& path,const ProjectDensityRecipe& recipe,const std::atomic<bool>* c,ProjectDensityProgress* p) {
    recipe.validate();detail::WorkflowInput input(path,1024ull*mib,true,c);
    auto report=detail::preview_density_input(input,recipe,c,p);input.verify(true,c);return report;
}
ProjectDensityReport detail::preview_density_input(const WorkflowInput& input,const ProjectDensityRecipe& recipe,const std::atomic<bool>* c,ProjectDensityProgress* p) {
    recipe.validate();checkpoint(c);ProjectDensityReport report;report.recipe=recipe;
    report.source=input.path();report.source_sha256=input.sha256();input.verify(false,c);
    auto project=Project::open_without_journal_recovery(report.source);input.verify(false,c);
    report.project_id=project->identity();report.state="preview";
    const auto catalog=project->images();
    auto ids=recipe.images;for(const auto& selected:recipe.selections) ids.push_back(selected.image_id);std::sort(ids.begin(),ids.end());
    if(recipe.all_images)for(const auto& image:catalog)ids.push_back(image.id);
    std::sort(ids.begin(),ids.end());
    if(ids.empty() || ids.size()>1000)throw std::invalid_argument("Select 1..1000 distinct project images");
    if(p){p->total.store(ids.size());p->phase.store(1);}
    for(auto id:ids) {
        checkpoint(c);input.verify(false,c);ProjectDensityImage row;row.id=id;
        if(p)p->image.store(id);
        try {
            const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& image){return image.id==id;});
            if(found==catalog.end())throw std::invalid_argument("Image is no longer in the source project");row.image_present=true;row.name=found->name;
            auto doc=project->document(id);row.x=doc->pixel_width_um();row.y=doc->pixel_height_um();
            if(!recipe.selections.empty()) {
                const auto selected=std::find_if(recipe.selections.begin(),recipe.selections.end(),[&](const auto& v){return v.image_id==id;});
                row.ids=selected->ids;row.hint_record=selected->record_id;
                for(auto& role:row.ids) std::sort(role.begin(),role.end());
            } else {
                std::array<std::set<std::string>,4> matched;
                for(const auto& obj:doc->annotations()) {
                    for(std::size_t role=0;role<4;++role) {
                        const auto& names=recipe.classes[role];
                        if(std::find(names.begin(),names.end(),obj.class_name)!=names.end()) {row.ids[role].push_back(obj.id);matched[role].insert(obj.class_name);}
                    }
                    if(recipe.all_cells && obj.kind==ObjectKind::Cell)row.ids[3].push_back(obj.id);
                }
                constexpr const char* roles[]={"windows","tissue","excluded","cells"};
                for(std::size_t role=0;role<4;++role) {
                    for(const auto& name:recipe.classes[role]) if(!matched[role].count(name)) row.issues.push_back(std::string(roles[role])+": no objects for class "+name);
                    std::sort(row.ids[role].begin(),row.ids[role].end());
                    const auto cap=role==3?100000u:10000u;
                    if(row.ids[role].size()>cap)row.issues.push_back(std::string(roles[role])+": exceeds "+std::to_string(cap)+" objects");
                }
            }
            if(row.issues.empty()){SpatialDensityRequest request(*doc,row.ids[0],row.ids[1],row.ids[2],row.ids[3],recipe.options);}
        }catch(const std::exception& e){row.issues.push_back(e.what());}
        if(!row.issues.empty())row.error=row.issues.front();
        report.images.push_back(std::move(row));if(p)p->completed.fetch_add(1);
    }
    input.verify(false,c);
    return report;
}
ProjectDensityReport run_project_density(const ProjectDensityReport& preview,const std::string& output,const std::atomic<bool>* c,ProjectDensityProgress* p,std::size_t csv_limit,const std::string& resume_from) {
    if(!preview.valid())throw std::invalid_argument("Review a successful preview before Run");preview.recipe.validate();
    if(!csv_limit || csv_limit>1024*mib)throw std::invalid_argument("CSV limit must be 1 byte..1 GiB");
    if(csv_limit<csv_header.size())throw std::invalid_argument("CSV limit is smaller than the header");
    checkpoint(c);no_sidecars(preview.source);ReaderFileGuard guard(preview.source);
    if(fingerprint(preview.source,c)!=preview.source_sha256)throw std::runtime_error("Saved project changed; preview again");
    auto project=Project::open_without_journal_recovery(preview.source);verify_source(preview.source,guard);
    if(project->identity()!=preview.project_id)throw std::runtime_error("Source project identity changed; preview again");
    const auto directory=std::filesystem::u8path(export_path(output));
    std::filesystem::path previous;
    if(!resume_from.empty()){
        previous=std::filesystem::canonical(std::filesystem::u8path(resume_from));
        if(!std::filesystem::is_directory(previous))throw std::invalid_argument("Previous run must be a directory");
        const auto next=std::filesystem::weakly_canonical(directory);
        const auto inside=[](const auto& a,const auto& b){auto x=a.begin(),y=b.begin();for(;y!=b.end();++x,++y)if(x==a.end() || *x!=*y)return false;return true;};
        if(inside(next,previous) || inside(previous,next))throw std::invalid_argument("Previous and new run folders must be separate, without nesting");
        for(auto parent=next.parent_path();!parent.empty();) {
            if(std::filesystem::exists(parent) && std::filesystem::equivalent(parent,previous))throw std::invalid_argument("New run cannot be nested inside the previous run");
            const auto above=parent.parent_path();if(above==parent)break;parent=above;
        }
    }
    if(!std::filesystem::create_directory(directory))throw std::runtime_error("Choose a new output directory");
    auto report=preview;report.state="running";report.output=directory.u8string();
    for(auto& row:report.images)row.state="not_run";
    std::string combined=csv_header;
    std::vector<ReusedFile> reused_files;
    if(p){p->total.store(report.images.size());p->completed.store(0);p->phase.store(2);}
    try {
        for(auto& row:report.images) {
            checkpoint(c);verify_source(preview.source,guard);if(p){p->image.store(row.id);p->phase.store(2);}
            try {
                auto doc=project->document(row.id);
                SpatialDensityRequest request(*doc,row.ids[0],row.ids[1],row.ids[2],row.ids[3],preview.recipe.options);
                std::shared_ptr<const SpatialDensityResult> result;
                if(!previous.empty()){if(p)p->phase.store(5);result=reuse_result(previous,report.project_id,row,request,c,reused_files);}
                else row.reuse_reason="No previous run selected";
                if(!result){if(p)p->kernel_calls.fetch_add(1);result=measure_spatial_density(request,c);}checkpoint(c);
                const auto native=spatial_density_csv(*result,csv_limit,c);const auto end=native.find('\n');
                if(csv_header.substr(csv_header.find("window_id,"))!=native.substr(0,end+1))throw std::runtime_error("Native CSV schema changed");
                std::string chunk;
                const auto checkpoint_path=row.reuse=="reused"?row.checkpoint:(directory/("image-"+std::to_string(row.id)+".qpxproj")).u8string();
                const auto record=row.reuse=="reused"?row.record:doc->keep_density(request,result);
                const auto prefix=quote_csv(project->identity())+","+std::to_string(row.id)+","+quote_csv(row.name)+","+std::to_string(record)+","+row.reuse+","+request.input_sha256()+","+quote_csv(checkpoint_path)+",";
                for(std::size_t at=end+1;at<native.size();) {
                    const auto next=native.find('\n',at);chunk+=prefix+native.substr(at,next-at)+"\n";at=next==std::string::npos?native.size():next+1;
                }
                if(chunk.size()>csv_limit || combined.size()>csv_limit-chunk.size())throw CsvLimit("Combined CSV byte limit exceeded");
                if(row.reuse!="reused") {
                Project saved;std::vector<ObjectClass> classes;for(const auto& [name,value]:project->classes())classes.push_back(value);
                saved.set_class_registry(classes,project->class_defaults());saved.set_metadata(project->metadata());
                const auto image=saved.add_document(doc,row.name);saved.set_image_metadata(image,project->image_metadata(row.id));
                saved.set_image_details(image,project->image_description(row.id),project->image_tags(row.id));if(auto view=project->view(row.id))saved.set_view(image,*view);
                if(p)p->phase.store(3);
                saved.save_new(checkpoint_path,512*mib,1024ull*mib,c,[&]{verify_source(preview.source,guard);});
                row.record=record;row.checkpoint_image=image;row.checkpoint=checkpoint_path;row.checkpoint_sha256=fingerprint(checkpoint_path,c);
                }
                verify_source(preview.source,guard);checkpoint(c);
                checkpoint_receipt(report,row,*result);
                combined+=chunk;
                row.input_sha256=request.input_sha256();row.result_input_sha256=result->input_sha256();row.result_sha256=result->sha256();row.rows=result->rows().size();row.state=row.reuse=="reused"?"reused":"saved";
                report.rows+=row.rows;++report.completed;
                if(row.reuse=="reused"){++report.reused;if(p)p->reused.fetch_add(1);}else ++report.computed;
            }catch(const CsvLimit& e) {row.error=e.what();row.state="failed";++report.failed;throw;}catch(const std::exception& e) {
                if(c && c->load())throw;
                row.error=e.what();row.state="failed";++report.failed;
            }
            if(p)p->completed.fetch_add(1);
        }
        checkpoint(c);verify_source(preview.source,guard);
        if(fingerprint(preview.source,c)!=preview.source_sha256)throw std::runtime_error("Saved source changed during run");
        for(const auto& held:reused_files){verify_source(held.path,*held.guard);if(fingerprint(held.path,c)!=held.sha)throw std::runtime_error("Reused checkpoint changed during run");}
        report.state=report.failed?"partial":"completed";
    }catch(const std::exception& e){report.state=c && c->load()?"cancelled":"partial";report.error=e.what();}
    if(p)p->phase.store(4);
    // A cancelled run still publishes its completed historical rows and errors.
    report.csv=(directory/(report.state=="completed"?"density.csv":"density.partial.csv")).u8string();
    write_new(report.csv,combined,csv_limit);
    report.report=(directory/"report.json").u8string();write_new(report.report,report_json(report),16*mib);
    return report;
}
void ProjectDensityJob::reset() {cancelled_.store(false);result_.reset();error_.clear();progress_.completed.store(0);progress_.total.store(0);progress_.image.store(0);progress_.phase.store(0);progress_.kernel_calls.store(0);progress_.reused.store(0);}
bool ProjectDensityJob::preview(const std::string& path,const ProjectDensityRecipe& recipe) {
    poll();if(busy())return false;recipe.validate();reset();future_=std::async(std::launch::async,[this,path,recipe]{return preview_project_density(path,recipe,&cancelled_,&progress_);});return true;
}
bool ProjectDensityJob::run(const ProjectDensityReport& preview,const std::string& output,const std::string& resume_from) {
    poll();if(busy())return false;auto captured=preview;reset();future_=std::async(std::launch::async,[this,captured,output,resume_from]{return run_project_density(captured,output,&cancelled_,&progress_,64u*1024*1024,resume_from);});return true;
}
void ProjectDensityJob::poll() {
    if(busy() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{result_=std::make_shared<const ProjectDensityReport>(future_.get());}catch(const std::exception& e){error_=e.what();}
    }
}
void ProjectDensityJob::finish() {cancel();if(busy())future_.wait();poll();}
ProjectDensityJob::~ProjectDensityJob(){finish();}
}
