#include "../gui_marker_rules.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/project.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void exercise(const fs::path& root,GLFWwindow* window,const std::string& real){
    std::shared_ptr<Document> doc;
    if(real.empty()){
        const auto file=root/"source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);doc=std::make_shared<Document>(open_tiff(file.u8string()));
        RawRegionRequest region;region.region={0,0,0,15,9};region.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=1.5;
        FluorescenceCellRequest request(*doc,region,0,options,{0,0});auto run=detect_fluorescence_cells(request);auto contours=trace_fluorescence_cells(run);
        publish_fluorescence_cells(*doc,request,*contours);
    }else doc=Project::open(real)->document(1);
    const auto ids=marker_cell_ids(*doc);check(!ids.empty(),"Native fluorescence cohort exists");
    MarkerRule rule;rule.gates={{"DNA",0},{"Absent",999}};rule.gates[0].threshold=-1;rule.nodes={{MarkerOperation::Gate,0,{}},{MarkerOperation::Gate,1,{}},{MarkerOperation::And,0,{0,1}}};
    detail::GuiMarkerRules panel;panel.set_rule(rule);panel.open();
    const auto frame=[&](Point cursor={-100,-100},int button=-1,bool blocked=false){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(button>=0)io.AddMouseButtonEvent(0,button==1);
        ImGui::NewFrame();panel.draw(*doc,ids.front(),blocked);ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);};
    const auto click=[&](detail::GuiMarkerRules::Action action,bool blocked=false){frame({-100,-100},-1,blocked);auto r=panel.bounds(action);check(r.width>0 && r.height>0,"Action is present");Point p{r.x+r.width/2,r.y+r.height/2};frame(p,-1,blocked);frame(p,1,blocked);frame(p,0,blocked);};
    const auto settle=[&]{for(int i=0;i<3000;++i){frame();if(!panel.busy())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Marker preview did not settle");};
    const auto rule_file=root/"rule.json",bad_file=root/"bad-rule.json";
    save_marker_rule_file(rule,rule_file.u8string());{std::ofstream out(bad_file);out<<"{}";}
    panel.set_file(bad_file.u8string());frame();click(detail::GuiMarkerRules::Action::LoadFile);check(!panel.error().empty() && marker_rule_ids(*doc).empty(),"Malformed UI file load visibly refuses without document mutation");
    panel.set_file(rule_file.u8string());click(detail::GuiMarkerRules::Action::LoadFile);if(!panel.error().empty())std::cerr<<"Rule load: "<<panel.error()<<"\n";check(panel.error().empty(),"Actual UI JSON load recovers from malformed input");
    click(detail::GuiMarkerRules::Action::Preview);settle();
    if(!panel.error().empty())std::cerr<<panel.error()<<'\n';
    check(panel.result() && panel.result()->counts().unknown==ids.size(),"Actual preview retains missing-channel Unknown");const auto digest=panel.result()->sha256();
    click(detail::GuiMarkerRules::Action::Keep);check(marker_rule_ids(*doc)==std::vector<std::string>{rule.sha256()},"Actual Keep stores exact recipe");
    click(detail::GuiMarkerRules::Action::LoadSaved);check(!panel.result() && panel.error().empty(),"Actual saved-rule Load invalidates old preview");
    click(detail::GuiMarkerRules::Action::Preview);settle();check(panel.result() && panel.result()->sha256()==digest,"Saved rule previews identical cohort");
    click(detail::GuiMarkerRules::Action::Apply,true);check(panel.state()=="ready","Blocked publication cannot apply");
    click(detail::GuiMarkerRules::Action::Apply);check(panel.error().empty() && panel.state()=="applied","Actual Apply delivers native classification");
    check(verify_marker_rule_publication(*doc,*panel.result()),"Applied classes and observations agree");frame();check(bool(panel.result()),"Apply revisions retain displayed result");
    doc->undo();frame();check(!panel.result() && panel.state()=="stale","Undo invalidates applied labels");
    click(detail::GuiMarkerRules::Action::Preview);settle();check(panel.result() && panel.result()->sha256()==digest,"Fresh capture after Undo replays exact result");
    panel.set_selected_only(true);frame();click(detail::GuiMarkerRules::Action::Preview);settle();check(panel.result() && panel.result()->rows().size()==1,"Selected-only uses native explicit cohort");
    panel.set_selected_only(false);frame();click(detail::GuiMarkerRules::Action::Preview);click(detail::GuiMarkerRules::Action::Cancel);settle();check(panel.state()!="applied","Cancel cannot publish classes");
    Project project;project.add_document(doc);const auto saved=root/"markers.qpx";project.save_new(saved.u8string());doc=Project::open(saved.u8string())->document(1);frame();check(!panel.result(),"Document replacement clears old authority");
    panel.set_rule(load_marker_rule(*doc,rule.sha256()));frame();click(detail::GuiMarkerRules::Action::Preview);settle();check(panel.result() && panel.result()->sha256()==digest,"Reopened desktop replay retains exact rule and cohort result");
    click(detail::GuiMarkerRules::Action::Apply);check(panel.state()=="applied" && panel.error().empty(),"Reopened desktop Apply");
    Project classified;classified.add_document(doc);classified.save_new((root/"classified.qpx").u8string());doc=Project::open((root/"classified.qpx").u8string())->document(1);frame();
    panel.set_rule(load_marker_rule(*doc,rule.sha256()));frame();click(detail::GuiMarkerRules::Action::Preview);settle();
    check(panel.result() && verify_marker_rule_publication(*doc,*panel.result()),"Saved desktop classes and unchanged observations verify after reopen");
    int width,height;glfwGetFramebufferSize(window,&width,&height);std::vector<unsigned char> pixels(std::size_t(width)*height*3);glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    {std::ofstream out(root/"marker-panel.ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";for(int y=height-1;y>=0;--y)out.write(reinterpret_cast<const char*>(pixels.data()+std::size_t(y)*width*3),width*3);}
    std::cout<<"Desktop marker workflow: "<<ids.size()<<" cells; Unknown, Keep, blocked Apply, Apply/Undo, selected cohort, cancel and exact reopened replay pass\n";
}
int main(int argc,char** argv){try{
    if(!has_project() || !has_tiff() || !has_geos()){std::cout<<"Optional project/TIFF/GEOS unavailable\n";return 0;}
    auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("marker-gui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1280,1000,"Native marker workflow",nullptr,nullptr);check(window,"GLFW window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    exercise(root,window,argc>2?argv[2]:"");ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
