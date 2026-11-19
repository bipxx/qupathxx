#include "project_source_internal.hpp"
#include "project_export_guard.hpp"
#include "atomic_export.hpp"
#include "expression_matrix_project_internal.hpp"
#include "expression_reduction_project_internal.hpp"
#include "filesystem_access.hpp"
#include "label_project.hpp"
#include "project_plugin.hpp"
#include "reader_file_guard.hpp"
#include "sha256.hpp"
#include "qupathxx/image_server.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_plugin_link.hpp"
#include <cstring>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace qpx {
std::uint64_t ProjectSourceReview::image_id() const {return impl_->image;}
const std::string& ProjectSourceReview::path() const {return impl_->path;}
const std::string& ProjectSourceReview::comparison() const {return impl_->comparison;}
const std::string& ProjectSourceReview::coverage() const {return impl_->coverage;}
std::shared_ptr<const SourceFileAssociation> ProjectSourceReview::association() const {return impl_->association;}
bool ProjectSourceReview::requires_acceptance() const {return impl_->comparison=="changed_bytes" || impl_->comparison=="changed_dependencies" || impl_->comparison=="changed_observed_source" || impl_->comparison=="changed_implementation";}
bool ProjectSourceReview::consumed() const {std::lock_guard<std::mutex> lock(impl_->mutex);return impl_->consumed;}
namespace detail {
void require_verified_project_source(const SourceFileAssociation& baseline,const ReaderSource& observed,
                                     const std::atomic<bool>* cancelled) {
    if(same_reader_files(baseline.source,observed))return;
    // Single-file review permits the same bytes at a new file identity.
    // Compound and directory checks use the complete saved observations.
    if(baseline.source.scope=="single_file" && observed.scope=="single_file" && observed.primary &&
       baseline.fingerprint.bytes==observed.primary->bytes) {
        FileFingerprintOptions limits;
        limits.byte_limit=baseline.fingerprint.bytes;
        const auto current=fingerprint_file(observed.primary->path,limits,cancelled);
        if(current->sha256==baseline.fingerprint.sha256 &&
           associate_source_file(observed,*current).status=="matching_metadata")return;
    }
    throw WorkflowInputChanged("The linked image differs from saved source verification; review and adopt it before continuing");
}
void require_verified_project_source(const SourceFileAssociation& baseline,const ImageServer& server,
                                     const std::atomic<bool>* cancelled) {
    server.require_raw_current(cancelled);
    const auto observed=server.reader_source();
    if(!observed)throw WorkflowInputChanged("The linked image has no observed source");
    require_verified_project_source(baseline,*observed,cancelled);
}
void visit_project_inputs(const Project& project,const std::vector<ProjectImage>& catalog,
                          const std::function<void(const std::string&,bool)>& visit,
                          const std::atomic<bool>* cancelled) {
    for(const auto& image:catalog) {
        workflow_checkpoint(cancelled);
        const auto document=project.document(image.id);
        visit_label_project_source_stores(document->image_data_properties(),[&](const std::string& path) {
            workflow_checkpoint(cancelled);
            visit(path,true);
        });
        if(!image.embedded && !image.source_path.empty())
            visit(image.source_path,is_project_image_directory_source(*document->server()));
    }
}
void guard_linked_project_image_output(const ImageServer& server,const std::string& linked_path,
                                       const SourceFileAssociation* reviewed,const std::string& output) {
    if(!linked_path.empty()) {
        if(is_project_image_directory_source(server))
            guard_directory_output(access_path(std::filesystem::u8path(linked_path),true).u8string(),
                                   access_path(std::filesystem::u8path(output),true).u8string());
        else
            guard_file_output(export_path(linked_path),"",output);
    }
    guard_project_plugin_output(server,output);
    if(reviewed)
        for(const auto& file:reviewed->source.companions)
            guard_file_output(file.path,file.identity,output);
}
void guard_project_inputs_output(const Project& project,const std::vector<ProjectImage>& catalog,
                                 const std::string& output,const std::atomic<bool>* cancelled) {
    const auto destination=access_path(std::filesystem::u8path(output),true).u8string();
    for(const auto& image:catalog) {
        workflow_checkpoint(cancelled);
        const auto document=project.document(image.id);
        visit_label_project_source_stores(document->image_data_properties(),[&](const std::string& path) {
            workflow_checkpoint(cancelled);
            guard_directory_output(access_path(std::filesystem::u8path(path),true).u8string(),destination);
        });
        guard_linked_project_image_output(*document->server(),image.embedded?std::string{}:image.source_path,
                                          project.source_verification(image.id).get(),output);
    }
}
void guard_project_export_output(const Project& project,const std::vector<ProjectImage>& catalog,
                                 const std::string& output,const std::atomic<bool>* cancelled) {
    guard_project_inputs_output(project,catalog,output,cancelled);
    const auto base=std::filesystem::u8path(project.path()).parent_path();
    for(const auto& image:catalog) {
        workflow_checkpoint(cancelled);
        const auto properties=project.document(image.id)->image_data_properties();
        guard_expression_project_output(properties,base,output);
        reduction::guard_project_output(properties,base,output);
    }
}
void guard_project_owned_copy_outputs(const Project& project, const std::vector<ProjectImage>& catalog,
                                      const std::filesystem::path& directory,
                                      const std::atomic<bool>* cancelled,
                                      const std::function<void(const std::string&)>& extra_protection) {
    const auto protect_new_copy = [&](const std::string& target) {
        workflow_checkpoint(cancelled);
        if (!std::filesystem::exists(access_path(std::filesystem::u8path(target), true))) {
            guard_project_inputs_output(project, catalog, target, cancelled);
            if(extra_protection)extra_protection(target);
        }
    };
    for (const auto& image : catalog) {
        workflow_checkpoint(cancelled);
        const auto properties = project.document(image.id)->image_data_properties();
        visit_owned_expression_outputs(properties, directory, protect_new_copy);
        reduction::visit_owned_outputs(properties, directory, protect_new_copy);
    }
}
namespace {
constexpr std::size_t limit=2*1024*1024;
struct Writer {
    std::vector<std::uint8_t> bytes{'Q','S','V',1};
    void integer(std::uint64_t v) {for(unsigned i=0;i<8;++i)bytes.push_back(std::uint8_t(v>>(8*i)));}
    void text(const std::string& s) {integer(s.size());bytes.insert(bytes.end(),s.begin(),s.end());}
    void file(const FileVersion& v) {for(const auto* s:{&v.path,&v.identity_scheme,&v.identity,&v.stamp_scheme,&v.modified_token,&v.change_token})text(*s);integer(v.bytes);integer(v.observed_unix_ms);}
};
struct Reader {
    const std::uint8_t* p;std::size_t left;
    std::uint64_t integer(std::uint64_t max=std::numeric_limits<std::int64_t>::max()) {
        if(left<8)throw std::invalid_argument("truncated source verification");std::uint64_t v=0;
        for(unsigned i=0;i<8;++i)v|=std::uint64_t(p[i])<<(8*i);p+=8;left-=8;
        if(v>max)throw std::invalid_argument("invalid source verification integer");return v;
    }
    std::string text() {auto n=integer(32768);if(n>left)throw std::invalid_argument("truncated source verification text");std::string out(reinterpret_cast<const char*>(p),std::size_t(n));p+=n;left-=n;return out;}
    FileVersion file() {FileVersion v;for(auto* s:{&v.path,&v.identity_scheme,&v.identity,&v.stamp_scheme,&v.modified_token,&v.change_token})*s=text();v.bytes=integer();v.observed_unix_ms=integer();return v;}
};
}
std::vector<std::uint8_t> encode_source_verification(const SourceFileAssociation& value) {
    // Existing validation/status authority remains the boundary contract.
    const auto checked=associate_source_file(value.source,value.fingerprint);
    if(checked.status!=value.status || (value.status!="matching_metadata" && value.status!="matching_primary_metadata"))throw std::invalid_argument("project verification requires matching reader metadata");
    Writer w;const auto& s=value.source;
    for(const auto* text:{&s.generation,&s.reader,&s.locator,&s.scope})w.text(*text);
    w.integer(bool(s.primary));if(s.primary)w.file(*s.primary);w.integer(s.companions.size());for(const auto& file:s.companions)w.file(file);
    if(s.scope=="directory_store"){w.bytes[3]=2;w.text(s.directory_sha256);}
    const auto& f=value.fingerprint;
    for(const auto* text:{&f.path,&f.sha256,&f.identity_scheme,&f.identity,&f.stamp_scheme,&f.modified_token,&f.change_token})w.text(*text);
    w.integer(f.bytes);w.integer(f.started_unix_ms);w.integer(f.finished_unix_ms);
    std::uint64_t bits;std::memcpy(&bits,&f.elapsed_ms,8);w.integer(bits);w.text(value.status);
    Sha256 hash;hash.update(w.bytes.data(),w.bytes.size());const auto digest=hash.finish();w.bytes.insert(w.bytes.end(),digest.begin(),digest.end());
    if(w.bytes.size()>limit)throw std::length_error("source verification exceeds 2 MiB");return w.bytes;
}
std::shared_ptr<const SourceFileAssociation> decode_source_verification(const std::uint8_t* p,std::size_t size) {
    if(!p || size<68 || size>limit || std::memcmp(p,"QSV",3) || (p[3]!=1 && p[3]!=2))throw std::invalid_argument("invalid source verification encoding");
    Sha256 hash;hash.update(p,size-64);if(hash.finish()!=std::string(reinterpret_cast<const char*>(p+size-64),64))throw std::invalid_argument("source verification checksum mismatch");
    Reader r{p+4,size-68};SourceFileAssociation value;auto& s=value.source;
    for(auto* text:{&s.generation,&s.reader,&s.locator,&s.scope})*text=r.text();if(r.integer(1))s.primary=std::make_shared<const FileVersion>(r.file());
    const auto n=r.integer(63);for(std::size_t i=0;i<n;++i)s.companions.push_back(r.file());
    if(p[3]==2){s.directory_sha256=r.text();if(s.scope!="directory_store")throw std::invalid_argument("Directory verification requires directory scope");}
    auto& f=value.fingerprint;for(auto* text:{&f.path,&f.sha256,&f.identity_scheme,&f.identity,&f.stamp_scheme,&f.modified_token,&f.change_token})*text=r.text();
    f.bytes=r.integer();f.started_unix_ms=r.integer();f.finished_unix_ms=r.integer();const auto bits=r.integer(std::numeric_limits<std::uint64_t>::max());std::memcpy(&f.elapsed_ms,&bits,8);value.status=r.text();
    if(r.left)throw std::invalid_argument("trailing source verification bytes");
    const auto canonical=encode_source_verification(value);
    if(canonical.size()!=size || !std::equal(canonical.begin(),canonical.end(),p))throw std::invalid_argument("noncanonical source verification");
    return std::make_shared<const SourceFileAssociation>(std::move(value));
}
std::vector<std::unique_ptr<ReaderFileGuard>> lock_source_verification_files(const SourceFileAssociation& association,const ImageServer& candidate) {
    const auto& expected=association.source;expected.validate();
    if(!expected.primary)throw std::invalid_argument("source verification requires an observed file");
    std::vector<std::unique_ptr<ReaderFileGuard>> guards;
    guards.push_back(std::make_unique<ReaderFileGuard>(expected.primary->path));
    if(expected.scope=="directory_store") {
        if(expected.reader.rfind("ome-zarr:",0)!=0 && !is_reader_plugin_binding(expected.reader))
            throw std::invalid_argument("Unsupported directory verification reader");
        const auto observed=candidate.reader_source();
        if(!observed || observed->reader!=expected.reader || !same_reader_files(expected,*observed))
            throw std::runtime_error("Reviewed directory observation changed; review again");
        // The opened reader owns the exact inventory and its admission limits.
        candidate.require_raw_current();
    }
    auto actual=expected;actual.primary=guards.back()->version();actual.companions.clear();
    for(const auto& file:expected.companions) {guards.push_back(std::make_unique<ReaderFileGuard>(file.path));actual.companions.push_back(*guards.back()->version());}
    if(!same_reader_files(expected,actual))throw std::runtime_error("reviewed source changed; hash and review again");
    if(association.fingerprint.path!=expected.primary->path) {
        guards.push_back(std::make_unique<ReaderFileGuard>(association.fingerprint.path));auto alias=expected;alias.primary=guards.back()->version();
        if(!same_reader_files(expected,alias))throw std::runtime_error("reviewed source alias changed; hash and review again");
    }
    for(const auto& guard:guards)guard->verify();return guards;
}
}
}
