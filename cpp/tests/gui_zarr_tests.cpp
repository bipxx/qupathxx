#include "../gui_raw.hpp"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <cmath>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    const auto root=fs::current_path()/("gui-zarr-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);const auto source=root/"source.zarr";fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",source,fs::copy_options::recursive);
    OmeZarrOpenOptions input;input.view.z=1;input.view.t=1;Project project;const auto id=project.add_document(std::make_shared<Document>(open_ome_zarr(source.u8string(),input)));auto doc=project.document(id);ObjectSelection selection(*doc);
    GuiOptions options;options.raw_open=true;TiffOpenOptions target;target.view.z=0;target.view.t=0;target.view.channels={0,1,2};target.view.lower={0,0,0};target.view.upper={3000,3000,3000};options.tiff_options=std::make_shared<const TiffOpenOptions>(target);RawRegionRequest request;request.region={0,0,0,7,5};request.channels=3;options.raw_request=std::make_shared<const RawRegionRequest>(request);
    check(glfwInit(),"GLFW initialization");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1100,850,"Native Zarr channels",nullptr,nullptr);check(window,"Create test window");glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 130");
    {
        detail::GuiRaw panel(options);
        const auto frame=[&](int click=-1,GuiInputTarget action=GuiInputTarget::ApplyTiffView){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();if(click>=0){const auto bounds=panel.bounds(action);if(!(bounds.width>0 && panel.enabled(action))){int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());std::ofstream out(root/"missing-action.ppm",std::ios::binary);out<<"P6\n"<<w<<" "<<h<<"\n255\n";for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);throw std::runtime_error("Actual channel action must be visible/enabled: "+std::to_string(int(action))+" / "+panel.state()+" / "+panel.analysis_state()+" / "+panel.error());}ImGui::GetIO().AddMousePosEvent(float(bounds.x+bounds.width/2),float(bounds.y+bounds.height/2));if(click<2)ImGui::GetIO().AddMouseButtonEvent(0,click==0);}ImGui::NewFrame();panel.draw(*doc,selection,&project,false,[](GuiInputTarget){});ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);};
        const auto settle=[&](auto predicate){for(int i=0;i<3000;++i){frame();if(predicate())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Native Zarr panel did not settle: "+panel.error());};
        const auto click=[&](GuiInputTarget action){std::cout<<"Click native action "<<int(action)<<std::endl;frame();frame(2,action);frame(0,action);frame(1,action);};
        settle([&]{return panel.state()=="ready";});click(GuiInputTarget::ApplyTiffView);settle([&]{return panel.state()=="ready" && doc->active_plane()==ObjectPlane(-1,0,0);});
        check(doc->server()->reader_id()=="ome-zarr" && doc->server()->raw_view()->channels==target.view.channels,"Actual channel/plane action did not use native Zarr view");click(GuiInputTarget::RawDataTab);click(GuiInputTarget::MeasureRawRegion);settle([&]{return panel.analysis_state()=="ready";});
        const auto& measured=panel.analysis_result()->region();std::cout<<"Measured "<<measured.region.x<<","<<measured.region.y<<" "<<measured.region.width<<"x"<<measured.region.height<<" level="<<measured.region.level<<" C="<<measured.first_channel<<"+"<<measured.channels<<" Z="<<measured.z<<" T="<<measured.t<<" means=";for(auto mean:panel.analysis_result()->statistics().means)std::cout<<(mean?std::to_string(*mean):"missing")<<" ";std::cout<<std::endl;
        const auto& stats=panel.analysis_result()->statistics();check(measured.region.x==0 && measured.region.y==0 && measured.region.width==7 && measured.region.height==5 && measured.channels==3 && measured.z==0 && measured.t==0 && stats.valid_pixels==35 && stats.means.size()==3,"Actual measurement changed rectangle/axes/counts");for(std::size_t c=0;c<3;++c)check(stats.finite_samples[c]==35 && stats.nonfinite_samples[c]==0 && stats.minima[c]==double(1000*c) && stats.maxima[c]==double(1000*c+46) && stats.means[c] && std::abs(*stats.means[c]-double(1000*c+23))<1e-12,"Actual Zarr measurement button produced wrong source values");click(GuiInputTarget::RawResultsTab);click(GuiInputTarget::KeepRawAnalysis);frame();check(doc->raw_analysis_record_count()==1,"Actual Keep action failed");
        project.save_new((root/"saved.qpx").u8string());auto saved=Project::open((root/"saved.qpx").u8string());check(saved->document(id)->active_plane()==ObjectPlane(-1,0,0) && saved->document(id)->raw_analysis_record_count()==1,"Actual UI result/view did not save/reopen");
        panel.finish(doc.get(),true);
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    if(argc==3){auto real=Project::open(argv[1]);GuiOptions display;display.raw_open=true;display.raw_tab=3;display.settle_tiles=true;display.width=1440;display.height=1000;display.replay_timeout_seconds=15;const auto report=run_project_gui(real,120,argv[2],display);check(report.uploaded_tiles>0 && report.tiff_state=="ready" && report.raw_error.empty() && report.raw_records==1,"Real saved Zarr project did not display native tiles/channel metadata/saved record");std::cout<<"Full real saved Zarr viewport/channel metadata/record display pass\n";}
    std::cout<<"Native GUI Zarr channel/plane/measure/keep/save/reopen controls pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
