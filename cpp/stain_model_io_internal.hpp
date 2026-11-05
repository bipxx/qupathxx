#pragma once
#include "qupathxx/stain_model_io.hpp"
#include <functional>
namespace qpx::detail {
// Recheck mutable file identities immediately before writing and committing.
void save_stain_model_guarded(const StainTransform&,const std::string&,const std::function<void()>& verify_output);
}
