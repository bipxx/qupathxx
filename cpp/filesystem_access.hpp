#pragma once
#include <filesystem>
#include <optional>

namespace qpx::detail {
// Physical access only: callers retain their logical paths for metadata/UI.
inline std::filesystem::path access_path(const std::filesystem::path& path,bool extended=false) {
#ifdef _WIN32
    auto value=std::filesystem::absolute(path).lexically_normal().native();
    if((extended || value.size()>240) && value.rfind(L"\\\\?\\",0)!=0) {
        if(value.rfind(L"\\\\",0)==0) value=L"\\\\?\\UNC\\"+value.substr(2);
        else value=L"\\\\?\\"+value;
    }
    return std::filesystem::path(value);
#else
    (void)extended;return path;
#endif
}
// Compare serialized locators without filesystem lookup or physical path edits.
// Missing sources remain savable. Empty means retain the absolute locator,
// including literal NT namespace semantics and incompatible filesystem roots.
inline std::filesystem::path relative_locator(const std::filesystem::path& path,const std::filesystem::path& directory) {
#ifdef _WIN32
    if(!path.is_absolute() || !directory.is_absolute()) return path.lexically_relative(directory);
    auto comparable=[](std::filesystem::path value)->std::optional<std::filesystem::path> {
        value.make_preferred();auto name=value.native();auto prefix=name.substr(0,8);
        for(auto& c:prefix) if(c>=L'a' && c<=L'z') c-=L'a'-L'A';
        if(prefix==L"\\\\?\\UNC\\") name=L"\\\\"+name.substr(8);
        else if(name.rfind(L"\\\\?\\",0)==0) {
            if(name.size()<7 || name[5]!=L':' || name[6]!=L'\\') return {};
            name=name.substr(4);
        } else if(name.rfind(L"\\\\.\\",0)==0) return {};
        const bool drive=name.size()>=3 && ((name[0]>=L'A' && name[0]<=L'Z') || (name[0]>=L'a' && name[0]<=L'z')) && name[1]==L':' && name[2]==L'\\';
        const bool unc=name.rfind(L"\\\\",0)==0;
        if(!drive && !unc) return {};
        if(drive && name[0]>=L'a' && name[0]<=L'z') name[0]-=L'a'-L'A';
        value=std::filesystem::path(name);
        for(const auto& component:value.relative_path()) {
            const auto part=component.native();if(part.empty()) continue;
            if(part.back()==L'.' || part.back()==L' ' || part.find_first_of(L"<>:\"|?*")!=std::wstring::npos) return {};
            for(auto c:part) if(c<32) return {};
            auto stem=part.substr(0,part.find(L'.'));
            while(!stem.empty() && stem.back()==L' ') stem.pop_back();
            for(auto& c:stem) if(c>=L'a' && c<=L'z') c-=L'a'-L'A';
            if(stem==L"CON" || stem==L"PRN" || stem==L"AUX" || stem==L"NUL" || stem==L"CONIN$" || stem==L"CONOUT$") return {};
            if(stem.size()==4 && (stem.substr(0,3)==L"COM" || stem.substr(0,3)==L"LPT")) {
                const auto c=stem[3];if((c>=L'1' && c<=L'9') || c==L'\u00b9' || c==L'\u00b2' || c==L'\u00b3') return {};
            }
        }
        return value;
    };
    const auto target=comparable(path),base=comparable(directory);
    if(!target || !base || target->root_name()!=base->root_name()) return {};
    if(target->root_name().native().rfind(L"\\\\",0)==0) {
        // A UNC share is a filesystem root; '..' cannot cross into another share.
        const auto a=target->relative_path(),b=base->relative_path();
        if(a.empty() || b.empty() || *a.begin()!=*b.begin()) return {};
    }
    return target->lexically_relative(*base);
#else
    return path.lexically_relative(directory);
#endif
}
}
