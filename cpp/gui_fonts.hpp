#pragma once
#include <imgui.h>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
namespace qpx::detail {
struct GuiFontAssets {std::vector<unsigned char> cjk,emoji;};
// Fixed packaged assets, authenticated once and retained immutably for the
// module lifetime. No working-directory or checkout-path fallback.
std::filesystem::path gui_font_directory();
std::vector<unsigned char> load_gui_font_asset(const std::filesystem::path&,std::size_t,const std::string&);
const GuiFontAssets& gui_font_assets();
ImFont* configure_gui_fonts(ImFontAtlas&,int maximum_texture);
}
