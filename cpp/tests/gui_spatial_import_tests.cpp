#include "../gui_spatial_import.hpp"
#include "qupathxx/object_properties.hpp"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void write(const fs::path& file,const std::string& value) {std::ofstream stream(file,std::ios::binary);stream<<value;check(bool(stream),"Cannot write import fixture");}
std::string bytes(const fs::path& file) {std::ifstream stream(file,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
void capture(const fs::path& path,GLFWwindow* window) {
    int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> pixels(std::size_t(w)*h*3);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)file.write(reinterpret_cast<const char*>(pixels.data()+std::size_t(y)*w*3),w*3);
}
int main(int argc,char** argv) {try {
    Project identity;const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/(identity.identity()+"-import-gui");fs::create_directories(root);
    Project project;auto doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));project.add_document(doc);project.save_new((root/"base.qpx").u8string());
    write(root/"counts.mtx","%%MatrixMarket matrix coordinate integer general\n2 3 3\n1 1 5\n2 1 2\n1 2 10\n");
    write(root/"features.tsv","feature_id\nG1\nG2\n");write(root/"positions.tsv","observation_id\tx_px\ty_px\nA\t1.5\t2\nB\t3\t4.5\nZero\t0\t0\n");
    SpatialImportRequest request;request.input_project=(root/"base.qpx").u8string();request.output_project=(root/"imported.qpx").u8string();request.image_id=1;
    request.matrix=(root/"counts.mtx").u8string();request.features=(root/"features.tsv").u8string();request.positions=(root/"positions.tsv").u8string();request.options.features={"G1","G2"};
    const auto original=bytes(root/"base.qpx");
    check(glfwInit(),"GLFW initialization");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1100,850,"Native spatial import",nullptr,nullptr);check(window,"Import window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().Fonts->AddFontDefault();
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    {
        auto wizard=std::make_unique<detail::GuiSpatialImporter>();wizard->set_request(request);wizard->read_images();wizard->wait();check(wizard->error().empty(),"Read images through native job");
        wizard->read_genes();wizard->wait();check(wizard->error().empty(),"Read features through native job");wizard->preview();wizard->wait();
        check(wizard->result() && !fs::exists(root/"imported.qpx") && bytes(root/"base.qpx")==original && !doc->object_count(),"Preview is owned and read-only");
        const auto frame=[&](int click_stage=-1,detail::GuiSpatialImporter::Action action=detail::GuiSpatialImporter::Action::Publish) {
            glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
            if(click_stage>=0) {const auto bounds=wizard->bounds(action);check(bounds.width>0,"Import action has usable bounds");
                ImGui::GetIO().AddMousePosEvent(float(bounds.x+bounds.width/2),float(bounds.y+bounds.height/2));ImGui::GetIO().AddMouseButtonEvent(0,click_stage==0);}
            ImGui::NewFrame();const auto opened=wizard->draw();ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.08f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());return opened;
        };
        for(int i=0;i<15;++i) {frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        for(int i=0;i<3000 && !wizard->preview_ready();++i) {frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        check(wizard->preview_ready(),"Image preview completes before import");
        frame();capture(root/"preview.ppm",window);glfwSwapBuffers(window);
        frame(0);glfwSwapBuffers(window);frame(1);glfwSwapBuffers(window);wizard->wait();
        check(wizard->phase()==SpatialImportPhase::Complete && wizard->error().empty(),"GUI import action publishes using native job");
        frame();capture(root/"complete.ppm",window);glfwSwapBuffers(window);
        frame(0,detail::GuiSpatialImporter::Action::Open);glfwSwapBuffers(window);
        const auto open_path=frame(1,detail::GuiSpatialImporter::Action::Open);glfwSwapBuffers(window);
        check(open_path==fs::weakly_canonical(fs::u8path(request.output_project)).u8string(),"Open action returns the exact new project to the existing app loader");
        const auto result=Project::open(request.output_project);const auto imported=result->document(1);
        check(imported->object_count()==3 && imported->annotation(1).properties->measurements().at("omics.count:G1")==5 && imported->annotation(2).properties->measurements().at("omics.count:G2")==0,"GUI import retains independent expected counts");
        check(imported->annotation(2).geometry.centroid().y==4.5 && imported->annotation(1).properties->metadata().at("omics.original_x")=="1.5","GUI import retains mapped and original coordinates");
        check(bytes(root/"base.qpx")==original && !doc->object_count(),"Import leaves current project/file intact");
        if(argc>2) {
            const auto demo=fs::u8path(argv[2]);request.input_project=(demo/"base.qpx").u8string();request.output_project=(root/"real-imported.qpx").u8string();request.matrix=(demo/"counts.mtx").u8string();request.features=(demo/"features.tsv").u8string();request.positions=(demo/"positions.tsv").u8string();
            request.options.features=read_spatial_feature_catalogue(request.features).features;wizard->set_request(request);wizard->read_images();wizard->wait();wizard->read_genes();wizard->wait();wizard->preview();wizard->wait();
            check(wizard->result() && wizard->result()->projection().observations.size()==2702,"Real preview covers all spots");
            for(int i=0;i<20;++i) {frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            for(int i=0;i<3000 && !wizard->preview_ready();++i) {frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            check(wizard->preview_ready(),"Real image preview completes before import");
            frame();capture(root/"real-preview.ppm",window);glfwSwapBuffers(window);frame(0);glfwSwapBuffers(window);frame(1);glfwSwapBuffers(window);wizard->wait();
            check(wizard->phase()==SpatialImportPhase::Complete && Project::open(request.output_project)->document(1)->object_count()==2702,"Real import completed through GUI without a shell");
        }
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Guided import: native catalogue/preview, actual GUI publication, exact counts/coordinates and unchanged prior project passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
