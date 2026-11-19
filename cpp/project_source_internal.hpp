#pragma once
#include "qupathxx/project_source.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
namespace qpx {
class Project;
class Document;
struct ProjectImage;
namespace detail {struct ReaderPluginLink;}
struct ProjectSourceReview::Impl {
    std::uint64_t image=0;
    std::string instance,previous_path,previous_generation,path,comparison,coverage;
    std::shared_ptr<const SourceFileAssociation> baseline,association;
    std::shared_ptr<ImageServer> candidate;
    std::shared_ptr<const detail::ReaderPluginLink> previous_plugin_link;
    mutable std::mutex mutex;bool consumed=false;
};
namespace detail {
std::shared_ptr<ImageServer> open_project_image_reader(const Document&,const std::atomic<bool>*);
void require_verified_project_source(const SourceFileAssociation&,const ReaderSource&,const std::atomic<bool>*);
void require_verified_project_source(const SourceFileAssociation&,const ImageServer&,const std::atomic<bool>*);
std::vector<std::uint8_t> encode_source_verification(const SourceFileAssociation&);
std::shared_ptr<const SourceFileAssociation> decode_source_verification(const std::uint8_t*,std::size_t);
class ReaderFileGuard;
std::vector<std::unique_ptr<ReaderFileGuard>> lock_source_verification_files(const SourceFileAssociation&,const ImageServer&);
void visit_project_inputs(const Project&,const std::vector<ProjectImage>&,
                          const std::function<void(const std::string&,bool directory)>&,
                          const std::atomic<bool>*);
void guard_linked_project_image_output(const ImageServer&,const std::string& linked_path,
                                       const SourceFileAssociation*,const std::string& absolute_destination);
void guard_project_inputs_output(const Project&,const std::vector<ProjectImage>&,
                                 const std::string& absolute_destination,const std::atomic<bool>*);
void guard_project_owned_copy_outputs(const Project&,const std::vector<ProjectImage>&,
                                     const std::filesystem::path& directory,const std::atomic<bool>*,
                                     const std::function<void(const std::string&)>& extra_protection={});
}
}
