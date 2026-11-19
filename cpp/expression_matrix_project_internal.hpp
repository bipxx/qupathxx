#pragma once
#include "qupathxx/image_data_properties.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
namespace qpx {class Project; class ExpressionMatrixSource;}
namespace qpx::detail {
// Only a private Save As working project can carry a pending attachment.
enum class ExpressionAttachmentMode { NewOnly, ReuseExact };
struct RestoredOwnedMatrix {
    std::string path,file_sha256;
    bool copied=false;
};
RestoredOwnedMatrix restore_owned_expression_matrix(const std::string& project,
    std::uint64_t image,const std::string& association_id,const std::string& backup,
    const std::atomic<bool>* cancelled);
std::string prepare_expression_matrix_attachment(Project&, std::uint64_t image,
    std::shared_ptr<const ExpressionMatrixSource>,const std::atomic<bool>*,
    ExpressionAttachmentMode=ExpressionAttachmentMode::NewOnly);
// Project snapshot serialization hooks; do not mutate live Document or undo state.
std::shared_ptr<const ImageDataProperties> expression_properties_for_save(
    std::shared_ptr<const ImageDataProperties>,const std::filesystem::path& source_directory,
    const std::filesystem::path& destination_directory,const std::atomic<bool>*,
    const std::function<void(const std::string&)>& protect_output);
void visit_owned_expression_outputs(const std::shared_ptr<const ImageDataProperties>&,
    const std::filesystem::path&,const std::function<void(const std::string&)>&);
std::shared_ptr<const ImageDataProperties> expression_properties_after_open(
    std::shared_ptr<const ImageDataProperties>,const std::filesystem::path& directory,bool unsaved_recovery=false);
// Prevent a project save from overwriting any associated matrix, including aliases.
void guard_expression_project_output(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path& directory,const std::string& destination);
void verify_owned_expression_files(const std::shared_ptr<const ImageDataProperties>&,
    const std::filesystem::path& directory,const std::atomic<bool>*);
}
