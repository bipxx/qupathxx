#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "expression_matrix_project_internal.hpp"
#include "expression_matrix_export.hpp"
#include "project_database_output.hpp"
#include "project_source_internal.hpp"
#include "filesystem_access.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <array>
#include <fstream>

namespace qpx {
namespace {
namespace fs=std::filesystem;
using V=ImageDataValue;
constexpr const char* property="qupathxx.expression-matrices/1";
constexpr const char* pending_property="qupathxx.pending-expression-attachment/1";
constexpr std::uint64_t maximum_store=64ull*1024*1024*1024;

void digest(const std::string& s) {
    if(s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Invalid expression association digest");
}
std::string owned_locator(const std::string& hash) {return ".qpx-expression/"+hash+".qmx";}
template<class T>const T& typed(const V& v) {
    const auto* value=std::get_if<T>(&v.value);
    if(!value)throw std::invalid_argument("Invalid expression association type");
    return *value;
}
const V& field(const V::Map& m,const char* key) {
    const auto it=m.find(key);
    if(it==m.end())throw std::invalid_argument("Incomplete expression association");
    return it->second;
}
const V::Map& records(const std::shared_ptr<const ImageDataProperties>& props,const char* key=property) {
    static const V::Map empty;
    if(!props)return empty;
    const auto it=props->values().find(key);
    return it==props->values().end()?empty:typed<V::Map>(it->second);
}
ExpressionMatrixAssociation decode(const std::string& id,const V& v,bool disk=false) {
    const auto& m=typed<V::Map>(v);
    if(m.size()!=8 || typed<std::string>(field(m,"schema"))!="qpx.expression-association/1")
        throw std::invalid_argument("Unsupported expression association schema");
    ExpressionMatrixAssociation a;
    a.path=typed<std::string>(field(m,"path"));a.owned=typed<bool>(field(m,"owned"));
    a.file_sha256=typed<std::string>(field(m,"file_sha256"));
    a.source_sha256=typed<std::string>(field(m,"source_sha256"));
    auto number=[&](const char* key) {
        const auto n=typed<std::int64_t>(field(m,key));
        if(n<0)throw std::invalid_argument("Negative expression association dimension");
        return std::uint64_t(n);
    };
    a.features=number("features");a.observations=number("observations");a.entries=number("entries");
    if(a.source_sha256!=id)throw std::invalid_argument("Expression association identity differs");
    if(disk && a.owned && a.path!=owned_locator(a.file_sha256))
        throw std::invalid_argument("Stored owned expression locator must be a canonical project child");
    // Only serialization may use an external relative locator; normalize before
    // exposing a live association. Owned locators are always the canonical child.
    if(disk && !a.owned) {
        detail::bounded_utf8(a.path,32768);workflow_path_argument(a.path);
        auto check=a;check.path=detail::export_path(a.path);check.validate();
    } else a.validate();
    return a;
}
V encode(const ExpressionMatrixAssociation& a) {
    a.validate();
    return V{V::Map{{"schema",V{std::string("qpx.expression-association/1")}},
        {"path",V{a.path}},{"owned",V{a.owned}},{"file_sha256",V{a.file_sha256}},
        {"source_sha256",V{a.source_sha256}},{"features",V{std::int64_t(a.features)}},
        {"observations",V{std::int64_t(a.observations)}},{"entries",V{std::int64_t(a.entries)}}}};
}
ExpressionMatrixAssociation decode_pending(const std::string& id,const V& value) {
    const auto association=decode(id,value);
    if(!association.owned || !fs::u8path(association.path).is_absolute())
        throw std::invalid_argument("Pending expression attachment requires an absolute owned source");
    return association;
}
fs::path directory(const Project& project) {
    if(project.path().empty())throw std::invalid_argument("Save the project before retaining owned expression data");
    return fs::u8path(project.path()).parent_path();
}
std::string resolve(const ExpressionMatrixAssociation& a,const fs::path& base) {
    if(!a.owned)return a.path;
    if(!base.empty())return (base/fs::u8path(owned_locator(a.file_sha256))).lexically_normal().u8string();
    if(fs::u8path(a.path).is_absolute())return a.path;
    throw std::invalid_argument("Owned expression locator needs its project directory");
}
void matches(const ExpressionMatrixAssociation& a,const ExpressionMatrixSource& source) {
    const auto& m=source.metadata();
    if(a.file_sha256!=source.file_sha256() || a.source_sha256!=m.source_sha256 ||
       a.features!=m.features || a.observations!=m.observations || a.entries!=m.entries ||
       (m.domain!=ExpressionValueDomain::Counts && m.domain!=ExpressionValueDomain::Continuous) ||
       (m.domain==ExpressionValueDomain::Counts?m.value_domain!="raw-counts":m.value_domain.empty() || m.value_domain=="raw-counts"))
        throw WorkflowInputChanged("Expression association source/axes/domain differ");
}
void not_linked(const fs::path& path) {
    std::error_code error;const auto status=fs::symlink_status(path,error);
    if(error && error!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("Inspect expression managed path",path,error);
    if(fs::is_symlink(status))throw std::invalid_argument("Managed expression paths cannot be symbolic links");
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::invalid_argument("Managed expression paths cannot be reparse points");
#endif
}
void verify_owned_copy(const ExpressionMatrixAssociation& a,const fs::path& target,
                       const std::atomic<bool>* cancelled) {
    const auto existing=open_expression_matrix(target.u8string(),a.file_sha256,64u*1024*1024,maximum_store,cancelled);
    matches(a,*existing);
}

void write_exact_qmx(const detail::ReaderFileGuard& guard,const std::string& digest,
                     detail::AtomicExportFile& output,const std::atomic<bool>* cancelled) {
    std::ifstream input(fs::u8path(guard.version()->path),std::ios::binary);
    if(!input)throw WorkflowInputChanged("Cannot read expression matrix copy source");
    std::array<char,65536> buffer;
    Sha256 hash;
    while(input) {
        workflow_checkpoint(cancelled);
        input.read(buffer.data(),buffer.size());
        const auto count=std::size_t(input.gcount());
        hash.update(buffer.data(),count);
        output.write(buffer.data(),count);
    }
    if(!input.eof() || input.bad() || output.bytes()!=guard.version()->bytes || hash.finish()!=digest)
        throw WorkflowInputChanged("Expression matrix copy differs from retained source");
    guard.verify();
}

// No deletion or overwrite: shared content-addressed files survive Undo/removal.
// A later project commit can fail leaving a complete unreferenced copy, never a
// partial valid project or an advertised incomplete matrix.
bool copy_owned(const ExpressionMatrixAssociation& a,const std::string& source,
                const fs::path& base,const std::atomic<bool>* cancelled,
                const std::function<void(const std::string&)>& protect_output) {
    workflow_checkpoint(cancelled);
    const auto target=detail::access_path(base/fs::u8path(owned_locator(a.file_sha256)),true);
    const auto folder=target.parent_path();
    const auto input_path=detail::access_path(fs::u8path(source));
    not_linked(folder);not_linked(target);
    if(fs::exists(target)) {
        // Ordinary saves preserve an existing owned reference without rehashing a
        // whole matrix. Exact integrity remains mandatory when opening/reading it.
        std::error_code error;
        if(fs::equivalent(input_path,target,error) && !error)return false;
        verify_owned_copy(a,target,cancelled);
        return false;
    }
    detail::ReaderFileGuard guard(input_path.u8string());
    for(const char* suffix:{"-wal","-shm","-journal"})
        if(fs::exists(detail::access_path(fs::u8path(source+suffix))))throw WorkflowInputChanged("Expression source has an active SQLite writer");
    if(guard.version()->bytes>maximum_store)throw WorkflowCaptureLimit("Owned expression store exceeds64GiB");
    protect_output(target.u8string());
    fs::create_directory(folder);not_linked(folder);
    detail::AtomicExportFile output(target.u8string(),std::size_t(maximum_store),cancelled);
    write_exact_qmx(guard,a.file_sha256,output,cancelled);
    workflow_checkpoint(cancelled);not_linked(folder);not_linked(target);
    protect_output(target.u8string());
    try {
        output.commit(false);
        return true;
    } catch(const std::runtime_error&) {
        // A concurrent writer may publish the same content-addressed asset.
        workflow_checkpoint(cancelled);
        if(!fs::exists(target))throw;
        not_linked(folder);not_linked(target);
        verify_owned_copy(a,target,cancelled);
        return false;
    }
}
void copy_into_project(const Project& project, const ExpressionMatrixAssociation& association,
                       const std::string& source, const std::atomic<bool>* cancelled) {
    const auto images = project.images();
    copy_owned(association, source, directory(project), cancelled, [&](const std::string& target) {
        detail::guard_project_inputs_output(project, images, target, cancelled);
    });
}
void publish(Document& doc,V::Map all) {
    auto values=doc.image_data_properties()?doc.image_data_properties()->values():V::Map{};
    if(all.empty())values.erase(property);else values[property]=V{std::move(all)};
    doc.set_image_data_properties(std::make_shared<const ImageDataProperties>(std::move(values)));
}
}

void ExpressionMatrixAssociation::validate()const {
    digest(file_sha256);digest(source_sha256);detail::bounded_utf8(path,32768);workflow_path_argument(path);
    if(!features || features>1000000 || !observations || observations>1000000 || entries>1000000000)
        throw std::invalid_argument("Expression association dimensions exceed contract");
    if(owned?(path!=owned_locator(file_sha256) && !fs::u8path(path).is_absolute()):!fs::u8path(path).is_absolute())
        throw std::invalid_argument("Invalid expression association locator");
}
void ExpressionMatrixAssociation::require_source(const ExpressionMatrixSource& source)const{validate();matches(*this,source);}
namespace {
std::string retain_matrix(Project& project,std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource> source,bool owned,bool pending,const std::atomic<bool>* cancelled,
    detail::ExpressionAttachmentMode mode=detail::ExpressionAttachmentMode::NewOnly) {
    if(!source)throw std::invalid_argument("Expression association requires a source");
    workflow_checkpoint(cancelled);auto doc=project.document(image);
    const auto& m=source->metadata();ExpressionMatrixAssociation a{source->path(),source->file_sha256(),m.source_sha256,m.features,m.observations,m.entries,owned};
    const bool copy_now=owned && !pending && !project.path().empty();
    if(copy_now)a.path=owned_locator(a.file_sha256);a.validate();matches(a,*source);
    const auto props=doc->image_data_properties();
    if(records(props,pending_property).count(a.source_sha256))
        throw WorkflowOutputExists("Expression source already retained");
    auto values=props?props->values():V::Map{};
    const auto& retained=records(props);
    if(const auto existing=retained.find(a.source_sha256);existing!=retained.end()) {
        if(mode!=detail::ExpressionAttachmentMode::ReuseExact)
            throw WorkflowOutputExists("Expression source already retained");
        matches(decode(existing->first,existing->second),*source);
        auto remaining=retained;
        remaining.erase(a.source_sha256);
        if(remaining.empty())values.erase(property);else values[property]=V{std::move(remaining)};
    }
    const auto key=pending?pending_property:property;
    auto all=records(props,key);
    all.emplace(a.source_sha256,encode(a));
    // Validate metadata capacity before publishing a managed file.
    values[key]=V{std::move(all)};auto prepared=std::make_shared<const ImageDataProperties>(std::move(values));
    source->require_current(cancelled);
    if(copy_now)copy_into_project(project,a,source->path(),cancelled);
    source->require_current(cancelled);doc->set_image_data_properties(std::move(prepared));return a.source_sha256;
}
}
std::string keep_expression_matrix(Project& project,std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource> source,bool owned,const std::atomic<bool>* cancelled) {
    return retain_matrix(project,image,std::move(source),owned,false,cancelled);
}
std::vector<std::string> expression_matrix_ids(const Project& project,std::uint64_t image) {
    std::vector<std::string> ids;
    for(const auto& [id,value]:records(project.document(image)->image_data_properties())){decode(id,value);ids.push_back(id);}
    return ids;
}
ExpressionMatrixAssociation load_expression_matrix_association(const Project& project,std::uint64_t image,const std::string& id) {
    const auto& all=records(project.document(image)->image_data_properties());
    const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Expression association not found");
    return decode(id,it->second);
}
std::shared_ptr<const ExpressionMatrixSource> reopen_expression_matrix(const Project& project,
    std::uint64_t image,const std::string& id,std::size_t work,const std::atomic<bool>* cancelled) {
    return expression_matrix_open_request(project,image,id).open(work,cancelled);
}
ExpressionMatrixOpenRequest expression_matrix_open_request(const Project& project,std::uint64_t image,const std::string& id) {
    const auto a=load_expression_matrix_association(project,image,id);
    return {a,resolve(a,a.owned && !project.path().empty()?directory(project):fs::path{})};
}
std::shared_ptr<const ExpressionMatrixSource> ExpressionMatrixOpenRequest::open(std::size_t work,const std::atomic<bool>* cancelled)const {
    association.validate();auto source=open_expression_matrix(path,association.file_sha256,work,maximum_store,cancelled);
    association.require_source(*source);return source;
}
void relink_expression_matrix(Project& project,std::uint64_t image,const std::string& id,
    std::shared_ptr<const ExpressionMatrixSource> source,const std::atomic<bool>* cancelled) {
    if(!source)throw std::invalid_argument("Expression relink requires a verified source");
    auto a=load_expression_matrix_association(project,image,id);matches(a,*source);source->require_current(cancelled);
    if(a.owned && !project.path().empty())copy_into_project(project,a,source->path(),cancelled);else a.path=source->path();
    auto doc=project.document(image);auto all=records(doc->image_data_properties());all.at(id)=encode(a);
    source->require_current(cancelled);publish(*doc,std::move(all));
}
void remove_expression_matrix(Project& project,std::uint64_t image,const std::string& id) {
    auto doc=project.document(image);auto all=records(doc->image_data_properties());
    const auto it=all.find(id);if(it==all.end())throw std::out_of_range("Expression association not found");
    decode(id,it->second);
    if(expression_matrix_points_reference(*doc,id))throw std::invalid_argument("Expression matrix is referenced by spatial observations");
    all.erase(it);publish(*doc,std::move(all));
}
void own_expression_matrix(Project& project,std::uint64_t image,const std::string& id,
    std::shared_ptr<const ExpressionMatrixSource> source,const std::atomic<bool>* cancelled) {
    if(!source)throw std::invalid_argument("Expression ownership requires a verified source");
    auto a=load_expression_matrix_association(project,image,id);matches(a,*source);source->require_current(cancelled);
    a.owned=true;a.path=project.path().empty()?source->path():owned_locator(a.file_sha256);
    auto doc=project.document(image);auto all=records(doc->image_data_properties());all.at(id)=encode(a);
    auto values=doc->image_data_properties()->values();values[property]=V{std::move(all)};
    auto prepared=std::make_shared<const ImageDataProperties>(std::move(values));
    if(!project.path().empty())copy_into_project(project,a,source->path(),cancelled);
    source->require_current(cancelled);doc->set_image_data_properties(std::move(prepared));
}

namespace detail {
std::size_t backup_expression_matrix(const ExpressionMatrixSource& source,const std::string& output,
    const std::atomic<bool>* cancelled,const std::function<void()>& protect_output) {
    const auto destination=export_path(output);
    const ReaderFileGuard guard(access_path(fs::u8path(source.path())).u8string());
    if(guard.version()->bytes>maximum_store)throw WorkflowCaptureLimit("Expression backup exceeds 64 GiB");
    const auto protect=[&] {
        protect_output();
        source.require_current(cancelled);
        guard_project_database_output(source.path(),guard.version()->identity,destination);
        if(has_sqlite_companion(destination))
            throw WorkflowOutputExists("QMX backup destination has a SQLite companion");
    };
    protect();
    if(fs::exists(access_path(fs::u8path(destination))))throw WorkflowOutputExists("QMX backup destination already exists");
    AtomicExportFile copy(access_path(fs::u8path(destination),true).u8string(),
                          std::size_t(guard.version()->bytes),cancelled);
    write_exact_qmx(guard,source.file_sha256(),copy,cancelled);
    protect();
    copy.commit(false);
    return copy.bytes();
}

RestoredOwnedMatrix restore_owned_expression_matrix(const std::string& input_path,
    std::uint64_t image,const std::string& association_id,const std::string& backup,
    const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const ReaderFileGuard input(export_path(input_path));
    const auto project=Project::open_without_journal_recovery(input.version()->path);
    const auto request=expression_matrix_open_request(*project,image,association_id);
    const auto& association=request.association;
    if(!association.owned)
        throw std::invalid_argument("Matrix restore requires an owned project association");
    const auto source=open_expression_matrix(backup,association.file_sha256,64u*1024*1024,
                                             maximum_store,cancelled);
    association.require_source(*source);
    const auto images=project->images();
    const auto protect=[&](const std::string& target) {
        input.verify();
        source->require_current(cancelled);
        for(const auto* suffix:{"","-journal","-wal","-shm"})
            guard_file_output(input.version()->path+suffix,"",target);
        for(const auto* suffix:{"-journal","-wal","-shm"}) {
            const auto sidecar=access_path(fs::u8path(request.path+suffix));
            not_linked(sidecar);
            if(fs::exists(sidecar))throw WorkflowInputChanged("Matrix restore target has SQLite sidecars");
        }
        guard_project_inputs_output(*project,images,target,cancelled);
    };
    protect(request.path);
    const bool copied=copy_owned(association,source->path(),directory(*project),cancelled,protect);
    input.verify();
    return {request.path,association.file_sha256,copied};
}

std::string prepare_expression_matrix_attachment(Project& project,std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource> source,const std::atomic<bool>* cancelled,
    ExpressionAttachmentMode mode) {
    return retain_matrix(project,image,std::move(source),true,true,cancelled,mode);
}
void visit_owned_expression_outputs(const std::shared_ptr<const ImageDataProperties>& props,
    const fs::path& destination, const std::function<void(const std::string&)>& visit) {
    for (const auto* key : {property,pending_property})
        for (const auto& [id, value] : records(props,key)) {
            const auto association = key==pending_property?decode_pending(id,value):decode(id,value);
            if (association.owned)
                visit((destination / fs::u8path(owned_locator(association.file_sha256))).u8string());
        }
}
std::shared_ptr<const ImageDataProperties> expression_properties_for_save(
    std::shared_ptr<const ImageDataProperties> props,const fs::path& from,const fs::path& to,
    const std::atomic<bool>* cancelled,const std::function<void(const std::string&)>& protect_output) {
    const auto& all=records(props);
    const auto& pending=records(props,pending_property);
    if(all.empty() && pending.empty())return props;
    auto transformed=all;
    for(const auto& [id,value]:pending)
        if(!transformed.emplace(id,value).second)throw WorkflowOutputExists("Expression source already retained");
    for(auto& [id,value]:transformed) {
        workflow_checkpoint(cancelled);const bool attaching=pending.count(id)!=0;
        auto a=attaching?decode_pending(id,value):decode(id,value);
        if(a.owned) {
            const auto source=attaching?a.path:resolve(a,from);
            copy_owned(a,source,to,cancelled,protect_output);
            std::get<V::Map>(value.value).at("path")=V{owned_locator(a.file_sha256)};
        }
        else {
            const auto relative=relative_locator(fs::u8path(a.path),to);
            if(!relative.empty())std::get<V::Map>(value.value).at("path")=V{relative.generic_u8string()};
        }
    }
    auto values=props->values();values.erase(pending_property);values[property]=V{std::move(transformed)};
    return std::make_shared<const ImageDataProperties>(std::move(values));
}
std::shared_ptr<const ImageDataProperties> expression_properties_after_open(
    std::shared_ptr<const ImageDataProperties> props,const fs::path& base,bool unsaved_recovery) {
    if(props && props->values().count(pending_property))
        throw std::invalid_argument("Stored project contains a pending expression attachment");
    const auto& all=records(props);if(all.empty())return props;
    auto transformed=all;
    for(auto& [id,value]:transformed) {
        auto a=decode(id,value,true);
        if(!a.owned && fs::u8path(a.path).is_relative())a.path=(base/fs::u8path(a.path)).lexically_normal().u8string();
        if(a.owned && unsaved_recovery)a.path=(base/fs::u8path(a.path)).lexically_normal().u8string();
        value=encode(a);
    }
    auto values=props->values();values[property]=V{std::move(transformed)};
    return std::make_shared<const ImageDataProperties>(std::move(values));
}
void guard_expression_project_output(std::shared_ptr<const ImageDataProperties> props,
    const fs::path& base,const std::string& destination) {
    for(const auto& [id,value]:records(props)) {
        const auto a=decode(id,value);guard_project_database_output(resolve(a,base),"",destination);
    }
    for(const auto& [id,value]:records(props,pending_property))
        guard_project_database_output(decode_pending(id,value).path,"",destination);
}
void verify_owned_expression_files(const std::shared_ptr<const ImageDataProperties>& props,
    const fs::path& directory,const std::atomic<bool>* cancelled) {
    for(const auto& [id,value]:records(props)) {
        workflow_checkpoint(cancelled);
        const auto a=decode(id,value);
        if(!a.owned) continue;
        const auto target=access_path(directory/fs::u8path(owned_locator(a.file_sha256)),true);
        not_linked(target.parent_path());
        not_linked(target);
        verify_owned_copy(a,target,cancelled);
    }
}
}
}
