#pragma once
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <optional>
#include <stdexcept>
#ifdef __APPLE__
#include <xlocale.h>
#endif

namespace qpx::detail {
// Immutable per-process conversion locale; never changes the caller's locale.
class DecimalLocale final {
#ifdef _WIN32
    _locale_t value_ = _create_locale(LC_NUMERIC, "C");
#else
    locale_t value_ = newlocale(LC_NUMERIC_MASK, "C", nullptr);
#endif
public:
    DecimalLocale() {
        if (!value_) throw std::runtime_error("cannot create decimal numeric locale");
    }
    DecimalLocale(const DecimalLocale&) = delete;
    DecimalLocale& operator=(const DecimalLocale&) = delete;
    ~DecimalLocale() {
#ifdef _WIN32
        _free_locale(value_);
#else
        freelocale(value_);
#endif
    }
    double parse(const char* text, char** end) const {
#ifdef _WIN32
        return _strtod_l(text, end, value_);
#else
        return strtod_l(text, end, value_);
#endif
    }
};

inline std::optional<double> positive_decimal_quantity(const char* text) {
    if (!text || !((*text >= '0' && *text <= '9') || *text == '.')) return {};
    // strtod also accepts hexadecimal literals; OME decimal quantities do not.
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) return {};
    static const DecimalLocale locale;
    char* end = nullptr;
    const double value = locale.parse(text, &end);
    // A representable positive subnormal remains valid even if libc sets ERANGE.
    if (end == text || *end || !std::isfinite(value) || value <= 0) return {};
    return value;
}
}
