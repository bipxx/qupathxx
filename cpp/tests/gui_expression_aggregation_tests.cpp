#include "gui_translation_trace.hpp"
#include "../gui_genes.hpp"
#include "../gui_fonts.hpp"
#include "qupathxx/object_properties.hpp"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace qpx;
namespace fs=std::filesystem;
using Panel=detail::GuiGeneDisplay;
using Aggregation=detail::GuiExpressionAggregation;
using Action=Aggregation::Action;
void check(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
void write(const fs::path& path,const std::string& value){std::ofstream out(path,std::ios::binary);out<<value;out.close();check(bool(out),"Write authored aggregation fixture");}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void screenshot(const fs::path& path,GLFWwindow* window){
    int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);
    out.close();check(bool(out),"Capture aggregation controls");
}
void label(std::array<char,257>& target,const std::string& value){check(value.size()<target.size(),"Test label capacity");target.fill(0);std::copy(value.begin(),value.end(),target.begin());}

void exercise(const fs::path& root,GLFWwindow* window){
    write(root/"features.tsv","feature_id\nG0\nG1\nG2\n");
    write(root/"observations.tsv","observation_id\nO0\nO1\nO2\nO3\nO4\n");
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n3 5 10\n1 1 9007199254740993\n1 2 1\n1 3 5\n1 4 2\n1 5 1\n2 1 3\n2 2 4\n2 4 2\n2 5 1\n3 3 18446744073709551615\n");
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(root/"source.qmx").u8string());
    const auto source=open_expression_matrix(imported.path,imported.file_sha256);const auto source_before=read(root/"source.qmx");
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(80,60,std::vector<std::uint8_t>(80*60*3,55)));
    auto image=project->add_document(doc);keep_expression_matrix(*project,image,source,true);const auto spatial_frame=image_coordinate_frame(*doc,"aggregation-image");
    for(std::size_t i=0;i<5;++i){const auto point=doc->add_detection(Roi::points({{10.+12*i,20.}}));doc->set_object_properties(point,expression_matrix_point_properties(source->metadata().source_sha256,"O"+std::to_string(i),spatial_frame));}
    const auto left=doc->add_roi(Roi::rectangle({5,15,25,10})),right=doc->add_roi(Roi::rectangle({32,15,30,10})),empty=doc->add_roi(Roi::rectangle({70,50,3,3}));
    auto selected=std::make_unique<ObjectSelection>(*doc);selected->replace({left,right,empty});
    const auto scope=expression_matrix_spatial_scopes(*doc).at(0);
    GeneViewOptions options;options.enabled=true;options.genes={"G1"};
    auto panel=std::make_unique<Panel>();panel->load_preference(gene_workspace_preference(options,scope.sha256()));
    detail::OutputGate gate;gate.bind([](bool,bool){return detail::ExportOwnerStamp{};},[&]{return std::vector<std::string>{project->path(),(root/"protected.csv").u8string()};});
    std::string* shown=nullptr;
    const auto frame=[&](Point cursor={-100,-100},int mouse=-1,bool blocked=false){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(mouse>=0)io.AddMouseButtonEvent(0,mouse==1);
        ImGui::NewFrame();ui::begin_frame();if(shown){ImGui::LogToBuffer();ImGui::GetCurrentContext()->LogWindow=nullptr;}
        panel->update(*doc,true,project.get(),image);panel->draw({},project.get(),image,blocked,&gate,{root.u8string()},selected.get());gate.draw([](const auto&){return true;});
        if(shown){*shown=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();}
        ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    const auto settle=[&]{for(int i=0;i<12000;++i){frame();glfwSwapBuffers(window);if(panel->settled()){check(panel->error().empty(),"Indexed Genes source: "+panel->error());return;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Aggregation GUI did not settle");};
    const auto raw_click=[&](Rect r,bool blocked=false){const Point p{r.x+r.width/2,r.y+r.height/2};frame(p,-1,blocked);glfwSwapBuffers(window);frame(p,1,blocked);glfwSwapBuffers(window);frame(p,0,blocked);glfwSwapBuffers(window);};
    const auto aggregation_tab=[&]{frame();glfwSwapBuffers(window);const auto r=panel->molecular_bounds(Panel::Action::AggregationTab);check(r.width>0 && panel->molecular_enabled(Panel::Action::AggregationTab),"Regional summaries tab must be visible");raw_click(r);frame();glfwSwapBuffers(window);};
    const auto click=[&](Action action){frame();glfwSwapBuffers(window);const auto r=panel->aggregation().bounds(action);if(!panel->aggregation().enabled(action)){screenshot(root/"unavailable-action.ppm",window);throw std::runtime_error("Aggregation action unavailable: "+std::to_string(int(action))+" / "+panel->aggregation().error());}raw_click(r);};
    settle();aggregation_tab();click(Action::Capture);check(panel->aggregation().drafts().size()==3,"Actual Capture did not use selected area annotations");
    label(panel->aggregation().drafts()[0].id,"left, literal 伪");label(panel->aggregation().drafts()[1].id,"right");label(panel->aggregation().drafts()[2].id,"empty");
    click(Action::Run);settle();const auto summary=panel->aggregation().result();check(summary && panel->aggregation().error().empty(),"Actual Run failed: "+panel->aggregation().error());
    check(summary->features().size()==3 && panel->session().dataset()->features().size()==1,"Regional analysis must use all source features, not displayed genes");
    check(summary->counts()==std::vector<std::uint64_t>{9007199254740994ULL,8,0,7,3,0,0,UINT64_MAX,0},"Literal exact full-source regional sums differ");
    check(summary->cohorts()[0].observations==std::vector<std::uint64_t>{0,1} && summary->cohorts()[1].observations==std::vector<std::uint64_t>{2,3,4} && summary->cohorts()[2].observations.empty(),"Captured ROI memberships differ");
    check(summary->mean(1,0)==3.5 && !summary->mean(0,2),"Native means/empty cohort contract differs");
    frame({},-1,true);check(!panel->aggregation().enabled(Action::Keep) && !panel->aggregation().enabled(Action::Run),"Busy project must disable edits/publication");glfwSwapBuffers(window);
    click(Action::Keep);check(expression_aggregation_ids(*project,image)==std::vector<std::string>{summary->id()},"Actual Keep failed");
    doc->undo();frame();glfwSwapBuffers(window);check(expression_aggregation_ids(*project,image).empty(),"Keep must be one undo");
    // A metadata undo changes the exact captured document. Explicitly rerun.
    click(Action::Run);settle();click(Action::Keep);frame();glfwSwapBuffers(window);check(!panel->aggregation().enabled(Action::Keep),"Duplicate Keep must be disabled");
    project->save_new((root/"saved.qpx").u8string());panel.reset();selected.reset();project=Project::open((root/"saved.qpx").u8string());doc=project->document(image);selected=std::make_unique<ObjectSelection>(*doc);selected->replace({left,right,empty});
    panel=std::make_unique<Panel>();panel->load_preference(gene_workspace_preference(options,scope.sha256()));settle();aggregation_tab();check(!panel->aggregation().result(),"Opening project must not silently run a saved analysis");
    click(Action::Reopen);settle();check(panel->aggregation().result()->recipe()==summary->recipe() && panel->aggregation().result()->counts()==summary->counts(),"Actual Reopen changed exact recipe or sums");
    panel->paths()[0]->set("summary.csv");panel->paths()[1]->set("members.csv");click(Action::Export);settle();click(Action::Membership);settle();
    save_expression_aggregation_csv(*panel->aggregation().result(),(root/"reference-summary.csv").u8string());save_expression_membership_csv(*panel->aggregation().result(),(root/"reference-members.csv").u8string());
    const auto csv=read(root/"summary.csv");check(csv==read(root/"reference-summary.csv") && csv.find("9007199254740994")!=std::string::npos && csv.find("18446744073709551615")!=std::string::npos,"Actual CSV is not exact native output");check(read(root/"members.csv")==read(root/"reference-members.csv"),"Actual membership CSV differs from native output");
    click(Action::Export);check(!panel->aggregation().error().empty() && read(root/"summary.csv")==csv && !gate.pending(),"Existing-file export must refuse visibly without overwrite dialog");
    panel->paths()[0]->set("protected.csv");click(Action::Export);check(!panel->aggregation().error().empty() && !fs::exists(root/"protected.csv"),"Protected nonexistent path must refuse");
    doc->update_roi(left,Roi::rectangle({5,15,24,10}));frame();glfwSwapBuffers(window);check(!panel->aggregation().enabled(Action::Keep),"Edited ROI must invalidate Keep");
    // Reopen replays captured memberships, not current edited geometries.
    click(Action::Reopen);settle();check(panel->aggregation().result()->recipe()==summary->recipe(),"Historical replay used edited ROI geometry");
    click(Action::Remove);check(expression_aggregation_ids(*project,image).empty(),"Actual Remove failed");doc->undo();check(expression_aggregation_ids(*project,image).size()==1,"Remove must be one undo");
    selected->replace({left,right});click(Action::Capture);panel->aggregation().pseudobulk(true);click(Action::Run);settle();check(!panel->aggregation().error().empty() && panel->aggregation().result()->recipe()==summary->recipe(),"Missing pseudobulk labels must fail and preserve old preview");
    for(std::size_t i=0;i<panel->aggregation().drafts().size();++i){auto& draft=panel->aggregation().drafts()[i];label(draft.sample,"sample "+std::to_string(i+1));label(draft.replicate,"replicate "+std::to_string(i+1));label(draft.group,i?"treated":"control");}
    click(Action::Run);settle();const auto bulk=panel->aggregation().result();check(bulk && bulk->kind()==ExpressionAggregationKind::Pseudobulk && bulk->counts()==std::vector<std::uint64_t>{9007199254740994ULL,8,7,3,0,UINT64_MAX},"Actual pseudobulk raw exact sums differ");check(bulk->cohorts()[1].sample=="sample 2" && bulk->cohorts()[1].replicate=="replicate 2" && bulk->cohorts()[1].group=="treated","Explicit cohort labels were lost");
    doc->update_roi(right,Roi::rectangle({5,15,57,10}));click(Action::Run);settle();check(!panel->aggregation().error().empty() && panel->aggregation().result()==bulk,"Overlapping pseudobulk regions must fail without replacing preview");
    frame();glfwSwapBuffers(window);check(!panel->aggregation().enabled(Action::Keep),"Failed new run must not stamp the old result as current");
    doc->update_roi(right,Roi::rectangle({32,15,30,10}));click(Action::Run);panel->aggregation().cancel();settle();check(panel->aggregation().state()=="cancelled" && panel->aggregation().result()==bulk,"Cancelled run must retain prior immutable preview");
    click(Action::Run);settle();click(Action::Keep);
    const auto style=ImGui::GetStyle();glfwSetWindowSize(window,2200,2000);
    for(int language:{0,2}){
        ui::i18n::choose(language);ImGui::GetStyle()=style;if(language){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        frame();glfwSwapBuffers(window);auto* tool=ImGui::FindWindowByName("Gene expression");check(tool,"Genes tool missing");ImGui::SetWindowPos(tool,{20,20});ImGui::SetWindowSize(tool,language?ImVec2{2100,1900}:ImVec2{1000,1500});ImGui::SetScrollY(tool,0);
        std::string text;shown=&text;for(int i=0;i<4;++i){frame();check(ui::layout_stats.overflow_items==0,"Aggregation normal/French200 overflow: "+ui::layout_stats.first_overflow);if(i==3)screenshot(root/(language?"aggregation-fr200.ppm":"aggregation-normal.ppm"),window);glfwSwapBuffers(window);}shown=nullptr;
        check(text.find("9007199254740994")!=std::string::npos && text.find(ui::i18n::text("Export summaries CSV"))!=std::string::npos && text.find(ui::i18n::text("Pseudobulk"))!=std::string::npos,"Exact results and translated actions must be drawn");
    }
    ui::i18n::choose(0);ImGui::GetStyle()=style;
    // A different image must retire the old per-image results and drafts.
    image=project->add_document(std::make_shared<Document>(Image(20,20,std::vector<std::uint8_t>(20*20*3,55))));doc=project->document(image);selected=std::make_unique<ObjectSelection>(*doc);frame();glfwSwapBuffers(window);
    check(!panel->aggregation().result() && panel->aggregation().drafts().empty(),"Cross-image session leaked aggregation state");
    panel.reset();check(!gate.watching("Genes"),"Completed exports left dangling watchers");check(read(root/"source.qmx")==source_before,"Desktop analysis modified original matrix bytes");
    std::cout<<"Desktop A02: actual tab/capture/run/keep/reopen/remove/export controls, exact UInt64/labels/empty/overlap, cancellation/stale/blocked/image scopes, one-undo metadata, saved replay, new-only protected outputs and normal/French200 readability pass\n";
}
int main(){std::cout<<std::unitbuf;try{
    const auto root=fs::current_path()/("gui-aggregation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);std::cout<<"Evidence: "<<root.u8string()<<'\n';
    check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1600,1800,"Native regional summaries",nullptr,nullptr);check(window,"Create aggregation window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;int maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);ImGui::GetIO().FontDefault=detail::configure_gui_fonts(*ImGui::GetIO().Fonts,maximum);
    ImGui::LoadIniSettingsFromMemory("[Window][Gene expression]\nPos=20,20\nSize=1000,1700\nCollapsed=0\n");ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 130");exercise(root,window);
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
