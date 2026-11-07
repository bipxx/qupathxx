#include "project_density_panel.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "batch_checkpoint.hpp"
#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
namespace qpx::detail {
namespace {
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
std::string report_json(const DensityPanelReport& report) {
    const auto quote=[](const std::string& s){return batch_json_quote(s);};
    std::string out="{\"schema\":\"qpx.gui-project-density/1\",\"state\":"+quote(report.state)+",\"error\":"+quote(report.error)+
        ",\"source\":"+quote(report.source)+",\"source_sha256\":"+quote(report.source_sha256)+",\"project_id\":"+quote(report.project_id)+
        ",\"completed\":"+std::to_string(report.completed)+",\"failed\":"+std::to_string(report.failed)+",\"rows\":"+std::to_string(report.rows)+",\"images\":[";
    bool first=true;for(const auto& row:report.images) {
        if(!first)out+=',';first=false;
        out+="{\"image_id\":"+std::to_string(row.id)+",\"image_name\":"+quote(row.name)+",\"state\":"+quote(row.state)+",\"error\":"+quote(row.error)+
            ",\"checkpoint\":"+quote(row.checkpoint)+",\"checkpoint_image_id\":"+std::to_string(row.checkpoint_image)+",\"record_id\":"+std::to_string(row.record)+
            ",\"input_sha256\":"+quote(row.input_sha256)+",\"result_sha256\":"+quote(row.result_sha256)+"}";
    }return out+"]}\n";
}
void write_new(const std::string& path,const std::string& data,std::size_t limit) {
    AtomicExportFile file(path,limit);file.write(data.data(),data.size());file.commit(false);
}
}
void DensityPanelRecipe::validate() const {
    options.validate();
    if(images.empty() || images.size()>1000 || std::set<std::uint64_t>(images.begin(),images.end()).size()!=images.size() || std::find(images.begin(),images.end(),0)!=images.end())
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
bool DensityPanelReport::valid() const {
    return state=="preview" && !images.empty() && error.empty() &&
        std::all_of(images.begin(),images.end(),[](const auto& row){return row.error.empty();});
}
DensityPanelReport preview_project_density(const std::string& path,const DensityPanelRecipe& recipe,const std::atomic<bool>* c,DensityPanelProgress* p) {
    recipe.validate();checkpoint(c);DensityPanelReport report;report.recipe=recipe;
    report.source=std::filesystem::canonical(std::filesystem::u8path(export_path(path))).u8string();no_sidecars(report.source);
    ReaderFileGuard guard(report.source);report.source_sha256=fingerprint(report.source,c);
    auto project=Project::open_without_journal_recovery(report.source);verify_source(report.source,guard);
    report.project_id=project->identity();report.state="preview";
    const auto catalog=project->images();
    auto ids=recipe.images;std::sort(ids.begin(),ids.end());
    if(p){p->total.store(ids.size());p->phase.store(1);}
    for(auto id:ids) {
        checkpoint(c);verify_source(report.source,guard);DensityPanelImage row;row.id=id;
        if(p)p->image.store(id);
        try {
            const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& image){return image.id==id;});
            if(found==catalog.end())throw std::invalid_argument("Image is no longer in the source project");row.name=found->name;
            auto doc=project->document(id);row.x=doc->pixel_width_um();row.y=doc->pixel_height_um();
            std::array<std::set<std::string>,4> matched;
            for(const auto& obj:doc->annotations()) {
                for(std::size_t role=0;role<4;++role) {
                    const auto& names=recipe.classes[role];
                    if(std::find(names.begin(),names.end(),obj.class_name)!=names.end()) {row.ids[role].push_back(obj.id);matched[role].insert(obj.class_name);}
                }
                if(recipe.all_cells && obj.kind==ObjectKind::Cell)row.ids[3].push_back(obj.id);
            }
            constexpr const char* roles[]={"windows","tissue","exclusions","Cells"};
            for(std::size_t role=0;role<4;++role) {
                for(const auto& name:recipe.classes[role]) if(!matched[role].count(name)) throw std::invalid_argument(std::string(roles[role])+": no objects for class "+name);
                std::sort(row.ids[role].begin(),row.ids[role].end());
            }
            SpatialDensityRequest request(*doc,row.ids[0],row.ids[1],row.ids[2],row.ids[3],recipe.options);
        }catch(const std::exception& e){row.error=e.what();}
        report.images.push_back(std::move(row));if(p)p->completed.fetch_add(1);
    }
    checkpoint(c);verify_source(report.source,guard);
    if(fingerprint(report.source,c)!=report.source_sha256)throw std::runtime_error("Saved source changed during preview");
    return report;
}
DensityPanelReport run_project_density(const DensityPanelReport& preview,const std::string& output,const std::atomic<bool>* c,DensityPanelProgress* p,std::size_t csv_limit) {
    if(!preview.valid())throw std::invalid_argument("Review a successful preview before Run");preview.recipe.validate();
    if(!csv_limit || csv_limit>1024*mib)throw std::invalid_argument("CSV limit must be 1 byte..1 GiB");
    if(csv_limit<csv_header.size())throw std::invalid_argument("CSV limit is smaller than the header");
    checkpoint(c);no_sidecars(preview.source);ReaderFileGuard guard(preview.source);
    if(fingerprint(preview.source,c)!=preview.source_sha256)throw std::runtime_error("Saved project changed; preview again");
    auto project=Project::open_without_journal_recovery(preview.source);verify_source(preview.source,guard);
    if(project->identity()!=preview.project_id)throw std::runtime_error("Source project identity changed; preview again");
    const auto directory=std::filesystem::u8path(export_path(output));
    if(!std::filesystem::create_directory(directory))throw std::runtime_error("Choose a new output directory");
    auto report=preview;report.state="running";report.output=directory.u8string();
    for(auto& row:report.images)row.state="not_run";
    std::string combined=csv_header;
    if(p){p->total.store(report.images.size());p->completed.store(0);p->phase.store(2);}
    try {
        for(auto& row:report.images) {
            checkpoint(c);verify_source(preview.source,guard);if(p){p->image.store(row.id);p->phase.store(2);}
            try {
                auto doc=project->document(row.id);
                SpatialDensityRequest request(*doc,row.ids[0],row.ids[1],row.ids[2],row.ids[3],preview.recipe.options);
                auto result=measure_spatial_density(request,c);checkpoint(c);
                const auto native=spatial_density_csv(*result,csv_limit,c);const auto end=native.find('\n');
                if(csv_header.substr(csv_header.find("window_id,"))!=native.substr(0,end+1))throw std::runtime_error("Native CSV schema changed");
                std::string chunk;
                const auto checkpoint_path=(directory/("image-"+std::to_string(row.id)+".qpxproj")).u8string();
                const auto record=doc->keep_density(request,result);
                const auto prefix=quote_csv(project->identity())+","+std::to_string(row.id)+","+quote_csv(row.name)+","+std::to_string(record)+",computed,"+request.input_sha256()+","+quote_csv(checkpoint_path)+",";
                for(std::size_t at=end+1;at<native.size();) {
                    const auto next=native.find('\n',at);chunk+=prefix+native.substr(at,next-at)+"\n";at=next==std::string::npos?native.size():next+1;
                }
                if(chunk.size()>csv_limit || combined.size()>csv_limit-chunk.size())throw CsvLimit("Combined CSV byte limit exceeded");
                Project saved;std::vector<ObjectClass> classes;for(const auto& [name,value]:project->classes())classes.push_back(value);
                saved.set_class_registry(classes,project->class_defaults());saved.set_metadata(project->metadata());
                const auto image=saved.add_document(doc,row.name);saved.set_image_metadata(image,project->image_metadata(row.id));
                saved.set_image_details(image,project->image_description(row.id),project->image_tags(row.id));if(auto view=project->view(row.id))saved.set_view(image,*view);
                if(p)p->phase.store(3);
                saved.save_new(checkpoint_path,512*mib,1024ull*mib,c,[&]{verify_source(preview.source,guard);});
                combined+=chunk;row.record=record;row.checkpoint_image=image;row.checkpoint=checkpoint_path;
                row.input_sha256=request.input_sha256();row.result_sha256=result->sha256();row.rows=result->rows().size();row.state="saved";
                report.rows+=row.rows;++report.completed;
            }catch(const CsvLimit& e) {row.error=e.what();row.state="failed";++report.failed;throw;}catch(const std::exception& e) {
                if(c && c->load())throw;
                row.error=e.what();row.state="failed";++report.failed;
            }
            if(p)p->completed.fetch_add(1);
        }
        checkpoint(c);verify_source(preview.source,guard);
        if(fingerprint(preview.source,c)!=preview.source_sha256)throw std::runtime_error("Saved source changed during run");
        report.state=report.failed?"partial":"completed";
    }catch(const std::exception& e){report.state=c && c->load()?"cancelled":"partial";report.error=e.what();}
    if(p)p->phase.store(4);
    // A cancelled run still publishes its completed historical rows and errors.
    report.csv=(directory/(report.state=="completed"?"density.csv":"density.partial.csv")).u8string();
    write_new(report.csv,combined,csv_limit);
    report.report=(directory/"report.json").u8string();write_new(report.report,report_json(report),16*mib);
    return report;
}
void ProjectDensityPanelJob::reset() {cancelled_.store(false);result_.reset();error_.clear();progress_.completed.store(0);progress_.total.store(0);progress_.image.store(0);progress_.phase.store(0);}
bool ProjectDensityPanelJob::preview(const std::string& path,const DensityPanelRecipe& recipe) {
    poll();if(busy())return false;recipe.validate();reset();future_=std::async(std::launch::async,[this,path,recipe]{return preview_project_density(path,recipe,&cancelled_,&progress_);});return true;
}
bool ProjectDensityPanelJob::run(const DensityPanelReport& preview,const std::string& output) {
    poll();if(busy())return false;auto captured=preview;reset();future_=std::async(std::launch::async,[this,captured,output]{return run_project_density(captured,output,&cancelled_,&progress_);});return true;
}
void ProjectDensityPanelJob::poll() {
    if(busy() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{result_=std::make_shared<const DensityPanelReport>(future_.get());}catch(const std::exception& e){error_=e.what();}
    }
}
void ProjectDensityPanelJob::finish() {cancel();if(busy())future_.wait();poll();}
ProjectDensityPanelJob::~ProjectDensityPanelJob(){finish();}
}
