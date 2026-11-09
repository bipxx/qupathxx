#pragma once
#include "qupathxx/image_data_properties.hpp"
#include <atomic>
#include <filesystem>
namespace qpx::detail {
// Project snapshot serialization hooks; do not mutate live Document or undo state.
std::shared_ptr<const ImageDataProperties> expression_properties_for_save(
    std::shared_ptr<const ImageDataProperties>,const std::filesystem::path& source_directory,
    const std::filesystem::path& destination_directory,const std::atomic<bool>*);
std::shared_ptr<const ImageDataProperties> expression_properties_after_open(
    std::shared_ptr<const ImageDataProperties>,const std::filesystem::path& directory);
// Prevent a project save from overwriting any associated matrix, including aliases.
void guard_expression_project_output(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path& directory,const std::string& destination);
}
