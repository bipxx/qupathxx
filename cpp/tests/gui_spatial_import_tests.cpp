#include "../gui_spatial_import.hpp"
#include "../gui_genes.hpp"
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
        write(root/"indexed-counts.mtx","%%MatrixMarket matrix coordinate integer general\n40 3 3\n1 1 5\n33 2 19\n40 3 9\n");
        std::string indexed_features="feature_id\n";
        for(int i=0;i<40;++i)indexed_features+="Idx"+std::string(i<10?"0":"")+std::to_string(i)+"\n";
        write(root/"indexed-features.tsv",indexed_features);write(root/"indexed-observations.tsv","observation_id\nA\nB\nZero\n");
        BackedSpatialImportRequest indexed_request;indexed_request.base.input_project=request.input_project;indexed_request.base.output_project=(root/"indexed.qpx").u8string();indexed_request.base.image_id=1;
        indexed_request.store=(root/"indexed.qmx").u8string();indexed_request.positions=request.positions;
        wizard->set_request(indexed_request);wizard->set_index_inputs((root/"indexed-counts.mtx").u8string(),(root/"indexed-features.tsv").u8string(),(root/"indexed-observations.tsv").u8string(),indexed_request.store);
        wizard->read_images();wizard->wait();wizard->read_genes();wizard->wait();
        check(wizard->indexed_source() && wizard->indexed_source()->metadata().features==40,"Desktop indexes the entire matrix through native worker");
        wizard->preview();wizard->wait();
        check(wizard->backed_result() && wizard->backed_result()->spatial().source()==wizard->indexed_source(),"Desktop preview reuses its opened source without a second fingerprint");
        for(int i=0;i<3000 && !wizard->preview_ready();++i){frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        check(wizard->preview_ready(),"Indexed preview image and spots settle");frame();capture(root/"indexed-preview.ppm",window);glfwSwapBuffers(window);
        frame(0);glfwSwapBuffers(window);frame(1);glfwSwapBuffers(window);wizard->wait();
        check(wizard->phase()==SpatialImportPhase::Complete && wizard->error().empty(),"Actual desktop import button publishes full indexed project");
        frame();glfwSwapBuffers(window);frame(0,detail::GuiSpatialImporter::Action::Open);glfwSwapBuffers(window);
        check(frame(1,detail::GuiSpatialImporter::Action::Open)==indexed_request.base.output_project,"Indexed Open returns exact project to app loader");glfwSwapBuffers(window);
        auto indexed_project=Project::open(indexed_request.base.output_project);auto indexed_doc=indexed_project->document(1);
        const auto scopes=expression_matrix_spatial_scopes(*indexed_doc);check(scopes.size()==1 && indexed_doc->annotation(1).properties->measurements().empty(),"Desktop persists native count-free scoped observations");
        auto genes=std::make_unique<detail::GuiGeneDisplay>();GeneViewOptions gene_options;gene_options.enabled=true;gene_options.genes={"Idx39","Idx00"};
        genes->load_preference(gene_workspace_preference(gene_options,scopes[0].sha256()));wizard->close();
        const auto gene_frame=[&](int click=-1,Rect action=Rect{}) {
            glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
            if(click>=0){check(action.width>0,"Indexed gene action has usable bounds");ImGui::GetIO().AddMousePosEvent(float(action.x+action.width/2),float(action.y+action.height/2));ImGui::GetIO().AddMouseButtonEvent(0,click==0);}
            ImGui::NewFrame();genes->update(*indexed_doc,true,indexed_project.get(),1);genes->draw(DisplaySettings{});ImGui::Render();
            int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        };
        const auto settle_genes=[&] {
            for(int i=0;i<3000;++i){gene_frame();glfwSwapBuffers(window);if(genes->settled())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            throw std::runtime_error("Indexed gene controller did not settle");
        };
        settle_genes();check(genes->session().result() && genes->session().result()->fields[0]->feature=="Idx39","Actual gene panel loads previously unprojected feature");
        check(genes->backed_catalogue(0) && genes->backed_catalogue(0)->features.size()==16 && genes->backed_catalogue(0)->matches==40,"Gene catalogue is bounded to one native page");
        for(unsigned page=1;page<=2;++page) {
            const auto next=genes->catalogue_next_bounds(0);gene_frame(0,next);glfwSwapBuffers(window);gene_frame(1,next);glfwSwapBuffers(window);settle_genes();
            check(genes->backed_catalogue(0) && genes->backed_catalogue(0)->offset==page*16,"Actual Next action advances native catalogue page");
        }
        const auto pick=genes->catalogue_feature_bounds(0,32);gene_frame(0,pick);glfwSwapBuffers(window);gene_frame(1,pick);glfwSwapBuffers(window);settle_genes();
        check(genes->session().result() && genes->session().result()->fields[0]->feature=="Idx32" && genes->session().dataset()->counts()==std::vector<std::uint64_t>{0,19,0},"Actual paged gene selection produces exact native values");
        gene_frame();capture(root/"indexed-gene-panel.ppm",window);glfwSwapBuffers(window);
        const auto old_owned=load_expression_matrix_association(*indexed_project,1,scopes[0].matrix_source_sha256);
        const auto moved_folder=root/"moved-indexed";fs::create_directory(moved_folder);indexed_project->save_new((moved_folder/"study.qpx").u8string());
        fs::rename(root/fs::u8path(old_owned.path),root/"old-owned-offline.qmx");settle_genes();
        check(genes->session().result() && fs::u8path(genes->session().spatial_snapshot()->source()->path()).parent_path()==moved_folder/".qpx-expression","Live desktop follows exact owned Save As and retires old locator");
        const auto changed_store=genes->session().spatial_snapshot()->source()->path();
        {std::ofstream changed(fs::u8path(changed_store),std::ios::binary|std::ios::app);changed.put('x');changed.close();check(bool(changed),"Actually mutate synthetic backing store");}
        gene_options.genes[0]="Idx00";genes->load_preference(gene_workspace_preference(gene_options,scopes[0].sha256()));settle_genes();
        check(!genes->session().result() && !genes->session().error().empty(),"Changed backing store produces visible error instead of stale values");
        genes.reset();
        if(argc>3) {
            const auto demo=fs::u8path(argv[2]);BackedSpatialImportRequest real;
            real.base.input_project=(demo/"base.qpx").u8string();real.base.output_project=(root/"real-indexed.qpx").u8string();real.base.image_id=1;
            real.store=argv[3];real.positions=(demo/"positions.tsv").u8string();real.owned=false;
            wizard->set_request(real);wizard->read_images();wizard->wait();wizard->read_genes();wizard->wait();wizard->preview();wizard->wait();
            check(wizard->backed_result() && wizard->indexed_source()->metadata().features==32285 && wizard->backed_result()->mapped_positions().size()==2702,"Real desktop preview retains full source and all spots");
            for(int i=0;i<3000 && !wizard->preview_ready();++i){frame();glfwSwapBuffers(window);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            check(wizard->preview_ready(),"Real indexed image/coordinate preview settles");frame();capture(root/"real-indexed-preview.ppm",window);glfwSwapBuffers(window);
            frame(0);glfwSwapBuffers(window);frame(1);glfwSwapBuffers(window);wizard->wait();
            check(wizard->phase()==SpatialImportPhase::Complete && Project::open(real.base.output_project)->document(1)->object_count()==2702,"Real full-matrix import completes through actual desktop action");
        } else if(argc>2) {
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
