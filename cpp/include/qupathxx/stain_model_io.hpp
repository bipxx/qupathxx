#pragma once
#include "qupathxx/stains.hpp"
#include <atomic>
#include <cstdint>
#include <string>
namespace qpx {
// Model-only exchange: no estimator result or source provenance is adopted.
std::string stain_model_json(const StainTransform&);
StainTransform stain_model_from_json(const std::string&);
void save_stain_model(const StainTransform&,const std::string& path);
StainTransform load_stain_model(const std::string& path);
// Export a saved image's model to a new file. Returns the exact model digest.
std::string export_project_stain_model(const std::string& project, std::uint64_t image,
    const std::string& new_model, const std::atomic<bool>* cancelled=nullptr);
// Save the exact model as one image's defaults in a new project. Returns its digest.
std::string apply_project_stain_model(const std::string& project, std::uint64_t image,
    const std::string& model, const std::string& new_project, const std::string& profile_name={},
    const std::atomic<bool>* cancelled=nullptr);
}
