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
using Fit=detail::GuiExpressionReduction;
using DE=detail::GuiExpressionDifferential;
using Aggregate=detail::GuiExpressionAggregation;
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
void close_value(double actual,double expected){check(std::abs(actual-expected)<1e-8,"Analytical desktop value differs");}
void write(const fs::path& path,const std::string& value){std::ofstream out(path,std::ios::binary);out<<value;out.close();check(bool(out),"Write molecular GUI fixture");}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void label(std::array<char,257>& target,const std::string& value){target.fill(0);check(value.size()<target.size(),"Authored label bound");std::copy(value.begin(),value.end(),target.begin());}
void screenshot(const fs::path& path,GLFWwindow* window){
    int w,h;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgb(std::size_t(w)*h*3);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";for(int y=h-1;y>=0;--y)out.write(reinterpret_cast<const char*>(rgb.data()+std::size_t(y)*w*3),w*3);out.close();check(bool(out),"Capture molecular GUI");
}
void exercise(const fs::path& root,GLFWwindow* window){
    write(root/"features.tsv","feature_id\nG0\nG1\nG2\n");write(root/"observations.tsv","observation_id\nO0\nO1\nO2\nO3\n");
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n3 4 8\n1 1 1\n1 2 3\n1 3 3\n1 4 7\n2 1 999\n2 2 997\n2 3 997\n2 4 993\n");
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(root/"source.qmx").u8string());
    const auto source=open_expression_matrix(imported.path,imported.file_sha256);const auto raw_before=read(root/"source.qmx");
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(80,60,std::vector<std::uint8_t>(80*60*3,55)));auto image=project->add_document(doc);keep_expression_matrix(*project,image,source,true);
    const auto coordinate_frame=image_coordinate_frame(*doc,"molecular-image");std::vector<std::uint64_t> regions;
    for(std::size_t i=0;i<4;++i){const double x=10.+15*i;const auto point=doc->add_detection(Roi::points({{x,20}}));doc->set_object_properties(point,expression_matrix_point_properties(source->metadata().source_sha256,"O"+std::to_string(i),coordinate_frame));regions.push_back(doc->add_roi(Roi::rectangle({x-4,15,8,10})));}
    const auto scope=expression_matrix_spatial_scopes(*doc).at(0);GeneViewOptions view;view.enabled=true;view.genes={"G0"};
    auto selection=std::make_unique<ObjectSelection>(*doc);selection->replace(regions);
    auto panel=std::make_unique<Panel>();panel->load_preference(gene_workspace_preference(view,scope.sha256()));
    detail::OutputGate gate;gate.bind([](bool,bool){return detail::ExportOwnerStamp{};},[&]{return std::vector<std::string>{project->path(),(root/"protected.csv").u8string()};});
    std::string* shown=nullptr;
    const auto frame=[&](Point cursor={-100,-100},int mouse=-1,bool blocked=false){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(mouse>=0)io.AddMouseButtonEvent(0,mouse==1);ImGui::NewFrame();ui::begin_frame();
        if(shown){ImGui::LogToBuffer();ImGui::GetCurrentContext()->LogWindow=nullptr;}
        panel->update(*doc,true,project.get(),image);panel->draw({},project.get(),image,blocked,&gate,{root.u8string()},selection.get());gate.draw([](const auto&){return true;});
        if(shown){*shown=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();}ImGui::Render();int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    const auto settle=[&]{for(int i=0;i<12000;++i){frame();glfwSwapBuffers(window);if(panel->settled())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Molecular GUI did not settle");};
    const auto raw_click=[&](Rect bounds){const Point p{bounds.x+bounds.width/2,bounds.y+bounds.height/2};for(int button:{-1,1,0}){frame(p,button);glfwSwapBuffers(window);}};
    const auto tab=[&](Panel::Action action){frame();glfwSwapBuffers(window);check(panel->molecular_enabled(action),"Molecular tab unavailable");raw_click(panel->molecular_bounds(action));frame();glfwSwapBuffers(window);};
    const auto fit=[&](Fit::Action action){frame();glfwSwapBuffers(window);check(panel->reduction().enabled(action),"Fit control unavailable: "+std::to_string(int(action))+" / "+panel->reduction().error());raw_click(panel->reduction().bounds(action));};
    const auto de=[&](DE::Action action){frame();glfwSwapBuffers(window);check(panel->differential().enabled(action),"DE control unavailable: "+std::to_string(int(action))+" / "+panel->differential().error());raw_click(panel->differential().bounds(action));};
    const auto aggregate=[&](Aggregate::Action action){frame();glfwSwapBuffers(window);check(panel->aggregation().enabled(action),"Aggregation control unavailable");raw_click(panel->aggregation().bounds(action));};
    const auto qc=[&](Panel::Action action){frame();glfwSwapBuffers(window);check(panel->molecular_enabled(action),"QC control unavailable");raw_click(panel->molecular_bounds(action));};
    const auto caption_click=[&](const char* caption){
        ui::LayoutTrace trace;auto* previous=ui::layout_trace;ui::layout_trace=&trace;frame();glfwSwapBuffers(window);ui::layout_trace=previous;
        const auto item=std::find_if(trace.items.begin(),trace.items.end(),[&](const auto& value){return value.kind==ui::LayoutKind::Input && value.caption==caption;});
        check(item!=trace.items.end(),std::string("Input caption missing: ")+caption);const auto& r=item->rect;raw_click({r.Min.x,r.Min.y,r.GetWidth(),r.GetHeight()});
    };
    settle();check(panel->error().empty(),"Initial indexed source failed");tab(Panel::Action::ReductionTab);
    if(!panel->analysis_source().matrix()){check(panel->analysis_source().open_enabled(),"Open full matrix unavailable");raw_click(panel->analysis_source().open_bounds());settle();}
    panel->reduction().settings().components=1;panel->reduction().settings().seed=UINT64_MAX;fit(Fit::Action::Run);settle();
    const auto fitted=panel->reduction().result();check(fitted && panel->reduction().error().empty(),"PCA failed: "+panel->reduction().error());
    check(fitted->features()==std::vector<std::uint64_t>{0,1,2} && fitted->observation_ids()==std::vector<std::string>{"O0","O1","O2","O3"} && panel->session().dataset()->features().size()==1,"PCA used overlay subset or lost source axes");
    close_value(fitted->means()[0],3.5);close_value(fitted->means()[1],996.5);close_value(std::abs(fitted->embedding()[0]),2.5*std::sqrt(2.0));check(fitted->options().seed==UINT64_MAX,"Full UInt64 seed was truncated");
    tab(Panel::Action::MolecularTab);qc(Panel::Action::Qc);settle();qc(Panel::Action::LogNormalized);settle();
    const auto normalized=panel->session().normalization_result();check(normalized && panel->session().qc_result()->totals()==std::vector<std::uint64_t>(4,1000),"Connected QC did not use the full source");qc(Panel::Action::Keep);settle();qc(Panel::Action::Raw);settle();
    tab(Panel::Action::ReductionTab);caption_click("Use kept normalization");fit(Fit::Action::Run);settle();
    check(panel->reduction().result() && panel->reduction().result()->value_domain()=="log1p-library-size-normalized" && panel->reduction().result()->options().normalization_recipe==normalized->recipe(),"Actual normalization choice did not use the kept full-source recipe");
    caption_click("Use kept normalization");panel->reduction().settings().center=false;panel->reduction().settings().components=2;fit(Fit::Action::Run);settle();check(panel->reduction().result() && !panel->reduction().result()->options().center && panel->reduction().result()->options().components==2,"Explicit uncentered SVD settings were ignored");
    panel->reduction().settings().center=true;panel->reduction().settings().components=1;fit(Fit::Action::Run);check(panel->reduction().busy(),"Run should own its pending work before the next poll");
    auto* hiding=ImGui::FindWindowByName("Gene expression");const auto title=hiding->TitleBarRect();raw_click({title.Max.x-28,title.Min.y+2,24,title.GetHeight()-4});settle();frame();glfwSwapBuffers(window);
    check(!hiding->Active && panel->reduction().state()=="ready" && panel->reduction().result()->id()==fitted->id(),"Closing Genes cancelled, lost or changed the running fit");panel->open();settle();
    frame();glfwSwapBuffers(window);check(!panel->reduction().enabled(Fit::Action::Keep),"Keep must require a native archive");
    panel->paths()[2]->set("fitted.qpr");fit(Fit::Action::Archive);settle();fit(Fit::Action::Keep);check(expression_reduction_ids(*project,image)==std::vector<std::string>{fitted->id()},"Fit Keep failed");
    doc->undo();check(expression_reduction_ids(*project,image).empty(),"Fit Keep must be one undo");fit(Fit::Action::Run);settle();fit(Fit::Action::Keep);
    fit(Fit::Action::Verify);settle();check(panel->reduction().error().empty() && panel->reduction().result()->id()==fitted->id(),"Explicit native fit verification failed");
    for(const auto& item:std::vector<std::pair<std::size_t,std::string>>{{3,"embedding.csv"},{4,"loadings.csv"},{5,"scatter.svg"}})panel->paths()[item.first]->set(item.second);
    fit(Fit::Action::Embedding);settle();fit(Fit::Action::Loadings);settle();fit(Fit::Action::Plot);settle();
    save_expression_embedding_csv(*fitted,(root/"reference-embedding.csv").u8string());save_expression_loadings_csv(*fitted,(root/"reference-loadings.csv").u8string());save_expression_reduction_svg(*fitted,(root/"reference-scatter.svg").u8string());
    for(const auto& name:{"embedding.csv","loadings.csv","scatter.svg"})check(read(root/name)==read(root/(std::string("reference-")+name)),"Actual fitted export differs from native bytes");
    fit(Fit::Action::Embedding);check(!panel->reduction().error().empty() && !gate.pending(),"Existing fitted CSV must refuse without overwrite dialog");panel->paths()[3]->set("protected.csv");fit(Fit::Action::Embedding);check(!panel->reduction().error().empty() && !fs::exists(root/"protected.csv"),"Protected fitted CSV path accepted");
    fit(Fit::Action::Remove);check(expression_reduction_ids(*project,image).empty(),"Fit Remove failed");doc->undo();fit(Fit::Action::Reopen);settle();check(panel->reduction().result()->embedding()==fitted->embedding() && panel->reduction().result()->id()==fitted->id(),"Archive open refitted or changed arrays");
    frame({},-1,true);check(!panel->reduction().enabled(Fit::Action::Run) && !panel->reduction().enabled(Fit::Action::Keep),"Blocked fit work accepted");glfwSwapBuffers(window);
    fit(Fit::Action::Remove);frame();glfwSwapBuffers(window);check(panel->reduction().enabled(Fit::Action::Keep),"Current unkept fit should be keepable before the edit");
    doc->rename_annotation(regions[0],"changed draft owner");frame();glfwSwapBuffers(window);check(!panel->reduction().enabled(Fit::Action::Keep),"Stale fit Keep accepted");
    fit(Fit::Action::Run);panel->reduction().cancel();settle();check(panel->reduction().state()=="cancelled" && panel->reduction().result()->id()==fitted->id(),"Cancelled fit replaced prior preview");
    fit(Fit::Action::Run);settle();fit(Fit::Action::Keep);
    std::cout<<"Fit controls, archive identity, exports and refusals passed\n";
    tab(Panel::Action::AggregationTab);aggregate(Aggregate::Action::Capture);panel->aggregation().pseudobulk(true);
    for(std::size_t i=0;i<4;++i){auto& row=panel->aggregation().drafts()[i];label(row.id,"region "+std::to_string(i));label(row.sample,"sample "+std::to_string(i));label(row.replicate,"subject "+std::to_string(i));label(row.group,i<2?"A":"B");}
    aggregate(Aggregate::Action::Run);settle();aggregate(Aggregate::Action::Keep);const auto pseudobulk=panel->aggregation().result();check(pseudobulk && pseudobulk->cohorts().size()==4,"Desktop pseudobulk failed");
    tab(Panel::Action::DifferentialTab);de(DE::Action::OpenBulk);settle();de(DE::Action::Run);settle();check(!panel->differential().error().empty() && !panel->differential().result(),"Empty explicit groups must refuse");
    panel->differential().groups("A","B");panel->differential().library_target()=1000;de(DE::Action::Run);settle();const auto differential=panel->differential().result();
    check(differential && panel->differential().error().empty() && differential->n_a()==2 && differential->n_b()==2 && differential->correction_features()==2,"Desktop DE failed or used wrong full universe");
    close_value(differential->genes()[0].effect,std::log(2.0));close_value(*differential->genes()[0].p,1-1/std::sqrt(2.0));check(differential->genes()[2].status==ExpressionDifferentialStatus::AllZero && !differential->genes()[2].p && !differential->genes()[2].q,"Desktop DE manufactured absent statistics");
    frame({},-1,true);check(!panel->differential().enabled(DE::Action::Run) && !panel->differential().enabled(DE::Action::Keep),"Blocked DE work accepted");glfwSwapBuffers(window);
    frame();glfwSwapBuffers(window);check(panel->differential().enabled(DE::Action::Keep),"Current DE preview should be keepable before the edit");
    doc->rename_annotation(regions[1],"new document revision");frame();glfwSwapBuffers(window);check(!panel->differential().enabled(DE::Action::Keep),"Stale DE Keep accepted");
    de(DE::Action::Run);panel->differential().cancel();settle();check(panel->differential().result()==differential,"Cancelled DE replaced the previous preview");
    frame();glfwSwapBuffers(window);check(!panel->differential().enabled(DE::Action::Keep),"Cancelled DE stamped the old preview as current");de(DE::Action::Run);settle();
    de(DE::Action::Keep);check(expression_differential_ids(*project,image)==std::vector<std::string>{differential->id()},"DE Keep failed");doc->undo();check(expression_differential_ids(*project,image).empty(),"DE Keep must be one undo");de(DE::Action::Run);settle();de(DE::Action::Keep);
    panel->paths()[6]->set("genes.csv");panel->paths()[7]->set("replicates.csv");de(DE::Action::Genes);settle();de(DE::Action::Replicates);settle();save_expression_differential_csv(*differential,(root/"reference-genes.csv").u8string());save_expression_differential_replicates_csv(*differential,(root/"reference-replicates.csv").u8string());
    check(read(root/"genes.csv")==read(root/"reference-genes.csv") && read(root/"replicates.csv")==read(root/"reference-replicates.csv"),"Actual DE exports differ from native bytes");de(DE::Action::Genes);check(!panel->differential().error().empty() && !gate.pending(),"Existing DE output accepted");
    panel->paths()[6]->set("protected.csv");de(DE::Action::Genes);check(!panel->differential().error().empty() && !fs::exists(root/"protected.csv"),"Protected DE path accepted");
    de(DE::Action::Remove);check(expression_differential_ids(*project,image).empty(),"DE Remove failed");doc->undo();de(DE::Action::Reopen);settle();check(panel->differential().result()->recipe()==differential->recipe(),"DE saved recipe replay differs");
    std::cout<<"Pseudobulk/DE controls, analytical values, exports and refusals passed\n";
    project->save_new((root/"study.qpx").u8string());std::cout<<"Saved owned project\n";
    panel.reset();selection.reset();std::cout<<"Retired original panel\n";
    project=Project::open((root/"study.qpx").u8string());doc=project->document(image);std::cout<<"Opened owned project\n";
    selection=std::make_unique<ObjectSelection>(*doc);panel=std::make_unique<Panel>();panel->load_preference(gene_workspace_preference(view,scope.sha256()));settle();
    tab(Panel::Action::ReductionTab);fit(Fit::Action::Reopen);settle();check(panel->reduction().result() && panel->reduction().error().empty(),"Owned fitted open failed: "+panel->reduction().error());check(panel->reduction().result()->id()==fitted->id() && panel->reduction().result()->embedding()==fitted->embedding(),"Owned Save As/open changed fitted identity");
    tab(Panel::Action::DifferentialTab);de(DE::Action::Reopen);settle();check(panel->differential().result() && panel->differential().error().empty(),"Owned DE open failed: "+panel->differential().error());check(panel->differential().result()->recipe()==differential->recipe(),"Project reopen changed DE identity");
    std::cout<<"Saved exact fit/DE replay passed\n";
    const auto style=ImGui::GetStyle();glfwSetWindowSize(window,2200,2200);
    for(int language:{0,2}){
        ui::i18n::choose(language);ImGui::GetStyle()=style;if(language){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        frame();glfwSwapBuffers(window);auto* tool=ImGui::FindWindowByName("Gene expression");ImGui::SetWindowPos(tool,{20,20});ImGui::SetWindowSize(tool,language?ImVec2{2100,2100}:ImVec2{1000,1700});
        for(const auto action:{Panel::Action::ReductionTab,Panel::Action::DifferentialTab}){
            tab(action);ImGui::SetScrollY(tool,0);std::string text;shown=&text;
            for(int i=0;i<4;++i){frame();check(ui::layout_stats.overflow_items==0,"Molecular normal/French200 overflow: "+ui::layout_stats.first_overflow);if(i==3)screenshot(root/((action==Panel::Action::ReductionTab?"fit-":"de-")+std::string(language?"fr200.ppm":"normal.ppm")),window);glfwSwapBuffers(window);}shown=nullptr;
            check(text.find(ui::i18n::text(action==Panel::Action::ReductionTab?"Export embedding CSV":"Export differential genes CSV"))!=std::string::npos,"Translated export control missing");
            if(action==Panel::Action::ReductionTab)check(text.find("18446744073709551615")!=std::string::npos,"Stored fit seed must be displayed independently of the next-run draft");
            else if(language==0)check(text.find("Result library target: 1000")!=std::string::npos,"Stored DE target must be displayed independently of the next-run draft");
        }
    }
    ui::i18n::choose(0);ImGui::GetStyle()=style;
    // Retire a controller with an active export: shared watcher callbacks may
    // outlive it; atomic completion or cancellation must both be safe.
    panel->paths()[6]->set("retiring-export.csv");de(DE::Action::Genes);
    image=project->add_document(std::make_shared<Document>(Image(20,20,std::vector<std::uint8_t>(20*20*3,55))));doc=project->document(image);keep_expression_matrix(*project,image,source,true);keep_expression_aggregation(*project,image,*pseudobulk);selection=std::make_unique<ObjectSelection>(*doc);frame();glfwSwapBuffers(window);
    check(!panel->reduction().result() && !panel->differential().result() && !panel->differential().parent(),"Cross-image analysis state leaked");
    tab(Panel::Action::ReductionTab);check(panel->analysis_source().open_enabled(),"Nonspatial retained matrix route missing");raw_click(panel->analysis_source().open_bounds());settle();panel->reduction().settings().components=1;fit(Fit::Action::Run);settle();check(panel->reduction().result() && panel->reduction().error().empty(),"Nonspatial PCA route failed");
    tab(Panel::Action::DifferentialTab);de(DE::Action::OpenBulk);settle();panel->differential().groups("A","B");panel->differential().library_target()=1000;de(DE::Action::Run);settle();check(panel->differential().result()->id()==differential->id(),"Nonspatial DE route changed exact identity");
    panel.reset();check(!gate.watching("Genes"),"Retired output left dangling watcher");check(read(root/"source.qmx")==raw_before,"Molecular desktop changed raw source bytes");
    std::cout<<"Molecular desktop end to end: full-source PCA/archive/verify/Keep/open/exports, ROI pseudobulk to replicate DE, analytical effects/statuses, saved exact replay, new-only/protected exports, undo/cancellation/stale/blocked/image retirement/nonspatial routes and normal/French200 readability pass\n";
}
int main(int argc,char** argv){std::cout<<std::unitbuf;try{
    if(argc==2){
        const auto project=Project::open(argv[1]);const auto image=project->active_image();const auto ids=expression_reduction_ids(*project,image);check(!ids.empty(),"Compatibility project has no fit");
        for(const auto& id:ids){const auto request=expression_reduction_open_request(*project,image,id);const auto result=request.open(detail::molecular_work);check(result->id()==id && result->options().seed==UINT64_MAX,"Old high-seed archive identity differs");replay_expression_reduction(request.parent.open(detail::molecular_work),*result,detail::molecular_work);}
        std::cout<<"Previously failing high-seed project opens and refits with exact fitted identity\n";return 0;
    }
    check(argc==1,"Expected no arguments or a previously failing project");
    if(!has_project() || !has_expression_matrix_storage() || !has_expression_reduction() || !has_expression_statistics()){
        std::cout<<"Optional project/matrix/algebra/statistics capability disabled; full molecular desktop test skipped\n";return 77;
    }
    const auto root=fs::current_path()/("gui-molecular-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);std::cout<<"Evidence: "<<root.u8string()<<'\n';check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(1600,1900,"Native molecular workflow",nullptr,nullptr);check(window,"Create molecular GUI window");glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;int maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);ImGui::GetIO().FontDefault=detail::configure_gui_fonts(*ImGui::GetIO().Fonts,maximum);ImGui::LoadIniSettingsFromMemory("[Window][Gene expression]\nPos=20,20\nSize=1000,1800\nCollapsed=0\n");ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 130");exercise(root,window);ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
