#pragma once
#include <string>

namespace qpx::detail {
// Format tokens are ASCII. Locale folding can corrupt multibyte UTF-8 paths.
inline void ascii_lower_in_place(std::string& value) noexcept {
    for (char& c : value)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
}
}
