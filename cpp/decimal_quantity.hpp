#pragma once
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <optional>
#include <stdexcept>
#include <string>
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

// Signed CSV fields use a strict decimal grammar, including decimal exponents.
// Keep this independent of floating from_chars availability in C++17 libraries.
inline std::optional<double> finite_decimal_number(const std::string& text) {
    std::size_t at=0;bool digits=false,nonzero=false;
    if(at<text.size() && text[at]=='-') ++at;
    auto consume_digits=[&] {
        while(at<text.size() && text[at]>='0' && text[at]<='9') {
            digits=true;nonzero=nonzero || text[at]!='0';++at;
        }
    };
    consume_digits();
    if(at<text.size() && text[at]=='.') {++at;consume_digits();}
    if(!digits) return {};
    if(at<text.size() && (text[at]=='e' || text[at]=='E')) {
        ++at;if(at<text.size() && (text[at]=='+' || text[at]=='-')) ++at;
        const auto begin=at;
        while(at<text.size() && text[at]>='0' && text[at]<='9') ++at;
        if(at==begin) return {};
    }
    if(at!=text.size()) return {};
    static const DecimalLocale locale;
    char* end=nullptr;const auto value=locale.parse(text.c_str(),&end);
    if(end!=text.c_str()+text.size() || !std::isfinite(value) || (value==0 && nonzero)) return {};
    return value;
}
}
