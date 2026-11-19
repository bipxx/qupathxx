#pragma once
#include <functional>
#include <memory>
#include <string>
namespace qpx {
class ImageDataProperties;
namespace detail {
// Native label-record authority; metadata-only guard, no provider loading/replay.
void visit_label_project_source_stores(const std::shared_ptr<const ImageDataProperties>&,
                                       const std::function<void(const std::string&)>&);
}
}
