#include "qupathxx/nucls_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "nucls_png.hpp"
#include "nucls_csv.hpp"
#include "nucls_geometry.hpp"
#include "utf8_text.hpp"
#include "strict_json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
namespace qpx {
namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
constexpr const char* class_columns[]={"raw_classification","main_classification","super_classification"};
bool known_class_column(const std::string& name){return std::any_of(std::begin(class_columns),std::end(class_columns),[&](const auto* value){return name==value;});}
Json pair_json(const NuclsPairReport& pair,const NuclsImportOptions& options) {
    Json out={{"name",pair.name},{"image",pair.image},{"annotations",pair.annotations},{"rows",pair.rows},{"imported",pair.imported},
        {"classes",pair.classes},{"source_bounds_outside",pair.source_bounds_outside},{"issue_count",pair.issue_count},{"issues",pair.issues}};
    if(!pair.image_sha256.empty()){out["image_sha256"]=pair.image_sha256;out["csv_sha256"]=pair.csv_sha256;}
    if(pair.width){out["width"]=*pair.width;out["height"]=*pair.height;out["calibration_um"]={options.mpp_x,options.mpp_y};}return out;
}
std::string report_json(const NuclsImportReport& report) {
    Json images=Json::array();for(const auto& pair:report.images)images.push_back(pair_json(pair,report.options));
    Json out={{"valid",report.valid},{"representation",report.options.representation},{"class_column",report.options.class_column},
        {"calibration_um",{report.options.mpp_x,report.options.mpp_y}},{"selected_pairs",report.selected_pairs},{"inspected_pairs",report.images.size()},
        {"images",images},{"written",report.written}};
    if(report.written)out["output"]=report.output;if(report.cancelled)out["cancelled"]=true;
    const auto text=out.dump(2,' ',true,Json::error_handler_t::replace)+"\n";
    if(text.size()>report.options.limits.report_bytes)throw std::length_error("NuCLS report exceeds byte limit");return text;
}
std::set<std::string> inventory(const fs::path& path,const std::string& extension,const std::set<std::string>* requested,std::size_t maximum,const std::atomic<bool>* cancelled) {
    std::set<std::string> names;
    for(const auto& item:fs::directory_iterator(path)) {
        workflow_checkpoint(cancelled);
        if(item.path().extension().u8string()!=extension)continue;
        const auto name=item.path().stem().u8string();if(requested && !requested->count(name))continue;
        if(item.is_regular_file())names.insert(name);
        if(!requested && names.size()>maximum)throw std::invalid_argument("Select1..100 NuCLS pairs");
        if(requested && names.size()==requested->size())break;
    }
    return names;
}
void issue(NuclsPairReport& pair,const std::string& message,const NuclsImportLimits& limits) {
    ++pair.issue_count;
    if(pair.issues.size()<limits.issues_per_image){auto size=std::min<std::size_t>(message.size(),2048);while(size && size<message.size() && (static_cast<unsigned char>(message[size])&0xc0)==0x80)--size;pair.issues.push_back(message.substr(0,size));}
}
}
void NuclsImportLimits::validate() const {
    if(!images || images>100 || !pixels || pixels>16000000 || !rows || rows>100000 || !vertices || vertices>1000000 || !issues_per_image || issues_per_image>50 ||
        !png_bytes || png_bytes>32u*1024*1024 || !csv_bytes || csv_bytes>16u*1024*1024 || !report_bytes || report_bytes>16u*1024*1024 || snapshot_bytes<1024*1024 || snapshot_bytes>512u*1024*1024 || capture_bytes<1024*1024 || capture_bytes>512u*1024*1024 || !file_bytes || file_bytes>1024ull*1024*1024)
        throw std::invalid_argument("Invalid native NuCLS import limits");
}
void NuclsImportOptions::validate() const {
    limits.validate();
    if((representation!="outlines" && representation!="centroids") || !known_class_column(class_column))throw std::invalid_argument("Invalid NuCLS representation or class column");
    if(!std::isfinite(mpp_x) || !std::isfinite(mpp_y) || mpp_x<=0 || mpp_y<=0)throw std::invalid_argument("Explicit X/Y calibration must be positive finite micrometers/pixel");
    if(names){std::set<std::string> unique;for(const auto& name:*names){detail::bounded_utf8(name,32768);if(name.empty() || name=="." || name==".." || name.find_first_of("/\\:")!=std::string::npos || !unique.insert(name).second)throw std::invalid_argument("Names must be distinct plain file stems");}if(names->empty() || names->size()>limits.images)throw std::invalid_argument("Select1..100 NuCLS pairs");}
}
NuclsImportReport import_nucls(const std::string& image_directory,const std::string& annotation_directory,const NuclsImportOptions& options,
    const std::string& output,const std::atomic<bool>* cancelled,NuclsImportProgress* progress) {
    options.validate();workflow_checkpoint(cancelled);
    if(options.pause_after_pair && !progress)throw std::invalid_argument("Pair acknowledgement requires a native progress controller");
    const auto image_root=fs::canonical(fs::u8path(detail::export_path(image_directory))),csv_root=fs::canonical(fs::u8path(detail::export_path(annotation_directory)));
    if(!fs::is_directory(image_root) || !fs::is_directory(csv_root))throw std::invalid_argument("Images and annotations must be directories");
    std::optional<std::set<std::string>> requested;
    if(options.names)requested.emplace(options.names->begin(),options.names->end());
    const auto image_files=inventory(image_root,".png",requested?&*requested:nullptr,options.limits.images,cancelled),csv_files=inventory(csv_root,".csv",requested?&*requested:nullptr,options.limits.images,cancelled);
    auto selected=image_files;selected.insert(csv_files.begin(),csv_files.end());if(options.names)selected=std::set<std::string>(options.names->begin(),options.names->end());
    if(selected.empty() || selected.size()>options.limits.images)throw std::invalid_argument("Select1..100 NuCLS pairs");
    fs::path target;if(!output.empty()){target=fs::weakly_canonical(fs::u8path(detail::export_path(output)));if(fs::exists(target))throw WorkflowOutputExists("NuCLS output already exists");}
    NuclsImportReport report;report.options=options;report.selected_pairs=selected.size();Project project;
    std::vector<std::unique_ptr<detail::WorkflowInput>> guards;std::size_t pixels=0,rows=0,vertices=0,captured_bytes=sizeof(Project);bool pixel_limit=false,capture_limit=false;
    const auto charge=[&](std::size_t bytes){workflow_capture_charge(captured_bytes,bytes,options.limits.capture_bytes);};
    if(progress){progress->reset();progress->total.store(selected.size());progress->phase.store(1);}
    for(const auto& name:selected) {
        if(cancelled && cancelled->load())break;
        NuclsPairReport pair;pair.name=name;pair.image=(image_root/fs::u8path(name+".png")).u8string();pair.annotations=(csv_root/fs::u8path(name+".csv")).u8string();
        const auto add_issue=[&](const std::string& error){issue(pair,error,options.limits);};
        try {
            if(!image_files.count(name))add_issue("missing file: "+pair.image);if(!csv_files.count(name))add_issue("missing file: "+pair.annotations);
            if(!pair.issue_count) {
                auto image_guard=std::make_unique<detail::WorkflowInput>(pair.image,options.limits.png_bytes,false,cancelled);
                auto csv_guard=std::make_unique<detail::WorkflowInput>(pair.annotations,options.limits.csv_bytes,false,cancelled);
                auto* image_source=image_guard.get();auto* csv_source=csv_guard.get();guards.push_back(std::move(image_guard));guards.push_back(std::move(csv_guard));
                pair.image_sha256=image_source->sha256();pair.csv_sha256=csv_source->sha256();
                auto rgb=detail::decode_nucls_rgb_png(*image_source,options.limits.pixels-pixels,cancelled);pixels+=std::size_t(rgb.width)*rgb.height;
                pair.width=rgb.width;pair.height=rgb.height;
                charge(sizeof(Document)+4096+2*name.size()+3*pair.image.size()+3*pair.annotations.size()+std::size_t(rgb.width)*rgb.height*3);
                auto doc=std::make_shared<Document>(Image(static_cast<int>(rgb.width),static_cast<int>(rgb.height),std::move(rgb.pixels)));doc->set_history_limits(0,0);doc->set_calibration(options.mpp_x,options.mpp_y);
                const auto text=detail::read_workflow_input(*csv_source,cancelled);detail::NuclsCsvReader reader(text);std::vector<std::string> header,fields;reader.next(header);
                std::set<std::string> unique(header.begin(),header.end());
                if(unique.size()!=header.size())throw std::invalid_argument("CSV requires unique NuCLS coordinate/type/class columns");
                for(const auto* column:{"type","xmin","ymin","xmax","ymax","coords_x","coords_y"})if(!unique.count(column))throw std::invalid_argument("CSV requires unique NuCLS coordinate/type/class columns");
                if(!unique.count(options.class_column))throw std::invalid_argument("CSV requires the selected NuCLS class column");
                std::size_t ordinal=0;
                while(reader.next(fields)) {
                    workflow_checkpoint(cancelled);if(fields.empty())continue;++ordinal;++rows;++pair.rows;if(progress)progress->rows.store(rows);
                    if(rows>options.limits.rows)throw std::length_error("Exceeds aggregate NuCLS annotation rows");
                    try {
                        if(fields.size()!=header.size())throw std::invalid_argument("CSV row width differs from header");
                        std::map<std::string,std::string> row;for(std::size_t i=0;i<header.size();++i)row.emplace(header[i],std::move(fields[i]));
                        const auto& classification=row.at(options.class_column);if(classification.empty() || detail::nucls_padded(classification))throw std::invalid_argument("Class must be nonempty with no surrounding whitespace");
                        auto geometry=detail::nucls_geometry(row);vertices+=geometry.vertices;
                        if(vertices>options.limits.vertices)throw std::length_error("Exceeds aggregate NuCLS vertices");
                        const auto bounds=geometry.outline.bounds();const bool outside=bounds.x<0 || bounds.y<0 || bounds.x+bounds.width>rgb.width || bounds.y+bounds.height>rgb.height;pair.source_bounds_outside+=outside;
                        if(options.representation=="centroids") {
                            const auto center=geometry.centroid;if(center.x<.5 || center.x>rgb.width-.5 || center.y<.5 || center.y>rgb.height-.5)throw std::invalid_argument("1px centroid marker does not fit image");
                            geometry.outline=Roi::rectangle({center.x-.5,center.y-.5,1,1});
                        }
                        std::map<std::string,std::string> metadata;for(const auto* key:class_columns)if(row.count(key))metadata[key]=row.at(key);
                        metadata["source_row"]=std::to_string(ordinal);metadata["source_type"]=row.at("type");metadata["representation"]=options.representation;metadata["class_column"]=options.class_column;
                        auto properties=std::make_shared<ObjectProperties>(std::move(metadata));
                        charge(sizeof(Annotation)+sizeof(ObjectClass)+384+3*classification.size()+2*geometry.outline.payload_bytes()+properties->payload_bytes());
                        if(!pair.classes.count(classification))doc->set_class(classification,0x008800);
                        const auto id=doc->add_cell(geometry.outline,geometry.outline,"Cell",0,classification);
                        doc->set_object_properties(id,std::move(properties));++pair.classes[classification];++pair.imported;
                    }catch(const WorkflowCaptureLimit& e){capture_limit=true;add_issue("annotation "+std::to_string(ordinal)+" (CSV line "+std::to_string(reader.line())+"): "+e.what());}
                    catch(const std::bad_alloc&){throw;}catch(const std::exception& e){add_issue("annotation "+std::to_string(ordinal)+" (CSV line "+std::to_string(reader.line())+"): "+e.what());}
                    if(vertices>options.limits.vertices || capture_limit)break;
                }
                if(!pair.issue_count) {
                    const auto image=project.add_document(doc,name);project.set_image_metadata(image,{{"importer","qpx.nucls/1"},{"representation",options.representation},{"class_column",options.class_column},
                        {"calibration_source","explicit caller X/Y micrometers/pixel"},{"image_path",image_source->path()},{"csv_path",csv_source->path()},
                        {"image_sha256",image_source->sha256()},{"csv_sha256",csv_source->sha256()},
                        {"object_semantics","annotated nuclei in native Cell body/nucleus slots; not whole-cell boundaries"}});
                }
            }
        }catch(const WorkflowCaptureLimit& e){capture_limit=true;add_issue(e.what());}
        catch(const detail::NuclsPixelLimit& e){pixel_limit=true;add_issue(e.what());}
        catch(const WorkflowInputChanged&){throw;}
        catch(const std::bad_alloc&){throw;}
        catch(const std::exception& e){if(cancelled && cancelled->load())break;add_issue(e.what());}
        report.images.push_back(std::move(pair));
        if(progress){progress->pairs.store(report.images.size());progress->events.publish(pair_json(report.images.back(),options).dump(),options.pause_after_pair,cancelled);}
        if(rows>options.limits.rows || vertices>options.limits.vertices || pixel_limit || capture_limit)break;
    }
    report.cancelled=cancelled && cancelled->load();report.valid=!report.cancelled && report.images.size()==selected.size() && std::all_of(report.images.begin(),report.images.end(),[](const auto& pair){return !pair.issue_count;});
    if(progress)progress->phase.store(2);
    for(const auto& guard:guards)guard->verify(true,report.cancelled?nullptr:cancelled);
    if(!output.empty() && report.valid) {
        if(progress)progress->phase.store(3);
        // Admit the terminal payload before a project can become visible.
        report.output=target.u8string();report.written=true;report.json=report_json(report);report.written=false;
        try{project.save_new(target.u8string(),options.limits.snapshot_bytes,options.limits.file_bytes,cancelled,[&]{for(const auto& guard:guards)guard->verify(false,cancelled);});}
        catch(...){workflow_checkpoint(cancelled);throw;}
        report.written=true;
    }else report.json=report_json(report);
    return report;
}
bool NuclsImportJob::run(const std::string& images,const std::string& annotations,const NuclsImportOptions& options,const std::string& output) {
    poll();if(busy())return false;workflow_path_argument(images);workflow_path_argument(annotations);workflow_path_argument(output,true);options.validate();cancelled_.store(false);progress_.reset();result_.reset();failure_=nullptr;error_.clear();
    future_=std::async(std::launch::async,[this,images=std::string(images),annotations=std::string(annotations),options=NuclsImportOptions(options),output=std::string(output)]{return import_nucls(images,annotations,options,output,&cancelled_,&progress_);});return true;
}
void NuclsImportJob::poll(){workflow_require_owner(owner_);if(future_.valid() && future_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{result_=std::make_shared<const NuclsImportReport>(future_.get());}catch(...){failure_=std::current_exception();error_=workflow_failure_message(failure_);}}}
void NuclsImportJob::finish(){workflow_require_owner(owner_);cancel();if(future_.valid())future_.wait();poll();}
NuclsImportJob::~NuclsImportJob(){cancel();if(future_.valid())future_.wait();}
}
