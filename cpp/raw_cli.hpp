#pragma once
#include <optional>
#include <string>
int raw_region_cli(int argc,char** argv,const std::string& reader="tiff",
                   const std::optional<std::string>& plugin_options=std::nullopt);
int raw_region_bundle_cli(int argc,char** argv,bool extract);
