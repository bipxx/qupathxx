#include "gui_fonts.hpp"
#include "gui_font_manifest.hpp"
#include "filesystem_access.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace qpx::detail {
namespace {const int font_module_anchor=0;}
std::filesystem::path gui_font_directory() {
#ifdef _WIN32
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&font_module_anchor),&module)) throw std::runtime_error("Cannot locate GUI font module");
    std::vector<wchar_t> name(512);
    for(;;) {
        const auto n=GetModuleFileNameW(module,name.data(),static_cast<DWORD>(name.size()));
        if(!n) throw std::runtime_error("Cannot locate GUI font module path");
        if(n<name.size()) return std::filesystem::path(std::wstring(name.data(),n)).parent_path()/"fonts";
        if(name.size()>=32768) throw std::runtime_error("GUI module path exceeds supported length");
        name.resize(std::min<std::size_t>(32768,name.size()*2));
    }
#else
    Dl_info module{};
    if(!dladdr(&font_module_anchor,&module) || !module.dli_fname) throw std::runtime_error("Cannot locate GUI font module path");
    return std::filesystem::absolute(std::filesystem::u8path(module.dli_fname)).parent_path()/"fonts";
#endif
}
std::vector<unsigned char> load_gui_font_asset(const std::filesystem::path& path,std::size_t expected,const std::string& sha256) {
    if(!expected || expected>20u*1024*1024 || sha256.size()!=64) throw std::invalid_argument("Invalid GUI font asset contract");
    const auto physical=access_path(path);std::error_code error;
    const auto bytes=std::filesystem::file_size(physical,error);
    if(error) throw std::runtime_error("Missing GUI font asset: "+path.u8string());
    if(bytes!=expected) throw std::runtime_error("GUI font asset size mismatch: "+path.u8string());
    std::ifstream input(physical,std::ios::binary);std::vector<unsigned char> data(expected);
    input.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(expected));
    if(input.gcount()!=static_cast<std::streamsize>(expected) || input.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Cannot read stable GUI font asset: "+path.u8string());
    Sha256 hash;hash.update(data.data(),data.size());
    if(hash.finish()!=sha256) throw std::runtime_error("GUI font asset checksum mismatch: "+path.u8string());
    return data;
}
const GuiFontAssets& gui_font_assets() {
    static const GuiFontAssets assets=[] {
        const auto directory=gui_font_directory();
        return GuiFontAssets{load_gui_font_asset(directory/gui_cjk_name,gui_cjk_bytes,gui_cjk_sha256),
                             load_gui_font_asset(directory/gui_emoji_name,gui_emoji_bytes,gui_emoji_sha256)};
    }();
    return assets;
}
ImFont* configure_gui_fonts(ImFontAtlas& atlas,int maximum_texture) {
    if(maximum_texture<256) throw std::invalid_argument("GUI font textures require a256-pixel device limit");
    const auto& assets=gui_font_assets();int limit=256;const auto cap=std::min(maximum_texture,2048);
    while(limit<=cap/2) limit*=2;
    atlas.TexMinWidth=std::min(512,limit);atlas.TexMinHeight=128;
    atlas.TexMaxWidth=atlas.TexMaxHeight=limit;atlas.TexDesiredFormat=ImTextureFormat_RGBA32;
    ImFontConfig base;base.SizePixels=13;
    auto* font=atlas.AddFontDefaultBitmap(&base);
    ImFontConfig merge;merge.MergeMode=true;merge.PixelSnapH=true;merge.SizePixels=13;merge.FontDataOwnedByAtlas=false;
    if(!font || !atlas.AddFontFromMemoryTTF(const_cast<unsigned char*>(assets.cjk.data()),static_cast<int>(assets.cjk.size()),13,&merge) ||
        !atlas.AddFontFromMemoryTTF(const_cast<unsigned char*>(assets.emoji.data()),static_cast<int>(assets.emoji.size()),13,&merge))
        throw std::runtime_error("Cannot initialize packaged GUI fonts");
    return font;
}
}
