#pragma once
#include "../gui_commands.hpp"
#include "../gui_theme.hpp"
#include "../gui_shell.hpp"
#include "../gui_batch.hpp"
#include "../gui_spatial.hpp"
#include "../gui_raw.hpp"
#include <set>

namespace command_checks {
inline void rendered_text(const char* text,ImVec2 position,const ImVec4* fine_clip=nullptr,ImU32 color=0) {
    if(!*text)return;
    const auto* actual=ImGui::GetWindowDrawList();ImDrawList expected(ImGui::GetDrawListSharedData());expected._ResetForNewFrame();
    const auto clip=actual->_CmdHeader.ClipRect;expected.PushClipRect({clip.x,clip.y},{clip.z,clip.w});expected.PushTexture(actual->_CmdHeader.TexRef);
    expected.AddText(ImGui::GetFont(),ImGui::GetFontSize(),position,color?color:ImGui::GetColorU32(ImGuiCol_Text),text,nullptr,0,fine_clip);
    check(expected.VtxBuffer.Size>0,"Expected table text has no visible glyphs");bool found=false;
    for(int at=0;at+expected.VtxBuffer.Size<=actual->VtxBuffer.Size && !found;++at) {
        found=true;for(int j=0;j<expected.VtxBuffer.Size;++j) {
            const auto& a=actual->VtxBuffer[at+j];const auto& e=expected.VtxBuffer[j];
            if(a.pos.x!=e.pos.x || a.pos.y!=e.pos.y || a.uv.x!=e.uv.x || a.uv.y!=e.uv.y || a.col!=e.col) {found=false;break;}
        }
    }
    check(found,"Rendered glyphs differ from table text: "+std::string(text));
}
inline void keys() {
    simplification_tools_checks::NativeUi host;auto& io=ImGui::GetIO();io.DeltaTime=.1f;
    ImGui::NewFrame();ImGui::Begin("Activity identity");
    for(int i=0;i<4;++i) {
        const auto expected=ImGui::GetCurrentWindow()->GetID(GuiWorkspace::panes[i]);
        ui::ActivityButton(GuiWorkspace::activity_label(i),ui::Icon::Project,false,ui::command_chord(ui::shell_commands[i]));
        check(ImGui::GetItemID()==expected,"Table-backed activity caption changed a legacy widget ID");
    }
    for(int i=0;i<8;++i) {
        ImGui::PushID(i);const auto expected=ImGui::GetCurrentWindow()->GetID("##tool");ImGui::PopID();
        ui::tool_button(i,0,ui::shell_commands[4+i]);
        check(ImGui::GetItemID()==expected,"Table-backed tool button changed a legacy widget ID");
    }
    for(const auto id:{ui::CommandId::Shell_Undo,ui::CommandId::Shell_Redo}) {
        const auto expected=ImGui::GetCurrentWindow()->GetID("##identity");
        ui::icon_action("##identity",ui::Icon::Undo,id);
        check(ImGui::GetItemID()==expected,"Command-keyed icon changed a legacy widget ID");
    }
    ImGui::End();ImGui::Render();
    auto frame=[&](ui::CommandId id,ui::ChordContext context) {
        ImGui::NewFrame();const bool fired=ui::chord_pressed(id,context);ImGui::EndFrame();return fired;
    };
    for(const auto& row:ui::command_table)if(row.chord) {
        const auto key=static_cast<ImGuiKey>(row.chord&~ImGuiMod_Mask_);
        auto release=[&] {io.AddKeyEvent(key,false);for(auto mod:{ImGuiMod_Ctrl,ImGuiMod_Alt,ImGuiMod_Shift,ImGuiMod_Super})io.AddKeyEvent(mod,false);frame(row.id,{});};
        auto press=[&](ImGuiKeyChord mods) {for(auto mod:{ImGuiMod_Ctrl,ImGuiMod_Alt,ImGuiMod_Shift,ImGuiMod_Super})io.AddKeyEvent(mod,(mods&mod)!=0);io.AddKeyEvent(key,true);};
        release();press(row.chord);check(!frame(row.id,{true,false,true}),"Chord escaped modal");
        release();press(row.chord);check(frame(row.id,{false,true,true})==(row.scope==ui::KeyScope::Global),"Chord scope disagrees with row");
        for(auto extra:{ImGuiMod_Ctrl,ImGuiMod_Alt,ImGuiMod_Shift,ImGuiMod_Super}) {
            release();press(row.chord^extra);check(!frame(row.id,{false,false,true}),"Chord accepted wrong modifiers");
        }
        release();press(row.chord);int count=0;for(int i=0;i<10;++i)count+=frame(row.id,{false,false,true});
        check(row.repeat?count>1:count==1,"Chord repeat disagrees with row: "+std::string(row.legacy_id));release();
    }
    std::cout<<"Command chords: every row, exact modifier toggles, dialog/text scope, ten held frames passed\n";
}
inline ui::ToolLauncher launcher(const std::function<void(ui::ToolId)>& open,const std::function<void(ui::ToolChild)>& child) {
    using T=ui::ToolId;using C=ui::ToolChild;
    return ui::ToolLauncher({{T::ObjectClassifier,[=]{open(T::ObjectClassifier);}},{T::PixelClassifier,[=]{open(T::PixelClassifier);}},
        {T::RawPixelClassifier,[=]{open(T::RawPixelClassifier);}},{T::Fluorescence,[=]{open(T::Fluorescence);}},
        {T::Spatial,[=]{open(T::Spatial);}},{T::Genes,[=]{open(T::Genes);}},{T::Registration,[=]{open(T::Registration);}},
        {T::SpatialImport,[=]{open(T::SpatialImport);}},{T::Batch,[=]{open(T::Batch);}},{T::Tma,[=]{open(T::Tma);}},
        {T::Density,[=]{open(T::Density);}},{T::Channels,[=]{open(T::Channels);}}},
        {{C::MarkerRules,[=]{child(C::MarkerRules);}},{C::DarkFlat,[=]{child(C::DarkFlat);}},{C::Unmixing,[=]{child(C::Unmixing);}},{C::SavedStreams,[=]{child(C::SavedStreams);}}});
}
inline void readers(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);
    const auto tools=launcher([](ui::ToolId){},[](ui::ToolChild){});
    const ui::ToolContext context{true,true,true};
    for(int language:{0,*ui::i18n::exact_language("fr")}) {
        ui::i18n::choose(language);ImGui::GetStyle().FontSizeBase=language?32:16;
        const std::string tag=language?"fr-200":"en";
        auto begin=[&](const char* title) {ImGui::SetNextWindowPos({30,30});ImGui::SetNextWindowSize({1500,1450});ImGui::Begin(title,nullptr,ImGuiWindowFlags_NoSavedSettings);};
        // Invoke the actual header item reader one at a time and inspect its
        // localized visible text and actual trailing-column argument.
        for(std::size_t i=0;i<ui::header_commands.size();++i) {
            auto draw=[&] {begin("Header row");ImGui::LogToBuffer();ui::HeaderTrace trace;
                const auto id=static_cast<ui::HeaderCommand>(i);ui::header_detail::item(id,[](ui::HeaderCommand){},&trace);
                const auto& row=ui::command(ui::command_id(id));
                check(trace.shown.at(id)==std::make_pair(std::string(ui::i18n::text(row.caption)),std::string(ui::command_chord(row.id))),"Rendered header row differs from table");
                check(std::string(host.context->LogBuffer.c_str()).find(ui::i18n::text(row.caption))!=std::string::npos,"Header caption not rendered");ImGui::LogFinish();ImGui::End();};
            host.settle(draw);
        }
        for(bool desktop:{false,true}) {
            auto draw=[&] {begin("File");ui::HeaderSnapshot snapshot;snapshot.desktop=desktop;ui::HeaderTrace trace;
                ui::header_detail::file(snapshot,[](ui::HeaderCommand){},&trace);
                check(trace.shown.at(ui::HeaderCommand::Close).first==ui::i18n::text(ui::command(desktop?ui::CommandId::DesktopExit:ui::CommandId::Header_Close).caption),"Desktop/embedded close caption");ImGui::End();};
            host.capture(dir/("t41-file-"+tag+(desktop?"-desktop":"-embedded")+".ppm"),draw);
        }
        auto catalogue=[&] {begin("Tool catalogue");char search[1]{};ui::ToolCatalogTrace trace;
            check(ui::ToolCatalog(tools,context,search,sizeof(search),&trace)==ui::tool_definitions.size(),"Catalogue row missing");
            for(const auto& tool:ui::tool_definitions) {const auto& row=ui::command(ui::command_id(tool.id));
                check(std::string(tool.label)==row.caption && std::string(tool.shortcut)==ui::command_chord(row.id),"Catalogue definition differs from table");
                const auto bounds=trace.rows.at(row.id);const float trailing=*tool.shortcut?ImGui::CalcTextSize(tool.shortcut).x+ui::design::spacing.md*ui::design::scale():0;
                const float y=float(bounds.y)+(float(bounds.height)-ImGui::GetFontSize())*.5f;
                const ImVec4 clip{float(bounds.x),float(bounds.y),float(bounds.x+bounds.width)-std::max(1.f,trailing),float(bounds.y+bounds.height)};
                rendered_text(ui::i18n::text(row.caption),{float(bounds.x),y},&clip);
                if(*tool.shortcut)rendered_text(tool.shortcut,{float(bounds.x+bounds.width)-trailing+ui::design::spacing.sm*ui::design::scale(),y},nullptr,ImGui::GetColorU32(ImGuiCol_TextDisabled));}
            ImGui::End();};
        host.capture(dir/("t41-catalogue-"+tag+".ppm"),catalogue);
        auto toolbar=[&] {begin("Toolbar");ImGui::LogToBuffer();for(int i=0;i<8;++i) {
            const auto id=ui::shell_commands[4+i];ui::tool_button(i,0,id);ImGui::SameLine();}
            ImGui::NewLine();const auto& labels=ui::shell_labels();
            for(std::size_t i=0;i<labels.size();++i) {
                const auto id=ui::shell_commands[i];std::string expected=ui::i18n::text(ui::command(id).caption);
                if(*ui::command_chord(id))expected+=" ("+std::string(ui::command_chord(id))+")";
                check(labels[i]==expected,"Shell label differs from table");ImGui::TextUnformatted(labels[i].c_str());
            }ImGui::LogFinish();ImGui::End();};
        host.capture(dir/("t41-toolbar-shell-labels-"+tag+".ppm"),toolbar);
    }
    ui::i18n::choose(0);
    std::cout<<"Rendered header, catalogue, toolbar and shell labels: English/French200, overflow0 passed\n";
}
inline void layouts(const std::filesystem::path& dir) {
    // Read the exact old title spelling, serialize its ImGui settings, and then
    // restore through the new definition title in a fresh context.
    const auto file=dir/"t41-legacy-tools.ini";
    std::array<ImVec2,16> expected_pos{},expected_size{};std::array<ImGuiID,16> expected_id{},expected_dock{};
    {
        simplification_tools_checks::NativeUi host;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;
        for(int frame=0;frame<5;++frame) {ImGui::NewFrame();const auto dock=ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport());
            if(frame==0) {ImGuiID left=0,right=0;ImGui::DockBuilderSplitNode(dock,ImGuiDir_Left,.45f,&left,&right);
                for(std::size_t i=0;i<16;++i)ImGui::DockBuilderDockWindow(ui::tool_legacy_titles[i],i%2?left:right);ImGui::DockBuilderFinish(dock);}
            for(std::size_t i=0;i<16;++i) {const auto* old=ui::tool_legacy_titles[i];
                ImGui::Begin(old);auto* win=ImGui::GetCurrentWindow();expected_pos[i]=win->Pos;expected_size[i]=win->Size;expected_id[i]=win->ID;expected_dock[i]=win->DockId;ImGui::End();}ImGui::Render();}
        ImGui::SaveIniSettingsToDisk(file.u8string().c_str());
    }
    {
        simplification_tools_checks::NativeUi host;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::LoadIniSettingsFromDisk(file.u8string().c_str());
        for(int frame=0;frame<5;++frame) {ImGui::NewFrame();ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport());for(std::size_t i=0;i<16;++i) {
            ImGui::Begin(ui::tool_window_label(i));const auto* win=ImGui::GetCurrentWindow();
            if(frame==4)check(win->ID==expected_id[i] && win->DockId==expected_dock[i] && expected_dock[i]!=0 && win->Pos.x==expected_pos[i].x && win->Pos.y==expected_pos[i].y && win->Size.x==expected_size[i].x && win->Size.y==expected_size[i].y,"Legacy tool layout moved: "+std::string(ui::tool_legacy_titles[i]));ImGui::End();}ImGui::Render();}
    }
    {
        const auto root=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        const auto retained=root/"build/ui-workflow-safety/m3/review-fix2-final-workflow-safety/modal-chords.ini";
        const auto saved=read(retained);check(!saved.empty(),"Retained pre-task layout missing");write(dir/"t41-retained-pre-task.ini",saved);
        simplification_tools_checks::NativeUi host;ImGui::LoadIniSettingsFromMemory(saved.c_str(),saved.size());
        std::map<std::size_t,std::pair<ImVec2,ImVec2>> positions;
        for(std::size_t i=0;i<16;++i)if(const auto* settings=ImGui::FindWindowSettingsByID(ImHashStr(ui::tool_legacy_titles[i]))) {
            check(settings->DockId==0,"Retained tool unexpectedly docked; restore its host before asserting placement");
            positions[i]={{float(settings->Pos.x),float(settings->Pos.y)},{float(settings->Size.x),float(settings->Size.y)}};
        }
        check(!positions.empty(),"Retained pre-task file contains no tool windows");
        for(int frame=0;frame<3;++frame) {ImGui::NewFrame();for(const auto& [i,expected]:positions) {
            ImGui::Begin(ui::tool_window_label(i));const auto* win=ImGui::GetCurrentWindow();
            check(win->Pos.x==expected.first.x && win->Pos.y==expected.first.y && win->Size.x==expected.second.x && win->Size.y==expected.second.y,"Actual pre-task layout moved a tool window");ImGui::End();}ImGui::Render();}
    }
    std::cout<<"All 16 old-title IDs/docks/positions/sizes restored; actual retained pre-task layout also restores every contained tool\n";
}
inline void catalogue_titles(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    auto doc=std::make_shared<Document>(open_ome_zarr(source.u8string()));Project project;project.add_document(doc);ObjectSelection selection(*doc);
    GuiOptions options;detail::GuiSpatial spatial(options);detail::GuiBatch batch(options);detail::GuiRaw raw(options);detail::GuiProjectDensity density;detail::GuiPixelStreamBrowser streams(options);
    auto tools=launcher([&](ui::ToolId id) {switch(id) {case ui::ToolId::Spatial:spatial.open();break;case ui::ToolId::Batch:batch.open();break;case ui::ToolId::Density:density.open();break;case ui::ToolId::Channels:raw.open();break;default:break;}},[&](ui::ToolChild id){if(id==ui::ToolChild::SavedStreams)streams.open();});
    auto mark=[](GuiInputTarget){};char search[128]{};ui::ToolCatalogTrace trace;
    const auto catalog=project.images();
    auto draw=[&] {
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({700,1000});ImGui::Begin("Title catalogue");ui::ToolCatalog(tools,{true,true,true},search,sizeof(search),&trace);ImGui::End();
        spatial.draw(*doc,selection,&project,&catalog,false,true,mark);
        batch.draw(&project,1,&selection,false,{TissueParameters{},nullptr,{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind::OpticalDensityMeans,RoiSamplingPolicy::GeometryCoverage,false,nullptr,false,std::vector<double>(6),std::vector<double>(3,255)},
            [](bool){return StainTransform({{.65,.70,.29},{.07,.99,.11}},{255,255,255});});
        raw.draw(*doc,selection,&project,false,mark);density.draw(&project,catalog,false);streams.draw(*doc,false,mark);
        for(std::size_t i:{4u,8u,10u,11u,15u})if(auto* win=ImGui::FindWindowByName(ui::tool_window_label(i));win && win->WasActive)ImGui::SetWindowPos(win,{800,20},ImGuiCond_Always);
    };
    for(const auto pair:std::array<std::pair<ui::CommandId,std::size_t>,5>{{{ui::CommandId::Tool_Spatial,4},{ui::CommandId::Tool_Batch,8},{ui::CommandId::Tool_Density,10},{ui::CommandId::Tool_Channels,11},{ui::CommandId::Child_SavedStreams,15}}}) {
        std::strcpy(search,ui::command(pair.first).caption);host.settle(draw);check(trace.rows.count(pair.first)==1,"Filtered catalogue title row missing");host.click(trace.rows.at(pair.first),draw);host.settle(draw);
        const auto* win=ImGui::FindWindowByName(ui::tool_window_label(pair.second));check(win && win->WasActive,"Catalogue did not open real retitled window");
        check(win->ID==ImHashStr(ui::tool_legacy_titles[pair.second]),"Catalogue title changed stable window ID");
    }
    std::cout<<"Five retitled real controllers opened from actual catalogue clicks, old IDs retained\n";
}
inline void menu_captures(const std::filesystem::path& directory) {
        const auto dir=std::filesystem::absolute(directory);std::filesystem::create_directories(dir);
        check(glfwInit()!=0,"Menu captures GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        for(bool french:{false,true}) {
            const int width=french?1440:1100,height=french?1000:800;
            auto* window=glfwCreateWindow(width,height,"Command menu captures",nullptr,nullptr);check(window,"Menu capture window");glfwMakeContextCurrent(window);
            {
                model_safety_checks::GlHost host(window);ui::i18n::choose(french?*ui::i18n::exact_language("fr"):0);ImGui::GetStyle().FontSizeBase=french?32:16;
                ui::HeaderSnapshot snapshot;snapshot.desktop=true;snapshot.workspace=true;snapshot.image=snapshot.project=snapshot.project_dirty=true;
                snapshot.can_undo=snapshot.can_redo=snapshot.can_copy=snapshot.can_paste=true;snapshot.reload_ui=true;
                ui::HeaderTrace trace;ImGuiID project_group=0;const auto tools=command_checks::launcher([](ui::ToolId){},[](ui::ToolChild){});
                auto draw=[&] {
                    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
                    ImGui::Begin("Header captures",nullptr,ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);
                    ui::draw_header(snapshot,[](ui::HeaderCommand){},[&] {
                        for(const auto* label:ui::workflow_task_labels)ui::MenuItem(label);
                        ImGui::Separator();ui::ToolMenus(tools,{true,true,true});project_group=ImGui::GetItemID();
                    },&trace);
                    for(const auto& [id,shown]:trace.shown) {
                        const auto row=id==ui::HeaderCommand::Close?ui::CommandId::DesktopExit:ui::command_id(id);
                        check(shown==std::make_pair(std::string(ui::i18n::text(ui::command(row).caption)),std::string(ui::command_chord(row))),"Popup caption/chord differs from table");
                    }
                    ImGui::End();
                };
                const std::string tag=french?"fr-200":"en";
                host.capture(dir/("t41f1-header-"+tag+".ppm"),draw);
                const char* names[]={"file","edit","view","analyze","help"};
                const char* captions[]={"File","Edit","View","Analyze","Help"};
                auto activate=[&](ImGuiID id) {
                    check(id!=0,"Header capture menu ID missing");ImGui::GetIO().AddMousePosEvent(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());host.frame(draw);
                    ImGui::ActivateItemByID(id);host.settle(draw);
                };
                for(int menu=0;menu<5;++menu) {
                    activate(trace.group_ids.at(captions[menu]));check(ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId),"Native header popup did not open: "+std::string(names[menu]));
                    host.capture(dir/("t41f1-menu-"+std::string(names[menu])+"-"+tag+".ppm"),draw);
                    if(menu==3) {
                        activate(project_group);
                        host.capture(dir/("t41f1-menu-analyze-project-"+tag+".ppm"),draw);
                    }
                    auto& io=ImGui::GetIO();
                    for(int escape=0;escape<3;++escape) {io.AddKeyEvent(ImGuiKey_Escape,true);host.frame(draw);io.AddKeyEvent(ImGuiKey_Escape,false);host.settle(draw);}
                }
            }
            glfwDestroyWindow(window);
        }
        glfwTerminate();std::cout<<"Actual header/File/Edit/View/Analyze/Help/Project tools popups English/French200, table agreement and overflow0 passed\n";
}
inline void ready_batch(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;
    auto project=fixture(dir/"ready-batch.qpxproj",32,32);auto doc=project->document(1);ObjectSelection selected(*doc);
    GuiOptions options;options.batch_output_directory=(dir/"ready-batch-output").u8string();
    detail::GuiBatch panel(options);panel.open();std::string rendered;
    auto draw=[&] {
        ImGui::LogToBuffer();
        panel.draw(project.get(),1,&selected,false,{TissueParameters{},nullptr,{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind::OpticalDensityMeans,RoiSamplingPolicy::GeometryCoverage,false,nullptr,false,std::vector<double>(6),std::vector<double>(3,255)},
            [](bool){return StainTransform({{.65,.70,.29},{.07,.99,.11}},{255,255,255});});
        rendered=host.context->LogBuffer.c_str();ImGui::LogFinish();
    };
    host.fit("Project batch",700,1100,draw);
    const auto target=host.action("Run batch (Ctrl+Alt+B)");check(target.enabled,"Ready batch Run must be enabled");
    auto& io=ImGui::GetIO();io.DeltaTime=.1f;
    io.AddMousePosEvent(float(target.bounds.x+target.bounds.width/2),float(target.bounds.y+target.bounds.height/2));
    for(int i=0;i<30;++i)host.frame(draw);
    check(rendered.find("Ctrl+Alt+B")==std::string::npos && !*ui::command_chord(ui::CommandId::Action_Run_batch_Ctrl_Alt_B),"Run batch tooltip advertises an opening chord");
    io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiMod_Alt,true);io.AddKeyEvent(ImGuiKey_B,true);
    for(int i=0;i<10;++i)host.frame(draw);
    io.AddKeyEvent(ImGuiKey_B,false);io.AddKeyEvent(ImGuiMod_Alt,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);host.settle(draw);
    check(host.action("Run batch (Ctrl+Alt+B)").enabled && !panel.busy() && !panel.finished() && !panel.result() && panel.error().empty() && panel.stats().state=="idle","Opening chord dispatched a ready batch Run");
    check(!std::filesystem::exists(dir/"ready-batch-output"),"Opening chord wrote batch output");
    std::cout<<"Rendered ready Run: no chord tooltip; Ctrl+Alt+B held ten frames starts nothing\n";
}
inline void run(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();keys();layouts(dir);catalogue_titles(dir);ready_batch(dir);
    check(glfwInit()!=0,"Command checks GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Command readers",nullptr,nullptr);check(window,"Command readers window");glfwMakeContextCurrent(window);
    readers(dir,window);glfwDestroyWindow(window);glfwTerminate();menu_captures(dir);
}
}
