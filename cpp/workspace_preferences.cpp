#include "qupathxx/core.hpp"
#include <cstdlib>
#include <filesystem>
namespace qpx {
std::string default_workspace_layout_path() {
#ifdef _WIN32
    auto environment=[](const wchar_t* name) {wchar_t* value=nullptr;std::size_t length=0;
        if(_wdupenv_s(&value,&length,name)!=0) return std::filesystem::path{};
        std::unique_ptr<wchar_t,decltype(&std::free)> owned(value,&std::free);
        return value?std::filesystem::path(value):std::filesystem::path{};};
    auto root=environment(L"APPDATA");
    if(root.empty()) {root=environment(L"USERPROFILE");if(!root.empty()) root/="AppData/Roaming";}
#else
    auto environment=[](const char* name) {const char* value=std::getenv(name);return value?std::filesystem::u8path(value):std::filesystem::path{};};
    auto root=environment("XDG_CONFIG_HOME");
    if(root.empty()) {root=environment("HOME");if(!root.empty()) root/=".config";}
#endif
    if(root.empty() || !root.is_absolute()) return {};
    return (root/"qupathxx"/"workspace-v1.ini").u8string();
}
}
