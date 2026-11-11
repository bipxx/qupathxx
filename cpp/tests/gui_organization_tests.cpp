#include "../gui_layout.hpp"
#include "../gui_workflow.hpp"
#include "../gui_project_browser.hpp"
#include "../gui_project_inspector.hpp"
#include "../gui_workspace.hpp"
#include "gui_dialog_checks.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_recovery.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace qpx;
static void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
static std::string read(const std::filesystem::path& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::filesystem::path& path,const std::string& data) {std::ofstream f(path,std::ios::binary);f<<data;check(bool(f),"write fixture");}
static GuiInput key(int frame,GuiInputKind kind) {GuiInput e;e.frame=frame;e.kind=kind;return e;}
static std::shared_ptr<Project> fixture(const std::filesystem::path& path,int width=1024,int height=768) {
    std::vector<std::uint8_t> pixels(std::size_t(width)*height*3);
    for(std::size_t i=0;i<pixels.size();i+=3) {pixels[i]=30;pixels[i+1]=120;pixels[i+2]=210;}
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(width,height,std::move(pixels)));
    const auto image=project->add_document(doc,"Research image");
    project->set_view(image,{{width/2.,height/2.},1.5,{}});project->save_new(path.u8string());return project;
}
static GuiOptions options(const std::filesystem::path& directory,const std::string& name,const char* pane,const char* task="measurements",float scale=1) {
    GuiOptions o;o.width=1100;o.height=800;o.ui_scale=scale;o.docked_workspace=true;o.settle_tiles=true;o.autosave_interval_seconds=0;
    if(scale>1) {o.width=1440;o.height=900;}
    o.layout_path=(directory/(name+".ini")).u8string();
    write(std::filesystem::u8path(o.layout_path),std::string("; qupathxx workspace layout v1\n; qupathxx language=en\n; qupathxx theme=dark\n; qupathxx sidebar=")+pane+"\n; qupathxx workflow="+task+"\n");
    return o;
}
static void geometry() {
    ui::ProjectPreferences preferences;const auto recent_base=std::filesystem::temp_directory_path()/"qpx-history";
    for(int i=0;i<10;++i)preferences.remember((recent_base/(std::to_string(i)+".qpxproj")).u8string());
    preferences.remember((recent_base/"5.qpxproj").u8string());preferences.group="Case / tissue";preferences.inspector_fraction=.63f;
    ui::ProjectPreferences restored;restored.load(preferences.header());
    check(restored.recent==preferences.recent && restored.recent.size()==8 && restored.group==preferences.group && restored.recent.front()==(recent_base/"5.qpxproj").u8string(),"recent projects must round-trip with bounded MRU order and grouping");
    check(std::abs(restored.inspector_fraction-.63f)<.0001f,"inspector divider must round-trip independently of project storage");
    restored.load("; qupathxx project-inspector=nan\n; qupathxx project-inspector=90000\n; qupathxx project-inspector=4500x\n");
    check(std::abs(restored.inspector_fraction-.63f)<.0001f,"invalid divider preferences must preserve the last usable split");
    ui::ProjectPreferences invalid;invalid.load("; qupathxx recent-project=zz\n; qupathxx recent-project=00\n");check(invalid.recent.empty(),"invalid recent paths must not become project requests");
    ProjectImage alpha,beta,gamma;alpha.id=1;alpha.name="Alpha";alpha.available=true;alpha.metadata={{"Case","R-024"},{"Stain","H&E"}};
    beta.id=2;beta.name="Beta";beta.tags={"Control"};beta.metadata={{"Case","R-024"},{"Stain","CD3"}};
    gamma.id=3;gamma.name="Other";gamma.available=true;
    const std::vector<ProjectImage> catalog{alpha,beta,gamma};ui::ProjectBrowser browser;browser.selected=2;
    browser.rebuild(catalog,"CONTROL");check(browser.matched==1 && browser.rows.front().image==1 && browser.selected==2 && browser.missing==1,"catalog search covers tags without activating documents; source problems count across the whole catalog");
    browser.group_key="Case";browser.rebuild(catalog,"");check(browser.matched==3 && browser.rows.size()==5 && browser.rows.front().group=="R-024" && browser.rows[3].group.empty(),"explicit metadata grouping and unassigned images");
    browser.toggle_group("R-024");browser.rebuild(catalog,"");check(browser.rows.size()==3 && browser.rows.front().count==2 && browser.matched==3 && browser.selected==2,"collapsed groups must retain catalog count and entry selection");
    browser.rebuild(catalog,"CD3");check(browser.rows.size()==2 && browser.rows.back().image==1,"search must reveal matching images in collapsed groups");
    browser.missing_only=true;browser.rebuild(catalog,"cd3");check(browser.matched==1 && browser.rows.back().image==1,"metadata and availability filters compose");
    browser.group_key="Guessed case";browser.rebuild(catalog,"");check(browser.group_key.empty() && browser.rows.size()==1 && catalog[1].metadata.at("Stain")=="CD3","unknown grouping cannot invent case metadata");
    auto repaired=catalog;repaired[1].available=true;browser.rebuild(repaired,"");
    check(browser.missing==0 && !browser.missing_only && browser.matched==3 && browser.selected==2,"repairing the last missing file restores all images without changing selection");
    check(ui::f5_action(false,false,false)==ui::F5Action::PixelClassifier && ui::f5_action(true,true,false)==ui::F5Action::Recoveries,"distinct F5 intents");
    check(ui::f5_action(true,false,false)==ui::F5Action::Ignore && ui::f5_action(false,true,false)==ui::F5Action::Ignore && ui::f5_action(true,true,true)==ui::F5Action::Ignore,"unassigned F5 modifiers");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;
    io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ImGui::GetStyle().FontSizeBase=ui::design::metrics.font_size;
    for(int frame=0;frame<3;++frame) {
        if(frame==2) io.DisplaySize={600,400};
        ImGui::NewFrame();ui::SetNextToolWindow("Bounded tool",{760,720});
        ImGui::Begin("Bounded tool");const auto* window=ImGui::GetCurrentWindow();const auto* vp=ImGui::GetMainViewport();
        const auto workspace_scale=ui::design::scale();
        {ui::design::Caption caption;ui::IconButton("Caption geometry check",ui::Icon::Plus);
            check(std::abs(ui::design::scale()-workspace_scale)<.001f && std::abs(ImGui::GetItemRectSize().y-ui::design::metrics.icon_size*workspace_scale)<.1f,"caption role changed action geometry: frame="+std::to_string(frame)+" scale="+std::to_string(ui::design::scale())+" base="+std::to_string(workspace_scale)+" height="+std::to_string(ImGui::GetItemRectSize().y)+" skipped="+std::to_string(window->SkipItems));}
        {ui::design::Heading heading(ui::design::metrics.title_size);
            check(std::abs(ui::design::scale()-workspace_scale)<.001f,"title role changed workspace scale");}
        check(window->Pos.x>=vp->WorkPos.x && window->Pos.y>=vp->WorkPos.y && window->Pos.x+window->Size.x<=vp->WorkPos.x+vp->WorkSize.x+1 && window->Pos.y+window->Size.y<=vp->WorkPos.y+vp->WorkSize.y+1,"tool window remains in viewport");
        const auto list=ui::CollectionHeight(100);check(list>=3*ImGui::GetTextLineHeightWithSpacing(),"readable collection rows");
        double value=1;char text[32]="unchanged";
        ui::SetNextItemWidth(-1);ui::InputDouble("Control value",&value);
        check(ImGui::GetItemRectMax().x<=window->WorkRect.Max.x+.75f,"label-column field stays in its parent group");
        ui::SetNextItemWidth(-1);ui::InputText("A longer control caption",text,sizeof(text));
        check(ImGui::GetItemRectMax().x<=window->WorkRect.Max.x+.75f && value==1 && std::string(text)=="unchanged","field layout retains values and trailing alignment");
        ui::CatalogEntry("Very long image label that must leave the status readable",true,"Viewing");
        check(ImGui::GetItemRectMax().x<=window->WorkRect.Max.x+.75f,"catalog full-row target fits its panel");
        if(frame==0) {ImGui::SetWindowSize({360,260});ImGui::SetWindowPos({120,100});}
        if(frame==1) check(window->Size.x==360 && window->Size.y==260,"tool window preserves user size");
        ImGui::End();ImGui::Render();
    }
    ImGui::DestroyContext();std::cout<<"Collection rows and bounded/user-sized tool windows passed\n";
}
static void inspector_controls() {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,700};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ProjectImage image;image.metadata={{"Case identifier with a long label","CASE-024 with a long value that must wrap without clipping"},{"Literal %s","Exact user value"}};
    image.description="Selected entry description";image.tags={"Selected tag"};float fraction=.45f;
    ImVec2 divider{};
    auto frame=[&](bool log=false) {
        ImGui::NewFrame();ui::begin_frame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({240,600});ImGui::Begin("Inspector controls");
        ui::ResizeDivider("Split",fraction,400,80,80);const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();divider={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f};
        if(log)ImGui::LogToBuffer();ui::ImageMetadata(image);
        const std::string contents=log?ImGui::GetCurrentContext()->LogBuffer.c_str():"";if(log)ImGui::LogFinish();
        check(ui::layout_stats.overflow_items==0,"inspector values must fit a narrow pane");ImGui::End();ImGui::Render();return contents;
    };
    frame();frame();auto text=frame(true);
    check(text.find("CASE-024")!=std::string::npos && text.find("Literal %s")!=std::string::npos && text.find("Exact user value")!=std::string::npos && text.find("Selected entry description")!=std::string::npos && text.find("Selected tag")!=std::string::npos,"inspector must display selected metadata/description/tags as literal text");
    io.AddMousePosEvent(divider.x,divider.y);io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(divider.x,divider.y-40);frame();io.AddMouseButtonEvent(0,false);frame();
    check(fraction>.53f && fraction<.57f,"dragging the divider upward must give the inspector more room");
    image.metadata={{"Other entry","SECOND-ENTRY"}};image.description.reset();image.tags.clear();text=frame(true);
    check(text.find("SECOND-ENTRY")!=std::string::npos && text.find("CASE-024")==std::string::npos && text.find("Selected tag")==std::string::npos,"changing the selected snapshot must replace inspector values");
    ImGui::DestroyContext();std::cout<<"Selected metadata, literal wrapped values and inspector divider interaction passed\n";
}
static void project_filter_controls() {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;
    io.ConfigInputTrickleEventQueue=false;io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ProjectImage ready,missing;ready.id=1;ready.available=true;ready.metadata={{"Case","A"}};missing.id=2;
    std::vector<ProjectImage> catalog{ready,missing};ui::ProjectBrowser browser;browser.selected=1;browser.rebuild(catalog,"");
    auto frame=[&] {
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({320,300});ImGui::Begin("Project filter controls");
        if(ui::project_browser_filters(browser))browser.rebuild(catalog,"");
        const auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();ImGui::End();ImGui::Render();
        return ImVec2((lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f);
    };
    auto click=[&](ImVec2 p) {io.AddMousePosEvent(p.x,p.y);io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
    // Establish the native window's final auto-fit/hover geometry before input.
    frame();frame();
    click(frame());check(browser.missing_only && browser.matched==1 && browser.rows.front().image==1 && browser.selected==1,"missing-files action must filter without opening a different image: active="+std::to_string(browser.missing_only)+" matched="+std::to_string(browser.matched));
    click(frame());check(!browser.missing_only && browser.matched==2 && browser.selected==1,"active missing filter must clear through its close action");
    catalog[1].available=true;browser.group_key="Case";browser.rebuild(catalog,"");
    click(frame());check(browser.group_key.empty() && browser.matched==2 && browser.selected==1,"active grouping must clear through its close action");
    ImGui::DestroyContext();std::cout<<"Contextual missing-files action and clear interaction passed\n";
}
static void docking_controls() {
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1200,850};io.DeltaTime=1.f/60;
    io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.ConfigInputTrickleEventQueue=false;
    io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
    GuiOptions opts;opts.docked_workspace=true;GuiWorkspace workspace(opts);bool tool_open=true;
    auto frame=[&] {
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Dock host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings);
        workspace.dockspace();Rect bounds;
        for(const auto* pane:GuiWorkspace::panes){workspace.begin(pane,bounds);ImGui::TextUnformatted("Panel");ImGui::End();}
        workspace.begin("Slide",bounds);ImGui::TextUnformatted("Canvas");ImGui::End();
        if(tool_open){ImGui::Begin("Closable tool",&tool_open);ImGui::TextUnformatted("Tool settings");ImGui::End();}
        ImGui::End();ImGui::Render();
    };
    frame();frame();frame();
    auto dock_tool=[&] {
        auto* slide=ImGui::FindWindowByName("Slide");check(slide && slide->DockId,"slide docking fixture");
        ImGuiID right=0,left=0;const auto parent=slide->DockId;
        ImGui::DockBuilderSplitNode(parent,ImGuiDir_Right,.28f,&right,&left);
        ImGui::DockBuilderDockWindow("Closable tool",right);
        auto* node=ImGui::DockBuilderGetNode(right);
        node->SetLocalFlags(node->LocalFlags|ImGuiDockNodeFlags_NoTabBar|ImGuiDockNodeFlags_HiddenTabBar);
        ImGui::DockBuilderFinish(parent);
    };
    dock_tool();frame();frame();frame();
    auto* tool=ImGui::FindWindowByName("Closable tool");
    check(tool && tool->DockNode && tool->DockNode->TabBar && tool->DockNode->HasCloseButton && !tool->DockNode->IsNoTabBar(),"docked tool lost its title/close controls");
    check(ImGui::FindWindowByName(GuiWorkspace::project_pane)->DockNode->IsNoTabBar(),"activity views gained redundant tabs");
    const auto* node=tool->DockNode;const auto& style=ImGui::GetStyle();
    io.AddMousePosEvent(node->Pos.x+node->Size.x-style.WindowBorderSize-style.FramePadding.x-ImGui::GetFontSize()/2,node->Pos.y+style.FramePadding.y+ImGui::GetFontSize()/2);
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
    check(!tool_open,"docked title close button did not dismiss the tool");
    tool_open=true;frame();frame();frame();
    tool=ImGui::FindWindowByName("Closable tool");check(tool->HasCloseButton,"reopened tool lost close control");
    if(tool->DockNode)ImGui::DockContextProcessUndockWindow(ImGui::GetCurrentContext(),tool,true);
    frame();frame();check(!tool->DockIsActive && tool->HasCloseButton,"undocked tool lost its title/close controls");
    dock_tool();frame();frame();frame();
    check(tool->DockNode && !tool->DockNode->IsNoTabBar() && tool->DockNode->HasCloseButton,"redocked tool lost close controls");
    ImGui::DestroyContext();std::cout<<"Legacy dock migration, title close/reopen and undock/redock passed\n";
}
static void navigation(const std::filesystem::path& directory) {
    auto project=fixture(directory/"navigation.qpxproj");auto doc=project->document(1);doc->add_rectangle({400,300,80,60});project->save(project->path());
    const auto before=read(directory/"navigation.qpxproj");const auto generation=project->generation();
    const std::pair<const char*,const char*> cases[]={{"project","measurements"},{"image","measurements"},{"annotations","measurements"},{"workflow","measurements"},{"workflow","region"},{"workflow","tissue"},{"workflow","cells"},{"workflow","tables"},{"workflow","tools"},{"workflow","files"}};
    int index=0;
    for(const auto& c:cases) {
        const auto name=std::string(c.first)+"-"+c.second;
        auto o=options(directory,name,c.first,c.second,index++%2?2:1);
        if(std::string(c.second)!="tools") o.inputs={key(3,GuiInputKind::SelectAllKey)};
        auto r=run_project_gui(project,14,(directory/(name+".ppm")).u8string(),o);
        check(r.replay_complete && r.canvas_visible && r.tiles_settled,"pane/task replay "+name);
        check(r.ui_overflow_items==0,"pane/task overflow "+name+": "+r.ui_overflow_label);
        if(std::string(c.second)=="tools") check(r.selected_ids.empty(),"Tools does not require an object selection");
        check(r.final_center.x==512 && r.final_center.y==384 && r.final_zoom==1.5,"navigation changed camera "+name);
        check(project->generation()==generation && doc->object_count()==1 && before==read(directory/"navigation.qpxproj"),"navigation changed project "+name);
        check(read(std::filesystem::u8path(o.layout_path)).find(std::string("; qupathxx workflow=")+c.second+"\n")!=std::string::npos,"task persistence "+name);
    }
    auto o=options(directory,"region-shortcut","project");o.analysis_white={201,212,223};
    o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::AnalyzeKey),key(9,GuiInputKind::KeepAnalysisKey)};
    auto r=run_project_gui(project,24,"",o);check(r.analysis_state=="ready" && r.analysis_records==1 && r.analysis_samples>0,"region run/keep shortcut through task navigation");
    const auto saved=doc->latest_analysis(doc->annotations().front().id);
    check(saved && saved->result.white==std::array<double,3>{201,212,223},"collapsed advanced controls preserve the configured scientific white reference");
    check(read(std::filesystem::u8path(o.layout_path)).find("; qupathxx workflow=region\n")!=std::string::npos,"analysis shortcut reveals Region");
    std::cout<<"All pane/task captures, identity/camera and native region run/keep passed\n";
}
static void overview_failure(const std::filesystem::path& directory) {
    auto project=fixture(directory/"overview.qpxproj",2049,2048);auto doc=project->document(1);
    auto o=options(directory,"overview-failed","project");auto baseline=run_project_gui(project,12,"",o);
    check(baseline.overview_state=="failed" && !baseline.overview_result && !baseline.overview_gpu_bytes,"overview budget failure fixture");
    const auto canvas=baseline.canvas_content_bounds;const Point p{baseline.final_center.x+(canvas.width/2-120)/baseline.final_zoom,baseline.final_center.y+(80-canvas.height/2)/baseline.final_zoom};
    const auto id=doc->add_rectangle({p.x-12,p.y-12,24,24});
    GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=p.x;down.y=p.y;auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
    auto r=run_project_gui(project,16,(directory/"overview-failed.ppm").u8string(),o);
    check(r.primary_selection==id && r.final_center.x==baseline.final_center.x && r.final_center.y==baseline.final_center.y && r.final_zoom==baseline.final_zoom,"unavailable overview blocks selection or pans");
    check(r.replay_complete && r.tiles_settled && !r.pending_tiles && r.overview_state=="failed" && r.overview_error.find("source pixel limit")!=std::string::npos && !r.overview_result && !r.overview_gpu_bytes,"failure remains explicit without partial preview");
    std::cout<<"Unavailable overview does not intercept canvas; original samples/calibration untouched\n";
}
static void catalog_navigation(const std::filesystem::path& directory) {
    const auto path=directory/"browser.qpxproj";auto project=fixture(path);const auto first=project->active_image();
    auto other=std::make_shared<Document>(Image(256,128,std::vector<std::uint8_t>(256*128*3,90)));
    const auto second=project->add_document(other,"Second image");project->set_view(second,{{128,64},1.25,{}});project->set_active_image(first);project->save(project->path());
    const auto bytes=read(path);
    auto o=options(directory,"browser-selection","project");
    GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::ProjectList;down.x=.4;down.y=.65;
    auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
    auto r=run_project_gui(project,16,(directory/"browser-single-click.ppm").u8string(),o);
    check(r.active_image==second && r.image_switches==1 && r.project_error.empty() && r.final_center.x==128 && r.final_center.y==64 && r.final_zoom==1.25,"one catalog click must open the image and restore its view");
    check(read(path)==bytes && project->document(first)->object_count()==0 && other->object_count()==0,"opening an entry changed saved project bytes or objects");
    project->set_active_image(first);project->save(project->path());
    const auto menu_generation=project->generation();const auto menu_bytes=read(path);
    auto more_down=down;more_down.x=.95;auto more_up=more_down;more_up.frame=5;more_up.kind=GuiInputKind::LeftUp;
    o=options(directory,"browser-menu","project");o.inputs={more_down,more_up};r=run_project_gui(project,16,(directory/"browser-row-menu.ppm").u8string(),o);
    check(r.active_image==first && !r.image_switches && r.final_center.x==512 && r.final_center.y==384 && r.final_zoom==1.5 && project->generation()==menu_generation && read(path)==menu_bytes,"a row menu must not open its image or change project data");
    o=options(directory,"browser-inspector","project");o.inputs={more_down,more_up,key(7,GuiInputKind::EscapeKey)};
    r=run_project_gui(project,16,(directory/"browser-selected-inspector.ppm").u8string(),o);
    check(r.active_image==first && !r.image_switches && project->generation()==menu_generation && read(path)==menu_bytes,"inspecting an unopened selected entry must preserve the viewed slide and saved data");
    o=options(directory,"browser-focused-enter","project");o.inputs={key(2,GuiInputKind::PolygonKey)};
    int frame=3;for(const Point point:{Point{400,300},Point{480,300},Point{480,360}}) {
        auto vertex=down;vertex.frame=frame++;vertex.target=GuiInputTarget::Image;vertex.x=point.x;vertex.y=point.y;o.inputs.push_back(vertex);
        vertex.frame=frame++;vertex.kind=GuiInputKind::LeftUp;o.inputs.push_back(vertex);
    }
    auto current_down=down;current_down.frame=10;current_down.y=.2;auto current_up=current_down;current_up.frame=11;current_up.kind=GuiInputKind::LeftUp;
    o.inputs.push_back(current_down);o.inputs.push_back(current_up);o.inputs.push_back(key(14,GuiInputKind::FinishKey));r=run_project_gui(project,22,"",o);
    check(r.active_image==first && !r.image_switches && !project->document(first)->object_count() && project->generation()==menu_generation && read(path)==menu_bytes,"Enter on the current catalog entry must not finish a canvas polygon");
    o=options(directory,"browser-remove","project");o.inputs={more_down,more_up,key(7,GuiInputKind::EscapeKey),key(9,GuiInputKind::RemoveImageKey)};
    auto confirm=down;confirm.frame=12;confirm.target=GuiInputTarget::ConfirmRemoveImage;confirm.wait_for=GuiInputWait::TargetReady;
    auto release=confirm;release.frame=13;release.kind=GuiInputKind::LeftUp;release.wait_for=GuiInputWait::Immediate;
    o.inputs.push_back(confirm);o.inputs.push_back(release);
    r=run_project_gui(project,24,"",o);
    check(r.active_image==first && r.image_switches==0 && project->images().size()==1 && project->images().front().id==first,"removal must target the browsed entry without opening it");
    std::cout<<"Catalog single-click camera restore, independent row actions and unopened-entry removal passed\n";
}
static int color_pixels(const std::filesystem::path& path,const Rect& bounds,std::array<unsigned char,3> color) {
    std::ifstream file(path,std::ios::binary);std::string magic;int width=0,height=0,maximum=0;file>>magic>>width>>height>>maximum;file.get();
    check(magic=="P6" && width>0 && height>0 && maximum==255,"native capture must be RGB PPM");
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size());check(bool(file),"native RGB capture read");
    int count=0;for(int y=std::max(0,int(bounds.y));y<std::min(height,int(bounds.y+bounds.height));++y)
        for(int x=std::max(0,int(bounds.x));x<std::min(width,int(bounds.x+bounds.width));++x) {
            const auto at=(std::size_t(y)*width+x)*3;count+=pixels[at]==color[0] && pixels[at+1]==color[1] && pixels[at+2]==color[2];
        }
    return count;
}
static void project_switcher(const std::filesystem::path& directory) {
    const auto a=directory/"Study A.qpxproj",b=directory/"Study B.qpxproj";auto initial=fixture(a);
    auto second=std::make_shared<Project>();second->add_document(std::make_shared<Document>(Image(64,64,std::vector<std::uint8_t>(64*64*3,90))),"Second study");
    second->set_view(second->active_image(),{{32,32},1.5,{}});second->save_new(b.u8string());
    auto make_options=[&](const std::string& name) {
        auto o=options(directory,name,"project");ui::ProjectPreferences prefs;prefs.remember(b.u8string());
        write(std::filesystem::u8path(o.layout_path),read(std::filesystem::u8path(o.layout_path))+prefs.header());
        GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=150./o.width;down.y=82./o.height;
        auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
        down.frame=9;down.y=134./o.height;up=down;up.frame=10;up.kind=GuiInputKind::LeftUp;o.inputs.push_back(down);o.inputs.push_back(up);return o;
    };
    const auto original=read(a);auto o=make_options("project-switcher");
    auto r=run_project_gui(initial,36,(directory/"project-switcher.ppm").u8string(),o);
    check(r.project_replacements==1 && initial->path()==b.u8string() && r.project_error.empty() && read(a)==original,"recent-project choice must use normal project replacement: "+r.project_error);
    check(color_pixels(directory/"project-switcher.ppm",{r.browser_bounds.x,r.browser_bounds.y,80,r.browser_bounds.height},{90,90,90})>200 && color_pixels(directory/"project-switcher.ppm",{r.browser_bounds.x,r.browser_bounds.y,80,r.browser_bounds.height},{30,120,210})==0,"project switch must replace the sidebar thumbnail cache");
    ui::ProjectPreferences saved;saved.load(read(std::filesystem::u8path(o.layout_path)));check(!saved.recent.empty() && saved.recent.front()==b.u8string(),"successful project switch must persist recent order");
    initial=Project::open(a.u8string());initial->rename_image(initial->active_image(),"Unsaved entry name");o=make_options("project-switcher-cancel");
    GuiInput cancel;cancel.frame=15;cancel.kind=GuiInputKind::LeftDown;cancel.target=GuiInputTarget::CancelRecoveryReplacement;cancel.wait_for=GuiInputWait::TargetReady;
    auto release=cancel;release.frame=16;release.kind=GuiInputKind::LeftUp;release.wait_for=GuiInputWait::Immediate;o.inputs.push_back(cancel);o.inputs.push_back(release);
    r=run_project_gui(initial,36,"",o);
    check(!r.project_replacements && initial->path()==a.u8string() && initial->dirty() && initial->images().front().name=="Unsaved entry name" && read(a)==original,"cancelling a recent-project switch must preserve unsaved work");
    std::cout<<"Recent project switch, cache replacement, persisted history and unsaved cancellation passed\n";
}
static void shortcuts(const std::filesystem::path& directory) {
    auto project=fixture(directory/"shortcuts.qpxproj");
    auto pixel=options(directory,"pixel-f5","project");pixel.inputs={key(3,GuiInputKind::PixelClassifierKey)};
    const auto p=run_project_gui(project,20,"",pixel);
    check(read(std::filesystem::u8path(pixel.layout_path)).find("[Window][Pixel classifiers]")!=std::string::npos && p.recovery_path.empty(),"F5 also starts recovery discovery");
    auto recovery=options(directory,"recovery-f5","project");recovery.inputs={key(3,GuiInputKind::FindRecoveriesKey)};
    const auto r=run_project_gui(project,24,"",recovery);
    check(r.autosave_error.empty() && read(std::filesystem::u8path(recovery.layout_path)).find("[Window][Pixel classifiers]")==std::string::npos,"recovery shortcut toggles classifier");
    std::cout<<"Pixel F5 and recovery Ctrl+Shift+F5 act independently\n";
}
static void relink_missing(const std::filesystem::path& directory) {
    const auto original=directory/"relink-original.ppm",replacement=directory/"relink-moved.ppm";
    write(original,std::string("P6\n64 64\n255\n")+std::string(64*64*3,char(90)));
    auto project=std::make_shared<Project>();const auto image=project->add_image(original.u8string());
    const auto path=directory/"relink.qpxproj";project->save_new(path.u8string());std::filesystem::rename(original,replacement);
    project=Project::open(path.u8string());check(!project->images().front().available,"moved-file fixture must be unavailable");
    auto o=options(directory,"relink-missing","project");o.project_image_path=replacement.u8string();
    GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::ProjectList;down.x=.85;down.y=.4;
    auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up,key(10,GuiInputKind::FinishKey)};
    const auto report=run_project_gui(project,28,(directory/"relink-restored.ppm").u8string(),o);
    check(report.project_error.empty() && project->images().front().available && report.uploaded_tiles && !report.pending_tiles,"native desktop must relink the missing source: "+report.project_error);
    const auto pixels=project->document(image)->server()->read_region({0,0,0,1,1}).bytes();
    check(pixels==std::vector<std::uint8_t>({90,90,90,255}),"relinked source changed RGB bytes");
    const auto generation=project->generation();const auto saved=read(path);
    o=options(directory,"relink-row-menu","project");down.x=.95;up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
    const auto menu=run_project_gui(project,16,(directory/"linked-image-row-menu.ppm").u8string(),o);
    check(menu.active_image==image && !menu.image_switches && menu.project_error.empty() && project->generation()==generation && read(path)==saved,"opening linked-image source actions must preserve image identity and project data");
    std::cout<<"Native missing-source desktop relink and exact RGB passed\n";
}
static void review_catalog(const std::filesystem::path& directory,const std::filesystem::path& path) {
    const auto original=read(path);
    for(const std::string mode:{"default","scaled","narrow","single-click","row-menu","grouped","group-folded","project-menu"}) {
        auto project=Project::open(path.u8string());const auto images=project->images();check(images.size()>=2,"review project needs two images");
        const auto first=project->active_image(),generation=project->generation();
        const float scale=mode=="scaled" || mode=="narrow"?2:1;
        auto o=options(directory,"review-"+mode,"project","region",scale);o.width=scale==1?1440:1600;o.height=scale==1?960:1000;
        if(mode=="narrow") {o.width=1100;o.height=800;}
        if(mode=="scaled") {auto ini=read(std::filesystem::u8path(o.layout_path));const auto at=ini.find("theme=dark");ini.replace(at,10,"theme=light");write(std::filesystem::u8path(o.layout_path),ini);}
        if(mode=="grouped" || mode=="group-folded") {
            write(std::filesystem::u8path(o.layout_path),read(std::filesystem::u8path(o.layout_path))+"; qupathxx project-group="+ui::ProjectPreferences::encode("Review scenario")+"\n");
            if(mode=="group-folded") {GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::ProjectList;down.x=.3;down.y=.04;auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};}
        }
        if(mode=="project-menu") {GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=150./o.width;down.y=82./o.height;auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};}
        if(mode=="single-click" || mode=="row-menu") {
            GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::ProjectList;down.x=mode=="row-menu"?.95:.4;down.y=1.5/images.size();
            auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
        } else if(o.inputs.empty()) {GuiInput move;move.frame=2;move.kind=GuiInputKind::Move;move.target=GuiInputTarget::Window;move.x=.9;move.y=.8;o.inputs={move};}
        const auto report=run_project_gui(project,24,(directory/("review-"+mode+".ppm")).u8string(),o);
        check(report.project_error.empty() && report.ui_overflow_items==0 && report.canvas_visible && report.tiles_settled && read(path)==original,"populated catalog review "+mode+": "+report.project_error+report.ui_overflow_label);
        const auto expected=mode=="single-click"?images[1].id:first;
        check(report.active_image==expected && report.image_switches==(mode=="single-click"?1:0),"review row action activated a different image: "+mode);
        const auto view=project->view(expected);if(view)check(report.final_center.x==view->center.x && report.final_center.y==view->center.y && report.final_zoom==view->zoom,"review camera changed: "+mode);
        if(mode!="single-click")check(project->generation()==generation,"non-opening row action changed project data: "+mode);
    }
    std::cout<<"Populated native catalog default/scaled/narrow/single-click/menu review passed\n";
}
static void close_dialogs(const std::filesystem::path& directory) {
    auto prepared=[&](const std::string& name) {
        auto project=fixture(directory/(name+".qpxproj"),128,96);auto doc=project->document(project->active_image());
        const auto object=doc->add_rectangle({10,12,30,24},"Original");project->save(project->path());doc->rename_annotation(object,"Captured");return project;
    };
    auto click=[&](std::vector<GuiInput>& inputs,int frame,GuiInputTarget target) {
        GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=target;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;inputs.push_back(down);inputs.push_back(up);
    };
    for(const std::string mode:{"normal","scaled","narrow","translated"}) {
        auto project=prepared("close-view-"+mode);const auto before=read(std::filesystem::u8path(project->path()));
        auto o=options(directory,"dialog-close-"+mode,"project","measurements",mode=="normal"?1:2);
        if(mode=="narrow" || mode=="translated") {o.width=900;o.height=700;}
        if(mode=="translated") {auto ini=read(std::filesystem::u8path(o.layout_path));const auto at=ini.find("language=en");ini.replace(at,11,"language=fr");write(std::filesystem::u8path(o.layout_path),ini);}
        const auto* language=std::getenv("QUPATHXX_UI_LANGUAGE");const std::string previous_language=language?language:"";
        if(mode=="translated") {
#ifdef _WIN32
            _putenv_s("QUPATHXX_UI_LANGUAGE","fr");
#else
            setenv("QUPATHXX_UI_LANGUAGE","fr",1);
#endif
        }
        o.inputs={key(3,GuiInputKind::CloseWindow)};const auto report=run_project_gui(project,20,(directory/("dialog-close-"+mode+".ppm")).u8string(),o);
        if(mode=="translated") {
#ifdef _WIN32
            _putenv_s("QUPATHXX_UI_LANGUAGE",previous_language.c_str());
#else
            if(language)setenv("QUPATHXX_UI_LANGUAGE",previous_language.c_str(),1);else unsetenv("QUPATHXX_UI_LANGUAGE");
#endif
        }
        check(report.ui_overflow_items==0 && report.project_error.empty() && project->dirty() && read(std::filesystem::u8path(project->path()))==before,"viewing the bounded close dialog changed data or overflowed");
    }
    {
        auto project=std::make_shared<Project>();project->add_document(std::make_shared<Document>(Image::demo()),"First-save image");
        auto o=options(directory,"dialog-close-unsaved","project","measurements",2);o.width=900;o.height=700;o.inputs={key(3,GuiInputKind::CloseWindow)};
        const auto report=run_project_gui(project,20,(directory/"dialog-close-unsaved.ppm").u8string(),o);
        check(report.ui_overflow_items==0 && report.project_error.empty() && project->dirty() && project->path().empty(),"first-save dialog must keep its editable destination and footer usable without publishing a project");
    }
    for(const std::string action:{"cancel","save","discard","recovery","failed-save-cancel"}) {
        auto project=prepared("close-action-"+action);auto doc=project->document(project->active_image());const auto path=std::filesystem::u8path(project->path());const auto before=read(path);
        auto o=options(directory,"dialog-close-action-"+action,"project","measurements",action=="save"?2:1);if(action=="save") {o.width=900;o.height=700;}
        o.autosave_path=(directory/("close-"+action+".qpxrecover")).u8string();
        if(action=="failed-save-cancel") {std::filesystem::rename(path,path.u8string()+".original");std::filesystem::create_directory(path);}
        o.inputs={key(5,GuiInputKind::CloseWindow)};
        if(action=="cancel") {o.inputs.push_back(key(9,GuiInputKind::UndoKey));click(o.inputs,14,GuiInputTarget::CancelClose);}
        if(action=="save" || action=="failed-save-cancel")click(o.inputs,12,GuiInputTarget::SaveClose);
        if(action=="discard") {o.inputs.insert(o.inputs.begin(),key(2,GuiInputKind::AutosaveKey));click(o.inputs,16,GuiInputTarget::DiscardClose);}
        if(action=="recovery")click(o.inputs,12,GuiInputTarget::KeepRecoveryClose);
        if(action=="failed-save-cancel")click(o.inputs,22,GuiInputTarget::CancelClose);
        const auto report=run_project_gui(project,80,"",o);
        check(report.replay_complete && report.ui_overflow_items==0 && doc->annotation(1).name=="Captured","close dialog action lost draft state, replay or bounds: "+action+" "+report.project_error);
        if(action=="save") {
            check(report.frames<80 && report.saves_completed==1 && report.project_error.empty() && !project->dirty() && Project::open(path.u8string())->document(1)->annotation(1).name=="Captured","scaled Save and close did not persist native state");
        } else if(action=="failed-save-cancel") {
            check(report.frames>=80 && !report.project_error.empty() && project->dirty() && read(path.u8string()+".original")==before,"failed save did not preserve state and allow Cancel");
        } else {
            check(project->dirty() && read(path)==before,"closing changed primary saved bytes: "+action);
            if(action=="cancel")check(report.frames>=80,"Cancel closed the workspace");
            // Discard may cancel the in-flight write before publication. Either
            // outcome must finish cleanup; published cleanup is checked separately.
            if(action=="discard")check(report.frames<80 && report.autosaves_completed<=1 && report.autosave_error.empty() && !report.recovery_path.empty() && !std::filesystem::exists(std::filesystem::u8path(report.recovery_path)),"Discard did not finish owned recovery cleanup: frames="+std::to_string(report.frames)+" completed="+std::to_string(report.autosaves_completed)+" path="+report.recovery_path+" error="+report.autosave_error);
            if(action=="recovery")check(report.frames<80 && report.autosaves_completed==1 && report.autosave_error.empty() && Project::recover(report.recovery_path)->document(1)->annotation(1).name=="Captured","Keep recovery and close did not await recoverable publication");
        }
    }
    std::cout<<"Shared close dialog: bounded normal/scaled/narrow, Cancel/blocked Undo, Save, Discard/owned recovery, Keep recovery and failed-save Cancel passed\n";
}
#include "gui_dialog_workflow_checks.hpp"
#include "gui_export_workflow_checks.hpp"
#include "gui_review_workflow_checks.hpp"
#include "gui_image_inspector_checks.hpp"
#include "gui_panel_refinement_checks.hpp"
#include "gui_header_grouping_checks.hpp"
int main(int argc,char** argv) {
    std::cout<<std::unitbuf;
    try {
        geometry();inspector_controls();project_filter_controls();docking_controls();if(!std::getenv("QUPATHXX_TEST_GUI")) {std::cout<<"Native organization replay skipped: opt-in QUPATHXX_TEST_GUI\n";return 0;}
        const auto dir=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/("qpx-ui-organization-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
        if(argc>2 && std::string(argv[2])=="--header-grouping") {header_grouping_checks::workflows(dir);return 0;}
        if(argc>2 && std::string(argv[2])=="--analysis-details") {panel_refinement_workflows(dir,true);return 0;}
        if(argc>2 && std::string(argv[2])=="--panel-refinement") {panel_refinement_workflows(dir);return 0;}
        if(argc>2 && std::string(argv[2])=="--image-inspector") {image_inspector_workflows(dir);return 0;}
        if(argc>2 && std::string(argv[2])=="--review-ux") {review_ux_workflows(dir);return 0;}
        if(argc>2 && std::string(argv[2])=="--export-sampling") {export_sampling_workflows(dir);return 0;}
        navigation(dir);catalog_navigation(dir);overview_failure(dir);shortcuts(dir);relink_missing(dir);project_switcher(dir);close_dialogs(dir);dialog_workflows(dir);if(argc>2)review_catalog(dir,std::filesystem::u8path(argv[2]));dialog_checks::metadata_views(dir);return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
