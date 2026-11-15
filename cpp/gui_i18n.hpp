#pragma once
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <optional>
#include <set>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace qpx::ui::i18n {
struct MissTrace {std::set<std::string> keys;};
inline thread_local MissTrace* miss_trace=nullptr;
using SurfaceTraces=std::map<std::string,MissTrace>;
inline thread_local SurfaceTraces* surface_traces=nullptr;
inline thread_local const char* surface_name="shell";
inline thread_local bool observe_surface_names=false;
inline int effective_language();
// Instrument only an explicitly observed draw; nested surfaces restore ownership.
class SurfaceTrace {
    MissTrace* previous_=miss_trace;
    const char* previous_name_=surface_name;
    std::string observed_name_;
public:
    explicit SurfaceTrace(const char* name) {select(name);}
    void select(const char* name) {if(observe_surface_names){observed_name_=name;surface_name=observed_name_.c_str();}if(surface_traces && effective_language()!=0)miss_trace=&(*surface_traces)[name];}
    ~SurfaceTrace(){miss_trace=previous_;surface_name=previous_name_;}
    SurfaceTrace(const SurfaceTrace&)=delete;
    SurfaceTrace& operator=(const SurfaceTrace&)=delete;
};
inline bool caption_key(std::string_view key) {
    if(key.empty() || key.front()=='#')return false;
    bool letter=false;
    for(std::size_t i=0;i<key.size();++i) {
        if(key[i]=='%') {
            if(i+1<key.size() && key[i+1]=='%'){++i;continue;}
            while(++i<key.size() && !std::strchr("diuoxXfFeEgGaAcspn",key[i])){}
        } else if((key[i]>='A' && key[i]<='Z') || (key[i]>='a' && key[i]<='z'))letter=true;
        else if(static_cast<unsigned char>(key[i])>=0xc2) {
            const auto first=static_cast<unsigned char>(key[i]);
            const std::size_t count=first<0xe0?2:first<0xf0?3:4;
            if(i+count>key.size())continue;
            unsigned int code=first&(count==2?0x1f:count==3?0x0f:0x07);bool valid=true;
            for(std::size_t n=1;n<count;++n) {
                const auto next=static_cast<unsigned char>(key[i+n]);
                if((next&0xc0)!=0x80){valid=false;break;}code=(code<<6)|(next&0x3f);
            }
            if(!valid || code>0x10ffff)continue;
            i+=count-1;
#ifdef _WIN32
            wchar_t chars[2];WORD types[2]{};int length=1;
            if(code>0xffff){code-=0x10000;chars[0]=wchar_t(0xd800+(code>>10));chars[1]=wchar_t(0xdc00+(code&0x3ff));length=2;}
            else chars[0]=wchar_t(code);
            if(GetStringTypeW(CT_CTYPE1,chars,length,types) && ((types[0]|types[1])&C1_ALPHA))letter=true;
#else
            if(std::iswalpha(static_cast<wint_t>(code)))letter=true;
#endif
        }
    }
    return letter;
}
struct Language {const char* code;const char* native_name;const char* english_name;};
inline constexpr Language languages[]={
    {"en","English","English"},{"es","Español","Spanish"},
    {"fr","Français","French"},{"de","Deutsch","German"},
    {"pt-BR","Português (Brasil)","Portuguese"},{"it","Italiano","Italian"},
    {"zh-CN","简体中文","Chinese Simplified"},{"zh-TW","繁體中文","Chinese Traditional"},
    {"ja","日本語","Japanese"},{"ko","한국어","Korean"},
    {"ru","Русский","Russian"},{"tr","Türkçe","Turkish"},
    {"nl","Nederlands","Dutch"},{"pl","Polski","Polish"},
    {"id","Bahasa Indonesia","Indonesian"},{"vi","Tiếng Việt","Vietnamese"}
};
inline constexpr int language_count=sizeof(languages)/sizeof(languages[0]);
struct Entry {const char* english;std::array<const char*,language_count-1> translated;};
inline constexpr Entry catalogue[]={
#include "gui_i18n_catalog.inc"
};
inline std::string normalized(std::string text) {
    // Fold the Latin/Cyrillic alphabets used by the built-in native names.
    // CJK remains unchanged. This does not touch any process-wide locale.
    std::string result;
    for(std::size_t p=0;p<text.size();) {
        const auto first=static_cast<unsigned char>(text[p]);
        std::size_t count=first<0x80?1:first>=0xc2 && first<=0xdf?2:first>=0xe0 && first<=0xef?3:first>=0xf0 && first<=0xf4?4:1;
        if(p+count>text.size()) count=1;
        unsigned int code=first & (count==1?0xff:count==2?0x1f:count==3?0x0f:0x07);
        for(std::size_t i=1;i<count;++i) {
            const auto next=static_cast<unsigned char>(text[p+i]);
            if((next&0xc0)!=0x80) {count=1;code=first;break;}code=(code<<6)|(next&0x3f);
        }
        p+=count;
        if(code=='_') code='-';
        else if(code>='A' && code<='Z') code+=32;
        else if((code>=0xc0 && code<=0xd6) || (code>=0xd8 && code<=0xde) || (code>=0x410 && code<=0x42f)) code+=32;
        else if(code>=0x400 && code<=0x40f) code+=80;
        else if(code==0x130 || code==0x131) code='i';
        else if(code==0x178) code=0xff;
        else if((code>=0x100 && code<=0x12f && code%2==0) || (code>=0x132 && code<=0x137 && code%2==0) ||
                (code>=0x14a && code<=0x177 && code%2==0) || (code>=0x1e00 && code<=0x1eff && code%2==0)) ++code;
        else if(code>=0x139 && code<=0x148 && code%2==1) ++code;
        if(code<0x80) result+=char(code);
        else if(code<0x800) {result+=char(0xc0|(code>>6));result+=char(0x80|(code&0x3f));}
        else if(code<0x10000) {result+=char(0xe0|(code>>12));result+=char(0x80|((code>>6)&0x3f));result+=char(0x80|(code&0x3f));}
        else {result+=char(0xf0|(code>>18));result+=char(0x80|((code>>12)&0x3f));result+=char(0x80|((code>>6)&0x3f));result+=char(0x80|(code&0x3f));}
    }
    return result;
}
inline std::optional<int> exact_language(std::string_view code) {
    const auto key=normalized(std::string(code));
    for(int i=0;i<language_count;++i) if(key==normalized(languages[i].code)) return i;
    return std::nullopt;
}
inline int match_system_language(std::string code) {
    const auto suffix=code.find_first_of(".@");if(suffix!=std::string::npos) code.resize(suffix);
    code=normalized(code);
    if(auto exact=exact_language(code)) return *exact;
    if(code.rfind("zh",0)==0) return code.find("hant")!=std::string::npos || code=="zh-hk" || code=="zh-mo"?7:6;
    const auto base=code.substr(0,code.find('-'));
    for(int i=0;i<language_count;++i) {auto candidate=normalized(languages[i].code);if(base==candidate.substr(0,candidate.find('-'))) return i;}
    return 0;
}
inline int system_language() {
    const char* code=std::getenv("LC_ALL");if(!code || !*code) code=std::getenv("LC_MESSAGES");
    if(!code || !*code) code=std::getenv("LANG");
    if(code && *code) return match_system_language(code);
#ifdef _WIN32
    wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
    if(GetUserDefaultLocaleName(name,LOCALE_NAME_MAX_LENGTH)) {
        std::string value;for(const auto c:name) {if(!c) break;if(c<128) value+=char(c);}
        return match_system_language(value);
    }
#endif
    return 0;
}
struct State {int choice=-1,effective=0;bool forced=false;};
inline thread_local State state;
inline void choose(int choice) {
    state.choice=choice>=0 && choice<language_count?choice:-1;
    state.effective=state.choice<0?system_language():state.choice;
}
inline int effective_language(){return state.effective;}
// The viewer's language belongs to its context. Never change the process locale
// or leak one viewer's settings into the next Python/native session.
struct Session {
    State previous=state;
    Session() {
        state={};choose(-1);
        if(const char* value=std::getenv("QUPATHXX_UI_LANGUAGE");value && *value) {
            if(std::string_view(value)=="auto") {choose(-1);state.forced=true;}
            else if(auto index=exact_language(value)) {choose(*index);state.forced=true;}
        }
    }
    ~Session(){state=previous;}
    Session(const Session&)=delete;Session& operator=(const Session&)=delete;
};
inline const char* preference() {return state.choice<0?"auto":languages[state.choice].code;}
inline void load_preference(std::string_view data) {
    if(state.forced) return;
    constexpr std::string_view prefix="; qupathxx language=";
    const auto line=data.find('\n');if(line==std::string_view::npos) return;
    data.remove_prefix(line+1);if(data.substr(0,prefix.size())!=prefix) return;
    data.remove_prefix(prefix.size());const auto end=data.find_first_of("\r\n");
    const auto code=data.substr(0,end);
    if(code=="auto") choose(-1);else if(auto language=exact_language(code)) choose(*language);
}
inline const char* text(const char* english) {
    if(state.effective==0 || !english || !*english) return english;
    // Axis/colour notation and registered format names are invariant data, even
    // when a generic combo or field adapter forwards them through this entry.
    for(const auto token:{"RGB","X","Y","OME-Zarr","OpenSlide","PPM"})if(std::strcmp(english,token)==0)return english;
    static const auto entries=[] {
        std::unordered_map<std::string_view,const Entry*> result;
        for(const auto& entry:catalogue) result.emplace(entry.english,&entry);
        return result;
    }();
    const auto found=entries.find(english);
    if(found==entries.end()) {
        if(miss_trace && caption_key(english))miss_trace->keys.insert(english);
        return english;
    }
    const auto* value=found->second->translated[state.effective-1];
    return value && *value?value:english;
}
// Only the visible caption changes. English IDs (including explicit ##/###
// suffixes) remain stable for editing, popup routing, docking and replay.
inline std::string label(const char* original) {
    if(!original || original[0]=='#' || state.effective==0) return original?original:"";
    const auto* end=std::strstr(original,"##");
    const std::string key(original,end?end:original+std::strlen(original));
    const auto* translated=text(key.c_str());
    if(key==translated) return original;
    const auto* stable=std::strstr(original,"###");
    return std::string(translated)+"###"+(stable?stable+3:original);
}
inline bool matches_search(const Language& language,std::string_view query) {
    const auto needle=normalized(std::string(query));
    return normalized(language.native_name).find(needle)!=std::string::npos ||
        normalized(language.english_name).find(needle)!=std::string::npos ||
        normalized(language.code).find(needle)!=std::string::npos;
}
}
