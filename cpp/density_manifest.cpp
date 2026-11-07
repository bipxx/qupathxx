#include "qupathxx/density_workflow.hpp"
#include "density_preview.hpp"
#include "strict_json.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <type_traits>
namespace qpx {
namespace {
using Json=nlohmann::json;
void object_fields(const Json& value,std::initializer_list<const char*> allowed) {
    if(!value.is_object()) throw std::invalid_argument("Manifest field requires an object");
    for(auto it=value.begin();it!=value.end();++it)
        if(std::none_of(allowed.begin(),allowed.end(),[&](const auto* key){return it.key()==key;}))
            throw std::invalid_argument("Unknown manifest field: "+it.key());
}
std::uint64_t positive(const Json& value) {
    if(!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>()<=0))
        throw std::invalid_argument("Manifest IDs and limits require positive integers");
    const auto n=value.get<std::uint64_t>();if(!n)throw std::invalid_argument("Manifest IDs and limits require positive integers");return n;
}
template<class T>void limit(T& output,const Json& value) {
    const auto n=positive(value);if(n>std::numeric_limits<T>::max())throw std::invalid_argument("Manifest limit exceeds native range");output=static_cast<T>(n);
}
}
struct ProjectDensityManifest::Impl {
    detail::WorkflowInput input;
    std::string project;
    ProjectDensityRecipe recipe;
    Impl(const std::string& path,const std::atomic<bool>* c):input(path,16u*1024*1024,false,c) {}
};
ProjectDensityManifest::ProjectDensityManifest(std::shared_ptr<const Impl> value):impl_(std::move(value)) {}
const std::string& ProjectDensityManifest::path() const {return impl_->input.path();}
const std::string& ProjectDensityManifest::sha256() const {return impl_->input.sha256();}
const std::string& ProjectDensityManifest::project() const {return impl_->project;}
const ProjectDensityRecipe& ProjectDensityManifest::recipe() const {return impl_->recipe;}
void ProjectDensityManifest::verify(bool rehash,const std::atomic<bool>* c) const {impl_->input.verify(rehash,c);}
ProjectDensityManifest load_project_density_manifest(const std::string& path,const std::atomic<bool>* c) {
    auto result=std::make_shared<ProjectDensityManifest::Impl>(path,c);
    std::ifstream input(std::filesystem::u8path(result->input.path()),std::ios::binary);if(!input)throw std::runtime_error("Cannot open manifest");
    std::string data;char block[65536];
    while(input) {
        workflow_checkpoint(c);input.read(block,sizeof(block));const auto size=std::size_t(input.gcount());
        if(size>16u*1024*1024-data.size())throw std::invalid_argument("Manifest exceeds16 MiB");data.append(block,size);
    }
    if(input.bad())throw std::runtime_error("Cannot read manifest");result->input.verify(false,c);
    detail::StrictJson strict("density manifest");if(!Json::sax_parse(data,&strict))throw std::invalid_argument("Invalid manifest JSON");
    workflow_checkpoint(c);
    try {
        const auto root=Json::parse(data);object_fields(root,{"schema","project","images","options"});
        if(root.at("schema")!="qpx.project-density/1" || !root.at("project").is_string() || root.at("project").get<std::string>().empty())
            throw std::invalid_argument("Expected qpx.project-density/1 and a nonempty project path");
        const auto source=std::filesystem::u8path(root.at("project").get<std::string>());
        const auto joined=std::filesystem::u8path(result->input.path()).parent_path()/source;
        result->project=std::filesystem::weakly_canonical(std::filesystem::u8path(detail::export_path(joined.u8string()))).u8string();
        const auto& images=root.at("images");if(!images.is_array() || images.empty() || images.size()>1000)throw std::invalid_argument("Select1..1000 manifest images");
        for(const auto& item:images) {
            object_fields(item,{"image_id","windows","tissue","excluded","cells","record_id"});ProjectDensitySelection selected;selected.image_id=positive(item.at("image_id"));
            constexpr const char* roles[]={"windows","tissue","excluded","cells"};
            for(std::size_t role=0;role<4;++role) {
                const auto& ids=item.at(roles[role]);if(!ids.is_array() || ids.size()>(role==3?100000u:10000u) || (role<2 && ids.empty()))throw std::invalid_argument("Invalid explicit manifest cohort");
                for(const auto& id:ids)selected.ids[role].push_back(positive(id));std::sort(selected.ids[role].begin(),selected.ids[role].end());
            }
            if(item.contains("record_id"))selected.record_id=positive(item.at("record_id"));result->recipe.selections.push_back(std::move(selected));
        }
        if(root.contains("options")) {
            const auto& o=root.at("options");object_fields(o,{"prefer_nucleus","polygonize_curves","snapshot_bytes","work_bytes","max_membership_tests","max_geometry_operations","max_geometry_vertices"});
            auto& options=result->recipe.options;
            for(const auto* name:{"prefer_nucleus","polygonize_curves"})if(o.contains(name)) {
                if(!o.at(name).is_boolean())throw std::invalid_argument("Density flag requires a boolean");
                if(std::string(name)=="prefer_nucleus")options.prefer_nucleus=o.at(name).get<bool>();else options.polygonize_curves=o.at(name).get<bool>();
            }
            if(o.contains("snapshot_bytes"))limit(options.snapshot_bytes,o.at("snapshot_bytes"));
            if(o.contains("work_bytes"))limit(options.work_bytes,o.at("work_bytes"));
            if(o.contains("max_membership_tests"))limit(options.max_membership_tests,o.at("max_membership_tests"));
            if(o.contains("max_geometry_operations"))limit(options.max_geometry_operations,o.at("max_geometry_operations"));
            if(o.contains("max_geometry_vertices"))limit(options.max_geometry_vertices,o.at("max_geometry_vertices"));
        }
        std::sort(result->recipe.selections.begin(),result->recipe.selections.end(),[](const auto& a,const auto& b){return a.image_id<b.image_id;});result->recipe.validate();
    }catch(const Json::exception& e){throw std::invalid_argument(std::string("Invalid density manifest: ")+e.what());}
    result->input.verify(false,c);return ProjectDensityManifest(std::move(result));
}
ProjectDensityManifestReport generate_project_density_manifest(const std::string& path,const ProjectDensityRecipe& selectors,const std::string& output,const std::atomic<bool>* c,ProjectDensityProgress* progress) {
    if(!selectors.selections.empty())throw std::invalid_argument("Manifest generation requires class selectors");
    selectors.capture_bytes();auto recipe=selectors;
    for(auto& names:recipe.classes){std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());}
    recipe.validate();workflow_checkpoint(c);
    std::filesystem::path target;
    if(!output.empty()) {
        target=std::filesystem::weakly_canonical(std::filesystem::u8path(detail::export_path(output)));
        auto scratch=target.parent_path()/std::filesystem::u8path("."+target.filename().u8string()+".part");
        if(std::filesystem::exists(target) || std::filesystem::exists(scratch))throw WorkflowOutputExists("Manifest destination or legacy scratch already exists");
    }
    detail::WorkflowInput source(path,1024ull*1024*1024,true,c);
    ProjectDensityManifestReport result;result.preview=detail::preview_density_input(source,recipe,c,progress);
    for(const auto& row:result.preview.images)if(!row.image_present)throw std::invalid_argument("unknown image IDs: "+std::to_string(row.id));
    constexpr const char* roles[]={"windows","tissue","excluded","cells"};
    Json entries=Json::array(),images=Json::array(),names=Json::object();
    for(std::size_t role=0;role<4;++role)names[roles[role]]=recipe.classes[role];
    for(const auto& row:result.preview.images) {
        Json entry={{"image_id",row.id}},counts=Json::object();
        for(std::size_t role=0;role<4;++role){entry[roles[role]]=row.ids[role];counts[roles[role]]=row.ids[role].size();}
        entries.push_back(std::move(entry));
        images.push_back({{"image_id",row.id},{"name",row.name},{"counts",counts},
            {"calibration_um",Json::array({row.x?Json(*row.x):Json(nullptr),row.y?Json(*row.y):Json(nullptr)})},{"issues",row.issues}});
    }
    const auto& o=recipe.options;const SpatialDensityOptions defaults;
    Json options={{"prefer_nucleus",o.prefer_nucleus},{"polygonize_curves",o.polygonize_curves}};
    if(o.snapshot_bytes!=defaults.snapshot_bytes)options["snapshot_bytes"]=o.snapshot_bytes;
    if(o.work_bytes!=defaults.work_bytes)options["work_bytes"]=o.work_bytes;
    if(o.max_membership_tests!=defaults.max_membership_tests)options["max_membership_tests"]=o.max_membership_tests;
    if(o.max_geometry_operations!=defaults.max_geometry_operations)options["max_geometry_operations"]=o.max_geometry_operations;
    if(o.max_geometry_vertices!=defaults.max_geometry_vertices)options["max_geometry_vertices"]=o.max_geometry_vertices;
    const Json manifest={{"schema","qpx.project-density/1"},{"project",source.path()},{"images",entries},{"options",options}};
    result.manifest_json=manifest.dump(2,' ',true)+"\n";
    if(result.manifest_json.size()>16u*1024*1024)throw std::length_error("Generated manifest exceeds16 MiB");
    const bool publish=!output.empty() && result.preview.valid();
    result.written=publish;if(publish)result.output=target.u8string();
    Json report={{"valid",result.preview.valid()},{"project",source.path()},{"project_sha256",source.sha256()},
        {"selectors",names},{"all_cells",recipe.all_cells},{"images",images},{"manifest",manifest},{"written",publish}};
    if(publish)report["output"]=result.output;result.json=report.dump(2,' ',true)+"\n";
    if(result.json.size()>64u*1024*1024)throw WorkflowCaptureLimit("Manifest preview report exceeds64 MiB");
    result.written=false;source.verify(true,c);
    if(publish) {
        if(progress)progress->phase.store(4);
        try{detail::AtomicExportFile file(target.u8string(),16u*1024*1024,c);file.write(result.manifest_json.data(),result.manifest_json.size());source.verify(false,c);file.commit(false);}
        catch(...){workflow_checkpoint(c);std::error_code error;if(std::filesystem::exists(target,error))throw WorkflowOutputExists("Manifest output already exists");throw;}
        result.written=true;
    }
    return result;
}

}
