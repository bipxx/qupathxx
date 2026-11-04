#pragma once
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
namespace qpx {
// Native candidate and immutable comparison. No project mutation before apply.
class ProjectSourceReview {
public:
    std::uint64_t image_id() const;
    const std::string& path() const;
    const std::string& comparison() const;
    const std::string& coverage() const;
    std::shared_ptr<const SourceFileAssociation> association() const;
    bool consumed() const;
private:
    friend class Project;
    struct Impl;std::shared_ptr<Impl> impl_;
    explicit ProjectSourceReview(std::shared_ptr<Impl> p):impl_(std::move(p)) {}
};
}
