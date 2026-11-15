#include "../gui_i18n.hpp"
#include "../gui_status_summary.hpp"
#include "gui_translation_trace.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace lang=qpx::ui::i18n;
static void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Vocabulary {
    std::vector<std::pair<std::string,std::regex>> rules;
    std::set<std::string> allowed;
    Vocabulary() {
        const auto path=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/gui-vocabulary.tsv";
        std::ifstream file(path);check(bool(file),"Missing shared vocabulary fixture");std::string line;
        while(std::getline(file,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            if(line.empty() || line[0]=='#')continue;
            const auto a=line.find('\t'),b=line.find('\t',a+1);
            check(a==1 && b!=std::string::npos && b+1<line.size(),"Invalid vocabulary fixture row");
            const auto key=line.substr(a+1,b-a-1),value=line.substr(b+1);
            if(line[0]=='R')rules.emplace_back(key,std::regex(value,std::regex::icase));
            else {check(line[0]=='A',"Unknown vocabulary fixture row");allowed.insert(key);}
        }
        check(!rules.empty(),"Empty vocabulary fixture");
    }
    void inspect(std::string value) const {
        value=value.substr(0,value.find("##"));if(allowed.count(value))return;
        for(const auto& rule:rules)if(std::regex_search(value,rule.second))throw std::runtime_error("Vocabulary "+rule.first+": "+value);
    }
};
static std::vector<std::string> formats(const std::string& text) {
    std::vector<std::string> result;
    for(std::size_t p=0;p<text.size();++p) if(text[p]=='%') {
        const auto start=p++;if(p<text.size() && text[p]=='%') continue;
        while(p<text.size() && std::strchr("diuoxXfFeEgGaAcspn",text[p])==nullptr) ++p;
        check(p<text.size(),"unterminated format");result.push_back(text.substr(start,p-start+1));
    }
    return result;
}
int main(int argc,char** argv) {std::cout<<std::unitbuf;std::cerr<<std::unitbuf;try {
    const Vocabulary vocabulary;
    {
        const auto previous=lang::state;std::size_t cases=0;
        for(int locale=0;locale<lang::language_count;++locale){lang::choose(locale);
            for(bool image:{false,true})for(bool project:{false,true})for(bool visible:{false,true})for(bool dirty:{false,true})for(bool compact:{false,true}) {
                char summary[512]{};qpx::ui::status_summary(summary,image,project,visible,dirty,1,1,128,96,687,1,0);
                const std::string text(summary);check(!text.empty() && text.front()!='|' && text.back()!='|',"Status has an outer separator");
                check(text.find(" |  | ")==std::string::npos,"Status has an empty part");
                if(!visible)check(text.find(lang::text("Objects hidden"))==0,"Hidden-object prefix lost");
                else if(image && !project)check(text.find("128 x 96 px")==0,"Image-only prefix changed");
                if(image)check(text.find("128 x 96 px")!=std::string::npos,"Image dimensions absent");
                char other[512]{};qpx::ui::status_summary(other,image,project,visible,dirty,1,1,128,96,687,1,0);check(text==other,"Compact layout changed summary content");(void)compact;++cases;
            }
        }
        lang::choose(0);char summary[512]{};qpx::ui::status_summary(summary,true,true,true,false,1,1,128,96,687,1,0);
        check(std::string(summary)=="1 of 1 images | 128 x 96 px | Zoom 687% | 1 objects | 0 selected","Status exact joined parts changed");
        char small[8]="123456";qpx::ui::status_part(small,"next");check(std::string(small)=="123456","Bounded join left a trailing separator");
        lang::state=previous;std::cout<<"Status summary: "<<cases<<" image/project/hidden/dirty/compact combinations across 16 languages; exact join and bounded atomic append passed\n";
    }
    if(argc==3 && std::string(argv[1])=="--lint-key") {vocabulary.inspect(argv[2]);return 0;}
    // Reading source keys too lets deliberate temporary catalogue violations
    // exercise the actual native rule without rebuilding a product per probe.
    {std::ifstream source(std::filesystem::path(__FILE__).parent_path().parent_path()/"gui_i18n_catalog.inc");
     check(bool(source),"Missing source catalogue");std::string line;
     while(std::getline(source,line))if(!line.empty() && line[0]=='{') {
         std::istringstream row(line.substr(1));std::string key;row>>std::quoted(key);check(bool(row),"Invalid source catalogue key");vocabulary.inspect(key);
     }}
    // The command table is a frontend header using ImGui types. Inspect its
    // caption field directly so this catalogue test stays independent of GL.
    {std::ifstream source(std::filesystem::path(__FILE__).parent_path().parent_path()/"gui_commands.hpp");
     check(bool(source),"Missing command table");std::string line;std::size_t captions=0;
     while(std::getline(source,line))if(line.find("CommandInfo{CommandId::")!=std::string::npos) {
         const auto comma=line.find(',');check(comma!=std::string::npos,"Invalid command row");
         std::istringstream row(line.substr(comma+1));std::string caption;row>>std::quoted(caption);check(bool(row),"Invalid command caption");vocabulary.inspect(caption);++captions;
     }check(captions>0,"No command captions inspected");}
    std::set<std::string> codes,keys;
    for(const auto& language:lang::languages) {check(codes.insert(language.code).second,"duplicate locale");check(*language.native_name && *language.english_name,"missing language name");}
    for(const auto& entry:lang::catalogue) {
        vocabulary.inspect(entry.english);
        check(keys.insert(entry.english).second,"duplicate translation key");
        for(const auto* translated:entry.translated) {
            check(translated && *translated,"missing translation");
            check(formats(entry.english)==formats(translated),"translation changed printf arguments");
        }
    }
    for(const auto* required:{"Choose an existing regular file","Choose an unused output filename","Export model file","Export pixel model file","Export model path","Open a project to add images."})check(keys.count(required)==1,"Path translation missing");
    for(const auto* required:{"Remove kept result","Remove kept result?","This cannot be undone. Save project then writes the removal to disk.",
        "This run is a recheck of kept result %llu.","Kept as record %llu.","This file is already in the project as %s.",
        "Kept as record %llu. Predict again to trace contours or add annotations.","Run batch"})
        check(keys.count(required)==1,"Pass A copy missing from the 15-language catalogue");
    for(const auto* required:{"Raw channels","Raw channels...","Hash file","Review for project","original channel samples","dark/flat-corrected samples","selected derived component/residual","all channels","The selected correction no longer matches this image. Choose it again in Dark/flat correction.","Training uses %zu labelled areas (%zu added by a classifier in this session).","No objects changed.","Applies to %zu cores.","Remove row %s (%zu cores)","Remove column %s (%zu cores)"})
        check(keys.count(required)==1,"Pass B copy missing from the 15-language catalogue");
    for(const auto* required:{"Fluorescence analysis needs a quantitative TIFF image.","Select an area annotation or turn off Use selected ROI","Choose an unmixing recipe in Linear unmixing, or turn off Use selected unmixing.","Tiled runs write masks and measurements to the folder above. They are not added to the project.","Open a project to run a batch.","Open an image to run a batch.","Enter an output folder.","Exported %llu rows to %s","Exported %zu objects to %s","Imported %zu objects from %s (Undo available)","Current image: %zu ROI / %zu tissue / %zu cell runs kept","Project: %zu ROI / %zu tissue / %zu cell runs kept","Exported measurements to %s"})
        check(keys.count(required)==1,"Pass C copy missing from the 15-language catalogue");
    for(const auto* required:{"Region","Tissue","Cells","Pixel classifier","Raw measurement","Pixel stream","Raw pixel file","Density","Neighborhood","Spatial distance","ROI plane:","Z %u / time %u","ROI plane: channel %d / Z %u / time %u","Changed %zu objects (Undo available)","Core %s-%s / %zu selected","Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction."})
        check(keys.count(required)==1,"Review caption missing from the 15-language catalogue");
    for(const auto* required:{"Browse","Previous files","All files","New folder name","Choose output folder","Choose folder","Choose an existing folder","Enter a folder name","Enter a filename","Choose a file path","Choose a folder path","Choose a path","Parent folder does not exist","Resolved path: %s","Invalid path text"})
        check(keys.count(required)==1,"Path caption missing from the 15-language catalogue");
    for(const auto* required:{"Export model file","Adding %zu of %zu","Using the first dropped path.","Image opening cancelled.","Clipboard","Export measurements CSV"})check(keys.count(required)==1,"Core path caption missing from the 15-language catalogue");
    for(const auto* required:{".geojson destination",".csv destination"})check(keys.count(required)==1,"Split output hint missing from the 15-language catalogue");
    for(const auto* required:{"Object classifiers","Export complete","Copy path","Open folder","Current image","Selected objects","Scope: %s / %llu rows","File fingerprint","Training checkpoint","Prediction file","Export QuPath grid","Export core metadata CSV","Export case summary CSV","Existing JSON destinations ask before replacement. If writing fails, choose a path and hash again."})check(keys.count(required)==1,"Output gate caption missing from the15-language catalogue");
    for(const auto* required:{"NumPy export preserves the rectangle, including pixels outside a nonrectangular ROI. Existing files ask before replacement.","Metadata CSV includes present, missing and vacant slots. It captures the grid when requested. Existing files ask before replacement."})check(keys.count(required)==1,"Overwrite-rule caption missing from the15-language catalogue");
    for(const auto* required:{"GeoJSON","Object measurements CSV","ROI measurements CSV","Tissue measurements CSV","Cell table CSV","Scope: %s / %llu objects","This file already exists and this export never replaces a file. Choose a new file name."})check(keys.count(required)==1,"Task2.3 fix1 caption missing from the15-language catalogue");
    check(lang::match_system_language("es_MX.UTF-8")==1,"Spanish region fallback");
    check(lang::match_system_language("pt_PT")==4,"Portuguese region fallback");
    check(lang::match_system_language("zh-Hant-HK")==7,"traditional script mapping");
    check(lang::match_system_language("zh-SG")==6,"simplified mapping");
    check(lang::match_system_language("unknown")==0,"unknown locale fallback");
    check(!lang::exact_language("not-a-language"),"invalid preference accepted");
    const auto initial=lang::state;
    {
        lang::MissTrace trace;lang::choose(2);
        lang::miss_trace=nullptr;lang::text("Unset trace caption");
        check(trace.keys.empty(),"Unset pointer recorded a miss");
        lang::miss_trace=&trace;lang::choose(0);lang::text("English trace caption");
        check(trace.keys.empty(),"English recorded a miss");
        lang::choose(2);
        for(const auto* key:{"","%s","%llu / %.5g","##identifier","123 / 45","—","×","%s —"})lang::text(key);
        check(trace.keys.empty(),"Non-caption recorded a miss");
#ifdef _WIN32
        check(lang::caption_key("é") && lang::caption_key("Ключ"),"Non-ASCII letters were not captions");
#endif
        for(const auto* token:{"RGB","X","Y","OME-Zarr","OpenSlide","PPM"})check(std::string(lang::text(token))==token,"Invariant notation translated");
        check(trace.keys.empty(),"Invariant data was recorded as a missing caption");
        lang::text("Missing trace caption");lang::text("Missing trace caption");
        check(trace.keys==std::set<std::string>{"Missing trace caption"},"Caption miss not deduplicated");
        lang::miss_trace=nullptr;lang::state=initial;
    }
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
