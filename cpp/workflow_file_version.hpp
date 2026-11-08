#pragma once
#include "qupathxx/source_identity.hpp"
namespace qpx::detail {
// Observe identity/stamps without a persistent read lock on workflow inputs.
std::shared_ptr<const FileVersion> workflow_file_version(const std::string&);
}
