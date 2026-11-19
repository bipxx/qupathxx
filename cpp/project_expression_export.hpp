#pragma once

#include "project_export_guard.hpp"
#include "project_source_internal.hpp"
#include "qupathxx/project.hpp"
#include "project_database_output.hpp"
#include "workflow_input.hpp"

namespace qpx::detail {
class ProjectExpressionOutput {
    std::string output_;
    WorkflowInput input_;
    std::shared_ptr<Project> project_;
    std::vector<ProjectImage> images_;
    const std::atomic<bool>* cancelled_;

    void protect_database(const std::string& output) const {
        input_.verify(false, cancelled_);
        guard_project_database_output(input_.path(), "", output);
    }
public:
    ProjectExpressionOutput(const std::string& input, const std::string& output,
                            std::uint64_t project_bytes, const std::atomic<bool>* cancelled)
        : output_(export_path(output)), input_(input, project_bytes, true, cancelled),
          project_(Project::open_without_journal_recovery(input_.path())), images_(project_->images()), cancelled_(cancelled) {
        protect();
    }

    const Project& project() const { return *project_; }
    Project& project() { return *project_; }
    const std::string& output() const { return output_; }

    void protect() const {
        protect_database(output_);
        guard_project_export_output(*project_, images_, output_, cancelled_);
    }

    void protect_managed_output(const std::string& output) const {
        protect_database(output);
        guard_project_inputs_output(*project_, images_, output, cancelled_);
    }
};
}
