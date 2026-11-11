#pragma once
#include "qupathxx/image_data_properties.hpp"
#include <atomic>
#include <filesystem>
namespace qpx::detail::reduction {
std::shared_ptr<const ImageDataProperties> properties_for_save(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,const std::filesystem::path&,const std::atomic<bool>*);
std::shared_ptr<const ImageDataProperties> properties_after_open(std::shared_ptr<const ImageDataProperties>,
    const std::filesystem::path&,bool unsaved_recovery);
void guard_project_output(std::shared_ptr<const ImageDataProperties>,const std::filesystem::path&,const std::string&);
}
