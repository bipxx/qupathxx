#pragma once
#include "gui_batch_recipe_reference.hpp"
#include "../batch_checkpoint.hpp"
namespace batch_recipe_checks {
using Access=detail::GuiBatchTestAccess;
struct Values {
    TissueParameters tissue;std::shared_ptr<const TissueGridSampling> tissue_grid;
    CellParameters cells;std::optional<GridCellSampling> cell_grid;std::optional<NominalCellRoiSampling> nominal;GridCellParameters grid;
    std::vector<double> vectors{.65,.70,.29,.07,.99,.11},white{255,255,255};
    std::shared_ptr<const PixelClassifier> model;
    detail::WorkflowSnapshot snapshot()const{return {tissue,tissue_grid,{},{},cells,{},{},cell_grid,nominal,grid,AnalysisKind::StainMeans,RoiSamplingPolicy::PolygonWinding,false,model,true,vectors,white};}
    StainTransform transform(bool stains)const{return stains?StainTransform({{vectors[0],vectors[1],vectors[2]},{vectors[3],vectors[4],vectors[5]}},{white[0],white[1],white[2]}):StainTransform({{1,0,0},{0,1,0}},{white[0],white[1],white[2]});}
};
inline void comparison() {
    Values v;v.tissue.threshold=.72;v.cells.threshold=.83;v.tissue.canonical_pyramid=true;v.tissue.smoothing_sigma=1.25;v.tissue.median_cleanup=true;v.tissue.sampling_policy=RoiSamplingPolicy::PolygonWinding;
    int count=0;
    for(int mode=0;mode<3;++mode)for(int kind=0;kind<7;++kind)for(bool linked:{false,true}) {
        GuiOptions initial;detail::GuiBatch panel(initial);auto& options=Access::options(panel);options.kind=static_cast<ProjectBatchKind>(kind);Access::image_settings(panel,linked);
        options.tissue.threshold=.19;options.cells.threshold=.27;options.tissue.canonical_pyramid=true;
        options.tissue_grid=std::make_shared<TissueGridSampling>();options.tissue_min_component_area_um2=0;
        v.cell_grid=mode==1?std::optional<GridCellSampling>(GridCellSampling{}):std::nullopt;v.nominal=mode==2?std::optional<NominalCellRoiSampling>(NominalCellRoiSampling{}):std::nullopt;
        if(mode==1 && kind>=5){std::strcpy(Access::raw_x(panel).data(),"0,64,32,0");std::strcpy(Access::raw_y(panel).data(),"0,64,32,0");}
        const auto snapshot=v.snapshot();auto expected=previous_run(options,linked,snapshot,Access::raw_x(panel),Access::raw_y(panel));const auto actual=panel.effective(snapshot);
        if(kind==0 && linked){check(actual.roi_intensity_statistics==v.snapshot().intensity_statistics,"Allowed intensity difference lost");expected.roi_intensity_statistics=v.snapshot().intensity_statistics;}
        if(kind!=1){check(!actual.tissue.canonical_pyramid && !actual.tissue_grid && !actual.tissue_min_component_area_um2 && !actual.tissue_max_component_area_um2,"Non-tissue settings leaked");expected.tissue.canonical_pyramid=false;expected.tissue_grid.reset();expected.tissue_min_component_area_um2.reset();expected.tissue_max_component_area_um2.reset();}
        Compare<ProjectBatchOptions>::run(actual,expected,"kind="+std::to_string(kind)+" linked="+std::to_string(linked));++count;
    }
    std::cout<<"Frozen pre-task Run comparison: "<<count<<" cases, all option members; allowed differences: linked ROI roi_intensity_statistics, non-Tissue tissue.canonical_pyramid/tissue_grid/tissue_min_component_area_um2/tissue_max_component_area_um2\n";
}
inline void recipes(const std::filesystem::path& dir) {
    Values v;v.tissue.threshold=.72;v.cells.threshold=.83;v.cells.sigma=0;
    auto project=fixture(dir/"recipe-source.qpxproj",128,96);auto doc=project->document(1);for(int i=0;i<64;++i)doc->add_rectangle({8,8,44,60});ObjectSelection selection(*doc);
    simplification_tools_checks::NativeUi host;
    for(auto kind:{ProjectBatchKind::Tissue,ProjectBatchKind::Roi,ProjectBatchKind::Cells}) {
        v.tissue.canonical_pyramid=kind!=ProjectBatchKind::Tissue;v.tissue.level=v.tissue.canonical_pyramid?1:-1;
        const auto path=dir/("recipe-"+std::to_string(int(kind)));GuiOptions initial;initial.batch_output_directory=path.u8string();detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=kind;
        std::string rendered;auto draw=[&]{panel.poll();ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});rendered=host.context->LogBuffer.c_str();ImGui::LogFinish();};
        host.fit("Project batch",700,1300,draw);
        if(kind==ProjectBatchKind::Tissue){const auto at=rendered.find("Threshold: >=");check(at!=std::string::npos && std::stod(rendered.substr(at+13))==v.tissue.threshold,"Displayed threshold differs from Workflow");v.tissue.threshold=.74;host.frame(draw);const auto next=rendered.find("Threshold: >=");check(std::stod(rendered.substr(next+13))==v.tissue.threshold,"Summary did not follow next-frame Workflow edit");}
        const auto captured=panel.effective(v.snapshot());
        {ui::CommandFrame command;command.request(ui::CommandId::Action_Run_batch_Ctrl_Alt_B);ui::CommandFrameScope scope(command);host.frame(draw);}
        check(panel.busy() && panel.stats().state=="running","Batch did not start native work: "+panel.error());
        const double before=v.tissue.threshold;v.tissue.threshold=.95;host.frame(draw);check(panel.stats().state!="cancelled" && panel.error().empty(),"Workflow edit cancelled batch");
        host.until(draw,[&]{return !panel.busy();},"Batch did not complete");
        check(panel.result() && panel.result()->saved==1 && panel.result()->failed==0,"Stable pyramid leaked into non-Tissue batch: "+panel.error());
        const auto image=panel.result()->images.front();const auto checkpoint=std::filesystem::u8path(image.path);check(std::filesystem::equivalent(checkpoint.parent_path().parent_path(),std::filesystem::absolute(path)),"Checkpoint outside expected output");auto reopened=Project::open(image.path);check(bool(reopened->document(image.image_id)),"Cannot open checkpoint");
        const ProjectBatchRequest expected(*project,v.transform(detail::GuiBatch::separates_stains(captured)),{},captured);check(panel.result()->parameters_sha256==expected.parameters_sha256(),"Run report differs from captured effective recipe");
        const auto report=read(std::filesystem::u8path(panel.result()->report_path));check(report.find(expected.parameters_sha256())!=std::string::npos,"Run report lacks recipe identity");
        const auto metadata=detail::read_batch_checkpoint(image.path);check(metadata.parameters_sha256==panel.result()->parameters_sha256 && metadata.parameters_json==expected.parameters_json(),"Report checkpoint recipe differs from captured recipe");
        if(kind==ProjectBatchKind::Tissue){const auto at=metadata.parameters_json.find("\"threshold\":");check(at!=std::string::npos && std::stod(metadata.parameters_json.substr(at+12))==captured.tissue.threshold,"Run report checkpoint threshold differs from displayed value");}
        if(kind==ProjectBatchKind::Roi)check(captured.roi_intensity_statistics && metadata.parameters_json.find("\"roi_intensity_statistics\":true")!=std::string::npos,"ROI did not capture shared Intensity statistics");
        v.tissue.threshold=before;
    }
    std::cout<<"Summary/Workflow/request/report agree; linked ROI statistics; canonical Tissue then ROI/Cells complete/checkpoints open; later edits preserve running recipe\n";
}
inline std::shared_ptr<const PixelClassifier> model(std::uint64_t seed) {
    std::vector<std::uint8_t> rgb(32*16*3,50);for(int y=0;y<16;++y)for(int x=0;x<32;++x)rgb[(y*32+x)*3]=x<16?220:30;
    Document doc(Image(32,16,std::move(rgb)));doc.set_class("A",0xff0000);doc.set_class("B",0x00ff00);const auto a=doc.add_rectangle({0,0,14,16}),b=doc.add_rectangle({18,0,14,16});doc.apply_classifications({{a,"A"},{b,"B"}});
    PixelTrainingOptions options;options.calibrated=false;options.features.sigmas={0};options.max_samples_per_class=32;ForestParameters forest;forest.trees=3;forest.max_depth=3;forest.seed=seed;
    const auto table=sample_pixel_training(PixelTrainingRequest(doc,{a,b},StainTransform({{1,0,0},{0,1,0}}),options));return std::make_shared<PixelClassifier>(train_pixel_classifier(*table,forest));
}
inline void pins(const std::filesystem::path& dir) {
    Values v;v.model=model(2);const auto other=model(7);check(v.model->sha256()!=other->sha256(),"Pinned fixture models identical");
    auto project=fixture(dir/"pin-source.qpxproj",32,32);auto doc=project->document(1);doc->add_rectangle({0,0,32,32});ObjectSelection selection(*doc);GuiOptions initial;initial.batch_output_directory=(dir/"unpin-output").u8string();detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=ProjectBatchKind::Pixels;
    simplification_tools_checks::NativeUi host;std::string rendered;auto draw=[&]{panel.poll();ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});rendered=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.fit("Project batch",800,1300,draw);
    const auto activate=[&](const char* label){auto* w=ImGui::FindWindowByName("Project batch");ImGuiWindow* body=nullptr;for(auto* c:w->DC.ChildWindows)if(c->Active)body=c;check(body,"Missing batch recipe body");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label(label).c_str(),0,body->ID));host.settle(draw);};
    activate("Use active pixel model");check(Access::options(panel).pixel_model && Access::options(panel).pixel_model->sha256()==v.model->sha256(),"Pin did not capture active model");
    check(rendered.find("Pinned model: "+ui::short_record_id(v.model->sha256()))!=std::string::npos && rendered.find("A")!=std::string::npos && rendered.find("B")!=std::string::npos,"Pinned model lacks classes/short identity");
    activate("Details");check(rendered.find(v.model->sha256())!=std::string::npos,"Details lacks full pixel model SHA-256");
    activate("Unpin");check(!Access::options(panel).pixel_model && rendered.find("Run captures the active pixel model.")!=std::string::npos,"Unpin did not restore active capture sentence");
    v.model=other;host.click("Run batch (Ctrl+Alt+B)",draw);host.until(draw,[&]{return !panel.busy();},"Unpinned run did not finish");check(panel.result() && panel.result()->saved==1,"Unpinned pixel batch failed: "+panel.error());
    const ProjectBatchRequest expected(*project,v.transform(false),{},panel.effective(v.snapshot()));check(panel.result()->parameters_sha256==expected.parameters_sha256(),"Unpinned run did not capture new active model");activate("Use active pixel model");check(Access::options(panel).pixel_model->sha256()==other->sha256(),"Second Pin did not use active model");
    std::cout<<"Pin/Unpin/pin again and post-Unpin run capture exact active model passed\n";
}
inline void invalid_axes(const std::filesystem::path& dir) {
    Values v;auto project=fixture(dir/"axes-source.qpxproj",32,32);ObjectSelection selection(*project->document(1));GuiOptions initial;initial.batch_output_directory=(dir/"axes-output").u8string();detail::GuiBatch panel(initial);panel.open();Access::raw_mode(panel);std::strcpy(Access::raw_x(panel).data(),"0,32,32,0");
    simplification_tools_checks::NativeUi host;auto draw=[&]{panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});};host.fit("Project batch",800,1300,draw);check(panel.error().empty(),"Raw axes failed during drawing");host.click("Run batch (Ctrl+Alt+B)",draw);check(panel.error()=="enter both raw axes" && !panel.busy(),"Raw axes Run refusal changed");
    std::cout<<"Invalid raw axes draw without failure; Run retains exact refusal\n";
}
inline void sampling_recovery(const std::filesystem::path& dir) {
    Values v;v.white[0]=0;auto project=fixture(dir/"sampling-source.qpxproj",32,32);auto doc=project->document(1);doc->add_rectangle({0,0,32,32});ObjectSelection selection(*doc);GuiOptions initial;detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=ProjectBatchKind::Tissue;
    simplification_tools_checks::NativeUi host;std::string rendered;auto draw=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});rendered=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.fit("Project batch",800,1300,draw);check(rendered.find("Sampling unavailable")!=std::string::npos,"Invalid white fixture did not expose sampling validation");v.white[0]=255;host.frame(draw);check(rendered.find("Sampling unavailable")==std::string::npos && rendered.find("px per sample")!=std::string::npos,"Correcting shared white did not clear sampling error");
    std::cout<<"Sampling validation error clears on the next frame after correcting shared white\n";
}
inline void sampling_target_cache(const std::filesystem::path& dir) {
    Values v;auto project=fixture(dir/"target-cache-source.qpxproj",32,32);auto doc=project->document(1);doc->set_history_limits(0,0);
    for(int i=0;i<50000;++i)doc->add_detection(Roi::rectangle({0,0,1,1}));
    ObjectSelection selection(*doc);const ObjectSelection* current_selection=&selection;std::uint64_t image=1;
    GuiOptions initial;detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=ProjectBatchKind::Tissue;
    simplification_tools_checks::NativeUi host;std::string text;auto draw=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),image,current_selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});text=host.context->LogBuffer.c_str();ImGui::LogFinish();};
    for(int frame=0;frame<200;++frame)host.frame(draw);
    check(Access::sampling_walks(panel)==1 && Access::sampling_visits(panel)==50000 && Access::sampling_target(panel)==0,"Idle no-match summary repeated its object walk");
    check(Access::effective_calls(panel)==200,"Linked summary did not call effective exactly once per frame");
    const auto annotation=doc->add_rectangle({0,0,16,16});host.frame(draw);
    check(Access::sampling_walks(panel)==2 && Access::sampling_target(panel)==annotation && text.find("Sampling example: active image, area #"+std::to_string(annotation))!=std::string::npos,"Document edit did not reveal the sampling example next frame");
    const auto toggle_selected=[&]{auto* window=ImGui::FindWindowByName("Project batch");check(window,"Cache test batch window missing");ImGuiWindow* body=nullptr;for(auto* child:window->DC.ChildWindows)if(child->Active)body=child;check(body,"Cache test recipe body missing");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Selected objects only",0,body->ID)));host.frame(draw);};
    toggle_selected();check(Access::selected_only(panel) && Access::sampling_walks(panel)==3 && Access::sampling_target(panel)==0,"Selected-only checkbox did not invalidate the target");
    selection.replace({annotation});host.frame(draw);check(Access::sampling_walks(panel)==4 && Access::sampling_target(panel)==annotation && text.find("Sampling example: active image, area #"+std::to_string(annotation))!=std::string::npos,"Late selected annotation did not become the example next frame");
    const auto walks=Access::sampling_walks(panel),visits=Access::sampling_visits(panel);for(int frame=0;frame<200;++frame)host.frame(draw);
    check(Access::sampling_walks(panel)==walks && Access::sampling_visits(panel)==visits,"Idle late selection repeated its object walk");
    selection.clear();host.frame(draw);check(Access::sampling_walks(panel)==walks+1 && Access::sampling_target(panel)==0,"Selection revision did not invalidate a matching target");
    toggle_selected();Access::options(panel).object_kind=ObjectKind::Detection;const auto previous=Access::sampling_walks(panel);host.frame(draw);check(Access::sampling_walks(panel)==previous+1 && Access::sampling_target(panel)==1,"Object kind did not invalidate the example target");
    ObjectSelection replacement(*doc);current_selection=&replacement;const auto replaced=Access::sampling_walks(panel);host.frame(draw);check(Access::sampling_walks(panel)==replaced+1,"Selection identity did not invalidate the example target");
    auto second=std::make_shared<Document>(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));second->add_detection(Roi::rectangle({1,1,4,4}));ObjectSelection second_selection(*second);image=project->add_document(second);current_selection=&second_selection;const auto changed=Access::sampling_walks(panel);host.frame(draw);check(Access::sampling_walks(panel)==changed+1 && Access::sampling_target(panel)==1,"Document identity did not invalidate the example target");
    std::cout<<"B1: 50000 nonmatching objects, 200 frames, 1 walk/50000 visits/200 effective calls; next-frame annotation, selected-only checkbox, selection revision/identity, object kind and document identity passed; late selection idle 200 frames/0 walks\n";
}
inline void pixel_output_sentence() {
    Values v;const auto active=model(2);auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));project->add_document(doc);ObjectSelection selection(*doc);
    const auto occurrences=[](const std::string& text,const std::string& word){std::size_t count=0,at=0;while((at=text.find(word,at))!=std::string::npos){++count;at+=word.size();}return count;};
    simplification_tools_checks::NativeUi host;
    for(int locale:{0,2})for(int state=0;state<3;++state) {
        ui::i18n::choose(locale);v.model=state==2?nullptr:active;GuiOptions initial;detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=ProjectBatchKind::Pixels;if(state==0)Access::options(panel).pixel_model=*active;
        std::string text;auto draw=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.fit("Project batch",900,1300,draw);
        const std::string sentence=ui::i18n::text("Each target produces a verified QPS file; only complete image bundles are reused. The open project is unchanged.");
        check(locale==0 || sentence!="Each target produces a verified QPS file; only complete image bundles are reused. The open project is unchanged.","Pixels output sentence has no French translation");
        check(occurrences(text,sentence)==1,"Pixels output sentence is missing or duplicated");
        check(occurrences(text,"Run captures the active pixel model.")==(state==1?1:0),"Pixels active-model sentence is missing or duplicated");
        check(text.find("Uses the pinned pixel model.")==std::string::npos,"Pixels output sentence retained the obsolete pin clause");
        if(state==2)check(occurrences(text,"Train or load a pixel model in Pixel classifiers first.")==1,"Pixels no-model guidance is duplicated");
        std::cout<<"B2: locale="<<(locale==0?"en":"fr")<<" state="<<state<<" output sentence=1 active-capture sentence="<<(state==1?1:0)<<" passed\n";
    }
}
inline void captures(const std::filesystem::path& dir) {
    check(glfwInit()!=0,"Batch captures GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);auto* window=glfwCreateWindow(2200,1600,"Batch recipes",nullptr,nullptr);check(window,"Batch captures window");glfwMakeContextCurrent(window);
    {
    model_safety_checks::GlHost host(window);const auto base=ImGui::GetStyle();Values v;v.model=model(2);auto project=fixture(dir/"capture-source.qpxproj",32,32);auto doc=project->document(1);doc->add_rectangle({0,0,32,32});ObjectSelection selection(*doc);
    for(int locale:{0,2})for(int k=0;k<4;++k)for(bool linked:{false,true}) {
        ui::i18n::choose(locale);ImGui::GetStyle()=base;if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        GuiOptions initial;initial.batch_output_directory=(dir/"capture-output").u8string();detail::GuiBatch panel(initial);panel.open();Access::options(panel).kind=static_cast<ProjectBatchKind>(k);Access::image_settings(panel,linked);if(k==3)Access::options(panel).pixel_model=*v.model;
        std::string text;auto draw=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;panel.draw(project.get(),1,&selection,false,v.snapshot(),[&](bool stains){return v.transform(stains);});text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.settle(draw);auto* w=ImGui::FindWindowByName(ui::tool_window_label(ui::ToolId::Batch));check(w,"Capture batch window missing");ImGui::SetWindowPos(w,{24,24});ImGui::SetWindowSize(w,locale==2?ImVec2{920,1440}:ImVec2{460,720});host.settle(draw);
        if(k<3){const std::string expected=ui::i18n::text("Each run keeps its starting settings. Results go to independent checkpoints; the open project is unchanged.");check(text.find(expected.substr(0,expected.find('.')+1))!=std::string::npos,"Batch starting-settings sentence bypasses translation");}
        const auto stem="batch-"+std::to_string(k)+(linked?"-linked":"-own")+(locale==2?"-fr200":"-normal");host.capture(dir/(stem+".ppm"),draw);for(auto* c:w->DC.ChildWindows)if(c->Active)ImGui::SetScrollY(c,c->ScrollMax.y);host.capture(dir/(stem+"-bottom.ppm"),draw);
    }}glfwDestroyWindow(window);glfwTerminate();std::cout<<"32 batch captures: ROI/Tissue/Cells/Pixels own/linked normal/French 200%, overflow=0\n";
}
inline void run(const std::filesystem::path& input) {
    const auto dir=std::filesystem::absolute(input).lexically_normal();std::filesystem::create_directories(dir);comparison();recipes(dir);pins(dir);invalid_axes(dir);sampling_recovery(dir);sampling_target_cache(dir);pixel_output_sentence();captures(dir);
}
}
