#include "native_workflow_cli.hpp"
#include <stdexcept>
namespace qpx::detail {
std::optional<int> native_workflow_cli(const std::vector<std::string>& args,const std::atomic<bool>&) {
    if(args.size()>1 && (args[1]=="--project-density" || args[1]=="--density-manifest" || args[1]=="--import-nucls"))
        throw std::runtime_error("Native workflow commands require project and GEOS support");
    return {};
}
}
