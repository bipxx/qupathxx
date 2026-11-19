#pragma once
#include "qupathxx/image_data_properties.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
namespace qpx::detail::reduction {
std::shared_ptr<const ImageDataProperties> properties_for_save(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,const std::filesystem::path&,const std::atomic<bool>*,
    const std::function<void(const std::string&)>& protect_output);
void visit_owned_outputs(const std::shared_ptr<const ImageDataProperties>&,
    const std::filesystem::path&,const std::function<void(const std::string&)>&);
std::shared_ptr<const ImageDataProperties> properties_after_open(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,bool unsaved_recovery);
void guard_project_output(std::shared_ptr<const ImageDataProperties>,const std::filesystem::path&,const std::string&);
}
