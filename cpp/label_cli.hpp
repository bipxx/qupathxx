#pragma once
#include <atomic>
#include <optional>
#include <string>
#include <vector>
namespace qpx::detail {
std::optional<int> label_cli(const std::vector<std::string>&, const std::atomic<bool>&);
std::string label_cli_help(const std::string& program);
}
