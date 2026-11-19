#pragma once
#include <string>
#include <vector>
namespace qpx {
// Native host bootstrap: configure an explicit absolute manifest once, before
// first source open/catalog use. Parsing an existing Project does not load it.
void configure_reader_plugins(const std::string& manifest_path);
// Configure 1 to 32 absolute manifests together. Each has its own package root.
// Both entry points configure the same one-shot catalog.
void configure_reader_plugin_manifests(const std::vector<std::string>& manifest_paths);
// Owned report; requesting the catalog freezes and admits configured modules.
std::string reader_catalog_json();
}
