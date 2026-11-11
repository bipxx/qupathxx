#pragma once
#include "../gui_header.hpp"
#include "qupathxx/provenance.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>

namespace header_grouping_checks {
using Command=ui::HeaderCommand;
struct Variant {const char* name;int width,height;float scale;const char* language;};
inline constexpr std::array variants={Variant{"normal",1100,800,1,"en"},Variant{"french-200",1440,1000,2,"fr"},Variant{"narrow",640,900,2,"fr"}};
inline int link_launches=0;
inline std::string launched_url;
inline bool refuse_link(ImGuiContext*,const char* url){++link_launches;launched_url=url;return false;}

struct NativeHeader {
    ui::i18n::State previous_language=ui::i18n::state;
    ui::design::State previous_theme=ui::design::state;
    ImGuiContext* previous_context=ImGui::GetCurrentContext();
    ImGuiContext* context=ImGui::CreateContext();
    ui::HeaderSnapshot snapshot;
    ui::HeaderTrace trace;
    detail::GuiHelpWindow help;
    std::map<Command,int> calls;
    int analyze_draws=0,language_shortcuts=0,help_shortcuts=0;
    bool settings_requested=false,help_open=false,link_probe=false;
    std::optional<ImVec2> settings_anchor;
    Rect link_bounds;
    std::string link_error;
    Variant variant;
    explicit NativeHeader(Variant input):variant(input) {
        ImGui::SetCurrentContext(context);
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={float(input.width),float(input.height)};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        ui::i18n::choose(*ui::i18n::exact_language(input.language));
        ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);ImGui::GetStyle().ScaleAllSizes(input.scale);ImGui::GetStyle().FontSizeBase=16*input.scale;
        snapshot.image=snapshot.project=snapshot.project_dirty=snapshot.can_undo=snapshot.can_redo=snapshot.can_copy=snapshot.can_paste=snapshot.workspace=snapshot.reload_ui=true;
        snapshot.ui_size=input.scale;snapshot.theme=ui::design::Theme::Light;
        settle();
    }
    ~NativeHeader(){ImGui::DestroyContext(context);ImGui::SetCurrentContext(previous_context);ui::i18n::state=previous_language;ui::design::state=previous_theme;}
    void frame() {
        ImGui::NewFrame();ui::begin_frame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::Begin("Header contract host",nullptr,ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking);
        ImGui::PopStyleVar();
        const auto shortcuts=ui::header_shortcuts(snapshot.editing_blocked);language_shortcuts+=shortcuts.language;help_shortcuts+=shortcuts.help;
        ui::draw_header(snapshot,[&](Command command){++calls[command];},[&]{++analyze_draws;},&trace);
        if(settings_requested) {
            const float side=ui::design::metrics.activity_width*variant.scale;
            ImGui::SetNextWindowPos(settings_anchor.value_or(ImVec2{side,float(variant.height)}),ImGuiCond_Appearing,{0,1});ImGui::OpenPopup("Workspace settings");settings_requested=false;
        }
        if(ui::BeginPopupMenu("Workspace settings")){ui::draw_header_settings(snapshot,[&](Command command){++calls[command];},&trace);ImGui::EndPopup();}
        help.draw(*build_environment());
        help_open=help.is_open();
        if(link_probe) {
            detail::help_detail::link("User guide###HelpUserGuide",detail::help_detail::user_guide_url,link_error);
            const auto p=ImGui::GetItemRectMin(),size=ImGui::GetItemRectSize();link_bounds={p.x,p.y,size.x,size.y};
        }
        ImGui::End();ImGui::Render();
    }
    void settle(){for(int i=0;i<4;++i)frame();}
    void click(Rect bounds) {
        check(bounds.width>0 && bounds.height>0,"Header action has no native hit target");auto& io=ImGui::GetIO();
        io.AddMousePosEvent(float(bounds.x+bounds.width/2),float(bounds.y+bounds.height/2));settle();
        io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);settle();
    }
    void escape(){auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);settle();}
    void close_menus(){for(int i=0;i<6 && !context->OpenPopupStack.empty();++i)escape();check(context->OpenPopupStack.empty(),"Header menu did not close through Escape");}
    void open(const char* caption) {
        const std::array<const char*,4> left={"File","Edit","View","Analyze"};
        if(std::string(caption)=="Help"){click(trace.menus[4]);return;}
        if(trace.compact) {
            click(trace.menus[0]);
            group(caption);
        } else {
            const auto found=std::find_if(left.begin(),left.end(),[&](const char* value){return std::string(value)==caption;});
            check(found!=left.end(),"Unknown test header group");click(trace.menus[std::size_t(found-left.begin())]);
        }
    }
    std::string state() const {
        std::string result=std::string(" / ")+variant.name+" / popups";
        for(const auto& popup:context->OpenPopupStack)result+=' '+std::string(popup.Window?popup.Window->Name:"<uncreated>");
        result+=" / commands";for(const auto& entry:trace.commands)result+=' '+std::to_string(int(entry.first));
        result+=" / mouse="+std::to_string(context->IO.MousePos.x)+","+std::to_string(context->IO.MousePos.y);
        result+=" / hovered="+std::string(context->HoveredWindow?context->HoveredWindow->Name:"<none>");return result;
    }
    void group(const char* caption) {
        check(trace.groups.count(caption)!=0,"Header subgroup is missing: "+std::string(caption)+state());
        const auto found=trace.group_ids.find(caption);
        check(found!=trace.group_ids.end() && found->second!=0,"Header subgroup has no captured native ID: "+std::string(caption)+state());
        // BeginMenu opens on hover as well as activation. Do not hover-open it,
        // then click a cached rectangle or derive an ID from a sibling popup.
        const auto id=found->second;
        ImGui::GetIO().AddMousePosEvent(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());frame();
        ImGui::ActivateItemByID(id);settle();
    }
    void command(Command command){check(trace.commands.count(command)!=0,"Header command is not presented: "+std::to_string(int(command))+state());const auto before=calls[command];click(trace.commands.at(command));check(calls[command]==before+1,"Header dispatched wrong or duplicate command: "+std::to_string(int(command))+state());}
    void chord(ImGuiKey key,bool ctrl=false,bool shift=false) {
        auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,ctrl);io.AddKeyEvent(ImGuiMod_Shift,shift);io.AddKeyEvent(key,true);frame();
        io.AddKeyEvent(key,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);io.AddKeyEvent(ImGuiMod_Shift,false);settle();
    }
};

static void headless() {
    for(const auto variant:variants) {
        NativeHeader f(variant);const auto initial=f.trace;
        const auto help=initial.menus[4];check(help.width>0 && help.height>0 && help.x+help.width<=variant.width+1,"Help is clipped outside the header");
        check(help.x>variant.width*.65 && initial.menus[0].x+initial.menus[0].width<=help.x,"Help is not on the right of the header");
        for(int i=0;i<4;++i)if(initial.menus[i].width>0) {
            const auto menu=initial.menus[i];check(menu.y==help.y && menu.height==help.height,"Header wrapped onto another row");
            check(menu.x>=0 && menu.x+menu.width<=help.x,"Header groups overlap Help");
            if(i && initial.menus[i-1].width>0)check(initial.menus[i-1].x+initial.menus[i-1].width<=menu.x+1,"Header groups overlap");
        }
        if(initial.compact)for(int i=1;i<4;++i)check(initial.menus[i].width==0,"Compact header duplicated a left group");
        f.open("File");f.command(Command::OpenProject);f.close_menus();
        f.open("Edit");f.command(Command::CopyObjects);f.close_menus();
        f.open("View");f.group("Panels");f.command(Command::ImagePanel);f.close_menus();
        f.open("View");f.group("Layout");f.command(Command::ResetLayout);f.close_menus();
        f.open("Analyze");check(f.analyze_draws>0,"Analyze did not invoke existing native tool menu authority");f.close_menus();
        f.open("Help");f.command(Command::QuickReference);f.close_menus();
        f.open("Help");f.command(Command::Documentation);f.close_menus();
        f.open("Help");f.command(Command::About);f.close_menus();
        f.settings_requested=true;f.settle();f.group("General");f.command(Command::ChooseLanguage);f.close_menus();
        f.settings_requested=true;f.settle();f.group("Appearance");f.command(Command::ThemeLight);f.close_menus();
        f.settings_requested=true;f.settle();f.group("Appearance");f.command(Command::IncreaseUiSize);f.close_menus();
        f.settings_requested=true;f.settle();f.group("Advanced");f.command(Command::ReloadUi);f.close_menus();
        f.chord(ImGuiKey_L,true,true);f.chord(ImGuiKey_F1);check(f.language_shortcuts==1 && f.help_shortcuts==1,"Native header shortcut changed or repeated");
        f.snapshot.editing_blocked=true;f.settle();f.chord(ImGuiKey_L,true,true);f.chord(ImGuiKey_F1);
        check(f.language_shortcuts==1 && f.help_shortcuts==1,"Blocked header shortcut escaped its owner guard");f.snapshot.editing_blocked=false;
        for(const auto page:{detail::HelpPage::QuickReference,detail::HelpPage::Documentation,detail::HelpPage::About}) {
            f.help.request(page);f.settle();check(f.help_open && f.help.page()==page,"Help did not open the selected private page");
            const auto* dialog=ImGui::FindWindowByName(ui::dialog_definition(ui::DialogKind::Help).name);
            check(dialog && dialog->Pos.x>=0 && dialog->Pos.y>=0 && dialog->Pos.x+dialog->Size.x<=variant.width+1 && dialog->Pos.y+dialog->Size.y<=variant.height+1,
                "Help dialog is outside the viewport");
            check(ui::layout_stats.overflow_items==0,"Help body controls overflow: "+std::string(variant.name));
            f.escape();check(!f.help_open,"Help Escape did not dismiss the registered modal");
        }
    }
    // Availability uses the same actual menu widgets; disabled actions must not
    // publish a callback even when their visible target is clicked.
    NativeHeader disabled(variants[0]);disabled.snapshot.project_dirty=false;disabled.open("File");
    check(disabled.trace.commands.count(Command::SaveProject)!=0,"Disabled Save project was hidden");disabled.click(disabled.trace.commands.at(Command::SaveProject));
    check(disabled.calls[Command::SaveProject]==0,"Clean project Save command escaped enablement");disabled.close_menus();
    disabled.snapshot.ui_size=3;disabled.settings_requested=true;disabled.settle();disabled.group("Appearance");
    disabled.click(disabled.trace.commands.at(Command::IncreaseUiSize));check(disabled.calls[Command::IncreaseUiSize]==0,"UI size exceeded its existing upper bound");disabled.close_menus();
    {
        NativeHeader compact(variants[0]);compact.snapshot.workspace=false;compact.settle();
        compact.open("Edit");compact.group("Workspace settings");compact.group("General");compact.command(Command::ChooseLanguage);compact.close_menus();
        compact.open("Edit");compact.group("Workspace settings");compact.group("Appearance");compact.command(Command::ThemeLight);compact.close_menus();
    }
    {
        NativeHeader link(variants[0]);link_launches=0;launched_url.clear();ImGui::GetPlatformIO().Platform_OpenInShellFn=refuse_link;link.link_probe=true;link.settle();
        check(link_launches==0 && link.link_error.empty(),"Passive Help layout launched a browser or created an error");
        link.click(link.link_bounds);
        check(link_launches==1 && launched_url==detail::help_detail::user_guide_url && link.link_error.find(launched_url)!=std::string::npos &&
            link.link_error.find("Could not open the link")!=std::string::npos,"Explicit failed Help link did not produce its bounded error and exact URL");
    }
    std::cout<<"Native header grouping: one row/right Help, compact groups, actual menus/settings/callbacks, shortcuts, owner guards and bounded Help pages/Escape passed\n";
}

static void click_window(std::vector<GuiInput>& inputs,int frame,Rect bounds,int width,int height) {
    GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;
    down.x=(bounds.x+bounds.width/2)/width;down.y=(bounds.y+bounds.height/2)/height;
    auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;inputs.push_back(down);inputs.push_back(up);
}
static void workflows(const std::filesystem::path& directory) {
    headless();
    int index=0;
    for(const auto variant:variants) {
        const auto page=index==0?Command::QuickReference:index==1?Command::Documentation:Command::About;++index;
        Rect help_menu,help_command;
        {NativeHeader native(variant);help_menu=native.trace.menus[4];native.open("Help");help_command=native.trace.commands.at(page);}
        const auto project=fixture(directory/(std::string(variant.name)+".qpxproj"),128,96);const auto document=project->document(1);
        const auto object=document->add_rectangle({10,12,30,24},"Original");document->set_calibration(.5,.25);project->save(project->path());document->rename_annotation(object,"Captured");
        const auto saved=read(std::filesystem::u8path(project->path()));const auto revision=document->revision();
        auto o=options(directory,variant.name,"project");o.width=variant.width;o.height=variant.height;o.ui_scale=variant.scale;
        auto ini=read(std::filesystem::u8path(o.layout_path));const auto at=ini.find("language=en");check(at!=std::string::npos,"Header fixture language marker missing");ini.replace(at,11,std::string("language=")+variant.language);write(std::filesystem::u8path(o.layout_path),ini);
        click_window(o.inputs,5,help_menu,o.width,o.height);click_window(o.inputs,11,help_command,o.width,o.height);o.inputs.push_back(key(18,GuiInputKind::UndoKey));
        const auto report=run_project_gui(project,26,(directory/(std::string(variant.name)+"-help.ppm")).u8string(),o);
        check(report.replay_complete && report.project_error.empty() && report.ui_overflow_items==0,"Header/Help native replay failed: "+std::string(variant.name)+" / "+report.ui_overflow_label);
        check(document->annotation(object).name=="Captured" && document->revision()==revision,"Help menu/modal allowed an underlying Undo");
        check(read(std::filesystem::u8path(project->path()))==saved && document->pixel_width_um()==.5 && document->pixel_height_um()==.25 &&
            document->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({30,120,210,255}),"Header changed saved scientific inputs");
        auto retained=read(std::filesystem::u8path(o.layout_path));check(retained.find(std::string("; qupathxx language=")+variant.language)!=std::string::npos,"Header changed language preference");
        o.inputs={key(5,GuiInputKind::EscapeKey)};GuiInput reset;reset.frame=10;reset.kind=GuiInputKind::LeftDown;reset.target=GuiInputTarget::ResetLayout;reset.wait_for=GuiInputWait::TargetReady;
        auto release=reset;release.frame=11;release.kind=GuiInputKind::LeftUp;release.wait_for=GuiInputWait::Immediate;o.inputs.push_back(reset);o.inputs.push_back(release);
        const auto reset_report=run_project_gui(project,22,(directory/(std::string(variant.name)+"-reset.ppm")).u8string(),o);
        retained=read(std::filesystem::u8path(o.layout_path));
        check(reset_report.replay_complete && reset_report.project_error.empty() && retained.find(std::string("; qupathxx language=")+variant.language)!=std::string::npos &&
            retained.find("; qupathxx theme=dark")!=std::string::npos && document->revision()==revision && read(std::filesystem::u8path(project->path()))==saved,
            "Reset layout changed appearance/language or project data");
    }
    {
        const auto variant=variants[0];Rect general,language,appearance,light;
        const auto project=fixture(directory/"settings.qpxproj",128,96);const auto document=project->document(1);
        const auto object=document->add_rectangle({10,12,30,24},"Original");document->set_calibration(.5,.25);project->save(project->path());document->rename_annotation(object,"Captured");
        const auto saved=read(std::filesystem::u8path(project->path()));const auto revision=document->revision();
        auto o=options(directory,"settings","project");
        const auto placement=run_project_gui(project,3,{},o);
        check(placement.replay_complete && placement.browser_bounds.width>0 && placement.browser_bounds.height>0,"Native Settings placement probe has no Project pane");
        // In the seeded default dock the rail ends beside the Project pane,
        // above the status footer. Reuse those actual native window bounds.
        const auto right=placement.browser_bounds.x,bottom=placement.browser_bounds.y+placement.browser_bounds.height;
        const auto side=ui::design::metrics.activity_row*placement.ui_scale;
        const Rect gear{right-ui::design::metrics.activity_width*placement.ui_scale,bottom-side,
            ui::design::metrics.activity_width*placement.ui_scale,side};
        {NativeHeader native(variant);native.settings_anchor=ImVec2{float(right),float(bottom)};native.settings_requested=true;native.settle();general=native.trace.groups.at("General");native.group("General");language=native.trace.commands.at(Command::ChooseLanguage);native.close_menus();
            native.settings_requested=true;native.settle();appearance=native.trace.groups.at("Appearance");native.group("Appearance");light=native.trace.commands.at(Command::ThemeLight);}
        click_window(o.inputs,5,gear,o.width,o.height);click_window(o.inputs,11,general,o.width,o.height);click_window(o.inputs,17,language,o.width,o.height);
        o.inputs.push_back(key(23,GuiInputKind::UndoKey));o.inputs.push_back(key(28,GuiInputKind::EscapeKey));o.inputs.push_back(key(31,GuiInputKind::EscapeKey));
        o.inputs.push_back(key(35,GuiInputKind::UndoKey));o.inputs.push_back(key(39,GuiInputKind::RedoKey));
        const auto language_report=run_project_gui(project,46,(directory/"settings-language-escape.ppm").u8string(),o);
        check(language_report.replay_complete && language_report.project_error.empty() && language_report.ui_overflow_items==0 && document->object_count()==1 &&
            document->annotation(object).name=="Captured" && document->revision()==revision+2,"Settings language modal did not block Undo, then release it after Escape / objects="+
            std::to_string(document->object_count())+" revision="+std::to_string(document->revision())+" expected="+std::to_string(revision+2)+
            " / "+(document->objects().count(object)?document->annotation(object).name:"<removed>")+" / "+language_report.project_error);
        check(read(std::filesystem::u8path(o.layout_path)).find("; qupathxx language=en")!=std::string::npos,"Language Escape changed the chosen language");
        o.inputs.clear();click_window(o.inputs,5,gear,o.width,o.height);click_window(o.inputs,11,appearance,o.width,o.height);click_window(o.inputs,17,light,o.width,o.height);
        GuiInput reset;reset.frame=28;reset.kind=GuiInputKind::LeftDown;reset.target=GuiInputTarget::ResetLayout;reset.wait_for=GuiInputWait::TargetReady;
        auto release=reset;release.frame=29;release.kind=GuiInputKind::LeftUp;release.wait_for=GuiInputWait::Immediate;o.inputs.push_back(reset);o.inputs.push_back(release);
        const auto theme_report=run_project_gui(project,38,(directory/"settings-light-reset.ppm").u8string(),o);const auto preferences=read(std::filesystem::u8path(o.layout_path));
        check(theme_report.replay_complete && theme_report.project_error.empty() && preferences.find("; qupathxx theme=light")!=std::string::npos &&
            preferences.find("; qupathxx language=en")!=std::string::npos,"Settings Appearance did not apply Light or Reset discarded preferences");
        check(document->annotation(object).name=="Captured" && document->revision()==revision+2 && read(std::filesystem::u8path(project->path()))==saved &&
            document->pixel_width_um()==.5 && document->pixel_height_um()==.25 &&
            document->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({30,120,210,255}),"Settings changed source/project scientific state");
    }
    std::cout<<"Actual header/Help: normal/French200/narrow, blocked underlying Undo, Reset preference retention and exact project/calibration/RGB passed\n";
}
}
