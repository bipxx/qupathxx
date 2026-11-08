#include "../gui_i18n.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace lang=qpx::ui::i18n;
static void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
static std::vector<std::string> formats(const std::string& text) {
    std::vector<std::string> result;
    for(std::size_t p=0;p<text.size();++p) if(text[p]=='%') {
        const auto start=p++;if(p<text.size() && text[p]=='%') continue;
        while(p<text.size() && std::strchr("diuoxXfFeEgGaAcspn",text[p])==nullptr) ++p;
        check(p<text.size(),"unterminated format");result.push_back(text.substr(start,p-start+1));
    }
    return result;
}
int main() {try {
    std::set<std::string> codes,keys;
    for(const auto& language:lang::languages) {check(codes.insert(language.code).second,"duplicate locale");check(*language.native_name && *language.english_name,"missing language name");}
    for(const auto& entry:lang::catalogue) {
        check(keys.insert(entry.english).second,"duplicate translation key");
        for(const auto* translated:entry.translated) {
            check(translated && *translated,"missing translation");
            check(formats(entry.english)==formats(translated),"translation changed printf arguments");
        }
    }
    check(lang::match_system_language("es_MX.UTF-8")==1,"Spanish region fallback");
    check(lang::match_system_language("pt_PT")==4,"Portuguese region fallback");
    check(lang::match_system_language("zh-Hant-HK")==7,"traditional script mapping");
    check(lang::match_system_language("zh-SG")==6,"simplified mapping");
    check(lang::match_system_language("unknown")==0,"unknown locale fallback");
    check(!lang::exact_language("not-a-language"),"invalid preference accepted");
    const auto initial=lang::state;
    {lang::Session session;lang::choose(6);
        check(std::string(lang::text("Project"))=="项目","Chinese translation");
        check(lang::label("Project###Project and objects")=="项目###Project and objects","dock identity changed");
        check(lang::label("Name##object")=="名称###Name##object","explicit widget identity changed");
        check(lang::label("##hidden")=="##hidden","hidden identity translated");
        check(std::string(lang::text("Unknown message"))=="Unknown message","missing-key fallback");
        lang::state.forced=false;
        lang::load_preference("; qupathxx workspace layout v1\n; qupathxx language=ja\n");
        check(lang::state.choice==8,"saved preference not applied");
        lang::load_preference("; qupathxx workspace layout v1\n; qupathxx language=bad\n");
        check(lang::state.choice==8,"unknown preference damaged state");
        lang::state.forced=true;
        lang::load_preference("; qupathxx workspace layout v1\n; qupathxx language=es\n");
        check(lang::state.choice==8,"explicit environment lost precedence");
        check(lang::matches_search(lang::languages[6],"Chinese"),"English search");
        check(lang::matches_search(lang::languages[6],"简体"),"native-name search");
        check(lang::matches_search(lang::languages[4],"PT-br"),"code search");
        check(lang::matches_search(lang::languages[10],"РУСС"),"Cyrillic case folding");
        check(lang::matches_search(lang::languages[15],"TIẾNG"),"Vietnamese case folding");
    }
    check(lang::state.choice==initial.choice && lang::state.effective==initial.effective && lang::state.forced==initial.forced,"session leaked language state");
    std::cout<<"GUI i18n: "<<keys.size()<<" entries, "<<codes.size()<<" languages; catalogue, formats, identity, detection, preference and session checks passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
