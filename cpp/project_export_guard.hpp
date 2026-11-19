// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>
#include <string>
#include <vector>

namespace qpx {
class Project;
struct ProjectImage;
namespace detail {
// Protect image inputs and retained analysis assets without opening their pixels
// or sidecar payloads. Use this for outputs that do not create managed assets.
void guard_project_export_output(const Project&,const std::vector<ProjectImage>&,
                                 const std::string& output,const std::atomic<bool>* cancelled);
}
}
