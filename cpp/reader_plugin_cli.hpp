// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "qupathxx/reader_plugins.hpp"
#include <stdexcept>

namespace qpx::detail {
// These CLIs keep argv[0] and allow reader bootstrap before the command.
inline bool consume_reader_plugin_prefix(std::vector<std::string>& args) {
    std::size_t first=1;
    std::vector<std::string> manifests;
    while(first<args.size() && args[first]=="--reader-plugins") {
        if(first+1==args.size() || args[first+1]=="--reader-plugins")
            throw std::invalid_argument("Missing absolute manifest for --reader-plugins");
        manifests.push_back(args[first+1]);
        first+=2;
    }
    if(manifests.empty())return false;
    configure_reader_plugin_manifests(manifests);
    args.erase(args.begin()+1,args.begin()+first);
    return true;
}
}
