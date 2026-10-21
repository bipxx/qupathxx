#pragma once
#include "qupathxx/core.hpp"
namespace qpx {
// Private save representation. Keep implementation details out of public
// bindings/header consumers; only document edits and the writer need the layout.
struct Document::ObjectBlock {
    std::vector<Annotation> values;
    std::size_t bytes=0;
};
}
