#pragma once
#include "qupathxx/scientific_grid.hpp"
#include <atomic>
#include <optional>
#include <string>
#include <vector>
namespace qpx::detail {
ScientificGridAxis raw_pixel_cli_axis(const std::string&);
std::optional<int> raw_pixel_cli(const std::vector<std::string>&,const std::atomic<bool>&);
}
