#pragma once
#include "../gui_tool_catalog.hpp"
#include "../gui_workflow_ui.hpp"
#include "qupathxx/tma.hpp"
#include "qupathxx/qptma.hpp"
static void tool_catalog_controls() {
    using T=ui::ToolId;
    // Explicit retained entry points: additions/removals require a coverage decision.
    const std::array expected={T::ObjectClassifier,T::PixelClassifier,T::RawPixelClassifier,T::Fluorescence,T::Spatial,T::Genes,T::Registration,T::SpatialImport,T::Batch,T::Tma,T::Density,T::Channels};
    check(expected.size()==ui::tool_definitions.size(),"tool coverage inventory changed");
    std::array<int,static_cast<std::size_t>(T::Count)> opened{};
    const auto action=[&](T id){return [&,id]{++opened[static_cast<std::size_t>(id)];};};
    const ui::ToolLauncher launcher({{T::ObjectClassifier,action(T::ObjectClassifier)},{T::PixelClassifier,action(T::PixelClassifier)},
        {T::RawPixelClassifier,action(T::RawPixelClassifier)},{T::Fluorescence,action(T::Fluorescence)},
        {T::Spatial,action(T::Spatial)},{T::Genes,action(T::Genes)},{T::Registration,action(T::Registration)},
        {T::SpatialImport,action(T::SpatialImport)},{T::Batch,action(T::Batch)},{T::Tma,action(T::Tma)},
        {T::Density,action(T::Density)},{T::Channels,action(T::Channels)}});
    for(auto id:expected)check(launcher.open(id,{true,true,true}),"retained tool lost its route");
    for(auto n:opened)check(n==1,"tool route called wrong controller");
    for(auto id:{T::Fluorescence,T::Genes,T::Registration,T::Tma,T::Density,T::Channels}) {
        check(!launcher.open(id,{}) && ui::tool_requirement(ui::tool_definition(id),{})!=nullptr,"unavailable tool must have a reason and refuse opening");
        check(opened[static_cast<std::size_t>(id)]==1,"disabled tool called its controller");
    }
    for(auto id:{T::ObjectClassifier,T::PixelClassifier,T::RawPixelClassifier,T::Spatial,T::SpatialImport,T::Batch})
        check(launcher.open(id,{}),"model/file tools must remain accessible without an image");
    bool missing=false,duplicate=false;
    try {ui::ToolLauncher absent({});}catch(const std::logic_error&){missing=true;}
    try {ui::ToolLauncher repeated({{T::Tma,action(T::Tma)},{T::Tma,action(T::Tma)}});}catch(const std::logic_error&){duplicate=true;}
    check(missing && duplicate,"tool routes must reject missing/duplicate handlers");
    check(ui::tool_matches(ui::tool_definition(T::Tma),"TMA") && ui::tool_matches(ui::tool_definition(T::Channels),"Zarr"),"tool search must cover abbreviations and quantitative inputs");
    for(const auto& child:ui::tool_children) {
        check(ui::tool_child_matches(child,child.label),"full nested feature name must be searchable");
        const auto prior=opened[static_cast<std::size_t>(child.parent)];
        check(launcher.open(child.id,{true,true,true}) && opened[static_cast<std::size_t>(child.parent)]==prior+1,"nested search must route into its existing native owner");
        if(child.parent==T::Fluorescence)check(!launcher.open(child.id,{}),"nested image tools must retain their parent context gate");
    }
    check(ui::tool_child_matches(ui::tool_children[1],"Dark"),"Dark must find Dark/flat correction");
    ui::i18n::Session language;
    ui::i18n::choose(*ui::i18n::exact_language("fr"));
    check(ui::tool_matches(ui::tool_definition(T::Density),"densit"),"localized tool names must be searchable");
    for(int language=1;language<ui::i18n::language_count;++language) {
        ui::i18n::choose(language);
        for(const auto& child:ui::tool_children) {
            const std::string translated=ui::i18n::text(child.label);
            check(translated!=child.label && ui::tool_child_matches(child,translated),"nested editors need searchable localized names in every language");
        }
        for(auto id:{T::Fluorescence,T::Density,T::Channels}) {
            const auto* reason=ui::tool_requirement(ui::tool_definition(id),{});
            check(std::string(ui::i18n::text(reason))!=reason,"new tool requirements need translations in every supported language");
        }
    }
    ui::i18n::choose(0);
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,900};io.DeltaTime=1.f/60;
    io.ConfigInputTrickleEventQueue=false;io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
    char search[128]{};std::size_t matching=0;ImVec2 hit{};
    ui::ToolContext context{true,true,true};
    auto frame=[&](float width=336,float scale=1) {
        ImGui::GetStyle().FontSizeBase=16*scale;ImGui::NewFrame();ui::begin_frame();
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({width,850});ImGui::Begin("Tool coverage",nullptr,ImGuiWindowFlags_NoSavedSettings);
        matching=ui::ToolCatalog(launcher,context,search,sizeof(search));
        check(ui::CollectionHeight(2)<ui::CollectionHeight(200) && ui::CollectionHeight(200)<850*.4f,"short results should shrink while long collections remain bounded");
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();hit={(lo.x+hi.x)/2,(lo.y+hi.y)/2};
        check(ui::layout_stats.overflow_items==0,"tool catalogue overflows narrow/scaled panel");ImGui::End();ImGui::Render();
    };
    frame();frame();check(matching==12,"all tools must be visible without opening another combo");
    context={};frame();check(matching==12,"disabled/conditional tools must remain discoverable");
    std::strcpy(search,"TMA");context={true,true,true};frame();frame();check(matching==1,"TMA filter must retain exactly its route");
    const auto before=opened[static_cast<std::size_t>(T::Tma)];
    io.AddMousePosEvent(hit.x,hit.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    check(opened[static_cast<std::size_t>(T::Tma)]==before+1,"visible tool row did not invoke its controller");
    std::strcpy(search,"no such tool");frame();check(matching==0,"empty tool search must report no matches");
    std::strcpy(search,"Dark");frame();check(matching==1,"visible search must expose the previously missing correction editor");
    search[0]=0;ui::i18n::choose(*ui::i18n::exact_language("fr"));frame(620,2);check(matching==12,"scaled localized catalogue lost tools");
    ImGui::GetStyle().FontSizeBase=16;
    float fitted=0;
    auto fit_frame=[&](bool resize=false) {
        ImGui::NewFrame();ui::SetNextToolWindow("Content fit test",{440,800},ui::ToolHeight::Content);
        if(resize)ImGui::SetNextWindowSize({440,520},ImGuiCond_Always);
        ImGui::Begin("Content fit test");ui::TextUnformatted("Short tool");ui::Button("Example action");fitted=ImGui::GetWindowHeight();ImGui::End();ImGui::Render();
    };
    for(int i=0;i<5;++i)fit_frame();check(fitted>=160 && fitted<300,"short tool must fit its content on first open");
    fit_frame(true);for(int i=0;i<3;++i)fit_frame();check(fitted==520,"content fit must retain an explicit user size afterward");
    std::size_t ini_size=0;const auto* ini=ImGui::SaveIniSettingsToMemory(&ini_size);const std::string saved(ini,ini_size);
    ImGui::DestroyContext();ImGui::CreateContext();auto& restored_io=ImGui::GetIO();restored_io.IniFilename=nullptr;restored_io.DisplaySize={800,900};restored_io.DeltaTime=1.f/60;
    restored_io.Fonts->AddFontDefault();restored_io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
    ImGui::LoadIniSettingsFromMemory(saved.data(),saved.size());for(int i=0;i<3;++i)fit_frame();check(fitted==520,"content fit must preserve saved size in a new session");
    ImGui::DestroyContext();std::cout<<"Tool coverage: 12 retained routes, context reasons, visible/filter/empty/translated-200% catalogue, complete handlers, first-fit/manual/reopened size passed\n";
}
static void tool_catalog_workflows(const std::filesystem::path& directory) {
    for(const std::string mode:{"hide","reopen","floating"}) {
        const auto project=fixture(directory/("lifecycle-"+mode+".qpxproj"),128,96);
        auto o=options(directory,"lifecycle-"+mode,"image","tools");
        const auto before=read(std::filesystem::u8path(project->path()));const auto revision=project->document(1)->revision();
        if(mode=="floating") {
            // Start with the real application's dock host ID; the headless
            // geometry fixture deliberately uses a different parent window.
            run_project_gui(project,4,{},o);
            auto ini=read(std::filesystem::u8path(o.layout_path));const auto start=ini.find("[Window][Image and display]");
            check(start!=std::string::npos,"actual Image preference fixture");const auto end=ini.find("\n[",start+1);
            ini.replace(start,end==std::string::npos?ini.size()-start:end-start,"[Window][Image and display]\nPos=520,120\nSize=336,390\nCollapsed=0\n");
            write(std::filesystem::u8path(o.layout_path),ini);
        }
        else {
            // The actual RmlUI activity rail, using ordinary mouse input.
            GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=23./o.width;down.y=98./o.height;
            auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
            if(mode=="reopen") {down.frame=9;up.frame=10;o.inputs.insert(o.inputs.end(),{down,up});}
        }
        const auto r=run_project_gui(project,18,(directory/("lifecycle-"+mode+".ppm")).u8string(),o);
        check(r.replay_complete && r.canvas_visible && !r.ui_overflow_items,"actual inspector lifecycle overflow/replay: "+mode);
        if(mode=="hide")check(r.display_bounds.width==0 && r.browser_bounds.width==0 && r.canvas_bounds.width>650,"activity click must hide left group and reclaim the actual viewer");
        if(mode=="reopen")check(r.display_bounds.width>200 && r.display_bounds.x<r.canvas_bounds.x,"second activity click must reveal the actual Image inspector on the left");
        if(mode=="floating")check(r.display_bounds.x>r.canvas_bounds.x && r.display_bounds.width>200 && r.display_bounds.height<700,"actual floating Image must fit its content with one title");
        check(before==read(std::filesystem::u8path(project->path())) && revision==project->document(1)->revision(),"inspector lifecycle changed scientific/project data");
    }
    for(const bool scaled:{false,true}) {
        const auto name=scaled?"tools-fr-2x":"tools";
        const auto project=fixture(directory/(std::string(name)+".qpxproj"),128,96);
        auto o=options(directory,name,"workflow","tools",scaled?2:1);
        if(scaled) {auto ini=read(std::filesystem::u8path(o.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(std::filesystem::u8path(o.layout_path),ini);}
        const auto before=read(std::filesystem::u8path(project->path()));const auto revision=project->document(1)->revision();
        const auto r=run_project_gui(project,14,(directory/(std::string(name)+".ppm")).u8string(),o);
        check(r.canvas_visible && r.replay_complete && r.ui_overflow_items==0,"visible tool catalogue must fit and retain the viewer");
        check(before==read(std::filesystem::u8path(project->path())) && revision==project->document(1)->revision(),"tool navigation changed project data");
        if(scaled)check(read(std::filesystem::u8path(o.layout_path)).find("; qupathxx language=fr\n")!=std::string::npos,"scaled tool capture must actually use French");
    }
    for(const std::string mode:{"case","geometry","metadata","qptma","qptma-unacknowledged"}) {
        const auto project=fixture(directory/("tma-"+mode+".qpxproj"),128,96);const auto doc=project->document(1);
        TmaGridOptions grid;grid.rows=2;grid.columns=3;grid.origin={24,24};grid.row_step={0,32};grid.column_step={32,0};grid.width=grid.height=20;
        const auto cores=doc->create_tma_grid(grid);project->save(project->path());const auto original=doc->annotation(cores.front()).geometry.bounds();
        auto o=options(directory,"tma-"+mode,"workflow","tools");o.tma_open=true;o.tma_case_id="Review case";o.tma_core_bounds=Rect{18,19,22,24};
        const auto output=directory/"tma-metadata.csv";if(mode=="metadata"){o.tma_tab=2;o.tma_metadata_path=output.u8string();}
        GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=mode=="case"?GuiInputTarget::ApplyTmaCase:mode=="geometry"?GuiInputTarget::ApplyTmaGeometry:GuiInputTarget::ExportTmaMetadata;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs={down,up};
        const auto qptma_output=directory/("tma-"+mode+".qptma");
        if(mode=="qptma" || mode=="qptma-unacknowledged") {
            o.qptma_output_path=qptma_output.u8string();o.inputs.clear();
            if(mode=="qptma") {
                down.target=GuiInputTarget::AcknowledgeQptmaLosses;up.target=down.target;o.inputs={down,up};
                down.frame=10;up.frame=11;down.target=up.target=GuiInputTarget::ExportQptma;o.inputs.insert(o.inputs.end(),{down,up});
            }
        }
        if(mode=="geometry") {
            // At the 800px test viewport the existing grid/export controls
            // require a normal window scroll to reach the geometry action.
            GuiInput move;move.frame=2;move.kind=GuiInputKind::Move;move.target=GuiInputTarget::Window;move.x=.5;move.y=.73;
            auto wheel=move;wheel.frame=3;wheel.kind=GuiInputKind::Wheel;wheel.wheel=-6;
            o.inputs.insert(o.inputs.begin(),{move,wheel});
        }
        if(mode=="metadata") {
            // Native omission details can leave only a sliver of Export CSV
            // visible. Scroll over the measured TMA body before clicking the
            // target's centre; IsItemVisible alone allows partial visibility.
            struct Probe {
                ui::WorkflowTrace trace;
                ui::WorkflowTrace* previous=ui::workflow_trace;
                Probe(){ui::workflow_trace=&trace;}
                ~Probe(){ui::workflow_trace=previous;}
            } probe;
            Rect body{};
            probe.trace.before_frame=[&](int) {
                const auto name=ui::i18n::label("Tissue microarray");
                if(const auto* window=ImGui::FindWindowByName(name.c_str());window && window->WasActive) {
                    const auto rect=window->InnerClipRect;
                    body={rect.Min.x,rect.Min.y,rect.GetWidth(),rect.GetHeight()};
                }
            };
            auto placement=o;placement.inputs.clear();
            const auto before=read(project->path());const auto revision=doc->revision();
            const auto measured=run_project_gui(project,4,{},placement);
            check(measured.replay_complete && body.width>0 && body.height>0 && read(project->path())==before && doc->revision()==revision,"TMA metadata placement probe changed data or failed to measure its native body");
            GuiInput move;move.frame=2;move.kind=GuiInputKind::Move;move.target=GuiInputTarget::Window;
            move.x=(body.x+body.width/2)/o.width;move.y=(body.y+body.height/2)/o.height;
            auto wheel=move;wheel.frame=3;wheel.kind=GuiInputKind::Wheel;wheel.wheel=-6;
            o.inputs.insert(o.inputs.begin(),{move,wheel});
        }
        const auto saved=read(std::filesystem::u8path(project->path()));const auto r=run_project_gui(project,28,(directory/("tma-"+mode+".ppm")).u8string(),o);
        check(r.replay_complete && r.tma_error.empty() && r.ui_overflow_items==0,"content-fit TMA lost an existing action: "+mode+" / "+r.tma_error);
        if(mode=="case") {check(doc->annotation(cores.front()).tma_core->case_id()=="Review case","TMA case edit missing");check(doc->undo() && doc->annotation(cores.front()).tma_core->case_id().empty(),"TMA case edit undo");}
        if(mode=="geometry") {const auto bounds=doc->annotation(cores.front()).geometry.bounds();check(bounds.x==18 && bounds.y==19 && bounds.width==22 && bounds.height==24,"TMA geometry edit missing");check(doc->undo() && doc->annotation(cores.front()).geometry.bounds().x==original.x,"TMA geometry undo");}
        if(mode=="metadata")check(r.tma_export_state=="saved" && read(output)==tma_metadata_csv(*doc->tma_grid()) && r.tma_cores==6,"TMA metadata export differs from exact native output");
        if(mode=="qptma")check(r.qptma_state=="saved" && read(qptma_output)==qptma_text(qptma_from_grid(*doc->tma_grid(),doc->server()->source_locator(),doc->tma_grid()->info()->name()).data),"regrouped QPTMA export must preserve exact native output and replay compatibility");
        if(mode=="qptma-unacknowledged")check(!std::filesystem::exists(qptma_output) && r.qptma_state=="idle","QPTMA export still requires explicit loss acknowledgment");
        check(saved==read(std::filesystem::u8path(project->path())),"TMA tool action unexpectedly saved the project");
    }
    std::cout<<"Tool catalogue normal/French-200% views preserve project data and visible viewer\n";
}
