#pragma once
#include "qupathxx/omero.hpp"

namespace qpx::detail {
// Private frontend entry point; tests reuse the native protocol transport seam.
int omero_cli(const std::vector<std::string>& args, const std::atomic<bool>& cancelled,
              std::shared_ptr<OmeroTransport> transport = {});
} // namespace qpx::detail
