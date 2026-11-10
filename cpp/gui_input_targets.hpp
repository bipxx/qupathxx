#pragma once
#include "qupathxx/core.hpp"
namespace qpx::detail {
inline constexpr auto gui_input_target_max=GuiInputTarget::CancelReplaceExport;
inline constexpr std::size_t gui_input_target_count=static_cast<std::size_t>(gui_input_target_max)+1;
}
