#pragma once
#include "gui_layout_recorder.hpp"
#include "gui_layout_scene_driver.hpp"
namespace layout_checks {
inline void scenes(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    for(int language:{0,2})for(int section=0;section<3;++section) {
        const auto name=std::string("settings-")+std::to_string(section)+(language==2?"-fr200":"-en200");
        auto project=fixture(dir/(name+".qpxproj"),128,96);
        auto initial=::options(dir,name,"project","region",2);initial.width=2200;initial.height=1600;
        if(language==2){auto text=read(initial.layout_path);text.replace(text.find("language=en"),11,"language=fr");write(initial.layout_path,text);}
        qpx::ui::LayoutTrace local;auto* previous=qpx::ui::layout_trace;
        if(!previous)qpx::ui::layout_trace=&local;
        qpx::ui::layout_trace->project_settings=[&](bool& visible,int& selected){visible=true;selected=section;
            if(auto* window=ImGui::FindWindowByName(qpx::ui::i18n::label("Project settings").c_str())) {ImGui::SetWindowPos(window,{300,60});ImGui::SetWindowSize(window,{1500,1450});}};
        const auto report=run_project_gui(project,12,(dir/(name+".ppm")).u8string(),initial);
        qpx::ui::layout_trace->project_settings={};qpx::ui::layout_trace=previous;
        check(report.ui_overflow_items==0,"Layout settings overflow: "+report.ui_overflow_label);
    }
    layout_scenes::run(dir);
}
inline void translation_round(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    for(int language:{2,10})for(const std::string surface:{"settings-0","settings-1","settings-2","project","image","annotations","workflow","region","cells"}) {
        const auto name="l3-"+surface+"-"+qpx::ui::i18n::languages[language].code+"200";
        auto project=fixture(dir/(name+".qpxproj"),128,96);project->document(1)->add_rectangle({5,5,20,20});
        const bool settings=surface.rfind("settings-",0)==0;
        const auto pane=settings?"project":(surface=="region" || surface=="cells")?"workflow":surface.c_str();
        auto initial=::options(dir,name,pane,surface=="cells"?"cells":"region",2);initial.width=2400;initial.height=1800;
        initial.inputs={key(3,GuiInputKind::SelectAllKey)};
        auto text=read(initial.layout_path);text.replace(text.find("language=en"),11,std::string("language=")+qpx::ui::i18n::languages[language].code);write(initial.layout_path,text);
        qpx::ui::LayoutTrace local;auto* previous=qpx::ui::layout_trace;if(!previous)qpx::ui::layout_trace=&local;
        const auto saved=qpx::ui::layout_trace->project_settings;
        qpx::ui::layout_trace->project_settings=[&](bool& visible,int& selected){visible=settings;if(settings){selected=surface.back()-'0';if(auto* window=ImGui::FindWindowByName(qpx::ui::i18n::label("Project settings").c_str())){ImGui::SetWindowPos(window,{300,60});ImGui::SetWindowSize(window,{1750,1650});}}};
        const auto report=run_project_gui(project,16,(dir/(name+".ppm")).u8string(),initial);
        qpx::ui::layout_trace->project_settings=saved;qpx::ui::layout_trace=previous;
        check(report.ui_overflow_items==0,"L3 localized capture overflow: "+name+" / "+report.ui_overflow_label);
        std::cout<<"L3 localized surface: "<<name<<" overflow0\n";
    }
}
inline void lint_files(const std::filesystem::path& dir) {
    const auto allowed=vocabulary();check(!allowed.empty(),"Icon vocabulary missing");
    std::set<std::string> errors;std::set<std::pair<std::string,int>> coverage;std::size_t items=0,files=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(dir))if(entry.path().extension()==".layout") {
        ++files;std::ifstream file(entry.path());std::string tag;
        while(file>>tag) {
            if(tag=="item") {
                std::string surface,window,command,caption;int kind,icon,language;unsigned region;float x0,y0,x1,y1,scale;
                file>>std::quoted(surface)>>std::quoted(window)>>kind>>icon>>std::quoted(command)>>std::quoted(caption)>>region>>x0>>y0>>x1>>y1>>language>>scale;
                check(bool(file),"Malformed rendered item: "+entry.path().u8string());++items;
                if(std::abs(scale-2.f)<.001f && (language==0 || language==2))coverage.emplace(surface,language);
                if(kind==int(qpx::ui::LayoutKind::Icon) || kind==int(qpx::ui::LayoutKind::Palette))
                    if(!allowed.count({icon,command}))errors.insert("P8 icon-meaning "+surface+" / "+command);
            } else if(tag=="error") {
                std::string principle,message,surface,window,command,caption;
                file>>principle>>message>>std::quoted(surface)>>std::quoted(window)>>std::quoted(command)>>std::quoted(caption);
                check(bool(file),"Malformed rendered violation");errors.insert(principle+" "+message+" / "+surface+" / "+window+" / "+command+" / "+caption);
            } else throw std::runtime_error("Unknown rendered trace item: "+tag);
        }
    }
    std::ifstream fixture(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/gui-layout-surfaces.tsv");std::string surface;
    check(bool(fixture),"Layout surface fixture missing");std::size_t surfaces=0;
    while(fixture>>std::quoted(surface)) {++surfaces;for(int language:{0,2})if(!coverage.count({surface,language}))errors.insert("Missing 200% layout surface: "+surface+(language==0?" / en":" / fr"));}
    for(const auto& line:errors)std::cout<<line<<'\n';
    std::cout<<"Layout lint: "<<files<<" rendered trace files, "<<items<<" items, "<<surfaces<<" required surfaces in English/French 200%, "<<errors.size()<<" violations\n";
    check(files>0 && items>0 && surfaces>0,"No rendered layout observations");check(errors.empty(),"Rendered layout lint failed");
}
inline void identities() {
    simplification_tools_checks::NativeUi host;char text[64]="Literal value";int number=3;double value=.25;
    for(int language:{0,2}) {
        qpx::ui::i18n::choose(language);ImGui::GetStyle().FontSizeBase=32;
        auto draw=[&]{ImGui::SetNextWindowSize({1200,900});ImGui::Begin("Legacy layout identities");
            const auto field=[&](const char* original,auto native) {const auto expected=ImGui::GetID(original);
                qpx::ui::legacy_field("Existing caption",original,[&](const char* id){check(ImGui::GetID(id)==expected,"Legacy field replay identity changed");return native(id);},true);};
            field("Class name",[&](const char* id){return ImGui::InputTextWithHint(id,"Name",text,sizeof(text));});
            field("Alpha byte",[&](const char* id){return ImGui::InputInt(id,&number);});
            field("##density-class-0",[&](const char* id){return ImGui::InputTextMultiline(id,text,sizeof(text),{0,80});});
            field("Numeric value",[&](const char* id){return ImGui::InputDouble(id,&value);});
            const auto combo_id=ImGui::GetID("Registry");
            {qpx::ui::LegacyComboScope combo{"Registry","Registry","Value",true};check(!combo && ImGui::GetItemID()==combo_id,"Legacy combo replay identity changed");}
            const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
            qpx::ui::field_with_actions("Grouped field",2,[&](const char* id){return ImGui::InputInt(id,&number);},[&](qpx::ui::ActionRow& row){
                row.next();qpx::ui::IconButton("Previous block",qpx::ui::Icon::Left);
                const auto first=ImGui::GetItemRectMin();row.next();qpx::ui::IconButton("Next block",qpx::ui::Icon::Right);
                check(std::abs(ImGui::GetItemRectMin().y-first.y)<.5f && std::abs(ImGui::GetItemRectMax().x-right)<.5f,"Grouped field icons must share one line and content edge");
            });
            ImGui::End();};host.settle(draw);
    }
    check(std::string(text)=="Literal value" && number==3 && value==.25,"Layout-only field adapters changed values");
    std::cout<<"Legacy text/integer/double/multiline/combo exact IDs and values: English/French 200% passed\n";
}
}
