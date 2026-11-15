#pragma once
#include "gui_translation_trace.hpp"
#include "../gui_spatial_import.hpp"
namespace translation_checks {
using namespace qpx;
// Return the preliminary draw's large compiler frame before entering run_gui.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
inline void localized_frames(const std::filesystem::path& dir,const std::set<std::string>& already_drawn) {
    check(glfwInit(),"translation GLFW init");
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Translation coverage",nullptr,nullptr);check(window,"translation GL window");glfwMakeContextCurrent(window);
    {
        model_safety_checks::GlHost host(window);ui::i18n::choose(2);
        ImGui::GetStyle().FontSizeBase=32;
        // The retained import driver draws only English; the other fifteen
        // specialist windows already have localized GL scenes.
        detail::GuiSpatialImporter importer;importer.open();
        auto import_draw=[&]{importer.draw();};
        host.capture(dir/"spatial-import-fr200.ppm",import_draw);
        importer.close();
        // Registered frames not opened by the existing localized scene drivers.
        // Real owner bodies remain covered by those drivers where available.
        for(const auto& definition:ui::dialog_definitions) {
            if(already_drawn.count(std::string("dialog/")+definition.name))continue;
            std::cout<<"Additional localized dialog frame: "<<definition.name<<'\n';
            ui::DialogState state(true,false,ui::dialog_variants(definition.kind).front());
            auto draw=[&]{
                ImGui::Begin("Translation fixture");
                ui::OpenDialog(definition.kind);
                ui::RenderDialog(definition.kind,state,{}, {},[](ui::DialogControls&){},[](ui::DialogAction){return ui::DialogOutcome::Refused;});
                ImGui::End();
            };
            host.capture(dir/("dialog-"+std::to_string(static_cast<int>(definition.kind))+"-fr200.ppm"),draw);
            // Closing the fixture's popup does not dispatch a product action.
            ImGui::ClosePopupsOverWindow(nullptr,false);
        }
        for(int language:{10,14}) {
            ui::i18n::choose(language);ImGui::GetStyle().FontSizeBase=16;
            Document doc(Image::demo());const auto roi=doc.add_rectangle({0,0,20,20},"Literal image %s###data");
            const AnalysisRequest request(doc,roi,StainTransform({{1,0,0},{0,1,0}}),AnalysisKind::OpticalDensityMeans);
            const auto record=doc.keep_analysis(request,analyze(request));ui::RecordRows rows;
            rows.rows.push_back(ui::record_row(record,doc.annotation(roi).name,"Region",doc.analysis_status(record)));
            float color[3]={.2f,.3f,.4f};
            auto draw=[&]{
                ui::i18n::SurfaceTrace surface("pane/Workflow");
                ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({1300,1000});ImGui::Begin("Record wording");
                ui::draw_record_list("Kept results",rows,0);
                ui::TextUnformatted("Keep adds this result to the project. Save project writes it to disk.");
                ui::TextUnformatted("The current model has not been kept or saved.");
                ui::i18n::MissTrace data_trace;auto* previous=ui::i18n::miss_trace;ui::i18n::miss_trace=&data_trace;
                const auto color_label=ui::data_label("Literal class %s###name","class-color");
                ui::ColorEdit3(color_label.c_str(),color,0,true);
                check(data_trace.keys.empty(),"Class-color data entered translation lookup");
                ui::i18n::miss_trace=previous;
                if(const char* probe=std::getenv("QUPATHXX_I18N_PROBE"))ui::TextUnformatted(probe);
                ImGui::End();
            };
            host.capture(dir/(std::string(ui::i18n::languages[language].code)+"-records.ppm"),draw);
        }
    }
    glfwDestroyWindow(window);glfwTerminate();
}
inline void supplemental(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    {
        const auto language=ui::i18n::state;auto* previous=ui::i18n::surface_traces;
        ui::i18n::choose(0);ui::tool_window_label(ui::ToolId::PixelClassifier);
        ui::i18n::SurfaceTraces traces;ui::i18n::surface_traces=&traces;ui::i18n::choose(2);
        {ui::i18n::SurfaceTrace surface("tool/Pixel classifier");ui::tool_window_label(ui::ToolId::PixelClassifier);}
        {ui::i18n::SurfaceTrace surface("tool/Genes");ui::tool_window_label(ui::ToolId::Genes);ui::tool_window_label(ui::ToolId::Genes);}
        const auto* gene=ui::tool_definitions.at(static_cast<std::size_t>(ui::ToolId::Genes)).label;
        bool has_row=false;for(const auto& row:ui::i18n::catalogue)if(std::string_view(row.english)==gene)has_row=true;
        check(!traces["tool/Pixel classifier"].keys.count(gene),"Shared title cache attributed another window's caption");
        check(traces["tool/Genes"].keys.count(gene)==(has_row?0u:1u),"Cached requested title was not measured");
        ui::i18n::surface_traces=previous;ui::i18n::state=language;
        std::cout<<"Title cache: only the requested surface; cached hits measured, keys deduplicated\n";
    }
    std::set<std::string> already_drawn;
    if(const char* path=std::getenv("QUPATHXX_I18N_TRACE")) {
        std::ifstream file(path);std::string surface,key;
        while(file>>std::quoted(surface)>>std::quoted(key))already_drawn.insert(surface);
    }
    localized_frames(dir,already_drawn);
    for(std::size_t task=0;task<ui::workflow_task_keys.size();++task) {
        if(already_drawn.count(std::string("task/")+ui::workflow_task_keys[task]))continue;
        const std::string name=std::string("additional-")+ui::workflow_task_keys[task];
        auto project=fixture(dir/(name+".qpxproj"),128,96);project->document(1)->add_rectangle({0,0,20,20});
        auto o=::options(dir,name,"workflow",ui::workflow_task_keys[task],2);o.width=2200;o.height=1600;
        auto settings=read(o.layout_path);settings.replace(settings.find("language=en"),11,"language=fr");write(o.layout_path,settings);
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        const auto report=run_project_gui(project,18,(dir/(name+"-fr200.ppm")).u8string(),o);
        check(report.ui_overflow_items==0,"Additional localized task overflow");
        std::cout<<"Additional localized task: "<<ui::workflow_task_keys[task]<<'\n';
    }
    for(int language:{10,14}) {
        const std::string code=ui::i18n::languages[language].code;
        auto project=fixture(dir/(code+".qpxproj"),128,96);auto doc=project->document(1);
        const auto roi=doc->add_rectangle({0,0,20,20});const AnalysisRequest request(*doc,roi,StainTransform({{1,0,0},{0,1,0}}),AnalysisKind::OpticalDensityMeans);
        doc->keep_analysis(request,analyze(request));project->save(project->path());
        auto o=::options(dir,code+"-tables","workflow","tables");o.width=2200;o.height=1600;
        auto settings=read(o.layout_path);settings.replace(settings.find("language=en"),11,"language="+code);write(o.layout_path,settings);
        // The fixture just wrote this project; show that fact beside its kept
        // count without starting a second asynchronous operation.
        ui::NotificationsTrace notice;bool posted=false;
        notice.before_frame=[&](ui::Notifications& notifications){if(!posted){notifications.post({ui::Severity::Success,"Project","Saved",project->path(),true});posted=true;}};
        ui::notifications_trace=&notice;
        const auto report=run_project_gui(project,24,(dir/(code+"-tables.ppm")).u8string(),o);
        ui::notifications_trace=nullptr;
        check(report.ui_overflow_items==0,"Translation Tables overflow");
    }
}
}
