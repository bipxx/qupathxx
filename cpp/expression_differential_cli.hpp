#pragma once
#include <atomic>
#include <optional>
#include <string>
#include <vector>
namespace qpx::detail {
std::optional<int> expression_differential_cli(const std::vector<std::string>&,const std::atomic<bool>& cancelled);
}
