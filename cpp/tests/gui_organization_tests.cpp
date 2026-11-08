#include "../gui_layout.hpp"
#include "../gui_workflow.hpp"
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
    check(ui::f5_action(false,false,false)==ui::F5Action::PixelClassifier && ui::f5_action(true,true,false)==ui::F5Action::Recoveries,"distinct F5 intents");
    check(ui::f5_action(true,false,false)==ui::F5Action::Ignore && ui::f5_action(false,true,false)==ui::F5Action::Ignore && ui::f5_action(true,true,true)==ui::F5Action::Ignore,"unassigned F5 modifiers");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60;
    io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ImGui::GetStyle().FontSizeBase=14;
    for(int frame=0;frame<3;++frame) {
        if(frame==2) io.DisplaySize={600,400};
        ImGui::NewFrame();ui::SetNextToolWindow("Bounded tool",{760,720});
        ImGui::Begin("Bounded tool");const auto* window=ImGui::GetCurrentWindow();const auto* vp=ImGui::GetMainViewport();
        check(window->Pos.x>=vp->WorkPos.x && window->Pos.y>=vp->WorkPos.y && window->Pos.x+window->Size.x<=vp->WorkPos.x+vp->WorkSize.x+1 && window->Pos.y+window->Size.y<=vp->WorkPos.y+vp->WorkSize.y+1,"tool window remains in viewport");
        const auto list=ui::CollectionHeight(100);check(list>=3*ImGui::GetTextLineHeightWithSpacing(),"readable collection rows");
        if(frame==0) {ImGui::SetWindowSize({360,260});ImGui::SetWindowPos({120,100});}
        if(frame==1) check(window->Size.x==360 && window->Size.y==260,"tool window preserves user size");
        ImGui::End();ImGui::Render();
    }
    ImGui::DestroyContext();std::cout<<"Collection rows and bounded/user-sized tool windows passed\n";
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
    auto o=options(directory,"region-shortcut","project");o.inputs={key(3,GuiInputKind::SelectAllKey),key(5,GuiInputKind::AnalyzeKey),key(9,GuiInputKind::KeepAnalysisKey)};
    auto r=run_project_gui(project,24,"",o);check(r.analysis_state=="ready" && r.analysis_records==1 && r.analysis_samples>0,"region run/keep shortcut through task navigation");
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
int main(int argc,char** argv) {
    try {
        geometry();if(!std::getenv("QUPATHXX_TEST_GUI")) {std::cout<<"Native organization replay skipped: opt-in QUPATHXX_TEST_GUI\n";return 0;}
        const auto dir=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/("qpx-ui-organization-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
        navigation(dir);overview_failure(dir);shortcuts(dir);return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
