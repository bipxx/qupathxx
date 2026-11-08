#pragma once
#include <string>
#include <vector>
namespace qpx {
// Main-thread native application entry point; contains no Python dependency.
int run_application(const std::vector<std::string>& arguments);
}
