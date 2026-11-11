#pragma once
static void right_docking_controls(const std::filesystem::path& directory) {
    const auto previous_language=ui::i18n::state;
    std::filesystem::create_directories(directory);
    auto initialize=[](float width,float scale) {
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={width,900};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
        ImGui::GetStyle().ScaleAllSizes(scale);ImGui::GetStyle().FontSizeBase=16*scale;
    };
    ImVec2 close_hits[4]{};
    bool busy=false;
    auto frame=[&](GuiWorkspace& workspace) {
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dock host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
        workspace.dockspace(busy);Rect bounds;
        for(int i=0;i<4;++i) {workspace.begin(GuiWorkspace::panes[i],bounds);workspace.title(GuiWorkspace::keys[i]);
            const auto* pane=ImGui::GetCurrentWindow();
            if(!pane->SkipItems && pane->DockNode) {const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();close_hits[i]={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};}
            ImGui::TextUnformatted("Panel content");ImGui::End();}
        workspace.begin("Slide",bounds);ImGui::TextUnformatted("Canvas");ImGui::End();ImGui::End();ImGui::Render();
    };
    auto settle=[&](GuiWorkspace& workspace) {for(int i=0;i<4;++i)frame(workspace);};
    const auto path=directory/"right-dock.ini";
    initialize(1440,1);
    GuiOptions options;options.docked_workspace=true;options.layout_path=path.u8string();options.reset_layout=true;
    {
        GuiWorkspace workspace(options);settle(workspace);
        auto* project=ImGui::FindWindowByName(GuiWorkspace::project_pane);auto* slide=ImGui::FindWindowByName("Slide");auto* workflow=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
        check(project->Pos.x+project->Size.x<=slide->Pos.x+1 && workflow->Pos.x>=slide->Pos.x+slide->Size.x-1,"default must put navigation / viewer / workflow in order");
        const auto* default_image=ImGui::FindWindowByName(GuiWorkspace::image_pane);
        check(default_image->DockId==project->DockId,"Image belongs to the left inspector group");
        check(slide->Size.x>400 && slide->Size.y>600,"default side docks must leave a usable viewer");
        auto& resize_io=ImGui::GetIO();const auto initial_width=workflow->Size.x;
        const auto* right_group=workflow->DockNode;const float divider_x=right_group->Pos.x-1,divider_y=right_group->Pos.y+right_group->Size.y*.7f;
        resize_io.AddMousePosEvent(divider_x,divider_y);settle(workspace);resize_io.AddMouseButtonEvent(0,true);frame(workspace);
        resize_io.AddMousePosEvent(divider_x-40,divider_y);settle(workspace);resize_io.AddMouseButtonEvent(0,false);settle(workspace);
        check(workflow->Size.x>initial_width+30,"right dock splitter must resize the inspector and viewer");
        workspace.select(GuiWorkspace::image_pane);settle(workspace);
        auto* dragged=ImGui::FindWindowByName(GuiWorkspace::image_pane);auto& io=ImGui::GetIO();
        io.AddMousePosEvent(dragged->Pos.x+70,dragged->Pos.y+20);io.AddMouseButtonEvent(0,true);frame(workspace);
        io.AddMousePosEvent(dragged->Pos.x+170,dragged->Pos.y+150);frame(workspace);frame(workspace);
        check(!dragged->DockId && project->DockId && project->DockId!=workflow->DockId,"heading drag must detach only that panel, leaving its siblings docked");
        io.AddMouseButtonEvent(0,false);frame(workspace);
        workspace.dock_right(GuiWorkspace::image_pane,true);settle(workspace);
        auto* image=ImGui::FindWindowByName(GuiWorkspace::image_pane);
        check(!image->DockId && !GuiWorkspace::can_dock_right(GuiWorkspace::image_pane),"Image must refuse right docking from a placement command");
        workspace.dock_right(GuiWorkspace::objects_pane,true);settle(workspace);
        auto* objects=ImGui::FindWindowByName(GuiWorkspace::objects_pane);
        check(objects->DockId!=workflow->DockId && objects->Pos.x>=slide->Pos.x+slide->Size.x-1 && objects->Pos.y>workflow->Pos.y,"permitted panel must stack on the right");
        workspace.select(GuiWorkspace::objects_pane);settle(workspace);check(objects->DockTabIsVisible,"rail must reveal a pane moved to a right group");
        workspace.float_panel(GuiWorkspace::image_pane);settle(workspace);
        check(!image->DockId && image->Size.x>200 && image->Pos.x>=0 && image->Size.y<400 && image->HasCloseButton && image->WindowBorderSize>0,"floating panel must have a bounded content fit, native title/close and border");
        check(workspace.save(),"floating workspace fixture save");write(directory/"floating-inspector.ini",read(path));
        // Exercise the actual native floating X, rather than only hide().
        io.AddMousePosEvent(image->Pos.x+image->Size.x-ImGui::GetFrameHeight()*.5f,image->Pos.y+ImGui::GetFrameHeight()*.5f);settle(workspace);
        io.AddMouseButtonEvent(0,true);frame(workspace);io.AddMouseButtonEvent(0,false);settle(workspace);
        check(!workspace.visible(GuiWorkspace::image_pane) && !image->Active,"floating close must hide the original pane");
        workspace.select(GuiWorkspace::image_pane);settle(workspace);check(image->Active && !image->DockId,"View/rail reveal must restore the floating pane");
        ImGui::DockBuilderDockWindow(GuiWorkspace::image_pane,workflow->DockId);settle(workspace);
        check(image->DockId==project->DockId,"native right drop must be corrected to the left, too");
        workspace.select(GuiWorkspace::image_pane);settle(workspace);const float expanded_from=slide->Size.x;
        workspace.activate(GuiWorkspace::image_pane);settle(workspace);
        check(!workspace.visible(GuiWorkspace::image_pane) && !workspace.visible(GuiWorkspace::project_pane) && workspace.visible(GuiWorkspace::workflow_pane) && workspace.visible(GuiWorkspace::objects_pane),"active left rail toggles the left group only");
        check(slide->Size.x>expanded_from+200 && workspace.active_pane()==-1,"hidden sidebar must release viewer space and rail highlight");
        workspace.activate(GuiWorkspace::image_pane);settle(workspace);
        check(workspace.visible(GuiWorkspace::image_pane) && image->DockId && slide->Size.x<expanded_from+10,"second activity click must restore the left inspector and divider");
        workspace.hide(GuiWorkspace::image_pane);settle(workspace);
        check(workspace.save(),"save hidden primary panel");
        ui::i18n::choose(*ui::i18n::exact_language("fr"));
        check(workspace.save(),"right-dock layout save");
        ui::i18n::choose(-1);ui::i18n::load_preference(read(path));
        check(std::string(ui::i18n::preference())=="fr","new layout markers must not displace the language preference during reopen");
    }
    ImGui::DestroyContext();
    // A legacy custom split must remain exactly as organized by its owner.
    auto legacy=read(path);const std::string version="; qupathxx panel-layout=2\n";legacy.erase(legacy.find(version),version.size());write(path,legacy);
    initialize(1440,1);options.reset_layout=false;
    {
        GuiWorkspace workspace(options);settle(workspace);check(workspace.loaded(),"right-dock layout reopen");
        const auto* image=ImGui::FindWindowByName(GuiWorkspace::image_pane);const auto* workflow=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);const auto* slide=ImGui::FindWindowByName("Slide");
        check(!workspace.visible(GuiWorkspace::image_pane) && (!image || !image->Active),"primary visibility must survive reopen");
        workspace.select(GuiWorkspace::image_pane);settle(workspace);
        image=ImGui::FindWindowByName(GuiWorkspace::image_pane);
        check(image->Pos.x<slide->Pos.x && ImGui::FindWindowByName(GuiWorkspace::objects_pane)->Pos.y>workflow->Pos.y,"valid custom stacked layout must survive reopen");
        workspace.dock_left(GuiWorkspace::workflow_pane);settle(workspace);
        check(ImGui::FindWindowByName(GuiWorkspace::workflow_pane)->DockId==ImGui::FindWindowByName(GuiWorkspace::image_pane)->DockId,"move to left group");
        ImGui::DockBuilderDockWindow(GuiWorkspace::image_pane,ImGui::FindWindowByName("Slide")->DockId);settle(workspace);
        check(!ImGui::FindWindowByName("Slide")->DockNode->IsNoTabBar(),"dropping a panel onto the viewer must retain tabs so the slide remains reachable");
        workspace.reset();settle(workspace);
        check(ImGui::FindWindowByName(GuiWorkspace::workflow_pane)->Pos.x>ImGui::FindWindowByName("Slide")->Pos.x,"reset must restore the right workflow dock");
        busy=true;
        for(int i=0;i<4;++i) {
            workspace.select(GuiWorkspace::panes[i]);settle(workspace);auto& io=ImGui::GetIO();const auto hit=close_hits[i];
            io.AddMousePosEvent(hit.x,hit.y);frame(workspace);io.AddMouseButtonEvent(0,true);frame(workspace);io.AddMouseButtonEvent(0,false);settle(workspace);
            check(!workspace.visible(GuiWorkspace::panes[i]),"every docked primary must close even while editing is busy");
            workspace.select(GuiWorkspace::panes[i]);settle(workspace);check(workspace.visible(GuiWorkspace::panes[i]),"every primary pane must reopen");
        }
        busy=false;
        workspace.dock_left(GuiWorkspace::workflow_pane);settle(workspace);check(workspace.save(),"save old single-sidebar fixture");
        workspace.dock_left(GuiWorkspace::image_pane);settle(workspace);check(workspace.save(),"save old single-sidebar fixture");
    }
    ImGui::DestroyContext();
    legacy=read(path);legacy.erase(legacy.find(version),version.size());write(path,legacy);initialize(1440,1);
    {
        GuiWorkspace workspace(options);settle(workspace);
        check(ImGui::FindWindowByName(GuiWorkspace::workflow_pane)->Pos.x>ImGui::FindWindowByName("Slide")->Pos.x,"old single-sidebar default must gain the right dock without reset");
    }
    ImGui::DestroyContext();
    for(const auto size:{std::pair{820.f,1.f},std::pair{1440.f,2.f}}) {
        initialize(size.first,size.second);GuiOptions compact;compact.docked_workspace=true;GuiWorkspace workspace(compact);settle(workspace);
        const auto* slide=ImGui::FindWindowByName("Slide");const auto* project=ImGui::FindWindowByName(GuiWorkspace::project_pane);const auto* workflow=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);
        check(workflow->DockId==project->DockId && slide->Size.x>size.first*.4f,"narrow/scaled default must avoid squeezing the viewer between two sidebars");
        ImGui::DestroyContext();
    }
    ui::ProjectPreferences preferences;
    check(preferences.list_height(700,44,2,112)==88,"short list must release unused space to metadata");
    check(preferences.list_height(700,44,200,112)<500,"long list must remain bounded and scrollable");
    check(preferences.list_height(700,44,0,112)==44,"empty list needs only one useful message row");
    preferences.inspector_resized=true;preferences.inspector_fraction=.6f;
    check(std::abs(preferences.list_height(700,44,2,112)-280)<.001f,"intentional user split must remain stable during filtering");
    ui::ProjectPreferences restored;restored.load(preferences.header());check(restored.inspector_resized,"manual split preference must persist");
    ui::i18n::state=previous_language;
    std::cout<<"Right docking: default geometry, move/stack/float/redock, save/reopen/reset, narrow/200% and content-sized lists passed\n";
}
