#include "../gui_fluorescence.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void capture(const fs::path& path,GLFWwindow* window){int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);}
void exercise(const fs::path& root,GLFWwindow* window,const std::string& real){
    const auto file=root/"source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);auto doc=std::make_shared<Document>(open_tiff(file.u8string()));
    const auto roi=doc->add_rectangle({1,1,12,7},"Analysis ROI");std::uint64_t selected=roi;
    detail::GuiFluorescence panel;panel.open();RawRegionRequest request;request.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=1.5;
    Viewport view;view.zoom=10;view.origin={0,0};
    const auto frame=[&](Point cursor={-100,-100},int button=-1,bool blocked=false){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(button>=0)io.AddMouseButtonEvent(0,button==1);
        ImGui::NewFrame();panel.draw(*doc,selected,blocked);panel.draw_preview(ImGui::GetBackgroundDrawList(),view,{800,100},{400,400});ImGui::Render();
        int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.08f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    const auto click=[&](detail::GuiFluorescence::Action action,bool blocked=false){const auto r=panel.bounds(action);check(r.width>0 && r.height>0,"Actual fluorescence action bounds");const Point p{r.x+r.width/2,r.y+r.height/2};frame(p,-1,blocked);glfwSwapBuffers(window);frame(p,1,blocked);glfwSwapBuffers(window);frame(p,0,blocked);glfwSwapBuffers(window);};
    const auto settled=[&]{for(unsigned i=0;i<3000;++i){frame();glfwSwapBuffers(window);if(!panel.busy())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Fluorescence UI did not settle");};
    frame();glfwSwapBuffers(window);panel.set_request(request,options,{0,0});frame();glfwSwapBuffers(window);
    click(detail::GuiFluorescence::Action::Run);settled();if(!panel.result() || !panel.error().empty()){frame();capture(root/"run-failure.ppm",window);std::cerr<<"Run diagnostic: "<<panel.error()<<" / result="<<bool(panel.result())<<" / busy="<<panel.busy()<<"\n";}check(panel.error().empty() && panel.result() && panel.result()->raster().measurements.size()==1,"Actual fluorescence Run delivers native result");
    check(panel.result()->raster().measurements[0].nucleus.channels[0].sum==1541,"GUI retains independently known original-channel sum");
    check(doc->object_count()==1 && !doc->image_data_properties(),"Preview does not publish or keep a record");
    frame();int w,h;glfwGetFramebufferSize(window,&w,&h);std::array<unsigned char,3> pixel{};glReadBuffer(GL_BACK);glReadPixels(915,h-165,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel.data());
    check(pixel[0]>75 && pixel[1]>75,"Preview overlays the independent level-zero nucleus center");capture(root/"raw-preview.ppm",window);glfwSwapBuffers(window);
    click(detail::GuiFluorescence::Action::KeepCells,true);settled();check(doc->object_count()==1,"Save/edit blocking prevents Keep cells");
    if(has_geos()){click(detail::GuiFluorescence::Action::KeepCells);for(int i=0;i<3;++i){frame({-100,-100},-1,true);glfwSwapBuffers(window);}check(doc->object_count()==1 && panel.busy(),"Ready contour delivery defers while editing/save is blocked");settled();check(panel.error().empty() && panel.published_cells()==1 && doc->object_count()==2,"Actual Keep traces and atomically publishes cells");
        const auto cell=doc->annotation(roi+1);check(cell.nucleus && cell.properties->measurements().at("fluorescence.cell.C0.mean")==102,"Published raw body facts match native result");doc->undo();check(doc->object_count()==1,"One undo removes complete cell publication");doc->redo();}
    click(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty() && !panel.kept_record().empty(),"Actual Keep retains compact native run");const auto record=panel.kept_record();const auto sha=panel.result()->sha256();
    frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Replay);settled();if(!panel.result() || !panel.error().empty() || panel.result()->sha256()!=sha){frame();capture(root/"replay-failure.ppm",window);std::cerr<<"Replay diagnostic: "<<panel.error()<<" / result="<<bool(panel.result())<<" / busy="<<panel.busy()<<" / kept="<<panel.kept_record()<<" / actual="<<(panel.result()?panel.result()->sha256():"none")<<" / expected="<<sha<<"\n";}check(panel.error().empty() && panel.result() && panel.result()->sha256()==sha,"Actual Replay verifies exact native run");
    if(has_geos()){const auto n=doc->object_count();click(detail::GuiFluorescence::Action::KeepCells);settled();check(!panel.error().empty() && panel.state()=="failed" && doc->object_count()==n,"Duplicate async Keep visibly fails without partial mutation");click(detail::GuiFluorescence::Action::Replay);settled();check(panel.error().empty() && panel.result(),"Replay clears prior publication error");}
    Project project;project.add_document(doc);const auto saved=root/"gui-cells.qpx";project.save_new(saved.u8string());doc=Project::open(saved.u8string())->document(1);frame();glfwSwapBuffers(window);
    check(!panel.result(),"Changing document clears prior preview authority");click(detail::GuiFluorescence::Action::Replay);settled();check(panel.error().empty() && panel.result() && panel.result()->sha256()==sha,"Reopened UI record replays exact source association");
    doc->update_roi(roi,Roi::rectangle({0,0,15,9}));frame();glfwSwapBuffers(window);check(!panel.result(),"ROI edits invalidate ready preview before Keep");
    doc->undo();frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Replay);settled();check(panel.result() && panel.error().empty(),"Replay recaptures authority after ROI Undo");
    click(detail::GuiFluorescence::Action::Remove);check(panel.error().empty() && fluorescence_cell_record_ids(*doc).empty(),"Actual Remove uses native record service");doc->undo();check(fluorescence_cell_record_ids(*doc)==std::vector<std::string>{record},"Remove is undoable");
    options.threshold=10000;panel.set_request(request,options,{0,0});frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();check(panel.result() && panel.result()->raster().measurements.empty(),"Empty native result remains inspectable");
    const auto before=doc->object_count();click(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty() && doc->object_count()==before && fluorescence_cell_record_ids(*doc).size()==2,"Zero-cell Keep does not fabricate cells");
    click(detail::GuiFluorescence::Action::Run);click(detail::GuiFluorescence::Action::Cancel);settled();check(doc->object_count()==before,"Cancellation never publishes objects");
    *doc=Document(doc->server());frame();glfwSwapBuffers(window);check(!panel.result() && panel.kept_record().empty(),"Same-address Document replacement still invalidates native capture authority");
    doc=std::make_shared<Document>(open_tiff(file.u8string()));frame();glfwSwapBuffers(window);check(!panel.result() && panel.kept_record().empty(),"Switching images discards prior result/record selection");
    if(!real.empty()){doc=std::make_shared<Document>(open_tiff(real));selected=0;frame();glfwSwapBuffers(window);RawRegionRequest r;r.region={0,0,0,doc->metadata().width,doc->metadata().height};r.channels=doc->server()->raw_metadata()->channels;
        FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;panel.set_request(r,p,{0,0},false);frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();
        check(panel.error().empty() && panel.result(),"Actual UI native detection on admitted real TIFF");frame();capture(root/"real-raw-preview.ppm",window);glfwSwapBuffers(window);
        if(has_geos()){click(detail::GuiFluorescence::Action::KeepCells);settled();check(panel.error().empty() && panel.published_cells()==panel.result()->raster().measurements.size(),"Real UI Keep preserves every detected label");}
        click(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty(),"Real UI Keep record");const auto digest=panel.result()->sha256();click(detail::GuiFluorescence::Action::Replay);settled();check(panel.error().empty() && panel.result()->sha256()==digest,"Real UI exact replay");
        std::cout<<"Real fluorescence UI: "<<panel.result()->raster().measurements.size()<<" candidates; no annotated accuracy claim\n";
    }
}
int main(int argc,char** argv){try{
    if(!has_project() || !has_tiff()){std::cout<<"GUI fluorescence: optional project/TIFF disabled\n";return 0;}
    const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("fluorescence-gui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1280,1000,"Native fluorescence workflow",nullptr,nullptr);check(window,"GLFW window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    exercise(root,window,argc>2?argv[2]:"");ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Fluorescence desktop actions, raw preview registration, native facts, blocking, keep/undo, records/replay, stale/switch and empty checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
