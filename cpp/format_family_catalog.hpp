#pragma once
// Private discovery only. Matrix and annotation operations keep their own typed
// requests and native authorities; image-reader discovery remains separate.
#include <string>

namespace qpx::detail {
std::string format_family_catalog_json();
}
