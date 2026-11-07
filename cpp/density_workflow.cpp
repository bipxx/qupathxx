#include "density_reuse.hpp"
#include "density_preview.hpp"
#include "density_formats.hpp"
#include "json_string.hpp"
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "atomic_export.hpp"
#include "strict_json.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <fstream>
namespace qpx {
namespace {
using detail::AtomicExportFile;
using detail::StrictJson;
using detail::json_string;
using detail::export_path;
using detail::density_reuse_compatible;
constexpr std::size_t mib=1024*1024;
using CsvLimit=ProjectDensityCsvBudget;
const std::string csv_header="project_id,image_id,image_name,record_id,reuse,request_input_sha256,checkpoint,window_id,window_revision,count,area_pixels,area_mm2,density_per_mm2,status,units,cell_count,tissue_count,exclusion_count,algorithm,geometry_policy,footprint_sha256,input_sha256,result_sha256,pixel_width_um,pixel_height_um,prefer_nucleus,polygonize_curves\n";
void checkpoint(const std::atomic<bool>* c) {workflow_checkpoint(c);}
std::string quote_csv(const std::string& text) {
    std::string out="\"";for(auto c:text) {out+=c;if(c=='"')out+='"';}return out+'"';
}
void write_new(const std::string& path,const std::string& data,std::size_t limit,const std::function<void()>& guard={}) {
    AtomicExportFile file(path,limit);file.write(data.data(),data.size());if(guard)guard();
    try{file.commit(false);}catch(...){std::error_code error;if(std::filesystem::exists(std::filesystem::u8path(path),error))throw WorkflowOutputExists("Output already exists");throw;}
}
using Json=nlohmann::json;
using detail::DensityFormat;
using detail::density_engine;

struct ReusedFile {
    std::unique_ptr<detail::WorkflowInput> input;
};
std::uint64_t positive_id(const Json& value) {
    if(!value.is_number_unsigned() && !value.is_number_integer())throw std::invalid_argument("Checkpoint requires integer IDs");
    if(value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>()<=0)throw std::invalid_argument("Checkpoint requires positive IDs");
    const auto id=value.get<std::uint64_t>();if(!id)throw std::invalid_argument("Checkpoint requires positive IDs");return id;
}
std::shared_ptr<const SpatialDensityResult> reuse_result(const std::filesystem::path& directory,const std::string& project_id,
    ProjectDensityImage& row,const SpatialDensityRequest& request,DensityFormat format,std::uint64_t byte_limit,
    const std::atomic<bool>* c,std::vector<ReusedFile>& held) {
    const auto path=directory/("image-"+std::to_string(row.id)+".json");
    if(!std::filesystem::exists(path)){row.reuse_reason="No complete checkpoint receipt";return {};}
    auto receipt_guard=std::make_unique<detail::WorkflowInput>(path.u8string(),mib,false,c);
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read checkpoint receipt");
    std::string data(mib+1,'\0');in.read(data.data(),data.size());data.resize(static_cast<std::size_t>(in.gcount()));
    if(in.bad() || data.size()>mib)throw std::length_error("Cannot read bounded checkpoint receipt");
    StrictJson strict("density checkpoint");if(!Json::sax_parse(data,&strict))throw std::invalid_argument("Invalid checkpoint JSON");
    const auto value=Json::parse(data);receipt_guard->verify(false,c);
    for(const auto* key:{"schema","project_id","image_id","engine","checkpoint","checkpoint_image_id","checkpoint_sha256","record_id","result_sha256","result_input_sha256"})
        if(!value.is_object() || !value.contains(key))throw std::invalid_argument("Checkpoint receipt is incomplete");
    if(value.at("schema")!=detail::density_checkpoint_schema(format) || value.at("project_id")!=project_id || positive_id(value.at("image_id"))!=row.id)
        throw std::invalid_argument("Checkpoint schema/source image identity differs");
    const auto& env=value.at("engine");
    for(const auto* name:{"source_tree_sha256","compiler","compiler_flags","configuration","platform","geos_version"})if(!env.at(name).is_string())throw std::invalid_argument("Invalid checkpoint engine");
    if(env!=density_engine(*build_environment())){row.reuse_reason="Engine identity changed";return {};}
    auto guard=std::make_unique<detail::WorkflowInput>(value.at("checkpoint").get<std::string>(),byte_limit,true,c);
    if(guard->sha256()!=value.at("checkpoint_sha256"))throw std::invalid_argument("Checkpoint checksum differs; previous file preserved");
    const auto image=positive_id(value.at("checkpoint_image_id")),record=positive_id(value.at("record_id"));
    auto saved=Project::open_without_journal_recovery(guard->path());auto doc=saved->document(image);auto stored=doc->density_record(record)->result;
    SpatialDensityRequest::replay(*doc,record);
    if(stored->sha256()!=value.at("result_sha256") || stored->input_sha256()!=value.at("result_input_sha256"))throw std::invalid_argument("Saved result identity differs");
    guard->verify(false,c);receipt_guard->verify(false,c);checkpoint(c);
    if(density_engine(*stored->provenance()->environment)!=env) {
        if(format==DensityFormat::GuiV1)throw std::invalid_argument("Saved result engine differs from receipt");
        row.reuse_reason="Saved result engine changed";return {};
    }
    if(!density_reuse_compatible(request,*stored)){row.reuse_reason="Captured inputs, policy or admission limits changed";return {};}
    row.reuse=format==DensityFormat::GuiV1?"reused":"checkpoint";
    row.reuse_reason="Unchanged native capture; saved record replayed (not recomputed)";
    row.checkpoint=guard->path();row.checkpoint_sha256=guard->sha256();row.checkpoint_image=image;row.record=record;
    if(format==DensityFormat::ProjectV1)row.receipt_json=data;
    held.push_back({std::move(guard)});held.push_back({std::move(receipt_guard)});return stored;
}
std::shared_ptr<const SpatialDensityResult> source_record(Document& doc,ProjectDensityImage& row,const SpatialDensityRequest& request) {
    if(!row.hint_record)return {};
    try {
        const auto record=doc.density_record(row.hint_record);SpatialDensityRequest::replay(doc,record->id);
        if(density_engine(*record->result->provenance()->environment)==density_engine(*build_environment()) && density_reuse_compatible(request,*record->result)) {
            row.record=record->id;row.reuse="source_record";return record->result;
        }
    }catch(const std::invalid_argument&){}catch(const std::out_of_range&){}catch(const std::length_error&){}
    return {};
}
void write_json_new(const std::string& path,const std::string& data,std::size_t limit,DensityFormat format,const std::function<void()>& guard={}) {
    if(format==DensityFormat::ProjectV1) {
        const auto target=std::filesystem::u8path(path);
        if(std::filesystem::exists(target.parent_path()/std::filesystem::u8path("."+target.filename().u8string()+".part")))
            throw WorkflowOutputExists("JSON scratch already exists");
    }
    write_new(path,data,limit,guard);
}
void checkpoint_receipt(const ProjectDensityReport& report,ProjectDensityImage& row,const SpatialDensityResult& result,DensityFormat format,
    const std::function<void()>& guard,const std::atomic<bool>* c,std::vector<ReusedFile>& held) {
    const auto data=detail::density_receipt(report,row,result,format).dump(2,' ',true);
    const auto path=(std::filesystem::u8path(report.output)/("image-"+std::to_string(row.id)+".json")).u8string();
    write_json_new(path,data,mib,format,guard);row.receipt_json=data;
    held.push_back({std::make_unique<detail::WorkflowInput>(path,mib,false,c)});
}
bool within(const std::filesystem::path& path,const std::filesystem::path& directory) {
    auto a=path.begin(),b=directory.begin();
    for(;b!=directory.end();++a,++b) {
        if(a==path.end())return false;
#ifdef _WIN32
        const auto x=a->wstring(),y=b->wstring();
        if(CompareStringOrdinal(x.data(),static_cast<int>(x.size()),y.data(),static_cast<int>(y.size()),TRUE)!=CSTR_EQUAL)return false;
#else
        if(*a!=*b)return false;
#endif
    }
    return true;
}
std::string context_csv(const std::string& text,DensityFormat format) {
    if(format==DensityFormat::ProjectV1 && text.find_first_of(",\"\n")==std::string::npos)return text;
    return quote_csv(text);
}
std::string csv_chunk(const ProjectDensityReport& report,const ProjectDensityImage& row,const SpatialDensityRequest& request,
    const SpatialDensityResult& result,const std::string& checkpoint_path,std::uint64_t record,DensityFormat format,
    std::size_t remaining,std::size_t native_limit,const std::atomic<bool>* c) {
    const auto native=spatial_density_csv(result,native_limit,c);const auto end=native.find('\n');
    if(csv_header.substr(csv_header.find("window_id,"))!=native.substr(0,end+1))throw std::runtime_error("Native CSV schema changed");
    const auto prefix=context_csv(report.project_id,format)+","+std::to_string(row.id)+","+context_csv(row.name,format)+","+
        std::to_string(record)+","+row.reuse+","+request.input_sha256()+","+context_csv(checkpoint_path,format)+",";
    std::string chunk;
    for(std::size_t at=end+1;at<native.size();) {
        checkpoint(c);const auto next=native.find('\n',at);if(next==std::string::npos)throw std::runtime_error("Incomplete native density CSV row");
        const auto bytes=next-at+1;
        if(prefix.size()>remaining-chunk.size() || bytes>remaining-chunk.size()-prefix.size())throw CsvLimit("Combined CSV byte limit exceeded");
        chunk+=prefix;chunk.append(native,at,bytes);at=next+1;
    }
    return chunk;
}
std::string short_error(const std::string& message,DensityFormat format) {
    if(format==DensityFormat::GuiV1)return message;
    std::size_t count=0,at=0;for(;at<message.size();++at)if((static_cast<unsigned char>(message[at])&0xc0)!=0x80 && count++==2048)break;
    return message.substr(0,at);
}
ProjectDensityReport execute_density(ProjectDensityReport report,const std::string& output,const ProjectDensityRunOptions& options,
    DensityFormat format,const ProjectDensityManifest* manifest,const std::atomic<bool>* c,ProjectDensityProgress* p) {
    options.validate();report.recipe.validate();
    if(options.csv_bytes<csv_header.size())throw ProjectDensityCsvBudget("CSV limit is smaller than the header");
    if(options.pause_after_image && !p)throw std::invalid_argument("Image acknowledgement requires a native progress controller");
    checkpoint(c);detail::WorkflowInput source(report.source,options.project_bytes,true,c);
    if(manifest)manifest->verify(false,c);
    else if(source.sha256()!=report.source_sha256)throw WorkflowInputChanged("Saved project changed; preview again");
    auto project=Project::open_without_journal_recovery(source.path());source.verify(false,c);
    if(!manifest && project->identity()!=report.project_id)throw WorkflowInputChanged("Source project identity changed; preview again");
    report.source=source.path();report.source_sha256=source.sha256();report.project_id=project->identity();
    const auto directory=std::filesystem::weakly_canonical(std::filesystem::u8path(export_path(output)));
    std::vector<std::filesystem::path> protected_inputs={std::filesystem::u8path(source.path())};
    if(manifest)protected_inputs.push_back(std::filesystem::u8path(manifest->path()));
    const auto catalog=project->images();
    for(const auto& image:catalog)if(!image.embedded && !image.source_path.empty())
        protected_inputs.push_back(std::filesystem::weakly_canonical(std::filesystem::u8path(export_path(image.source_path))));
    for(const auto& input:protected_inputs)if(within(input,directory))throw std::invalid_argument("Output directory contains a protected project/manifest/image input");
    std::filesystem::path previous;
    if(!options.resume_from.empty()) {
        previous=std::filesystem::canonical(std::filesystem::u8path(export_path(options.resume_from)));
        if(!std::filesystem::is_directory(previous) || within(directory,previous) || within(previous,directory))
            throw std::invalid_argument("Previous and new run folders must be separate directories, without nesting");
        for(auto parent=directory.parent_path();!parent.empty();) {
            if(std::filesystem::exists(parent) && std::filesystem::equivalent(parent,previous))throw std::invalid_argument("New run cannot be nested inside the previous run");
            const auto above=parent.parent_path();if(above==parent)break;parent=above;
        }
    }
    if(std::filesystem::exists(directory))throw WorkflowOutputExists("Choose a new output directory");
    if(format==DensityFormat::ProjectV1 && !directory.parent_path().empty())std::filesystem::create_directories(directory.parent_path());
    if(!std::filesystem::create_directory(directory))throw WorkflowOutputExists("Choose a new output directory");
    report.state="running";report.output=directory.u8string();
    report.completed=report.computed=report.reused=report.failed=report.rows=0;report.error.clear();
    for(auto& row:report.images) {
        row.state="not_run";row.error.clear();row.rows=0;row.record=row.checkpoint_image=0;
        row.reuse="computed";row.receipt_json.clear();row.checkpoint.clear();row.checkpoint_sha256.clear();
        row.input_sha256.clear();row.result_input_sha256.clear();row.result_sha256.clear();
    }
    std::string combined=csv_header;std::vector<ReusedFile> held;
    const auto verify=[&](bool rehash,const std::atomic<bool>* stop) {
        source.verify(rehash,stop);if(manifest)manifest->verify(rehash,stop);
    };
    if(p){p->reset();p->total.store(report.images.size());p->phase.store(2);}
    try {
        for(auto& row:report.images) {
            checkpoint(c);if(format==DensityFormat::GuiV1)verify(false,c);
            if(p){p->image.store(row.id);p->phase.store(2);}
            bool fatal=false;
            try {
                if(format==DensityFormat::ProjectV1)verify(false,c);
                const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& image){return image.id==row.id;});
                if(found==catalog.end())throw std::invalid_argument("Image does not exist in the source project");
                row.name=found->name;row.image_present=true;auto doc=project->document(row.id);
                SpatialDensityRequest request(*doc,row.ids[0],row.ids[1],row.ids[2],row.ids[3],report.recipe.options);
                std::shared_ptr<const SpatialDensityResult> result;
                if(!options.recompute && !previous.empty()) {
                    if(p)p->phase.store(5);
                    result=reuse_result(previous,report.project_id,row,request,format,options.checkpoint_bytes,c,held);
                }else row.reuse_reason=options.recompute?"Recomputation requested":"No previous run selected";
                if(!result && format==DensityFormat::ProjectV1 && !options.recompute)result=source_record(*doc,row,request);
                if(!result){if(p)p->kernel_calls.fetch_add(1);result=measure_spatial_density(request,c);}checkpoint(c);
                const bool from_checkpoint=row.reuse=="reused" || row.reuse=="checkpoint";
                const auto checkpoint_path=from_checkpoint?row.checkpoint:(directory/("image-"+std::to_string(row.id)+".qpxproj")).u8string();
                const auto record=(from_checkpoint || row.reuse=="source_record")?row.record:doc->keep_density(request,result);
                std::string chunk;
                if(format==DensityFormat::GuiV1)chunk=csv_chunk(report,row,request,*result,checkpoint_path,record,format,options.csv_bytes-combined.size(),options.csv_bytes,c);
                if(!from_checkpoint) {
                    Project saved;std::vector<ObjectClass> classes;for(const auto& [name,value]:project->classes())classes.push_back(value);
                    saved.set_class_registry(classes,project->class_defaults());saved.set_metadata(project->metadata());
                    const auto image=saved.add_document(doc,row.name,found->embedded?"":found->source_path,found->reader);
                    saved.set_image_metadata(image,project->image_metadata(row.id));saved.set_image_details(image,project->image_description(row.id),project->image_tags(row.id));
                    if(auto view=project->view(row.id))saved.set_view(image,*view);if(p)p->phase.store(3);
                    saved.save_new(checkpoint_path,512*mib,options.checkpoint_bytes,c,[&]{verify(false,c);});
                    auto guard=std::make_unique<detail::WorkflowInput>(checkpoint_path,options.checkpoint_bytes,true,c);
                    row.record=record;row.checkpoint_image=image;row.checkpoint=guard->path();row.checkpoint_sha256=guard->sha256();held.push_back({std::move(guard)});
                }
                verify(false,c);checkpoint(c);row.input_sha256=request.input_sha256();row.result_input_sha256=result->input_sha256();row.result_sha256=result->sha256();
                checkpoint_receipt(report,row,*result,format,[&]{verify(false,c);},c,held);
                if(format==DensityFormat::ProjectV1)chunk=csv_chunk(report,row,request,*result,checkpoint_path,record,format,options.csv_bytes-combined.size(),64*mib,c);
                checkpoint(c);combined+=chunk;row.rows=result->rows().size();row.state=from_checkpoint?"reused":"saved";
                report.rows+=row.rows;++report.completed;
                if(row.reuse!="computed"){++report.reused;if(p)p->reused.fetch_add(1);}else ++report.computed;
            }catch(const WorkflowInputChanged& e) {
                row.error=short_error(e.what(),format);row.state="failed";++report.failed;report.error=row.error;fatal=true;
            }catch(const CsvLimit& e) {
                row.error=short_error(e.what(),format);row.state="failed";++report.failed;report.error=row.error;fatal=true;
            }catch(const std::exception& e) {
                if(c && c->load()) {
                    if(format==DensityFormat::GuiV1)throw;
                    row.state="cancelled";row.error="cancelled; no partial image CSV published";
                }else {row.error=short_error(e.what(),format);row.state="failed";++report.failed;}
            }
            if(p){p->completed.fetch_add(1);p->publish_image(detail::density_image_json(row,format),options.pause_after_image,c);}
            if((c && c->load()) || fatal || (row.state=="failed" && options.stop_on_error))break;
        }
    }catch(const std::exception& e){report.error=short_error(e.what(),format);}
    try {verify(true,nullptr);for(const auto& file:held)file.input->verify(true,nullptr);}catch(const std::exception& e){report.error=short_error(e.what(),format);}
    report.not_run=std::count_if(report.images.begin(),report.images.end(),[](const auto& row){return row.state=="not_run";});
    report.state=c && c->load()?"cancelled":report.failed || !report.error.empty()?"partial":"completed";
    if(p)p->phase.store(4);
    report.csv=(directory/(report.state=="completed"?"density.csv":"density.partial.csv")).u8string();
    const auto publication_guard=[&]{verify(false,nullptr);for(const auto& file:held)file.input->verify(false,nullptr);};
    // Failed integrity is still reported as a partial historical run; complete publication rechecks every guard.
    if(report.state=="completed")write_new(report.csv,combined,options.csv_bytes,publication_guard);
    else write_new(report.csv,combined,options.csv_bytes);
    report.report=(directory/"report.json").u8string();report.json=detail::density_report_json(report,format);
    write_json_new(report.report,report.json,16*mib,format,report.state=="completed"?std::function<void()>(publication_guard):std::function<void()>{});return report;
}

}
namespace {
constexpr std::size_t recipe_limit=128u*1024*1024,report_limit=256u*1024*1024;
void image_capture(std::size_t& bytes,const ProjectDensityImage& row) {
    const auto charge=[&](std::size_t n){workflow_capture_charge(bytes,n,report_limit);};
    charge(sizeof(ProjectDensityImage));for(const auto& ids:row.ids)charge(ids.size()*sizeof(std::uint64_t));
    for(const auto* text:{&row.name,&row.error,&row.state,&row.checkpoint,&row.input_sha256,&row.result_sha256,&row.reuse,&row.reuse_reason,&row.checkpoint_sha256,&row.result_input_sha256,&row.receipt_json})charge(text->size());
    for(const auto& issue:row.issues)charge(sizeof(std::string)+issue.size());
}
}
std::size_t ProjectDensityRecipe::capture_bytes() const {
    std::size_t bytes=sizeof(*this);const auto charge=[&](std::size_t n){workflow_capture_charge(bytes,n,recipe_limit);};
    charge(images.size()*sizeof(std::uint64_t));charge(selections.size()*sizeof(ProjectDensitySelection));
    for(const auto& image:selections)for(const auto& role:image.ids)charge(role.size()*sizeof(std::uint64_t));
    for(const auto& role:classes)for(const auto& name:role)charge(sizeof(std::string)+name.size());return bytes;
}
std::size_t ProjectDensityReport::capture_bytes() const {
    std::size_t bytes=sizeof(*this);const auto charge=[&](std::size_t n){workflow_capture_charge(bytes,n,report_limit);};charge(recipe.capture_bytes());
    for(const auto* text:{&source,&source_sha256,&project_id,&state,&error,&output,&csv,&report,&manifest,&manifest_sha256,&json})charge(text->size());
    for(const auto& image:images)image_capture(bytes,image);return bytes;
}
void ProjectDensityReport::validate_preview() const {
    if(!valid())throw std::invalid_argument("Review a successful preview before Run");recipe.validate();capture_bytes();workflow_path_argument(source);
    if(images.size()>1000)throw std::invalid_argument("Select1..1000 preview images");std::set<std::uint64_t> seen;
    for(const auto& image:images) {
        if(!image.id || !seen.insert(image.id).second)throw std::invalid_argument("Preview image IDs must be positive and distinct");
        for(std::size_t role=0;role<4;++role)if(image.ids[role].size()>(role==3?100000u:10000u) || (role<2 && image.ids[role].empty()))throw std::invalid_argument("Preview cohorts exceed native bounds");
    }
}
void ProjectDensityRecipe::validate() const {
    capture_bytes();options.validate();
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
    const auto catalog=project->images();std::size_t captured_bytes=report.capture_bytes();
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
        image_capture(captured_bytes,row);report.images.push_back(std::move(row));if(p)p->completed.fetch_add(1);
    }
    input.verify(false,c);
    return report;
}
void ProjectDensityRunOptions::validate() const {
    workflow_path_argument(resume_from,true);
    for(const auto value:{std::uint64_t(csv_bytes),project_bytes,checkpoint_bytes})if(!value || value>1024ull*mib)
        throw std::invalid_argument("Workflow byte limits must be 1 byte..1 GiB");
}
void ProjectDensityProgress::reset() {
    completed.store(0);total.store(0);kernel_calls.store(0);reused.store(0);image.store(0);phase.store(0);
    events_.reset();
}
std::vector<std::string> ProjectDensityProgress::take_images() {return events_.take();}
void ProjectDensityProgress::continue_images() {events_.acknowledge();}
void ProjectDensityProgress::publish_image(std::string json,bool pause,const std::atomic<bool>* c) {events_.publish(std::move(json),pause,c);}
ProjectDensityReport run_project_density(const ProjectDensityReport& preview,const std::string& output,const std::atomic<bool>* c,ProjectDensityProgress* p,std::size_t csv_limit,const std::string& resume_from) {
    preview.validate_preview();
    ProjectDensityRunOptions options;options.csv_bytes=csv_limit;options.resume_from=resume_from;
    return execute_density(preview,output,options,DensityFormat::GuiV1,nullptr,c,p);
}
ProjectDensityReport run_project_density(const ProjectDensityManifest& manifest,const std::string& output,const ProjectDensityRunOptions& options,const std::atomic<bool>* c,ProjectDensityProgress* p) {
    options.validate();manifest.verify(false,c);ProjectDensityReport report;report.recipe=manifest.recipe();
    report.source=manifest.project();report.manifest=manifest.path();report.manifest_sha256=manifest.sha256();
    for(const auto& selected:report.recipe.selections){ProjectDensityImage row;row.id=selected.image_id;row.ids=selected.ids;row.hint_record=selected.record_id;report.images.push_back(std::move(row));}
    return execute_density(std::move(report),output,options,DensityFormat::ProjectV1,&manifest,c,p);
}
void ProjectDensityJob::reset() {cancelled_.store(false);result_.reset();manifest_result_.reset();failure_=nullptr;error_.clear();progress_.reset();}
bool ProjectDensityJob::preview(const std::string& path,const ProjectDensityRecipe& recipe) {
    poll();if(busy())return false;workflow_path_argument(path);recipe.validate();reset();future_=std::async(std::launch::async,[this,path=std::string(path),recipe=ProjectDensityRecipe(recipe)]{return preview_project_density(path,recipe,&cancelled_,&progress_);});return true;
}
bool ProjectDensityJob::generate_manifest(const std::string& path,const ProjectDensityRecipe& recipe,const std::string& output) {
    poll();if(busy())return false;workflow_path_argument(path);workflow_path_argument(output,true);recipe.capture_bytes();reset();
    manifest_future_=std::async(std::launch::async,[this,path=std::string(path),recipe=ProjectDensityRecipe(recipe),output=std::string(output)]{return generate_project_density_manifest(path,recipe,output,&cancelled_,&progress_);});return true;
}
bool ProjectDensityJob::run(const ProjectDensityReport& preview,const std::string& output,const std::string& resume_from) {
    poll();if(busy())return false;workflow_path_argument(output);workflow_path_argument(resume_from,true);preview.validate_preview();reset();future_=std::async(std::launch::async,[this,preview=ProjectDensityReport(preview),output=std::string(output),resume_from=std::string(resume_from)]{return run_project_density(preview,output,&cancelled_,&progress_,64u*1024*1024,resume_from);});return true;
}
bool ProjectDensityJob::run(const ProjectDensityManifest& manifest,const std::string& output,const ProjectDensityRunOptions& options) {
    poll();if(busy())return false;workflow_path_argument(output);options.validate();reset();
    future_=std::async(std::launch::async,[this,manifest=ProjectDensityManifest(manifest),output=std::string(output),options=ProjectDensityRunOptions(options)]{return run_project_density(manifest,output,options,&cancelled_,&progress_);});return true;
}
void ProjectDensityJob::poll() {
    workflow_require_owner(owner_);
    if(future_.valid() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{result_=std::make_shared<const ProjectDensityReport>(future_.get());}catch(...){failure_=std::current_exception();error_=workflow_failure_message(failure_);}
    }
    if(manifest_future_.valid() && manifest_future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{manifest_result_=std::make_shared<const ProjectDensityManifestReport>(manifest_future_.get());}catch(...){failure_=std::current_exception();error_=workflow_failure_message(failure_);}
    }
}
void ProjectDensityJob::finish() {workflow_require_owner(owner_);cancel();if(future_.valid())future_.wait();if(manifest_future_.valid())manifest_future_.wait();poll();}
ProjectDensityJob::~ProjectDensityJob(){cancel();if(future_.valid())future_.wait();if(manifest_future_.valid())manifest_future_.wait();}
}
