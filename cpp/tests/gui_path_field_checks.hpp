#pragma once
#include "qupathxx/core.hpp"
#include "../gui_path_field.hpp"
#include "../gui_project_preferences.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace qpx::ui {
struct PathFieldTestAccess {
    static int draws(const PathField& field){return field.draw_count_frame_==ImGui::GetFrameCount()?field.draw_count_:0;}
    static auto refreshes(const PathField& field){return field.refresh_count_;}
    static const PathSpec& spec(const PathField& field){return field.spec_;}
    static ImVec2 center(const PathField& field){return field.input_bounds_.GetCenter();}
    static Rect browse(const PathField& field){const auto& r=field.browse_bounds_;return {r.Min.x,r.Min.y,r.GetWidth(),r.GetHeight()};}
    static detail::GuiImportPicker& picker(PathField& field){return *field.picker_;}
    static bool row_disjoint(const PathField& field){return !field.input_bounds_.Overlaps(field.browse_bounds_);}
    static bool tooltip_requested(const PathField& field){return field.tooltip_requested_;}
    static bool clipped(const PathField& field){return field.clipped_;}
};
}
namespace qpx::detail {
struct GuiImportPickerTestAccess {
    static auto names(const GuiImportPicker& picker) {std::vector<std::string> names;for(const auto& entry:picker.entries_)names.push_back(entry.path().filename().u8string());return names;}
    static void next(GuiImportPicker& picker) {picker.page();}
    static void previous(GuiImportPicker& picker) {picker.previous();}
    static bool more(const GuiImportPicker& picker) {return picker.iterator_!=picker.end_ || picker.page_index_+1<picker.pages_.size();}
    static void all(GuiImportPicker& picker,bool value) {picker.all_files(value);}
    static std::string choose(const GuiImportPicker& picker) {return picker.choose();}
    static PickerMode mode(const GuiImportPicker& picker){return picker.mode_;}
    static std::string filename(const GuiImportPicker& picker){return picker.filename_.data();}
};
}
namespace path_field_checks {
inline void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
// Select through the real picker, with no text-input event. Callers then use
// their original native action and retain its original scientific assertions.
template<class Frame>void choose(qpx::ui::PathField& field,const std::string& path,Frame frame) {
    using namespace qpx;
    const auto& spec=ui::PathFieldTestAccess::spec(field);
    const auto mode=spec.role==ui::PathRole::OpenFile?detail::PickerMode::OpenFile:spec.role==ui::PathRole::NewFile?detail::PickerMode::NewFile:spec.role==ui::PathRole::OpenFolder?detail::PickerMode::OpenFolder:detail::PickerMode::NewFolder;
    field.set("");field.open_picker({},path);
    ui::PathFieldTestAccess::picker(field).open(path,spec.extensions,mode);
    frame();frame();auto* window=ImGui::FindWindowByName("Choose import file");require(window!=nullptr,"Browse-only picker missing");
    const char* action=mode==detail::PickerMode::OpenFile?"Choose file":mode==detail::PickerMode::NewFile?"Choose new output":mode==detail::PickerMode::OpenFolder?"Choose folder":"Choose output folder";
    ImGui::ActivateItemByID(ImHashStr(action,0,window->ID));frame();frame();
    require(!field.picker_active() && field.state().resolved==std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string(),"Browse-only choice differs");
}
inline void headless(const std::filesystem::path& directory,bool bulk=true) {
    namespace fs=std::filesystem;using namespace qpx;using A=detail::GuiImportPickerTestAccess;
    const auto root=fs::absolute(directory/"paths").lexically_normal();fs::create_directories(root/"project");fs::create_directories(root/"last");
    auto last=(root/"last").u8string();ui::PathContext ctx{(root/"project").u8string(),&last};
    require(ui::resolve_user_path("results.csv",ctx)==(root/"project"/"results.csv").u8string(),"project relative resolution");
    ctx.project_directory.clear();require(ui::resolve_user_path("results.csv",ctx)==(root/"last"/"results.csv").u8string(),"last directory resolution");
    const auto unicode=(root/fs::u8path(u8"\u96ea-\u00e9.csv")).u8string();require(ui::resolve_user_path(unicode,ctx)==unicode,"absolute non-ASCII round trip");
    const auto absolute=(root/"project/../exact.csv").u8string();require(ui::resolve_user_path(absolute,ctx)==absolute,"absolute path spelling must remain unchanged");
    require(ui::resolve_user_path(u8"\u96ea.csv",ctx)==(root/"last"/fs::u8path(u8"\u96ea.csv")).u8string(),"relative UTF-8 round trip");
    last.clear();require(ui::resolve_user_path("results.csv",ctx)==(fs::current_path()/"results.csv").u8string(),"process directory fallback");
    require(ui::resolve_user_path("",ctx).empty(),"empty path remains empty");
    const auto existing=root/"project"/"results.csv";{std::ofstream f(existing);f<<"original";}
    ui::PathField field({"Results###fixed-path",ui::PathRole::NewFile,{".csv"},"results.csv"});ctx.project_directory=(root/"project").u8string();field.refresh(ctx);field.set("results.csv");
    require(field.state().valid && field.state().exists && field.state().problem.empty() && field.state().resolved==existing.u8string(),"existing output state");
    field.drop("missing/results.csv");require(!field.state().exists && !field.state().valid && field.state().problem=="Parent folder does not exist","missing parent state");
    field.set(".");require(field.state().exists && !field.state().valid && field.state().problem=="Choose a file path","folder given to file role");
    ui::PathField input({"Input",ui::PathRole::OpenFile,{}});input.refresh(ctx);input.set("later.csv");require(!input.state().valid && !input.state().exists,"missing input");
    {std::ofstream f(root/"project/later.csv");f<<"later";}require(!input.state().exists,"state must cache external writes");input.refresh(ctx);require(input.state().exists && input.state().valid,"explicit refresh after write");
    ui::ProjectPreferences preferences;preferences.last_directory=unicode;ui::ProjectPreferences restored;restored.load(preferences.header());require(restored.last_directory==unicode,"last folder UTF-8 hex round trip");
    for(const auto& invalid:std::vector<std::string>{"zz","00",ui::ProjectPreferences::encode("relative"),std::string(65538,'a'),"c080"}) {
        restored.load("; qupathxx last-folder="+invalid+"\n");require(restored.last_directory.empty(),"invalid folder preference ignored");
    }
    restored.last_directory=unicode;restored.load("; no last-folder\n");require(restored.last_directory.empty(),"missing folder preference clears it");
    if(bulk) {
    const auto sparse=root/"sparse";fs::create_directories(sparse);
    for(int i=0;i<1000;++i){std::ofstream f(sparse/("hidden-"+std::to_string(i)+".txt"));}
    {std::ofstream f(sparse/"selected.qpxproj");}
    detail::GuiImportPicker picker;picker.open(sparse.u8string(),{".qpxproj"},detail::PickerMode::OpenFile);
    require(A::names(picker)==std::vector<std::string>{"selected.qpxproj"},"page one must scan past 1000 nonmatching files");
    A::all(picker,true);require(A::names(picker).size()==64 && A::more(picker),"All files reveals filtered files with 64-entry pages");
    const auto first=A::names(picker);A::next(picker);const auto second=A::names(picker);require(second!=first,"next page advances");
    A::previous(picker);require(A::names(picker)==first,"Previous restores exact page");A::next(picker);require(A::names(picker)==second,"forward replay restores exact page");
    picker=detail::GuiImportPicker{};require(sparse.is_absolute() && sparse.parent_path()==root && sparse.filename()=="sparse" && !fs::is_symlink(sparse),"Sparse cleanup boundary");fs::remove_all(sparse);
    }
    detail::GuiImportPicker picker;
    const auto sorted=root/"sorted";fs::create_directories(sorted/"z-folder");fs::create_directories(sorted/"A-folder");
    for(const char* name:{"z.qpxproj","B.qpxproj","a.qpxproj","hidden.txt"}){std::ofstream f(sorted/name);}
    picker.open(sorted.u8string(),{".qpxproj"},detail::PickerMode::OpenFile);
    require(A::names(picker)==std::vector<std::string>{"A-folder","z-folder","a.qpxproj","B.qpxproj","z.qpxproj"},"folders first then case-insensitive names");
    for(auto mode:{detail::PickerMode::OpenFolder,detail::PickerMode::NewFolder}) {
        picker.open(sorted.u8string(),{},mode);A::all(picker,true);require(A::names(picker)==std::vector<std::string>{"A-folder","z-folder"},"folder roles only list folders even with All files");
    }
    // A capped scan must keep Next enabled even when it found no matches.
    if(bulk) {
    const auto capped=root/"capped";fs::create_directories(capped);
    for(int i=0;i<16385;++i){std::ofstream f(capped/(std::to_string(i)+".txt"));}
    picker.open(capped.u8string(),{".qpxproj"},detail::PickerMode::OpenFile);require(A::names(picker).empty() && A::more(picker),"scan cap preserves continuation");
    A::next(picker);require(!A::more(picker),"capped continuation reaches end");A::previous(picker);require(A::more(picker),"empty cached page retains next page");
    picker=detail::GuiImportPicker{};require(capped.is_absolute() && capped.parent_path()==root && capped.filename()=="capped" && !fs::is_symlink(capped),"Capped cleanup boundary");fs::remove_all(capped);
    }
    std::cout<<"Path resolution/state/preferences and picker filters/sort/64-match pages/16384-entry cap/Previous passed\n";
}
inline void controls(const std::filesystem::path& directory) {
    using namespace qpx;const auto language=ui::i18n::state;
    for(int locale:{0,2}) {
        ui::i18n::choose(locale);ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1100,850};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=ui::design::metrics.font_size*2;}
        std::string last=(directory/"paths/last").u8string();ui::PathContext ctx{(directory/"paths/project").u8string(),&last};
        ui::PathField field({"Filename###PathReplay",ui::PathRole::NewFile,{".csv"},"suggestion.csv"});field.set("initial.csv");
        auto frame=[&](bool focus=false) {
            ImGui::NewFrame();ui::begin_frame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1000,800});ImGui::Begin("Path controls");
            const auto expected=ImHashStr("##value",0,ImHashStr("Filename###PathReplay",0,ImGui::GetCurrentWindow()->ID));
            if(focus)field.request_focus();int hooks=0;
            const bool changed=field.draw(ctx,false,[&]{++hooks;require(ImGui::GetItemID()==expected,"hook must see exact input widget ID");});
            require(ui::PathFieldTestAccess::row_disjoint(field),"Path input overlaps Browse in compact row");
            require(hooks==1,"one input replay hook per frame");require(ui::layout_stats.overflow_items==0,"path controls overflow at normal/French 200%");
            ImGui::End();ImGui::Render();return changed;
        };
        frame();frame();frame(true);frame();io.AddInputCharactersUTF8("x");require(frame(),"actual text edit must report change");
        require(field.state().resolved==std::filesystem::absolute(std::filesystem::u8path(ctx.project_directory)/std::filesystem::u8path(field.text())).lexically_normal().u8string(),"edit refreshes state");
        require(ImGui::GetActiveID()!=0,"drop fixture must have an active text edit");field.drop("dropped.csv");frame();
        require(std::string(field.text())=="dropped.csv","Active ImGui edit buffer overwrote a dropped path");
        ctx.project_directory=(directory/"paths/last").u8string();frame();require(field.state().resolved.find("last")!=std::string::npos,"project context change refreshes state");
        ctx.project_directory.clear();last=(directory/"paths/project").u8string();frame();require(field.state().resolved.find("project")!=std::string::npos,"last directory value change refreshes state");
        field.set("");ImGui::NewFrame();ui::begin_frame();ImGui::Begin("Presentation");ImGui::LogToBuffer();field.draw(ctx);auto content=std::string(ImGui::GetCurrentContext()->LogBuffer.c_str());ImGui::LogFinish();ImGui::End();ImGui::Render();
        require(content.find("Resolved path:")==std::string::npos && content.find(ui::i18n::text("Choose a path"))==std::string::npos,"empty path shows no extra line");
        field.set(std::filesystem::absolute(directory/"absolute.csv").u8string());ImGui::NewFrame();ImGui::Begin("Presentation");ImGui::LogToBuffer();field.draw(ctx);content=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();ImGui::Render();
        require(content.find(ui::i18n::text("Resolved path: %s"))==std::string::npos && content.find("Resolved path:")==std::string::npos,"absolute path must not repeat a resolved line");
        // A separate, inactive input isolates clipping/hover from the prior keyboard edit.
        ImGui::ClearActiveID();auto tooltip_field=std::make_unique<ui::PathField>(ui::PathSpec{"##Tooltip path",ui::PathRole::NewFile,{".csv"}});tooltip_field->refresh(ctx);
        auto tooltip_frame=[&]{ImGui::NewFrame();ui::begin_frame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({500,200});ImGui::Begin("Path hover");tooltip_field->draw(ctx);require(ui::layout_stats.overflow_items==0,"Path tooltip fixture overflow");ImGui::End();ImGui::Render();};
        tooltip_field->set(std::filesystem::absolute(directory/(std::string(180,'x')+".csv")).u8string());tooltip_frame();
        auto center=ui::PathFieldTestAccess::center(*tooltip_field);io.AddMousePosEvent(center.x,center.y);tooltip_frame();tooltip_frame();
        require(tooltip_field->hovered() && ui::PathFieldTestAccess::clipped(*tooltip_field) && ui::PathFieldTestAccess::tooltip_requested(*tooltip_field),"Clipped hovered path must show its full resolved path tooltip");
        bool tooltip_found=false;
        for(const auto* window:ImGui::GetCurrentContext()->Windows)if((window->Flags&ImGuiWindowFlags_Tooltip) && window->LastFrameActive==ImGui::GetFrameCount()) {
            tooltip_found=true;require(window->ContentSizeIdeal.x<=window->WorkRect.GetWidth()+.75f,"Full path tooltip text must wrap inside its viewport-bounded window");
        }
        require(tooltip_found,"Clipped path tooltip window was not rendered");
        tooltip_field->set("x");tooltip_frame();tooltip_frame();
        require(tooltip_field->hovered() && !ui::PathFieldTestAccess::clipped(*tooltip_field) && !ui::PathFieldTestAccess::tooltip_requested(*tooltip_field),"Short hovered path must not show a clipping tooltip");
        field.open_picker(ctx);frame();frame();require(field.picker_active(),"draw alone must service its open picker");
        auto* picker=ImGui::FindWindowByName("Choose import file");require(picker!=nullptr,"PathField picker modal missing");
        ImGui::ActivateItemByID(ImHashStr("Cancel file choice",0,picker->ID));frame();frame();require(!field.picker_active(),"PathField picker Cancel did not dismiss");
        ImGui::DestroyContext();
    }
    ui::i18n::state=language;std::cout<<"PathField exact input IDs/hook/edit/context refresh and normal/French 200% layout passed\n";
}
}
