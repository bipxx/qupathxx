#pragma once
#include "qupathxx/project.hpp"
#include "qupathxx/interchange.hpp"
namespace qpx {
struct QuPathProjectImage {
    std::string source_id,name,source_path,reader;
    std::uint64_t native_id=0;
    std::size_t objects=0;
    std::vector<std::string> source_object_ids;std::vector<std::uint64_t> imported_object_ids;
    bool available=false;
};
// Private native candidate: preview never modifies a caller's workspace.
class QuPathProjectPreview {
public:
    const std::string& source_path() const;
    const std::vector<QuPathProjectImage>& images() const;
    const std::vector<InterchangeIssue>& issues() const;
    bool consumed() const;
    std::shared_ptr<Project> import_new(const std::string& destination,bool allow_losses=false,const std::atomic<bool>* cancelled=nullptr) const;
private:
    struct Impl;std::shared_ptr<Impl> impl_;
    explicit QuPathProjectPreview(std::shared_ptr<Impl> p):impl_(std::move(p)) {}
    friend std::shared_ptr<const QuPathProjectPreview> preview_qupath_project(const std::string&,const std::map<std::string,std::string>&,const std::string&,const std::atomic<bool>*);
};
std::shared_ptr<const QuPathProjectPreview> preview_qupath_project(const std::string& path,
    const std::map<std::string,std::string>& source_overrides={},const std::string& exchange_directory="",const std::atomic<bool>* cancelled=nullptr);
}
