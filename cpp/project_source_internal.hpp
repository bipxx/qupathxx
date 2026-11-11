#pragma once
#include "qupathxx/project_source.hpp"
#include <mutex>
namespace qpx {
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
std::vector<std::uint8_t> encode_source_verification(const SourceFileAssociation&);
std::shared_ptr<const SourceFileAssociation> decode_source_verification(const std::uint8_t*,std::size_t);
class ReaderFileGuard;
std::vector<std::unique_ptr<ReaderFileGuard>> lock_source_verification_files(const SourceFileAssociation&);
}
}
