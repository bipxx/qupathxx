#pragma once
#include "gui_i18n.hpp"
#include <stdexcept>
namespace qpx::ui {
// UI guards carry their catalogue key by type. Native exception sentences are
// never classified by their spelling, translated, or used as a string protocol.
struct NoticeMessageKey {
    const char* key;
    explicit NoticeMessageKey(const char* value):key(value){}
    virtual ~NoticeMessageKey()=default;
};
template<class Base>struct NoticeException final:Base,NoticeMessageKey {
    explicit NoticeException(const char* key):Base(i18n::text(key)),NoticeMessageKey(key){}
};
}
