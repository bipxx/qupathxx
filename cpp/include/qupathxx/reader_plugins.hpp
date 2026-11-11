#pragma once
#include <string>
namespace qpx {
// Native host bootstrap: configure an explicit absolute manifest once, before
// first source open/catalog use. Parsing an existing Project does not load it.
void configure_reader_plugins(const std::string& manifest_path);
// Owned report; requesting the catalog freezes and admits configured modules.
std::string reader_catalog_json();
}
