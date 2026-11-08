#pragma once
#include <atomic>
#include <cstdint>
#include <string>
namespace qpx {
// Native project -> checkpoint -> model -> complete stream -> new project.
// Source files are unchanged; existing output directories are refused.
std::string run_raw_classifier_workflow(const std::string& input_project,
    const std::string& output_directory,std::uint64_t image_id,std::uint32_t channels=1,
    const std::atomic<bool>* cancelled=nullptr);
}
