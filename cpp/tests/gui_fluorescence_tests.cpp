#include "gui_translation_trace.hpp"
#include "gui_path_field_checks.hpp"
#include "../gui_fluorescence.hpp"
#include "../gui_pixel_classifier.hpp"
#include "../gui_raw_pixel.hpp"
#include "object_forest_fixture.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <imgui.h>
#include <imgui_internal.h>
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
namespace qpx::detail {
struct GuiPixelClassifierTestAccess {
    static void stop_contours(GuiPixelClassifier& panel,Document& doc,const PixelClassifier& model,std::uint64_t roi) {
        panel.adopt(std::make_shared<const PixelClassifier>(model),doc);
        panel.prediction_request_.emplace(doc,roi,model);
        check(panel.prediction_.request(*panel.prediction_request_),"Pixel test prediction submission");panel.prediction_.wait(doc);
        const auto prediction=panel.prediction_.result();check(bool(prediction),"Pixel test prediction failed");
        panel.preview_.update(prediction,panel.palette_,true,panel.preview_limit_);check(panel.preview_ready(),"Pixel Stop fixture needs a ready preview");
        if(has_geos()) {
            check(panel.contours_.request(*panel.prediction_request_,prediction),"Pixel contour job submission");
            check(panel.contours_.busy() && !panel.prediction_.busy(),"Stop fixture must cancel tracing, not prediction");
            panel.stop_busy_work();panel.contours_.wait(doc);
            check(panel.result()==prediction && panel.preview_ready(),"Stop tracing discarded the finished pixel prediction/preview");
        }
    }
};
struct GuiPixelStreamBrowserTestAccess {
    static void mark_kept(GuiPixelStreamBrowser& panel,std::uint64_t id){panel.kept_=panel.selected_=id;panel.kept_link_=panel.stats_.link;}
    static void verified(GuiPixelStreamBrowser& panel,const std::string& path) {
        check(panel.job_.open(path,0,1ULL<<30),"Browser fixture open");panel.job_.wait();panel.update();
        check(panel.job_.verify(),"Browser fixture Verify");panel.job_.wait();panel.update();check(panel.stats_.verified,"Browser fixture not verified");
    }
};
}
void finished_pixel_work(const fs::path& root,GLFWwindow* window) {
    Document doc(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));const auto roi=doc.add_rectangle({0,0,32,32});
    PixelFeatureOptions features;features.sigmas={0};StainTransform transform({{1,0,0},{0,1,0}});std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& f:pixel_feature_schema(features,transform))schema.emplace_back(f.name,f.unit);
    PixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),features,transform,PixelResolution{});
    GuiOptions options;detail::GuiPixelClassifier pixel(options);detail::GuiPixelClassifierTestAccess::stop_contours(pixel,doc,model,roi);
    PixelStreamOptions settings;settings.block_size=32;PixelPredictionStream stream(doc,roi,model,settings);const auto file=root/"finished-stream.qpxpixels";save_pixel_stream(stream,file.u8string());
    options.pixel_stream_browser_open=true;detail::GuiPixelStreamBrowser browser(options);detail::GuiPixelStreamBrowserTestAccess::verified(browser,file.u8string());const auto link=browser.stats().link;
    auto frame=[&](Point p={-100,-100},int down=-1){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(p.x),float(p.y));if(down>=0)io.AddMouseButtonEvent(0,down==1);ImGui::NewFrame();browser.update();browser.draw(doc,false,[](GuiInputTarget){});ImGui::Render();glfwSwapBuffers(window);};
    for(int i=0;i<4;++i)frame();auto* w=ImGui::FindWindowByName("Saved pixel stream files");check(w,"Browser window absent");
    const float edge=ImGui::GetFontSize()+ImGui::GetStyle().FramePadding.y*2;Point close{w->Pos.x+w->Size.x-edge*.5,w->Pos.y+edge*.5};
    frame(close);frame(close,1);frame(close,0);frame();
    check(!w->Active,"Browser X did not hide window");
    check(browser.stats().verified && browser.stats().link==link,"Browser X discarded verified stream authority");
    browser.open();frame();check(browser.stats().verified && browser.stats().link==link,"Browser reopen lost verified stream");
    std::map<std::uint64_t,std::shared_ptr<const PixelStreamRecord>> records;auto identity=std::make_shared<const char>(0);
    browser.catalog(doc,records,identity);const auto kept=doc.keep_pixel_stream(link);records.emplace(kept,doc.pixel_stream_record(kept));browser.catalog(doc,records,identity);detail::GuiPixelStreamBrowserTestAccess::mark_kept(browser,kept);
    doc.remove_pixel_stream_record(kept);records.clear();browser.catalog(doc,records,identity);frame();
    check(!browser.kept() && browser.stats().verified && browser.stats().link==link,"Removed stream record retained its receipt or lost the verified link");
    const auto keep=browser.bounds(GuiInputTarget::KeepPixelStreamLink);Point keep_at{keep.x+keep.width/2,keep.y+keep.height/2};frame(keep_at);frame(keep_at,1);frame(keep_at,0);frame();
    check(doc.pixel_stream_record_count()==1 && browser.kept()!=kept,"Verified stream could not be kept again after removal");
    std::cout<<"Pixel Stop retains ready prediction/preview; actual browser X/reopen retains verified link passed\n";
}
namespace qpx::detail {
struct GuiRawPixelTestAccess {
    static RawPixelFrontend& jobs(GuiRawPixel& panel){return panel.jobs_;}
    static void record(GuiRawPixel& panel,std::uint64_t id){panel.record_=id;}
    static auto selected(const GuiRawPixel& panel){return panel.record_;}
    static auto palette(GuiRawPixel& panel,const Document& doc,std::size_t size,std::shared_ptr<const RawPixelClassifier> model){return panel.palette(doc,std::vector<std::uint64_t>(size),model);}
};
}
void raw_record_lifecycle(const fs::path& root,GLFWwindow* window) {
    const auto image=root/"review-raw.tif",file=root/"review-raw.qws";qpx_test::raw_pixel_tiff_fixture(image,15,9);Document doc(open_tiff(image.u8string()));ObjectSelection selection(doc);
    ScientificRawLattice lattice(15,9,{0,15,15,0},{0,9,9,0});ScientificRawOptions channels;channels.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};
    RawPixelRecipe recipe(*doc.server()->raw_metadata(),lattice,channels,features);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema())schema.emplace_back(f.name,f.unit);
    auto model=std::make_shared<const RawPixelClassifier>(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),recipe);RawPixelTiledPredictionRequest request(doc,*model,lattice);
    save_raw_pixel_stream(request,file.u8string());GuiOptions options;GuiRawPixelOptions raw;raw.open=true;raw.show_overlay=false;options.raw_pixel=std::make_shared<const GuiRawPixelOptions>(raw);detail::GuiRawPixel panel(options);panel.update(doc);
    auto& jobs=detail::GuiRawPixelTestAccess::jobs(panel);jobs.open(file.u8string(),{},0);jobs.wait(doc);panel.update(doc);check(bool(jobs.report().link),"Raw removal fixture could not verify file");const auto kept=jobs.keep_link(doc);
    const auto colors=detail::GuiRawPixelTestAccess::palette(panel,doc,3,model);check(colors.size()==3 && colors[0]!=colors[2] && colors[1]!=colors[2],"Raw fallback beyond model classes collapsed");
    detail::RecordRemovalDialog removal;std::string error;
    auto frame=[&](Point at={-100,-100},int down=-1){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(at.x),float(at.y));if(down>=0)io.AddMouseButtonEvent(0,down==1);ImGui::NewFrame();panel.update(doc);panel.draw(doc,selection,nullptr,false,true,[](GuiInputTarget){},&removal,1);removal.draw(doc,1,error);ImGui::Render();glfwSwapBuffers(window);};
    auto click=[&](GuiInputTarget target){frame();const auto r=panel.bounds(target);check(r.width>0 && panel.enabled(target),"Raw record control absent/disabled");Point at{r.x+r.width/2,r.y+r.height/2};frame(at);frame(at,1);frame(at,0);frame();};
    for(int i=0;i<4;++i)frame();auto* w=ImGui::FindWindowByName("Raw pixel classifier");ImGui::SetWindowPos(w,{10,10});ImGui::SetWindowSize(w,{900,950});frame();click(GuiInputTarget::RawPixelRecordsTab);
    detail::GuiRawPixelTestAccess::record(panel,UINT64_MAX);click(GuiInputTarget::RawPixelRemoveRecord);
    check(!panel.report().error.empty() && !removal.pending() && doc.raw_pixel_stream_record_count()==1,"Invalid typed Raw record ID escaped its error boundary");
    detail::GuiRawPixelTestAccess::record(panel,kept);click(GuiInputTarget::RawPixelRemoveRecord);check(removal.pending(),"Raw removal confirmation did not open");
    const auto* modal=ImGui::FindWindowByName(ui::dialog_definition(ui::DialogKind::RemoveKeptResult).name);check(modal,"Raw removal modal absent");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("Remove kept result").c_str(),0,modal->ID));frame();frame();
    check(!doc.raw_pixel_stream_record_count() && !panel.report().stream_record && panel.enabled(GuiInputTarget::RawPixelKeepLink),"Confirmed Raw removal stranded the Keep receipt");click(GuiInputTarget::RawPixelKeepLink);
    check(doc.raw_pixel_stream_record_count()==1 && panel.report().stream_record!=kept,"Verified raw link could not be kept again after removal");
    std::cout<<"Review Raw: invalid typed selection errors without confirmation; confirmed removal resets receipt; Keep again and extra-class palette passed\n";
}
void exercise(const fs::path& root,GLFWwindow* window,const std::string& real){
    ui::WorkflowTrace actions;struct RestoreTrace {ui::WorkflowTrace* previous;~RestoreTrace(){ui::workflow_trace=previous;}} restore_trace{ui::workflow_trace};ui::workflow_trace=&actions;
    const auto file=root/"source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);auto doc=std::make_shared<Document>(open_tiff(file.u8string()));
    const auto roi=doc->add_rectangle({1,1,12,7},"Analysis ROI");std::uint64_t selected=roi;
    detail::GuiFluorescence panel;panel.open();RawRegionRequest request;request.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=1.5;
    Viewport view;view.zoom=10;view.origin={0,0};
    std::string visible_text;
    const auto frame=[&](Point cursor={-100,-100},int button=-1,bool blocked=false){
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();auto& io=ImGui::GetIO();io.AddMousePosEvent(float(cursor.x),float(cursor.y));if(button>=0)io.AddMouseButtonEvent(0,button==1);
        ImGui::NewFrame();ui::reset_workflow_trace();ImGui::LogToBuffer();panel.draw(*doc,selected,blocked);visible_text=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();panel.draw_preview(ImGui::GetBackgroundDrawList(),view,{800,100},{400,400});ImGui::Render();
        int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.08f,.08f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    const auto click=[&](detail::GuiFluorescence::Action action,bool blocked=false){const auto r=panel.bounds(action);check(r.width>0 && r.height>0,"Actual fluorescence action bounds");const Point p{r.x+r.width/2,r.y+r.height/2};frame(p,-1,blocked);glfwSwapBuffers(window);frame(p,1,blocked);glfwSwapBuffers(window);frame(p,0,blocked);glfwSwapBuffers(window);};
    const auto settled=[&]{for(unsigned i=0;i<3000;++i){frame();glfwSwapBuffers(window);if(!panel.busy())return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}throw std::runtime_error("Fluorescence UI did not settle");};
    selected=0;frame();glfwSwapBuffers(window);
    check(visible_text.find("Select an area annotation or turn off Use selected ROI")!=std::string::npos,"Unselected fluorescence Run has no visible reason");
    const auto tiff=doc;doc=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));frame();glfwSwapBuffers(window);
    check(visible_text.find("Fluorescence analysis needs a quantitative TIFF image.")!=std::string::npos && visible_text.find("Select an area annotation or turn off Use selected ROI")==std::string::npos,"Fluorescence reasons are not in prerequisite order");
    doc=tiff;selected=roi;
    frame();glfwSwapBuffers(window);panel.set_request(request,options,{0,0});frame();glfwSwapBuffers(window);
    click(detail::GuiFluorescence::Action::Domain);frame();glfwSwapBuffers(window);
    check(visible_text.find("Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction.")!=std::string::npos && visible_text.find("no longer matches")==std::string::npos,"Never-selected correction was described as mismatched");
    click(detail::GuiFluorescence::Action::Run);check(!panel.result(),"Missing correction allowed Run");click(detail::GuiFluorescence::Action::Domain);
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
    click(detail::GuiFluorescence::Action::Remove);check(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId),"Undoable removal opened a modal");check(panel.error().empty() && fluorescence_cell_record_ids(*doc).empty(),"Actual Remove uses native record service");doc->undo();check(fluorescence_cell_record_ids(*doc)==std::vector<std::string>{record},"Remove is undoable");
    options.threshold=10000;panel.set_request(request,options,{0,0});frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();check(panel.result() && panel.result()->raster().measurements.empty(),"Empty native result remains inspectable");
    const auto before=doc->object_count();click(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty() && doc->object_count()==before && fluorescence_cell_record_ids(*doc).size()==2,"Zero-cell Keep does not fabricate cells");
    click(detail::GuiFluorescence::Action::Run);click(detail::GuiFluorescence::Action::Cancel);settled();check(doc->object_count()==before,"Cancellation never publishes objects");
    *doc=Document(doc->server());frame();glfwSwapBuffers(window);check(!panel.result() && panel.kept_record().empty(),"Same-address Document replacement still invalidates native capture authority");
    doc=std::make_shared<Document>(open_tiff(file.u8string()));frame();glfwSwapBuffers(window);check(!panel.result() && panel.kept_record().empty(),"Switching images discards prior result/record selection");
    request.region={0,0,0,15,9};options.threshold=10000;panel.set_request(request,options,{0,0},false);
    FluorescenceTileOptions tiled;tiled.width=6;tiled.height=5;tiled.halo=8;panel.set_tiling(true,tiled,"");path_field_checks::choose(*panel.paths().back(),(root/"tiled-results").u8string(),[&]{frame();});
    frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run,true);settled();check(!panel.tiled_result(),"Blocked editing/save state prevents tiled run");
    click(detail::GuiFluorescence::Action::Run);settled();check(panel.error().empty() && panel.tiled_result() && panel.tiled_result()->state=="ready" && panel.tiled_result()->segmented_tiles==6 && !panel.result(),"Actual tiled Run uses native checkpoints and typed summary");
    frame();glfwSwapBuffers(window);
    check(visible_text.find("Tiled runs write masks and measurements to the folder above. They are not added to the project.")!=std::string::npos,"Tiled run destination guidance missing");
    check(visible_text.find("Add cells")!=std::string::npos && visible_text.find("Keep result")!=std::string::npos,"Tiled result hides its Add/Keep actions");
    for(const auto* key:{"Keep fluorescence cells","Keep fluorescence run"}){const auto found=actions.actions.find(key);check(found!=actions.actions.end() && found->second.disabled && found->second.reason==ui::i18n::text("Run without tiles to keep results in the project."),"Tiled Add/Keep is not disabled with its reason");}
    const auto tiled_objects=doc->object_count();const auto tiled_records=fluorescence_cell_record_ids(*doc);const auto tiled_report=panel.tiled_result();
    click(detail::GuiFluorescence::Action::KeepCells);click(detail::GuiFluorescence::Action::KeepRecord);
    check(!panel.busy() && doc->object_count()==tiled_objects && fluorescence_cell_record_ids(*doc)==tiled_records && panel.tiled_result()==tiled_report && !panel.result(),"Disabled tiled Add/Keep changed native results or started work");
    const auto tiled_identity=panel.tiled_result()->output_sha256;frame();capture(root/"tiled-results.ppm",window);glfwSwapBuffers(window);
    click(detail::GuiFluorescence::Action::Run);settled();check(panel.tiled_result() && panel.tiled_result()->reused_tiles==6 && !panel.tiled_result()->segmented_tiles && panel.tiled_result()->output_sha256==tiled_identity,"Actual Run/resume reuses native masks/moments without resegmentation");
    doc->set_calibration(.5,.5);frame();glfwSwapBuffers(window);check(!panel.tiled_result(),"Ready tiled report invalidates on owner calibration change");doc->undo();
    options.threshold=10001;panel.set_request(request,options,{0,0},false);frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();
    check(!panel.error().empty() && panel.state()=="failed" && !panel.tiled_result(),"Changed tiled options show native checkpoint mismatch instead of stale results");
    frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Tiling);options.threshold=10000;panel.set_request(request,options,{0,0},false);
    frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();check(panel.result() && !panel.tiled_result() && panel.error().empty(),"Actual mode toggle returns to the existing native single-region workflow");
    if(!real.empty()){doc=std::make_shared<Document>(open_tiff(real));selected=0;frame();glfwSwapBuffers(window);RawRegionRequest r;r.region={0,0,0,doc->metadata().width,doc->metadata().height};r.channels=doc->server()->raw_metadata()->channels;
        FluorescenceCellOptions p;p.threshold_method=FluorescenceThreshold::Otsu256;p.min_nucleus_area=5;p.cell_expansion=3;panel.set_request(r,p,{0,0},false);frame();glfwSwapBuffers(window);click(detail::GuiFluorescence::Action::Run);settled();
        check(panel.error().empty() && panel.result(),"Actual UI native detection on admitted real TIFF");frame();capture(root/"real-raw-preview.ppm",window);glfwSwapBuffers(window);
        if(has_geos()){click(detail::GuiFluorescence::Action::KeepCells);settled();check(panel.error().empty() && panel.published_cells()==panel.result()->raster().measurements.size(),"Real UI Keep preserves every detected label");}
        click(detail::GuiFluorescence::Action::KeepRecord);check(panel.error().empty(),"Real UI Keep record");const auto digest=panel.result()->sha256();click(detail::GuiFluorescence::Action::Replay);settled();check(panel.error().empty() && panel.result()->sha256()==digest,"Real UI exact replay");
        std::cout<<"Real fluorescence UI: "<<panel.result()->raster().measurements.size()<<" candidates; no annotated accuracy claim\n";
    }
}
int main(int argc,char** argv){std::cout<<std::unitbuf;std::cerr<<std::unitbuf;try{
    if(!has_project() || !has_tiff()){std::cout<<"GUI fluorescence: optional project/TIFF disabled\n";return 0;}
    const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("fluorescence-gui-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
    check(glfwInit(),"GLFW init");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1280,1000,"Native fluorescence workflow",nullptr,nullptr);check(window,"GLFW window");glfwMakeContextCurrent(window);
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigInputTrickleEventQueue=false;ImGui::GetIO().Fonts->AddFontDefault();ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
    finished_pixel_work(root,window);exercise(root,window,argc>2?argv[2]:"");raw_record_lifecycle(root,window);ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"Fluorescence desktop actions, raw preview registration, native facts, blocking, keep/undo, records/replay, stale/switch and empty checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
