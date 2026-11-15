#pragma once
#include "../gui_record_list.hpp"
#include "../gui_cell_record_rows.hpp"
#include "object_forest_fixture.hpp"
#include "gui_record_fix1_checks.hpp"
#include "qupathxx/measurement_tables.hpp"
namespace qpx::detail {
struct RecordListTestAccess {
    static std::uint64_t selected(const GuiRawPixel& panel){return panel.record_;}
    static bool busy(const GuiRawPixel& panel){return panel.jobs_.busy();}
};
}
namespace record_list_checks {
using namespace qpx;
inline void confirm_removal() {
    auto* modal=ImGui::FindWindowByName(ui::dialog_definition(ui::DialogKind::RemoveKeptResult).name);
    check(modal,"Registered record confirmation missing");
    ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("Remove kept result").c_str(),0,modal->ID));
}
inline void lists(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;
    std::vector<ui::RecordListItem> trace;ui::record_list_trace=&trace;
    struct Reset {~Reset(){ui::record_list_trace=nullptr;}} reset;
    ui::RecordRows cache;std::size_t builds=0;
    for(int frame=0;frame<100;++frame)cache.refresh(&host,1,0,[&](auto& rows){++builds;for(std::uint64_t i=1;i<=10000;++i)rows.push_back(ui::record_row(i,"Literal %s%n","Cells","current"));});
    check(builds==1 && cache.rows.size()==10000,"Rows rebuilt without native invalidation");
    std::uint64_t selected=41;
    auto draw=[&]{trace.clear();ImGui::SetNextWindowSize({600,500});ImGui::Begin("Clipped records");selected=ui::draw_record_list("Records",cache,selected,{});ImGui::End();};
    host.settle(draw);check(!trace.empty() && trace.size()<20,"Record list is not clipped");
    const auto chosen=trace.front();host.click(chosen.bounds,draw);check(selected==chosen.row.id,"Row selection requires a typed identifier");
    check(ui::short_record_id(std::string(64,'a'))==std::string(8,'a') && ui::short_record_id("1234")=="1234","Short native identifier format changed");
    std::cout<<"Record list cache/10000 rows/clipping/literal labels/click selection passed\n";
}
inline void object(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;Document doc(Image(128,64,std::vector<std::uint8_t>(128*64*3,80)));
    std::vector<std::uint64_t> ids;for(int i=0;i<6;++i)ids.push_back(doc.add_detection(Roi::rectangle({double(i*15),1,4,4}),"Object",0,i<3?"small":"large"));
    ObjectFeatureRequest request(doc,ids);auto model=ForestModel::from_bytes(qpx_test::object_forest_fixture());
    ObjectClassifierJob job;check(job.predict(request,model),"Fixture classifier predict");check(job.wait(doc).state=="ready","Fixture classifier did not finish");job.apply(doc);
    const auto record=doc.classifier_records().back();GuiOptions options;options.classifier_open=true;options.classifier_record_id=record->id;
    detail::GuiObjectClassifier panel(options);detail::RecordRemovalDialog removal;ObjectSelection selection(doc);std::string error;std::set<GuiInputTarget> marks;
    auto draw=[&]{panel.update(doc,selection);panel.draw(doc,selection,false,true,[&](GuiInputTarget t){marks.insert(t);},nullptr,{},&removal,1);removal.draw(doc,1,error);};
    host.fit("Object classifiers",900,1300,draw);
    host.click("Use saved classifier model",draw);check(panel.record_loaded() && panel.record_id()==record->id && panel.model()->sha256()==record->archive->model()->sha256(),"Scripted classifier record/Use native identity changed");
    if(!host.trace.actions.count("Remove kept result###Remove classifier record")) {
        auto* window=ImGui::FindWindowByName("Object classifiers");ImGui::ActivateItemByID(ImHashStr(ui::i18n::label("Project classifier records").c_str(),0,window->ID));host.settle(draw);
    }
    host.click("Remove kept result###Remove classifier record",draw);host.settle(draw);
    check(removal.pending() && doc.classifier_record_count()==1,"Remove did not open registered confirmation");confirm_removal();host.settle(draw);
    check(doc.classifier_record_count()==0 && error.empty(),"Confirmed object classifier Remove did not reach native authority");
    check(marks.count(GuiInputTarget::UseClassifierRecord),"Use classifier replay target disappeared");
    std::cout<<"Object classifier scripted selection/Use exact model/replay mark/confirmed Remove passed\n";
}
inline void raw_pixel(const std::filesystem::path& dir) {
    qpx_test::raw_pixel_tiff_fixture(dir/"raw.tiff",12,8);Document doc(open_tiff((dir/"raw.tiff").u8string()));
    ScientificRawLattice lattice(12,8,{0,12,12,0},{0,8,8,0});ScientificRawOptions channels;channels.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};
    RawPixelRecipe recipe(*doc.server()->raw_metadata(),lattice,channels,features);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema())schema.emplace_back(f.name,f.unit);
    RawPixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),recipe);RawPixelTiledPredictionRequest request(doc,model,lattice);
    const auto file=(dir/"raw.qws").u8string();save_raw_pixel_stream(request,file);const auto link=prepare_raw_pixel_stream_link(file);
    const auto first=doc.keep_raw_pixel_stream(link,"First raw");const auto file2=(dir/"second-raw.qws").u8string();save_raw_pixel_stream(request,file2);const auto second=doc.keep_raw_pixel_stream(prepare_raw_pixel_stream_link(file2),"Second raw");
    // A scripted ID which never matched must not open/remove either real file.
    {
        GuiOptions scripted;auto invalid=std::make_shared<GuiRawPixelOptions>();invalid->open=true;invalid->record_id=UINT64_MAX;invalid->show_overlay=false;scripted.raw_pixel=invalid;
        detail::GuiRawPixel missing(scripted);detail::RecordRemovalDialog confirm;ObjectSelection empty(doc);simplification_tools_checks::NativeUi ui;
        auto draw_missing=[&]{missing.update(doc);missing.draw(doc,empty,nullptr,false,true,[](GuiInputTarget){},&confirm,1);};
        ui.fit("Raw pixel classifier",1000,1300,draw_missing);ui.click(missing.bounds(GuiInputTarget::RawPixelRecordsTab),draw_missing);
        for(const auto action:{GuiInputTarget::RawPixelOpenRecord,GuiInputTarget::RawPixelTraceFile,GuiInputTarget::RawPixelReplayFile,GuiInputTarget::RawPixelRemoveRecord}) {
            ui.click(missing.bounds(action),draw_missing);ui.settle(draw_missing);
            check(detail::RecordListTestAccess::selected(missing)==UINT64_MAX && !missing.report().error.empty() && !missing.report().link && !confirm.pending() && doc.raw_pixel_stream_record_count()==2,"Never-matched scripted Raw ID substituted another file or crossed its error boundary");
        }
    }
    GuiOptions options;auto raw=std::make_shared<GuiRawPixelOptions>();raw->open=true;raw->record_id=first;raw->show_overlay=false;options.raw_pixel=raw;
    detail::RecordRemovalDialog removal;std::string error;detail::GuiRawPixel panel(options);ObjectSelection selection(doc);simplification_tools_checks::NativeUi host;std::vector<ui::RecordListItem> trace;ui::record_list_trace=&trace;struct Reset{~Reset(){ui::record_list_trace=nullptr;}}reset;
    auto draw=[&]{trace.clear();panel.update(doc);panel.draw(doc,selection,nullptr,false,true,[](GuiInputTarget){},&removal,1);removal.draw(doc,1,error);};host.fit("Raw pixel classifier",1000,1300,draw);host.click(panel.bounds(GuiInputTarget::RawPixelRecordsTab),draw);
    no_invented_states(trace,"Raw pixel");check(detail::RecordListTestAccess::selected(panel)==first,"First update lost scripted Raw pixel record identifier");
    const auto row=std::find_if(trace.begin(),trace.end(),[&](const auto& row){return row.row.id==second;});check(row!=trace.end(),"Raw pixel record list missing native record");const auto bounds=row->bounds;host.click(bounds,draw);
    check(detail::RecordListTestAccess::selected(panel)==second,"Raw pixel row click did not select native ID");host.click(panel.bounds(GuiInputTarget::RawPixelOpenRecord),draw);host.until(draw,[&]{return !detail::RecordListTestAccess::busy(panel);},"Raw pixel Open did not finish");
    check(panel.report().link && std::filesystem::equivalent(std::filesystem::u8path(panel.report().link->path()),std::filesystem::u8path(doc.raw_pixel_stream_record(second)->link->path())) && panel.report().link->same_content(*doc.raw_pixel_stream_record(second)->link),"Raw pixel Open used a different native file link");
    host.click(panel.bounds(GuiInputTarget::RawPixelRemoveRecord),draw);check(removal.pending(),"Selected Raw file removal did not confirm");confirm_removal();host.settle(draw);
    check(doc.raw_pixel_stream_record_count()==1 && detail::RecordListTestAccess::selected(panel)==first,"Removed Raw selection did not fall back through native lookup");
    host.click(panel.bounds(GuiInputTarget::RawPixelOpenRecord),draw);host.until(draw,[&]{return !detail::RecordListTestAccess::busy(panel);},"Fallback Raw Open did not finish");
    check(panel.report().error.empty() && panel.report().link && panel.report().link->same_content(*doc.raw_pixel_stream_record(first)->link) && std::filesystem::equivalent(std::filesystem::u8path(panel.report().link->path()),std::filesystem::u8path(doc.raw_pixel_stream_record(first)->link->path())),"Fallback Raw Open failed or used wrong native link");
    host.click(panel.bounds(GuiInputTarget::RawPixelRemoveRecord),draw);confirm_removal();host.settle(draw);check(!detail::RecordListTestAccess::selected(panel) && !doc.raw_pixel_stream_record_count() && !panel.enabled(GuiInputTarget::RawPixelOpenRecord),"Empty Raw catalogue kept an actionable missing selection");
    std::cout<<"Raw pixel native removal/fallback/Open/empty selection passed\n";
    std::cout<<"Raw pixel scripted identifier/row click/Open exact native file link passed\n";
}
inline void spatial(const std::filesystem::path& dir) {
    simplification_tools_checks::NativeUi host;Document doc(Image(64,64,std::vector<std::uint8_t>(64*64*3,80)));
    const auto a=doc.add_rectangle({1,1,2,2}),b=doc.add_rectangle({10,10,2,2}),c=doc.add_rectangle({30,30,2,2});
    SpatialDistanceOptions distance;distance.units=SpatialDistanceUnits::Pixels;
    SpatialDistanceRequest first(doc,{a},{b},distance);const auto one=doc.keep_spatial(first,measure_spatial_distances(first));
    SpatialDistanceRequest second(doc,{a},{c},distance);const auto two=doc.keep_spatial(second,measure_spatial_distances(second));
    GuiOptions options;auto spatial=std::make_shared<GuiSpatialOptions>();spatial->open=true;spatial->record_id=one;spatial->analysis=distance;spatial->csv_path=(dir/"selected-spatial.csv").u8string();options.spatial=spatial;
    detail::GuiSpatial panel(options);ObjectSelection selection(doc);detail::RecordRemovalDialog removal;detail::OutputGate gate;
    std::vector<ui::RecordListItem> trace;ui::record_list_trace=&trace;struct Reset{~Reset(){ui::record_list_trace=nullptr;}}reset;
    std::set<GuiInputTarget> marks;auto mark=[&](GuiInputTarget t){marks.insert(t);};
    auto draw=[&]{trace.clear();panel.draw(doc,selection,nullptr,nullptr,false,true,mark,&removal,1,&gate,{dir.u8string()});};
    host.fit("Spatial analysis",900,1300,draw);check(panel.report().result && panel.report().result->sha256()==doc.spatial_record(one)->result->sha256(),"Scripted Spatial identifier did not preselect");
    for(const auto& row:trace){const auto raw=doc.spatial_status(row.row.id);check(row.row.state && row.second_line==ui::caption(ui::parse_record_state(raw),raw),"Spatial row does not render native live status");}const auto found=std::find_if(trace.begin(),trace.end(),[&](const auto& row){return row.row.id==two;});check(found!=trace.end(),"Second Spatial row absent");const auto bounds=found->bounds;host.click(bounds,draw);host.settle(draw);
    check(panel.report().result && panel.report().result->sha256()==doc.spatial_record(two)->result->sha256(),"Spatial table does not follow highlighted row");
    host.click(panel.bounds(GuiInputTarget::SpatialInspect),draw);host.click(panel.bounds(GuiInputTarget::SpatialExport),draw);host.until(draw,[&]{return !panel.report().busy;},"Spatial export did not finish");
    check(read(dir/"selected-spatial.csv")==spatial_distance_csv(*doc.spatial_record(two)->result),"Spatial export is not exact highlighted result");
    host.click(panel.bounds(GuiInputTarget::SpatialVerify),draw);host.until(draw,[&]{return !panel.report().busy;},"Spatial Verify did not finish");check(panel.report().state=="verified","Selected Spatial Verify failed");
    check(marks.count(GuiInputTarget::SpatialRecordId) && marks.count(GuiInputTarget::SpatialInspect) && marks.count(GuiInputTarget::SpatialVerify) && marks.count(GuiInputTarget::SpatialRemove),"Spatial replay targets disappeared");
    std::cout<<"Spatial scripted selection/click highlight/exact native Inspect Verify CSV/replay marks passed\n";
}
inline void batch(const std::filesystem::path& dir) {
    auto project=simplification_tools_checks::density_fixture(dir/"batch-source.qpxproj");auto doc=project->document(project->active_image());ObjectSelection selection(*doc);GuiOptions options;options.batch_options=std::make_shared<ProjectBatchOptions>();options.batch_output_directory=(dir/"batch-output").u8string();
    detail::GuiBatch panel(options);panel.open();simplification_tools_checks::NativeUi host;
    auto draw=[&]{panel.poll();panel.draw(project.get(),project->active_image(),&selection,false,{TissueParameters{},nullptr,{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind::OpticalDensityMeans,RoiSamplingPolicy{},false,nullptr,false,std::vector<double>(6),std::vector<double>(3,255)},[](bool){return StainTransform({{1,0,0},{0,1,0}});});};
    host.fit("Project batch",900,1300,draw);host.click("Run batch (Ctrl+Alt+B)",draw);host.until(draw,[&]{return !panel.busy();},"Batch did not finish");check(panel.result() && panel.result()->saved==2,"Small batch failed");
    const auto image=panel.result()->images.front();std::cout<<"Batch native checkpoint path: "<<image.path<<'\n';auto opened=Project::open(image.path);auto loaded=opened->document(image.image_id);MeasurementTableRequest table(*loaded,MeasurementTableKind::Roi);save_measurements_csv(table,(dir/"checkpoint-table.csv").u8string());check(read(dir/"checkpoint-table.csv")==measurements_csv(table),"Desktop checkpoint table export differs");
    std::string folder;ui::shell_open_override=[&](const std::string& path){folder=path;};
    auto* window=ImGui::FindWindowByName("Project batch");check(window,"Batch window missing");
    // The action lives in the scrolling batch body. Find its child scope.
    ImGuiWindow* body=nullptr;for(auto* child:window->DC.ChildWindows)if(child->Active)body=child;check(body,"Batch output body missing");
    ImGui::SetScrollY(body,body->ScrollMax.y);host.settle(draw);host.click("Open folder###Batch output folder",draw);host.settle(draw);ui::shell_open_override={};
    check(std::filesystem::equivalent(std::filesystem::u8path(folder),std::filesystem::u8path(panel.result()->report_path).parent_path()),"Batch Open folder did not use finished output folder");
    const auto missing=std::filesystem::u8path(panel.result()->images.back().path);check(missing.is_absolute() && std::filesystem::equivalent(missing.parent_path().parent_path(),std::filesystem::absolute(dir)/"batch-output"),"Batch check attempted to remove outside its output");check(std::filesystem::remove(missing),"Cannot retire one fixture checkpoint for mixed reuse");
    ImGui::SetScrollY(body,0);host.settle(draw);host.click("Run batch (Ctrl+Alt+B)",draw);host.until(draw,[&]{return !panel.busy();},"Mixed reuse batch did not finish");check(panel.result()->saved==1 && panel.result()->reused==1,"Mixed batch did not preserve one fresh and one reused image");
    std::string rendered;auto logged=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;draw();rendered=host.context->LogBuffer.c_str();ImGui::LogFinish();};ImGui::SetScrollY(body,body->ScrollMax.y);host.settle(logged);
    for(const auto& row:panel.result()->images){const auto word=std::string(ui::caption(ui::parse_run_state(row.state),row.state));check((row.state=="reused"?word=="Reused":word=="Saved") && rendered.find(row.image_name+": "+word)!=std::string::npos,"Batch image line hides reused vs freshly saved");}
    std::cout<<"Batch checkpoint desktop-open/exact table export/finished output folder/shell override passed\n";
}
inline void all_tables_window(const std::shared_ptr<Project>& project,const std::shared_ptr<Document>& doc,const std::filesystem::path& dir,bool french) {
    const std::string name=french?"tables-all-fr200":"tables-all-normal";
    auto o=options(dir,name,"workflow","tables",french?2.f:1.f);o.width=french?2200:1440;o.height=french?2000:1200;
    o.autosave_path=(dir/(name+".qpxrecover")).u8string();o.cell_table_path=(dir/(name+".csv")).u8string();
    if(french){auto settings=read(o.layout_path);settings.replace(settings.find("language=en"),11,"language=fr");write(o.layout_path,settings);}
    simplification_checks::TraceScope scope;std::vector<ui::RecordListItem> rows;ui::record_list_trace=&rows;
    struct Reset{~Reset(){ui::record_list_trace=nullptr;}} reset;
    bool opened=false,chosen=false,saw_rows=false,pressed=false,clicked=false,kind_open=false,kind_changed=false,reset_seen=false;
    ui::combo_observer=[&](const char* label,int current,int count){if(std::string(label)=="Scope"){check(current>=0 && current<count,"Scope combo received out-of-range selection");if(kind_changed){check(current==0 && count==2,"Tables reset Scope after drawing invalid index");reset_seen=true;}}};
    struct ComboReset{~ComboReset(){ui::combo_observer={};}} combo_reset;
    scope.trace.before_frame=[&](int frame){
        auto* pane=ImGui::FindWindowByName(GuiWorkspace::workflow_pane);ImGuiWindow* body=nullptr;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active && window->ParentWindow==pane && std::strstr(window->Name,"Workflow task body"))body=window;
        if(frame==3){check(body,"Tables body missing");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Scope",0,body->ID)));opened=true;}
        if(opened && !chosen && frame>=4){auto* popup=ImGui::FindWindowByName("##Combo_00");if(popup && popup->Active){const int item=2;const auto item_scope=ImHashData(&item,sizeof(item),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("Current image (all kept runs)"),0,item_scope));chosen=true;}}
        if(chosen && rows.size()==2){saw_rows=true;for(const auto& row:rows)check(row.row.label=="Named annotation #"+std::to_string(row.row.id) && row.row.state==ui::RecordState::Current,"Actual Tables row lost object name/state");}
        if(frame==7 && !rows.empty()){ImGui::ActivateItemByID(rows.front().widget);clicked=true;}
        if(frame>=8 && chosen)for(const auto& row:rows)check(!row.highlighted,"Informational all-kept list retained an export-looking selection");
        if(frame==12){const auto found=scope.trace.actions.find("Export all kept image cells");check(body && found!=scope.trace.actions.end() && found->second.enabled,"All-kept image export control missing");ImGui::ActivateItemByID(ImHashStr(ui::workflow_label("Export all kept image cells","Export CSV").c_str(),0,body->ID));pressed=true;}
        if(frame==16)dialog_checks::capture(dir/(name+".ppm"),glfwGetCurrentContext());
        if(frame==17){check(body,"Tables body missing after Export");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Table",0,body->ID)));kind_open=true;}
        if(kind_open && !kind_changed && frame>=18){auto* popup=ImGui::FindWindowByName("##Combo_00");if(popup && popup->Active){const int item=1;const auto item_scope=ImHashData(&item,sizeof(item),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("ROI channel means"),0,item_scope));kind_changed=true;}}
        rows.clear();
    };
    const auto report=run_project_gui(project,30,(dir/(name+"-scope-reset.ppm")).u8string(),o);
    check(opened && chosen && saw_rows && pressed && clicked && kind_changed && reset_seen && report.ui_overflow_items==0,"Actual all-kept Tables selection/capture failed");
    check(report.cell_table_state=="succeeded" && read(dir/(name+".csv"))==cell_measurements_csv(CellTableRequest(*doc)),"Actual all-kept Tables GUI export differs from native current-image request: "+report.cell_table_state+" / "+report.cell_table_error);
    std::cout<<name<<": actual Scope selection/all kept names/states/exact CSV/zero overflow passed\n";
}
inline void tables(const std::filesystem::path& dir) {
    std::vector<std::uint8_t> rgb(64*64*3,255);for(int y=0;y<64;++y)for(int x=0;x<64;++x)if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100)for(int c=0;c<3;++c)rgb[(y*64+x)*3+c]=0;
    auto doc=std::make_shared<Document>(Image(64,64,std::move(rgb)));const auto id=doc->add_rectangle({0,0,64,64});doc->rename_annotation(id,"Named annotation");CellParameters p;p.sigma=0;
    CellRequest request(*doc,id,StainTransform({{1,0,0},{0,1,0}}),p);const auto result=detect_cells(request);check(result->features().size()==2,"Cells fixture facts changed");
    const auto first=doc->keep_cells(request,result),second=doc->keep_cells(request,result);
    auto project=std::make_shared<Project>();project->add_document(doc);project->save_new((dir/"tables-project.qpxproj").u8string());
    const auto before=cell_measurements_csv(CellTableRequest(*doc,{second}));write(dir/"selected-annotation-before.csv",before);
    ui::RecordRows rows;ui::cell_record_rows(*doc,rows);check(rows.rows.size()==2 && rows.rows[0].id==first && rows.rows[1].id==second,"Tables omitted kept cell runs");
    for(const auto& row:rows.rows)check(row.label=="Named annotation #"+std::to_string(row.id) && row.state==ui::RecordState::Current,"Tables object name/state missing");
    const auto all=cell_measurements_csv(CellTableRequest(*doc));check(all!=before && CellTableRequest(*doc).record_count()==2,"All-kept scope silently exports selected annotation only");
    auto o=options(dir,"selected-annotation","workflow","tables");o.autosave_path=(dir/"tables.qpxrecover").u8string();o.cell_table_path=(dir/"selected-annotation-after.csv").u8string();o.inputs={key(3,GuiInputKind::SelectAllKey),key(7,GuiInputKind::ExportCellTableKey)};
    const auto report=run_project_gui(project,25,(dir/"selected-annotation-diagnostic.ppm").u8string(),o);
    std::cout<<"Selected annotation export: state="<<report.cell_table_state<<" error="<<report.cell_table_error<<" path="<<report.cell_table_path<<" selected="<<report.selected_ids.size()<<" replay="<<report.replay_complete<<'\n';
    check(report.cell_table_state=="succeeded" && read(dir/"selected-annotation-after.csv")==before,"Selected-annotation GUI export changed bytes");
    std::cout<<"Tables every kept run/object names/states and selected-annotation GUI CSV byte comparison passed\n";
    all_tables_window(project,doc,dir,false);all_tables_window(project,doc,dir,true);
    Document copied=*doc;doc->rename_annotation(id,"First shared-source document");copied.rename_annotation(id,"Second shared-source document");check(copied.server()==doc->server() && copied.revision()==doc->revision(),"Cache isolation fixture is not sharing source/revision");
    ui::cell_record_rows(*doc,rows);ui::cell_record_rows(copied,rows);check(rows.rows.front().label.find("Second shared-source document")==0,"Tables reused another document's same-source/same-revision rows");
}
inline void pixel(const std::filesystem::path& dir) {
    Document doc(Image(32,24,std::vector<std::uint8_t>(32*24*3,80)));const auto roi=doc.add_rectangle({0,0,32,24});
    PixelFeatureOptions features;features.sigmas={0};StainTransform transform({{1,0,0},{0,1,0}});std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:pixel_feature_schema(features,transform))schema.emplace_back(f.name,f.unit);
    PixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),features,transform,PixelResolution{});
    PixelPredictionRequest a(doc,roi,model);const auto first=doc.keep_pixel_prediction(a,archive_pixel_prediction(predict_pixel_region(a)),"First mask");
    PixelPredictionRequest b(doc,roi,model);const auto second=doc.keep_pixel_prediction(b,archive_pixel_prediction(predict_pixel_region(b)),"Second mask");
    GuiOptions options;options.pixel_open=true;options.pixel_show_overlay=false;detail::GuiPixelClassifier panel(options);ObjectSelection selection(doc);detail::RecordRemovalDialog removal;std::string error;
    simplification_tools_checks::NativeUi host;std::vector<ui::RecordListItem> trace;ui::record_list_trace=&trace;struct Reset{~Reset(){ui::record_list_trace=nullptr;}}reset;
    std::set<GuiInputTarget> marks;auto mark=[&](GuiInputTarget t){marks.insert(t);};auto draw=[&]{trace.clear();panel.update(doc,selection);panel.draw(doc,selection,false,true,mark,[&](bool){return transform;},{0,0,32,24},&removal,1);removal.draw(doc,1,error);};host.fit("Pixel classifiers",1000,1300,draw);
    auto* window=ImGui::FindWindowByName("Pixel classifiers");ImGui::SetScrollY(window,window->ScrollMax.y);host.settle(draw);
    no_invented_states(trace,"Pixel classifier");const auto row=std::find_if(trace.begin(),trace.end(),[&](const auto& row){return row.row.id==second;});check(row!=trace.end(),"Pixel list omitted second record");const auto bounds=row->bounds;host.click(bounds,draw);check(panel.archives().selected()==second,"Pixel row selection lost native ID");
    host.click("Use saved model",draw);check(panel.record_loaded() && panel.model()->sha256()==doc.pixel_classifier_record(second)->archive->model()->sha256(),"Use kept pixel model changed native authority");
    ImGui::SetScrollY(window,window->ScrollMax.y);host.settle(draw);host.click("Preview saved mask",draw);check(panel.historical_result()==doc.pixel_classifier_record(second)->archive->result(),"Pixel Preview did not use highlighted archive");
    host.click("Remove record",draw);check(removal.pending() && doc.pixel_classifier_record_count()==2,"Pixel Remove bypassed confirmation");confirm_removal();host.settle(draw);check(doc.pixel_classifier_record_count()==1 && doc.pixel_classifier_record(first) && error.empty(),"Pixel Remove changed a different record");
    check(marks.count(GuiInputTarget::UsePixelRecordModel) && marks.count(GuiInputTarget::PreviewPixelRecord) && marks.count(GuiInputTarget::RemovePixelRecord),"Pixel action replay targets disappeared");
    std::cout<<"Pixel row selection/Use exact model/Preview exact archive/confirmed Remove/replay marks passed\n";
}
inline void raw_measurements(const std::filesystem::path& dir) {
    qpx_test::raw_pixel_tiff_fixture(dir/"measurements.tiff",12,8);Document doc(open_tiff((dir/"measurements.tiff").u8string()));const auto roi=doc.add_rectangle({0,0,12,8});doc.rename_annotation(roi,"Raw annotation");
    RawRegionRequest region;region.region={0,0,0,6,6};region.channels=2;RawAnalysisRequest a(doc,region,roi);const auto first=doc.keep_raw_analysis(a,analyze_raw(a));region.region.width=5;RawAnalysisRequest b(doc,region,roi);const auto second=doc.keep_raw_analysis(b,analyze_raw(b));
    GuiOptions options;options.raw_open=true;options.raw_tab=3;options.raw_record_id=first;options.raw_csv_path=(dir/"selected-raw.csv").u8string();detail::GuiRaw panel(options);ObjectSelection selection(doc);detail::RecordRemovalDialog removal;detail::OutputGate gate;std::string error;
    simplification_tools_checks::NativeUi host;std::vector<ui::RecordListItem> trace;ui::record_list_trace=&trace;struct Reset{~Reset(){ui::record_list_trace=nullptr;}}reset;
    std::set<GuiInputTarget> marks;auto draw=[&]{trace.clear();panel.draw(doc,selection,nullptr,false,[&](GuiInputTarget t){marks.insert(t);},&removal,1,&gate,{dir.u8string()});removal.draw(doc,1,error);};host.fit("Quantitative channels",1000,1300,draw);host.until(draw,[&]{return panel.ready(doc);},"Raw metadata did not finish");
    check(panel.record_id()==first,"Raw scripted identifier did not preselect");const auto row=std::find_if(trace.begin(),trace.end(),[&](const auto& row){return row.row.id==second;});check(row!=trace.end(),"Raw list omitted second record");const auto bounds=row->bounds;host.click(bounds,draw);check(panel.record_id()==second,"Raw row selection lost native ID");
    host.click(panel.bounds(GuiInputTarget::VerifyRawAnalysisRecord),draw);host.until(draw,[&]{return panel.record_verified();},"Raw selected-record native recheck did not verify");
    auto* window=ImGui::FindWindowByName("Quantitative channels");auto* tabs=ImGui::GetCurrentContext()->TabBars.GetByKey(ImHashStr("Quantitative views",0,window->ID));check(tabs && tabs->SelectedTabId,"Raw Records tab scope missing");ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Only selected record",0,tabs->SelectedTabId)));host.settle(draw);host.click(panel.bounds(GuiInputTarget::ExportRawMeasurements),draw);host.until(draw,[&]{return panel.csv_state()=="saved";},"Raw selected CSV did not finish");
    check(read(dir/"selected-raw.csv")==raw_analysis_csv(RawTableRequest(doc,{second})),"Raw CSV did not use highlighted native record");host.click(panel.bounds(GuiInputTarget::RemoveRawAnalysisRecord),draw);check(removal.pending(),"Raw measurement Remove confirmation missing");confirm_removal();host.settle(draw);check(doc.raw_analysis_record_count()==1 && doc.raw_analysis_record(first) && error.empty(),"Raw Remove changed a different record");
    check(marks.count(GuiInputTarget::VerifyRawAnalysisRecord) && marks.count(GuiInputTarget::RemoveRawAnalysisRecord),"Raw replay marks disappeared");
    std::cout<<"Raw measurement scripted selection/row click/exact Verify CSV/confirmed Remove/replay marks passed\n";
}
inline void run(const std::filesystem::path& dir){std::filesystem::create_directories(dir);presentation(dir);recipe_choices(dir);raw_planes(dir);lists(dir);object(dir);raw_pixel(dir);spatial(dir);batch(dir);tables(dir);pixel(dir);raw_measurements(dir);}
}
