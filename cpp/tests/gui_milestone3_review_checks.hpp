#pragma once
#include "gui_model_safety_checks.hpp"
namespace milestone3_review_checks {
using namespace qpx;
using A=detail::ModelSafetyTestAccess;
using model_safety_checks::click;
inline void pixel(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);
    std::vector<std::uint8_t> pixels(32*16*3);for(int y=0;y<16;++y)for(int x=0;x<32;++x){const auto i=(y*32+x)*3;pixels[i]=x<16?220:30;pixels[i+1]=x<16?30:220;pixels[i+2]=50;}
    Document doc(Image(32,16,std::move(pixels)));doc.set_class("Old A",0xff0000);doc.set_class("Old B",0x030303);
    const auto a=doc.add_rectangle({0,0,14,16}),b=doc.add_rectangle({18,0,14,16}),roi=doc.add_rectangle({0,0,32,16});doc.classify_annotation(a,"Old A");doc.classify_annotation(b,"Old B");ObjectSelection selection(doc);selection.replace({roi});
    GuiOptions o;o.pixel_open=true;o.pixel_object_id=roi;o.pixel_preview_budget=1024*1024;
    auto training=std::make_shared<PixelTrainingOptions>();training->calibrated=false;training->features.sigmas={0};training->max_samples_per_class=64;o.pixel_training_options=training;
    auto forest=std::make_shared<ForestParameters>();forest->trees=3;forest->max_depth=3;o.pixel_parameters=forest;
    detail::GuiPixelClassifier panel(o);detail::RecordRemovalDialog removal;detail::OutputGate gate;std::string error;
    auto draw=[&]{panel.update(doc,selection);panel.update_preview();panel.draw(doc,selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,32,16},&removal,1,&gate,{dir.u8string()});model_safety_checks::mask(panel.preview(),panel.historical_result()?"Kept mask pixels":"Live prediction pixels");removal.draw(doc,1,error);gate.draw([](const auto&){return true;});};
    host.fit("Pixel classifiers",1100,1500,draw);
    auto train=[&]{++A::parameters(panel).seed;const auto before=panel.model();click(host,panel,GuiInputTarget::TrainPixelClassifier,draw);host.until(draw,[&]{return panel.model()!=before && panel.trained();},"Review Pixel train failed");check(A::unsaved(panel),"Fresh model warning missing");};
    auto predict=[&]{click(host,panel,GuiInputTarget::PredictPixelClasses,draw);host.until(draw,[&]{return panel.predicted();},"Review Pixel predict failed");};
    auto keep=[&](GuiInputTarget target){click(host,panel,target,draw);host.until(draw,[&]{return !panel.archives().busy();},"Review Pixel Keep failed");check(panel.archives().state()=="kept" && !A::unsaved(panel),"Kept archive still warns that its model is session-only");return panel.archives().kept();};
    auto remove=[&](std::uint64_t id){A::select(panel,id);click(host,panel,GuiInputTarget::RemovePixelRecord,draw);check(removal.pending(),"Review Pixel remove did not confirm");record_list_checks::confirm_removal();host.settle(draw);check(error.empty(),"Review Pixel native remove failed");};
    panel.reveal_target(GuiInputTarget::KeepPixelTraining);host.settle(draw);
    check(host.action("Keep training cohort").reason==ui::i18n::text("Train a model first."),"Pixel training reason suggests loading a model");
    train();panel.reveal_target(GuiInputTarget::KeepPixelTraining);host.settle(draw);
    check(host.action("Keep training cohort").enabled && host.action("Keep training cohort").reason.empty(),"Training did not enable Pixel Keep training");
    predict();const auto old_record=keep(GuiInputTarget::KeepPixelPrediction);const auto old=panel.model();
    doc.classify_annotation(a,"New A");doc.classify_annotation(b,"New B");A::options(panel).features.sigmas={0,1};A::scales(panel);train();
    predict();const auto held_prediction=keep(GuiInputTarget::KeepPixelPrediction);remove(held_prediction);check(A::unsaved(panel),"Removing the prediction that holds the model did not restore warning");
    // Record edits invalidate the native capture. Train a fresh native capture
    // before exercising Keep training data; never bypass that validation.
    train();const auto held_training=keep(GuiInputTarget::KeepPixelTraining);remove(held_training);check(A::unsaved(panel),"Removing training that holds the model did not restore warning");
    panel.reveal_target(GuiInputTarget::SavePixelModel);host.settle(draw);
    check(host.action("Save pixel model").reason==ui::i18n::text("Choose a file path first."),"Pixel Save did not name the missing path");
    const auto file=dir/"review-model.qpc";core_path_checks::find(panel.paths(),"Export pixel model file").set(file.u8string());host.settle(draw);
    check(host.action("Save pixel model").enabled && host.action("Save pixel model").reason.empty(),"Entering a path did not enable Save");
    click(host,panel,GuiInputTarget::SavePixelModel,draw);host.until(draw,[&]{return !A::io(panel);},"Review Save did not finish");check(panel.io_state()=="saved" && !A::unsaved(panel),"Successful Save is not retained as a fact");detail::OutputGateTestAccess::clear_success(gate);
    // Keep an unrelated old archive available for the historical view.
    predict();panel.reveal_target(GuiInputTarget::PublishPixelContours);host.settle(draw);
    check(host.action("Publish class annotations").reason==ui::i18n::text("Trace contours"),"Pixel Add did not name its next step");
    click(host,panel,GuiInputTarget::TracePixelContours,draw);host.until(draw,[&]{return panel.contours_ready();},"Review contour trace failed");host.settle(draw);
    check(host.action("Publish class annotations").enabled && host.action("Publish class annotations").reason.empty(),"Tracing did not enable Pixel Add");
    const auto live=A::live(panel);const auto geometry=A::native_contours(panel);const auto sha=panel.model()->sha256();const auto count=doc.pixel_classifier_record_count();const auto objects=doc.objects().size();
    A::select(panel,old_record);click(host,panel,GuiInputTarget::PreviewPixelRecord,draw);host.settle(draw);
    check(panel.historical_result() && A::live(panel)==live && A::native_contours(panel)==geometry && !panel.traced(),"Historical view changed live objects or exposed live contours");
    const std::array<std::pair<GuiInputTarget,const char*>,3> actions{{{GuiInputTarget::TracePixelContours,"Trace pixel contours"},{GuiInputTarget::PublishPixelContours,"Publish class annotations"},{GuiInputTarget::KeepPixelPrediction,"Keep prediction mask"}}};
    for(const auto& [target,key]:actions){panel.reveal_target(target);host.settle(draw);const auto state=host.action(key);check(!state.enabled && !state.primary,"Live prediction action enabled/primary under kept mask");check(state.reason==ui::i18n::text("Close the kept mask first."),"Kept mask action does not explain how to return to live work");click(host,panel,target,draw);}
    check(A::live(panel)==live && A::native_contours(panel)==geometry && doc.pixel_classifier_record_count()==count && doc.objects().size()==objects,"Disabled replay target changed hidden live work");
    auto text=model_safety_checks::logged(host,draw);check(text.find("32 x 16 / level")==std::string::npos && text.find("512 classified pixels")==std::string::npos && text.find("Prediction: ")==std::string::npos && text.find("Contours: ")==std::string::npos,"Kept view prints live prediction/contour numbers");
    auto palette=A::palette_items(panel);A::palette_items(panel).resize(1);text=model_safety_checks::logged(host,draw);check(text.find("Old A")!=std::string::npos && text.find("Old B")==std::string::npos,"Legend exceeds historical palette bounds");A::palette_items(panel)=palette;host.settle(draw);
    const auto style=ImGui::GetStyle();
    for(int locale:{0,2}) {
        ui::i18n::choose(locale);ImGui::GetStyle()=style;if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        host.settle(draw);auto* w=ImGui::FindWindowByName(ui::i18n::label("Pixel classifiers").c_str());check(w,"Review Pixel window missing");ImGui::SetWindowPos(w,{24,24});ImGui::SetWindowSize(w,locale?ImVec2{1500,1500}:ImVec2{1100,1500});
        ImGui::SetScrollY(w,w->ScrollMax.y);host.settle(draw);host.capture(dir/(locale?"review-kept-fr200.ppm":"review-kept-normal.ppm"),draw);
        host.click("Close kept mask###Close kept pixel mask",draw);host.settle(draw);
        check(!panel.historical_result() && A::display_source(panel)==live && panel.model()->sha256()==sha && panel.traced()==geometry,"Close kept mask recomputed or replaced preserved live work");
        for(const auto& [target,key]:actions){panel.reveal_target(target);host.settle(draw);check(host.action(key).enabled,"Close kept mask failed to restore live action");}
        host.capture(dir/(locale?"review-live-fr200.ppm":"review-live-normal.ppm"),draw);
        A::select(panel,old_record);click(host,panel,GuiInputTarget::PreviewPixelRecord,draw);
    }
    ui::i18n::choose(0);ImGui::GetStyle()=style;host.settle(draw);auto* w=ImGui::FindWindowByName("Pixel classifiers");ImGui::SetScrollY(w,w->ScrollMax.y);host.settle(draw);host.click("Close kept mask###Close kept pixel mask",draw);
    click(host,panel,GuiInputTarget::PublishPixelContours,draw);check(panel.contours_published() && doc.objects().size()>objects,"Restored Add action did not reach native publication");
    predict();const auto new_record=keep(GuiInputTarget::KeepPixelPrediction);A::select(panel,new_record);click(host,panel,GuiInputTarget::UsePixelRecordModel,draw);remove(new_record);check(!A::unsaved(panel),"Removing a kept record forgot the successful Save");PixelPredictionRequest old_request(doc,roi,*old);const auto other_old=doc.keep_pixel_prediction(old_request,archive_pixel_prediction(predict_pixel_region(old_request)),"Second holder of old model");
    A::select(panel,old_record);host.settle(draw);click(host,panel,GuiInputTarget::UsePixelRecordModel,draw);check(!A::unsaved(panel),"Use lost kept model fact");remove(old_record);check(!A::unsaved(panel),"Removing one of two matching model records lost the remaining holder");remove(other_old);check(A::unsaved(panel),"Removing final unsaved model holder did not restore warning");
    core_path_checks::find(panel.paths(),"Export pixel model file").set((dir/"review-old-model.qpc").u8string());click(host,panel,GuiInputTarget::SavePixelModel,draw);host.until(draw,[&]{return !A::io(panel);},"Review old-model Save did not finish");check(!A::unsaved(panel),"Saving restored old model did not clear warning");
    std::cout<<"Review A/B/C: disabled replay targets, exclusive numbers/contours, exact Close restoration, dark swatch legend/palette bound, prediction/training removal and saved facts passed\n";
}
inline void tables(const std::filesystem::path& dir) {
    auto project=std::make_shared<Project>();std::vector<std::uint8_t> rgb(64*64*3,255);for(int y=0;y<64;++y)for(int x=0;x<64;++x)if((x-25)*(x-25)+(y-32)*(y-32)<=100)for(int c=0;c<3;++c)rgb[(y*64+x)*3+c]=0;
    auto doc=std::make_shared<Document>(Image(64,64,std::move(rgb)));const auto roi=doc->add_rectangle({0,0,64,64});CellParameters p;p.sigma=0;CellRequest request(*doc,roi,StainTransform({{1,0,0},{0,1,0}}),p);const auto detected=detect_cells(request);doc->keep_cells(request,detected);doc->keep_cells(request,detected);project->add_document(doc);project->save_new((dir/"source-project.qpxproj").u8string());
    const auto expected=cell_measurements_csv(CellTableRequest(*doc));
    for(bool french:{false,true}) {
        const auto name=std::string("review-table-source")+(french?"-fr200":"-normal");auto o=options(dir,name,"workflow","tables",french?2.f:1.f);o.width=french?2200:1440;o.height=french?2000:1200;o.autosave_path=(dir/(name+".qpxrecover")).u8string();o.cell_table_path=(dir/(name+".csv")).u8string();o.cell_table_require_current=false;o.inputs={key(2,GuiInputKind::SelectAllKey)};
        if(french){auto text=read(o.layout_path);text.replace(text.find("language=en"),11,"language=fr");write(o.layout_path,text);}
        simplification_checks::TraceScope scope;bool live_chosen=false,restored=false,before_disabled=false,after_disabled=false,exported=false;int frame_now=0;
        ui::combo_observer=[&](const char* label,int current,int){if(std::string(label)=="Cell source" && frame_now>=38){check(current==1,"All-kept scope rewrote shared Live cell objects preference");restored=true;}};
        struct Reset{~Reset(){ui::combo_observer={};}} reset;
        scope.trace.before_frame=[&](int frame){frame_now=frame;auto* pane=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);ImGuiWindow* body=nullptr;for(auto* win:ImGui::GetCurrentContext()->Windows)if(win->Active && win->ParentWindow==pane && std::strstr(win->Name,"Workflow task body"))body=win;
            auto open=[&](const char* label){check(body,"Review Tables body missing");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr(label,0,body->ID)));};
            auto choose=[&](int index,const char* label){auto* popup=ImGui::FindWindowByName("##Combo_00");check(popup && popup->Active,"Review Tables combo missing");const auto seed=ImHashData(&index,sizeof(index),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text(label),0,seed));};
            auto task=[&]{check(pane,"Review workflow pane missing");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("##Task",0,pane->ID)));};
            auto choose_task=[&](const char* label){auto* popup=ImGui::FindWindowByName("##Combo_00");check(popup && popup->Active,"Review task combo missing");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label(label).c_str(),0,popup->ID));};
            if(frame==5)open("Cell source");if(frame==6){choose(1,"Live cell objects");live_chosen=true;}
            if(frame==9)task();if(frame==10)choose_task("Watershed cells");
            auto disabled=[&]{const auto i=scope.trace.actions.find("Export cell table (Ctrl+Alt+X)");check(i!=scope.trace.actions.end() && !i->second.enabled,"Cells Export changed from disabled under Live cell objects");};
            if(frame==14){disabled();before_disabled=true;}if(frame==15)task();if(frame==16)choose_task("Tables");
            if(frame==20)open("Scope");if(frame==21)choose(2,"Current image (all kept runs)");
            if(frame==26){const auto i=scope.trace.actions.find("Export all kept image cells");check(i!=scope.trace.actions.end() && i->second.enabled && body,"Review all-kept export missing");ImGui::ActivateItemByID(ImHashStr(ui::workflow_label("Export all kept image cells","Export CSV").c_str(),0,body->ID));exported=true;}
            if(frame==30)dialog_checks::capture(dir/(name+"-tables.ppm"),glfwGetCurrentContext());
            if(frame==34)open("Scope");if(frame==35)choose(0,"Current image");
            if(frame==40)task();if(frame==41)choose_task("Watershed cells");if(frame==46){disabled();after_disabled=true;dialog_checks::capture(dir/(name+"-cells.ppm"),glfwGetCurrentContext());}
        };
        const auto report=run_project_gui(project,55,"",o);check(live_chosen && restored && before_disabled && after_disabled && exported && report.ui_overflow_items==0,"Review table source round trip incomplete/overflow");check(report.cell_table_state=="succeeded" && read(dir/(name+".csv"))==expected,"All-kept export bytes changed under Live source preference");
    }
    std::cout<<"Review E: actual Live source/all-kept/back, disabled Cells Export before/after, exact all-kept CSV, normal/French200 overflow0 passed\n";
}
inline void run(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();check(glfwInit()!=0,"Review GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);auto* window=glfwCreateWindow(2200,1600,"Milestone 3 review",nullptr,nullptr);check(window,"Review window");glfwMakeContextCurrent(window);pixel(dir,window);glfwDestroyWindow(window);glfwTerminate();tables(dir);
}
}
