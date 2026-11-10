#include "../gui_registration.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool same_properties(const std::shared_ptr<const ImageDataProperties>& a,const std::shared_ptr<const ImageDataProperties>& b) {return a==b || (a && b && a->equals(*b));}
void near(double a,double b,double tolerance=1e-7){check(std::isfinite(a) && std::abs(a-b)<tolerance,"Interactive landmark coordinate mismatch");}
void capture(const fs::path& path,GLFWwindow* window) {
    int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> pixels(std::size_t(w)*h*3);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)file.write(reinterpret_cast<const char*>(pixels.data()+std::size_t(y)*w*3),w*3);
}
ExpressionDataset dataset(const Document& doc){ExpressionCaptureOptions options;options.coordinate_frame="image-level-zero-px";return ExpressionDataset::capture(doc,options);}
void run(std::shared_ptr<Document> doc,const fs::path& root,GLFWwindow* window,bool real) {
    Project saved;saved.add_document(doc);const auto image_id=saved.identity()+":1";
    detail::GuiLandmarkRegistration panel;panel.open();
    const auto frame=[&](Point cursor={-100,-100},int button=-1,bool blocked=false) {
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
        ImGui::GetIO().AddMousePosEvent(float(cursor.x),float(cursor.y));if(button>=0)ImGui::GetIO().AddMouseButtonEvent(0,button==1);
        ImGui::NewFrame();panel.draw(*doc,image_id,{},blocked);ImGui::Render();
        int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.08f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    const auto click=[&](detail::GuiLandmarkRegistration::Action action,bool blocked=false) {
        const auto r=panel.bounds(action);check(r.width>0,"Registration action bounds");const Point p{r.x+r.width/2,r.y+r.height/2};
        frame(p,1,blocked);glfwSwapBuffers(window);frame(p,0,blocked);glfwSwapBuffers(window);
    };
    frame();glfwSwapBuffers(window);
    auto original=dataset(*doc);
    if(real) {
        // Controlled misalignment of an in-memory copy of the admitted dataset.
        // The original real fixture file is never saved or overwritten.
        for(const auto& observation:original.observations())doc->edit_vertex(observation.object_id,0,0,{observation.position.x+10,observation.position.y-8});
        frame();glfwSwapBuffers(window);
    }
    const auto baseline=dataset(*doc);const auto properties=doc->image_data_properties();
    std::vector<LandmarkPair> landmarks;
    for(std::size_t i=0;i<3;++i) {const auto row=i*(baseline.observations().size()-1)/2;const auto p=baseline.observations()[row].position;
        landmarks.push_back({"L"+std::to_string(i+1),p,{p.x-10,p.y+8},i>0});}
    panel.set_landmarks(landmarks,{LandmarkModel::Translation});
    for(unsigned i=0;i<3000;++i) {frame();glfwSwapBuffers(window);if(panel.settled())break;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(panel.error().empty() && panel.settled(),"Registration canvas/data ready");
    click(detail::GuiLandmarkRegistration::Action::Preview);check(panel.fit() && panel.error().empty(),"Actual Fit preview action");
    near(panel.fit()->transform().matrix[2],-10);near(panel.fit()->transform().matrix[5],8);
    check(same_properties(doc->image_data_properties(),properties) && dataset(*doc).sha256()==baseline.sha256(),"Preview does not mutate source or study");
    if(!real) {
        const auto handle=panel.handle_position(0,true);frame(handle,1);glfwSwapBuffers(window);
        frame({handle.x+20,handle.y+10});glfwSwapBuffers(window);frame({handle.x+20,handle.y+10},0);glfwSwapBuffers(window);
        check(!panel.fit() && panel.landmarks()[0].target.x>landmarks[0].target.x,"Actual target drag changes draft and invalidates preview");
        near(panel.landmarks()[0].source.x,landmarks[0].source.x);check(dataset(*doc).sha256()==baseline.sha256(),"Drag never edits source geometry");
        panel.set_landmarks(landmarks,{LandmarkModel::Translation});frame();glfwSwapBuffers(window);click(detail::GuiLandmarkRegistration::Action::Preview);
    }
    frame();capture(root/(real?"real-preview.ppm":"preview.ppm"),window);glfwSwapBuffers(window);
    click(detail::GuiLandmarkRegistration::Action::Accept,true);check(!active_registration_id(doc->image_data_properties()),"Blocked publication is disabled");
    frame();glfwSwapBuffers(window);click(detail::GuiLandmarkRegistration::Action::Accept);
    check(panel.error().empty() && active_registration_id(doc->image_data_properties()),"Actual Accept publishes native registration");
    const auto accepted=stored_landmark_fit(doc->image_data_properties(),"landmarks");check(accepted && accepted->sha256()==panel.fit()->sha256(),"UI acceptance retains exact fit evidence");
    const auto field=active_registration_field(*doc,baseline);check(field && field->positions.size()==baseline.observations().size(),"Accepted native mapping covers observations");
    if(real)for(std::size_t i=0;i<original.observations().size();++i) {near(field->positions[i].x,original.observations()[i].position.x);near(field->positions[i].y,original.observations()[i].position.y);}
    frame();capture(root/(real?"real-accepted.ppm":"accepted.ppm"),window);glfwSwapBuffers(window);
    doc->undo();check(same_properties(doc->image_data_properties(),properties),"One native undo restores all registration state");doc->redo();
    frame();glfwSwapBuffers(window);click(detail::GuiLandmarkRegistration::Action::Load);
    check(panel.fit() && panel.fit()->sha256()==accepted->sha256(),"Load saved preserves coefficients/evidence without refitting");
    check(dataset(*doc).sha256()==baseline.sha256(),"Accept/undo/load retain exact raw data");
    const auto file=root/(real?"real-aligned.qpx":"aligned.qpx");saved.save_new(file.u8string());
    const auto reopened=Project::open(file.u8string());const auto restored=reopened->document(1);
    check(stored_landmark_fit(restored->image_data_properties(),"landmarks")->sha256()==accepted->sha256() && dataset(*restored).sha256()==baseline.sha256(),"UI-authored project save/reopen exact fit/source identity");
    doc=restored;frame();glfwSwapBuffers(window);click(detail::GuiLandmarkRegistration::Action::Load);
    check(panel.error().empty() && panel.fit() && panel.fit()->sha256()==accepted->sha256(),"Reopened project loads saved landmarks through the actual UI with persistent image identity");
}
int main(int argc,char** argv){try {
    Project identity;const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/(identity.identity()+"-registration-gui");fs::create_directories(root);
    check(glfwInit(),"GLFW initialize");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1280,1000,"Native landmark alignment",nullptr,nullptr);check(window,"GLFW create");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    {
        auto doc=std::make_shared<Document>(Image(128,128,std::vector<std::uint8_t>(128*128*3,80)));
        unsigned n=0;for(const Point p:std::vector<Point>{{30,30},{60,40},{80,90}}) {
            const auto id=doc->add_detection(Roi::points({p}));
            doc->set_object_properties(id,std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},
                {"omics.matrix_sha256",std::string(64,'a')},{"omics.features_sha256",std::string(64,'b')},{"omics.positions_sha256",std::string(64,'c')},
                {"omics.observation_id","spot"+std::to_string(n)}},std::map<std::string,double>{{"omics.count:G",double(++n)}}));
        }
        run(doc,root,window,false);
        if(argc>2) {const auto project=Project::open_without_journal_recovery(argv[2]);run(project->document(1),root,window,true);}
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Landmark GUI: drag/preview/accept, blocked publication, unchanged source, native undo, load and save/reopen passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
