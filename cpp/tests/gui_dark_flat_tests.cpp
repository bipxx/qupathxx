#include "../gui_fluorescence.hpp"
#include "../gui_dark_flat.hpp"
#include "../gui_marker_rules.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/marker_rules.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <exception>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void expect_close(double a,double b){check(std::isfinite(a) && std::abs(a-b)<1e-10,"Desktop corrected value differs from independent oracle");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void field(const fs::path& path,std::uint16_t a,std::uint16_t b){qpx_test::raw_pixel_tiff_fixture(path,15,9);std::fstream out(path,std::ios::binary|std::ios::in|std::ios::out);out.seekp(160);for(int i=0;i<135;++i)for(auto v:{a,b}){out.put(char(v&255));out.put(char(v>>8));}out.close();check(bool(out),"Write independent field fixture");}
void capture(const fs::path& path,GLFWwindow* window){int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);}
void exercise(const fs::path& root,GLFWwindow* window,bool handoff){
    const auto signal=root/"signal.tif",dark=root/"dark.tif",flat=root/"flat.tif";qpx_test::raw_pixel_tiff_fixture(signal,15,9);field(dark,10,300);field(flat,20,320);
    const auto before_signal=bytes(signal),before_dark=bytes(dark),before_flat=bytes(flat);
    auto doc=std::make_shared<Document>(open_tiff(signal.u8string()));const auto roi=doc->add_rectangle({1,1,12,7},"ROI");
    detail::GuiFluorescence panel;auto& tool=panel.correction_tool();tool.open();const DarkFlatOptions options{{10,20},0};RawRegionRequest preview{{1,1,0,12,7},1,1};tool.set_preview(preview);
    struct Diagnostics {detail::GuiFluorescence& panel;detail::GuiDarkFlat& tool;fs::path root;GLFWwindow* window;
        ~Diagnostics(){if(std::uncaught_exceptions())try{std::cerr<<"Correction state="<<tool.state()<<" error="<<tool.error()<<" kept="<<tool.kept_record()<<" busy="<<tool.busy()<<" / fluorescence state="<<panel.state()<<" error="<<panel.error()<<" kept="<<panel.kept_record()<<"\n";capture(root/"failure.ppm",window);}catch(...){}}} diagnostics{panel,tool,root,window};
    std::unique_ptr<detail::GuiMarkerRules> markers;bool draw_parent=true;
    const auto frame=[&](Point cursor={-100,-100},int mouse=-1,bool blocked=false){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(mouse>=0)io.AddMouseButtonEvent(0,mouse==1);ImGui::NewFrame();if(draw_parent)panel.draw(*doc,roi,blocked);if(markers)markers->draw(*doc,0,blocked);ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);};
    const auto click_rect=[&](Rect r,bool blocked){check(r.width>0 && r.height>0,"Actual desktop action bounds");const Point p{r.x+r.width/2,r.y+r.height/2};frame(p,-1,blocked);frame(p,1,blocked);frame(p,0,blocked);};
    const auto click_tool=[&](detail::GuiDarkFlat::Action action,bool blocked=false){frame({-100,-100},-1,blocked);click_rect(tool.bounds(action),blocked);};
    const auto click_panel=[&](detail::GuiFluorescence::Action action,bool blocked=false){frame({-100,-100},-1,blocked);click_rect(panel.bounds(action),blocked);};
    const auto settle=[&](bool blocked=false){for(int i=0;i<3000;++i){frame({-100,-100},-1,blocked);if(!tool.busy() && !panel.busy())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Correction desktop did not settle");};
    tool.set_fields((root/"missing.tif").u8string(),flat.u8string(),options);frame();click_tool(detail::GuiDarkFlat::Action::Load);settle();check(!tool.error().empty() && tool.state()=="failed","Missing field produces visible native error");
    tool.set_fields(dark.u8string(),flat.u8string(),options);frame();click_tool(detail::GuiDarkFlat::Action::Load);
    for(int i=0;i<15;++i)frame({-100,-100},-1,true);check(tool.busy() && tool.state()=="opening","Blocked owner defers ready-field delivery");settle();
    if(!tool.error().empty())std::cerr<<tool.error()<<'\n';check(tool.error().empty() && tool.state()=="ready","Actual field Load captures native correction");
    click_tool(detail::GuiDarkFlat::Action::Preview,true);check(!tool.result(),"Blocked preview action is disabled");click_tool(detail::GuiDarkFlat::Action::Preview);settle();
    check(tool.result() && tool.result()->valid_pixels()==84 && tool.result()->finite_samples()==std::vector<std::uint64_t>{84},"Actual preview shows selected raw channel counts");expect_close(std::get<std::vector<double>>(tool.result()->samples().storage())[0],-184);
    click_tool(detail::GuiDarkFlat::Action::Fields);click_tool(detail::GuiDarkFlat::Action::Image);frame();capture(root/"signed-correction-preview.ppm",window);
    click_tool(detail::GuiDarkFlat::Action::Keep);check(tool.error().empty() && !tool.kept_record().empty(),"Actual Keep stores native recipe");const auto correction_id=tool.kept_record();
    doc->undo();frame();check(dark_flat_recipe_ids(*doc).empty() && tool.kept_record().empty(),"Metadata Undo updates correction controls");doc->redo();frame();
    click_tool(detail::GuiDarkFlat::Action::LoadSaved);settle();click_tool(detail::GuiDarkFlat::Action::Preview);settle();check(tool.error().empty() && tool.result(),"Saved correction loads and previews through native restore");
    RawRegionRequest selection;selection.channels=2;FluorescenceCellOptions cells;cells.threshold=90;cells.cell_expansion=1.5;panel.set_request(selection,cells,{0,0});
    click_tool(detail::GuiDarkFlat::Action::Use);check(panel.uses_correction(),"Actual Use command explicitly selects corrected fluorescence");
    click_panel(detail::GuiFluorescence::Action::Run);settle();if(!panel.error().empty())std::cerr<<panel.error()<<'\n';check(panel.result() && panel.error().empty() && panel.result()->measurement_domain()=="qpx.dark-flat-corrected-samples/1","Actual fluorescence Run uses selected corrected source");
    expect_close(panel.result()->raster().measurements.at(0).nucleus.channels[0].sum.value(),1401);expect_close(panel.result()->raster().measurements.at(0).nucleus.channels[1].sum.value(),-1259);expect_close(panel.result()->raster().measurements.at(0).cytoplasm->channels[1].mean.value(),2513./13-300);
    const auto digest=panel.result()->sha256();
    click_panel(detail::GuiFluorescence::Action::KeepCells);settle();check(panel.error().empty() && panel.published_cells()==1,"Corrected desktop Keep publishes one native cell");
    doc->undo();frame();check(doc->object_count()==1 && panel.published_cells()==0,"Undo clears actual publication count");doc->redo();frame();check(panel.published_cells()==1,"Redo restores actual publication count");
    click_panel(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty() && !panel.kept_record().empty(),"Actual Keep retains corrected run");const auto record=panel.kept_record();
    doc->undo();frame();check(panel.kept_record().empty(),"Run metadata Undo reconciles retained label");doc->redo();frame();check(panel.kept_record()==record,"Run metadata Redo restores retained label");
    Project project;project.add_document(doc);project.save_new((root/"corrected-ui.qpx").u8string());doc=Project::open((root/"corrected-ui.qpx").u8string())->document(1);frame();
    click_panel(detail::GuiFluorescence::Action::Replay);settle();check(panel.error().empty() && panel.result() && panel.result()->sha256()==digest && panel.result()->correction_record()==correction_id,"Reopened desktop Replay preserves exact corrected run/source identity");
    click_panel(detail::GuiFluorescence::Action::Domain);check(!panel.uses_correction(),"Actual domain checkbox restores original default");cells.threshold=100;panel.set_request(selection,cells,{0,0});frame();click_panel(detail::GuiFluorescence::Action::Run);settle();check(panel.result() && panel.result()->measurement_domain()=="original-channel-samples" && panel.result()->correction_record().empty(),"Original route stays explicitly independent");expect_close(panel.result()->raster().measurements.at(0).nucleus.channels[0].sum.value(),1541);
    draw_parent=false;markers=std::make_unique<detail::GuiMarkerRules>();MarkerRule rule;rule.domain="qpx.dark-flat-corrected-samples/1";rule.gates={{"Signed marker",1}};rule.gates[0].threshold=0;rule.nodes={{MarkerOperation::Gate,0,{}}};markers->set_rule(rule);markers->open();frame();
    const auto click_marker=[&](detail::GuiMarkerRules::Action a){frame();click_rect(markers->bounds(a),false);};click_marker(detail::GuiMarkerRules::Action::Preview);for(int i=0;i<3000 && markers->busy();++i){frame();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(markers->result() && markers->result()->counts().negative==1 && markers->result()->counts().positive==0,"Actual marker UI evaluates corrected negative observations");click_marker(detail::GuiMarkerRules::Action::Apply);check(markers->error().empty() && verify_marker_rule_publication(*doc,*markers->result()),"Actual marker Apply retains corrected facts");doc->undo();frame();check(!markers->result(),"Marker Undo invalidates applied preview authority");markers.reset();draw_parent=true;
    frame();click_panel(detail::GuiFluorescence::Action::Correction);click_tool(detail::GuiDarkFlat::Action::LoadSaved);settle();click_tool(detail::GuiDarkFlat::Action::Remove);check(!tool.error().empty() && load_dark_flat_recipe(*doc,correction_id).sha256()==correction_id,"Referenced correction removal visibly refuses");
    check(bytes(signal)==before_signal && bytes(dark)==before_dark && bytes(flat)==before_flat,"Desktop preview/publication/replay preserve source bytes");
    if(handoff){std::cout<<"Desktop correction handoff passes; source fixtures preserved for standalone replay\n";return;}
    {std::ofstream changed(flat,std::ios::binary|std::ios::app);changed.put('x');}click_tool(detail::GuiDarkFlat::Action::Preview);settle();check(!tool.error().empty() && !tool.result() && tool.state()=="failed","Changed field fails preview without stale values");click_tool(detail::GuiDarkFlat::Action::Use);check(!tool.error().empty() && !panel.uses_correction(),"Stale correction cannot be handed to fluorescence");
    tool.set_fields(dark.u8string(),flat.u8string(),options);frame();click_tool(detail::GuiDarkFlat::Action::Load);click_tool(detail::GuiDarkFlat::Action::Cancel,true);settle(true);check(tool.state()=="cancelled" && !tool.result(),"Cancel consumes pending worker without owner publication");
    click_tool(detail::GuiDarkFlat::Action::Load);*doc=Document(doc->server());settle();check(!tool.result() && tool.kept_record().empty(),"Same-address replacement discards pending field authority");
    std::cout<<"Desktop correction: signed preview, async/blocked delivery, recipe Undo/load, explicit corrected/original run, cell Undo/Redo, exact reopened replay, marker Apply/Undo, source guards and cancel pass\n";
}
int main(int argc,char** argv){try{if(!has_project() || !has_tiff() || !has_geos()){std::cout<<"Optional project/TIFF/GEOS unavailable\n";return 0;}const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("dark-flat-gui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);auto* window=glfwCreateWindow(1280,1100,"Native correction workflow",nullptr,nullptr);check(window,"GLFW window");glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");exercise(root,window,argc>2 && std::string(argv[2])=="--handoff");ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
