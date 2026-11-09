#include "qupathxx/expression_matrix_project.hpp"
#include "qupathxx/expression_matrix_spatial.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "expression_matrix_project_internal.hpp"
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
const V::Map& records(const std::shared_ptr<const ImageDataProperties>& props) {
    static const V::Map empty;
    if(!props)return empty;
    const auto it=props->values().find(property);
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
       m.domain!=ExpressionValueDomain::Counts || m.value_domain!="raw-counts")
        throw WorkflowInputChanged("Expression association source/axes/counts differ");
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
// No deletion or overwrite: shared content-addressed files survive Undo/removal.
// A later project commit can fail leaving a complete unreferenced copy, never a
// partial valid project or an advertised incomplete matrix.
void copy_owned(const ExpressionMatrixAssociation& a,const std::string& source,
                const fs::path& base,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    const auto folder=base/".qpx-expression",target=base/fs::u8path(owned_locator(a.file_sha256));
    not_linked(folder);not_linked(target);
    if(fs::exists(target)) {
        // Ordinary saves preserve an existing owned reference without rehashing a
        // whole matrix. Exact integrity remains mandatory when opening/reading it.
        std::error_code error;
        if(fs::equivalent(fs::u8path(source),target,error) && !error)return;
        auto existing=open_expression_matrix(target.u8string(),a.file_sha256,64u*1024*1024,maximum_store,cancelled);
        matches(a,*existing);return;
    }
    fs::create_directory(folder);not_linked(folder);
    detail::ReaderFileGuard guard(source);
    for(const char* suffix:{"-wal","-shm","-journal"})
        if(fs::exists(fs::u8path(source+suffix)))throw WorkflowInputChanged("Expression source has an active SQLite writer");
    if(guard.version()->bytes>maximum_store)throw WorkflowCaptureLimit("Owned expression store exceeds64GiB");
    std::ifstream input(fs::u8path(guard.version()->path),std::ios::binary);
    if(!input)throw WorkflowInputChanged("Cannot read owned expression source");
    detail::AtomicExportFile output(target.u8string(),std::size_t(maximum_store),cancelled);
    std::array<char,65536> buffer;Sha256 hash;
    while(input) {
        workflow_checkpoint(cancelled);input.read(buffer.data(),buffer.size());
        const auto count=std::size_t(input.gcount());hash.update(buffer.data(),count);output.write(buffer.data(),count);
    }
    if(!input.eof() || input.bad() || output.bytes()!=guard.version()->bytes || hash.finish()!=a.file_sha256)
        throw WorkflowInputChanged("Owned expression copy differs from retained source");
    guard.verify();workflow_checkpoint(cancelled);not_linked(folder);not_linked(target);output.commit(false);
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
std::string keep_expression_matrix(Project& project,std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource> source,bool owned,const std::atomic<bool>* cancelled) {
    if(!source)throw std::invalid_argument("Expression association requires a source");
    workflow_checkpoint(cancelled);auto doc=project.document(image);
    const auto& m=source->metadata();ExpressionMatrixAssociation a{source->path(),source->file_sha256(),m.source_sha256,m.features,m.observations,m.entries,owned};
    if(owned && !project.path().empty())a.path=owned_locator(a.file_sha256);a.validate();matches(a,*source);
    auto all=records(doc->image_data_properties());
    if(all.count(a.source_sha256))throw WorkflowOutputExists("Expression source already retained");
    all.emplace(a.source_sha256,encode(a));
    // Validate metadata capacity before publishing a managed file.
    auto values=doc->image_data_properties()?doc->image_data_properties()->values():V::Map{};
    values[property]=V{all};auto prepared=std::make_shared<const ImageDataProperties>(std::move(values));
    source->require_current(cancelled);
    if(owned && !project.path().empty())copy_owned(a,source->path(),directory(project),cancelled);
    source->require_current(cancelled);doc->set_image_data_properties(std::move(prepared));return a.source_sha256;
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
    const auto a=load_expression_matrix_association(project,image,id);
    auto source=open_expression_matrix(resolve(a,a.owned && !project.path().empty()?directory(project):fs::path{}),a.file_sha256,work,maximum_store,cancelled);
    matches(a,*source);return source;
}
void relink_expression_matrix(Project& project,std::uint64_t image,const std::string& id,
    std::shared_ptr<const ExpressionMatrixSource> source,const std::atomic<bool>* cancelled) {
    if(!source)throw std::invalid_argument("Expression relink requires a verified source");
    auto a=load_expression_matrix_association(project,image,id);matches(a,*source);source->require_current(cancelled);
    if(a.owned && !project.path().empty())copy_owned(a,source->path(),directory(project),cancelled);else a.path=source->path();
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
    if(!project.path().empty())copy_owned(a,source->path(),directory(project),cancelled);
    source->require_current(cancelled);doc->set_image_data_properties(std::move(prepared));
}

namespace detail {
std::shared_ptr<const ImageDataProperties> expression_properties_for_save(
    std::shared_ptr<const ImageDataProperties> props,const fs::path& from,const fs::path& to,
    const std::atomic<bool>* cancelled) {
    const auto& all=records(props);if(all.empty())return props;
    auto transformed=all;
    for(auto& [id,value]:transformed) {
        workflow_checkpoint(cancelled);auto a=decode(id,value);
        if(a.owned) {
            copy_owned(a,resolve(a,from),to,cancelled);
            std::get<V::Map>(value.value).at("path")=V{owned_locator(a.file_sha256)};
        }
        else {
            const auto relative=relative_locator(fs::u8path(a.path),to);
            if(!relative.empty())std::get<V::Map>(value.value).at("path")=V{relative.generic_u8string()};
        }
    }
    auto values=props->values();values[property]=V{std::move(transformed)};
    return std::make_shared<const ImageDataProperties>(std::move(values));
}
std::shared_ptr<const ImageDataProperties> expression_properties_after_open(
    std::shared_ptr<const ImageDataProperties> props,const fs::path& base,bool unsaved_recovery) {
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
        const auto a=decode(id,value);guard_file_output(resolve(a,base),"",destination);
    }
}
}
}
