#pragma once
#include "../gui_fluorescence.hpp"
#include "../gui_dark_flat.hpp"
#include "../gui_linear_unmixing.hpp"
#include "../gui_marker_rules.hpp"
#include <set>
#include "../gui_fonts.hpp"
#include "gui_record_fixture.hpp"
namespace tool_path_checks {
using namespace qpx;
using A=ui::PathFieldTestAccess;
using P=detail::GuiImportPickerTestAccess;
inline void controls(const std::filesystem::path& directory) {
    namespace fs=std::filesystem;
    const auto dir=fs::absolute(directory).lexically_normal(),project=dir/"tool-project";
    fs::create_directories(project);fs::create_directories(dir/"folder");
    GuiOptions o;o.qptma_input_path=o.qptma_output_path="relative.qptma";
    o.tma_measurements_path=o.tma_metadata_path=o.raw_csv_path="relative.csv";
    o.raw_numpy_path="relative.npy";o.batch_output_directory="relative-folder";
    o.classifier_model_path="relative.qpxforest";o.pixel_model_path="relative.qpc";
    o.pixel_stream_path="relative.qps";o.pixel_stream_read_path="read.qps";
    auto raw_options=std::make_shared<GuiRawPixelOptions>();
    raw_options->model_path="relative.qrm";raw_options->stream_path="relative.qws";
    raw_options->training.checkpoint_path=raw_options->training.checkpoint.resume_path="relative.qtc";
    raw_options->save.resume_path="resume.qws";o.raw_pixel=raw_options;
    auto spatial_options=std::make_shared<GuiSpatialOptions>();spatial_options->csv_path="relative.csv";o.spatial=spatial_options;
    auto tma=std::make_unique<detail::GuiTma>(o);auto raw=std::make_unique<detail::GuiRaw>(o);
    auto spatial=std::make_unique<detail::GuiSpatial>(o);auto batch=std::make_unique<detail::GuiBatch>(o);
    auto density=std::make_unique<detail::GuiProjectDensity>();auto object=std::make_unique<detail::GuiObjectClassifier>(o);
    auto pixel=std::make_unique<detail::GuiPixelClassifier>(o);auto rp=std::make_unique<detail::GuiRawPixel>(o);
    auto fluorescence=std::make_unique<detail::GuiFluorescence>();
    fluorescence->set_tiling(true,{},"relative-folder");
    fluorescence->correction_tool().set_fields("dark.tif","flat.tiff",DarkFlatOptions{{1},0});
    fluorescence->unmixing_tool().set_matrix_file("mix.json");
    // Marker paths are included in the fluorescence family's paths() list.
    std::vector<ui::PathField*> fields;
    for(auto list:{tma->paths(),raw->paths(),spatial->paths(),batch->paths(),density->paths(),object->paths(),pixel->paths(),rp->paths(),fluorescence->paths()})
        fields.insert(fields.end(),list.begin(),list.end());
    check(fields.size()==29,"Tool path registry count changed without updating coverage");
    std::string last;ui::PathContext context{project.u8string(),&last};
    const std::map<std::string,std::string> expected{
        {"##QPTMA input",o.qptma_input_path},{"##QPTMA output",o.qptma_output_path},
        {"##TMA measurements path",o.tma_measurements_path},{"##TMA metadata path",o.tma_metadata_path},
        {"##raw NumPy path",o.raw_numpy_path},{"##raw CSV path",o.raw_csv_path},{"CSV path","relative.csv"},
        {"##output",o.batch_output_directory},{"Raw model",""},{"##density-previous",""},{"##density-output",""},
        {"Model file",o.classifier_model_path},{"Export model file",o.classifier_model_path},
        {"Pixel model file",o.pixel_model_path},{"Export pixel model file",o.pixel_model_path},
        {"Pixel stream file",o.pixel_stream_path},{"Saved stream file",o.pixel_stream_read_path},
        {"Model path",raw_options->model_path},{"Export model path",raw_options->model_path},
        {"Prediction file",raw_options->stream_path},{"Open prediction file",raw_options->stream_path},
        {"Training checkpoint",raw_options->training.checkpoint_path},{"Resume training from",raw_options->training.checkpoint.resume_path},
        {"Resume prediction from",raw_options->save.resume_path},{"Tiled results folder","relative-folder"},
        {"Dark TIFF path","dark.tif"},{"Flat TIFF path","flat.tiff"},{"Mixing matrix JSON path","mix.json"},{"Rule JSON path",""}};
    check(sizeof(ui::PathField)<40000,"PathField eagerly owns a picker");
    for(auto* field:fields) {
        const auto label=std::string(A::spec(*field).label);field->sync(context);
        check(std::string(field->text())==expected.at(label),"Option verbatim prefill: "+label);
        const auto value=expected.at(label);
        check(field->state().resolved==(value.empty()?std::string{}:fs::absolute(fs::u8path(value)).lexically_normal().u8string()),"Script option process-relative resolution: "+label);
        check(!field->picker_active() && !field->take_picked(),"Unallocated picker must be inactive");
    }
    for(auto pair:{std::pair{"Model file","Export model file"},std::pair{"Pixel model file","Export pixel model file"},std::pair{"Model path","Export model path"},std::pair{"Open prediction file","Prediction file"}}) {
        auto& input=core_path_checks::find(fields,pair.first);auto& output=core_path_checks::find(fields,pair.second);
        const auto saved=std::string(output.text());input.set("loaded.input");check(std::string(output.text())==saved,"Read field changed write field");
        output.set("saved.output");check(std::string(input.text())=="loaded.input","Write field changed read field");
    }
    auto marker_option=std::make_unique<detail::GuiMarkerRules>();marker_option->set_file("relative-rule.json");marker_option->paths()[0]->sync(context);check(std::string(marker_option->paths()[0]->text())=="relative-rule.json" && marker_option->paths()[0]->state().resolved==(fs::current_path()/"relative-rule.json").lexically_normal().u8string(),"Marker option process-relative semantics");
    auto forms=std::make_unique<detail::GuiProjectForms>(o);
    for(int locale:{0,2}) {
        simplification_tools_checks::NativeUi host;ui::i18n::choose(locale);
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        for(auto* field:fields) {
            const auto& spec=A::spec(*field);const auto label=std::string(spec.label);
            const bool folder=spec.role==ui::PathRole::OpenFolder || spec.role==ui::PathRole::NewFolder;
            const auto selected=folder?(spec.role==ui::PathRole::NewFolder?dir/"new-folder":dir/"folder"):dir/fs::u8path("chosen"+(spec.extensions.empty()?std::string{}:spec.extensions.front()));
            if(!folder){std::ofstream f(selected);f<<"picker fixture";}
            field->set("");
            auto draw=[&]{ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({520.f,600.f});ImGui::Begin("Tool path IDs");
                const auto seed=ImGui::GetCurrentWindow()->IDStack.back();
                const auto id=spec.unscoped_id?ImHashStr(spec.label,0,seed):ImHashStr("##value",0,ImHashStr(spec.label,0,seed));
                field->draw(context,false,[&]{check(ImGui::GetItemID()==id,"Original tool input ID changed: "+label);});
                check(ui::layout_stats.overflow_items==0,"Tool path normal/French200 overflow: "+label+" / "+ui::layout_stats.first_overflow);
                ImGui::End();};
            host.settle(draw);
            const auto at=A::center(*field);
            check(detail::route_path_drop({{selected.u8string()},at.x,at.y},*forms,fields,false),"Tool field drop not routed: "+label);
            check(std::string(field->text())==selected.u8string() && !forms->adding(),"Tool field drop started Add: "+label);
            field->set("");host.settle(draw);
            host.click(A::browse(*field),draw);
            check(field->picker_active(),"Browse did not open tool picker: "+label);
            const auto mode=spec.role==ui::PathRole::OpenFile?detail::PickerMode::OpenFile:spec.role==ui::PathRole::NewFile?detail::PickerMode::NewFile:spec.role==ui::PathRole::OpenFolder?detail::PickerMode::OpenFolder:detail::PickerMode::NewFolder;
            check(P::mode(A::picker(*field))==mode,"Browse role/mode differs: "+label);
            // Select a fixture from the chooser without sending keyboard text.
            A::picker(*field).open(selected.u8string(),spec.extensions,mode);host.settle(draw);
            if(folder)for(const auto& name:P::names(A::picker(*field)))check(fs::is_directory((mode==detail::PickerMode::NewFolder?dir:selected)/fs::u8path(name)),"Folder role listed a file");
            core_path_checks::picker_action(mode==detail::PickerMode::OpenFile?"Choose file":mode==detail::PickerMode::NewFile?"Choose new output":mode==detail::PickerMode::OpenFolder?"Choose folder":"Choose output folder");
            host.settle(draw);
            check(!field->picker_active() && field->take_picked() && field->state().resolved==selected.u8string(),"Browse completion differs: "+label);
        }
    }
    {
        simplification_tools_checks::NativeUi host;auto& field=*fields.front();int changes=0;
        auto draw=[&]{ImGui::Begin("Central picker completion");field.draw_picker(context);if(field.draw(context))++changes;ImGui::End();};
        host.settle(draw);changes=0;const auto selected=(dir/"central.csv").u8string();field.open_picker(context,selected);
        A::picker(field).open(selected,{".csv"},detail::PickerMode::NewFile);host.settle(draw);
        core_path_checks::picker_action("Choose new output");host.settle(draw);
        check(changes==1 && field.take_picked() && !field.take_picked(),"Centrally serviced picker must notify its owner exactly once");
        field.drop("drop.csv");host.settle(draw);check(changes==2,"Drop did not notify owner exactly once");
        field.drop("queued.csv");field.set_option("script.csv");host.settle(draw);
        check(field.state().resolved==(fs::current_path()/"script.csv").lexically_normal().u8string(),"Old pending edit changed new option semantics");
    }
    std::cout<<"29 tool fields: exact IDs/options/process resolution, split isolation, real Browse modes/filters/folders, drop without Add and normal/French200 zero overflow passed\n";
}
inline void batch_operation(const std::filesystem::path& directory) {
    namespace fs=std::filesystem;const auto dir=fs::absolute(directory).lexically_normal();
    const auto project=simplification_tools_checks::density_fixture(dir/"browse-batch-source.qpxproj");
    auto doc=project->document(project->active_image());ObjectSelection selection(*doc);GuiOptions options;options.batch_options=std::make_shared<ProjectBatchOptions>();
    auto panel=std::make_unique<detail::GuiBatch>(options);panel->open();simplification_tools_checks::NativeUi host;
    auto draw=[&]{panel->poll();panel->draw(project.get(),project->active_image(),&selection,false,{TissueParameters{},nullptr,{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind::OpticalDensityMeans,RoiSamplingPolicy{},false,nullptr,false,std::vector<double>(6),std::vector<double>(3,255)},[](bool){return StainTransform({{1,0,0},{0,1,0}});});};
    host.fit("Project batch",700,900,draw);const auto output=dir/"browse-batch-output";
    path_field_checks::choose(*panel->paths()[0],output.u8string(),[&]{host.frame(draw);});
    host.click("Run batch (Ctrl+Alt+B)",draw);host.until(draw,[&]{return !panel->busy();},"Browse-only batch did not settle");
    check(panel->error().empty() && panel->result() && panel->result()->state=="succeeded" && panel->result()->images.size()==2,"Browse-only batch failed: "+panel->error());
    check(fs::is_directory(output),"Browse-only batch did not publish output directory");
    std::cout<<"Batch Browse-only output selection, actual Run and two-image publication passed\n";
}
inline void windows(const std::filesystem::path& directory) {
    namespace fs=std::filesystem;const auto dir=fs::absolute(directory).lexically_normal();
    check(glfwInit()!=0,"Tool capture GLFW initialization");
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(1440,1000,"Tool path views",nullptr,nullptr);check(window,"Tool capture window");glfwMakeContextCurrent(window);
    const auto source=fs::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    for(int locale:{0,2}) {
        simplification_tools_checks::NativeUi host;ui::i18n::choose(locale);ImGui::GetIO().DisplaySize={1440,1000};
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
        int maximum_texture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum_texture);
        ImGui::GetIO().Fonts->Clear();ImGui::GetIO().FontDefault=detail::configure_gui_fonts(*ImGui::GetIO().Fonts,maximum_texture);
        struct BackendCleanup {~BackendCleanup(){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();}} backend_cleanup;
        {
        auto doc=record_fixture::source(dir);const auto recipe_rows=record_fixture::seed(*doc,dir);Project project;project.add_document(doc);ObjectSelection selection(*doc);
        TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.width=grid.height=1;grid.origin={1,2};grid.column_step={3,0};doc->create_tma_grid(grid);
        // Real retained rows make the changed list controls visible in both locales.
        const auto record_roi=doc->add_rectangle({0,0,6,4});doc->rename_annotation(record_roi,"Capture ROI");
        std::vector<std::uint64_t> object_ids;for(int i=0;i<6;++i)object_ids.push_back(doc->add_detection(Roi::rectangle({double(i),0,1,1}),"Capture object",0,i<3?"small":"large"));
        ObjectFeatureOptions object_features;object_features.calibrated=false;ObjectFeatureRequest object_request(*doc,object_ids,object_features);const auto object_model=ForestModel::from_bytes(qpx_test::object_forest_fixture());ObjectClassifierJob object_job;check(object_job.predict(object_request,object_model) && object_job.wait(*doc).state=="ready","Capture classifier fixture");object_job.apply(*doc);
        PixelFeatureOptions pixel_features;pixel_features.sigmas={0};StainTransform pixel_transform({{1,0,0},{0,1,0}});std::vector<std::pair<std::string,std::string>> pixel_schema;for(const auto& f:pixel_feature_schema(pixel_features,pixel_transform))pixel_schema.emplace_back(f.name,f.unit);
        PixelClassifier pixel_model(ForestModel::from_bytes(qpx_test::object_forest_fixture(pixel_schema)),pixel_features,pixel_transform,PixelResolution{});PixelPredictionRequest pixel_request(*doc,record_roi,pixel_model);doc->keep_pixel_prediction(pixel_request,archive_pixel_prediction(predict_pixel_region(pixel_request)),"Capture pixel mask");
        RawRegionRequest raw_region;raw_region.region={0,0,0,6,4};raw_region.channels=2;RawAnalysisRequest raw_request(*doc,raw_region,record_roi);const auto raw_id=doc->keep_raw_analysis(raw_request,analyze_raw(raw_request));
        SpatialDistanceOptions spatial_options;spatial_options.units=SpatialDistanceUnits::Pixels;SpatialDistanceRequest spatial_request(*doc,{record_roi},{object_ids.front()},spatial_options);const auto spatial_id=doc->keep_spatial(spatial_request,measure_spatial_distances(spatial_request));
        ScientificRawLattice lattice(15,9,{0,15,15,0},{0,9,9,0});ScientificRawOptions raw_channels;raw_channels.channels=2;RawPixelFeatureOptions raw_features;raw_features.offsets={0,0};raw_features.scales={1,1};RawPixelRecipe raw_recipe(*doc->server()->raw_metadata(),lattice,raw_channels,raw_features);std::vector<std::pair<std::string,std::string>> raw_schema;for(const auto& f:raw_recipe.feature_schema())raw_schema.emplace_back(f.name,f.unit);RawPixelClassifier raw_model(ForestModel::from_bytes(qpx_test::object_forest_fixture(raw_schema)),raw_recipe);RawPixelTiledPredictionRequest raw_file_request(*doc,raw_model,lattice);const auto raw_file=(dir/("capture-"+std::to_string(locale)+".qws")).u8string();save_raw_pixel_stream(raw_file_request,raw_file);const auto raw_file_id=doc->keep_raw_pixel_stream(prepare_raw_pixel_stream_link(raw_file),"Capture raw file");
        GuiOptions o;o.tma_open=o.raw_open=o.classifier_open=o.pixel_open=true;o.pixel_stream_browser_open=false;o.tma_tab=2;o.raw_tab=1;o.raw_record_id=raw_id;
        auto bo=std::make_shared<ProjectBatchOptions>();bo->kind=ProjectBatchKind::Roi;o.batch_options=bo;
        auto ro=std::make_shared<GuiRawPixelOptions>();ro->open=true;ro->record_id=raw_file_id;o.raw_pixel=ro;
        auto so=std::make_shared<GuiSpatialOptions>();so->open=true;so->record_id=spatial_id;so->analysis=spatial_options;o.spatial=so;
        auto tma=std::make_unique<detail::GuiTma>(o);auto raw=std::make_unique<detail::GuiRaw>(o);auto record_options=o;record_options.raw_tab=3;auto raw_records=std::make_unique<detail::GuiRaw>(record_options);auto batch=std::make_unique<detail::GuiBatch>(o);batch->open();detail::GuiBatchTestAccess::raw_mode(*batch);
        auto density=std::make_unique<detail::GuiProjectDensity>();density->open();auto spatial=std::make_unique<detail::GuiSpatial>(o);
        auto object=std::make_unique<detail::GuiObjectClassifier>(o);auto pixel=std::make_unique<detail::GuiPixelClassifier>(o);auto rp=std::make_unique<detail::GuiRawPixel>(o);
        auto browser=std::make_unique<detail::GuiPixelStreamBrowser>(o);browser->open();
        auto fluo=std::make_unique<detail::GuiFluorescence>();fluo->open();fluo->set_tiling(true);auto dark=std::make_unique<detail::GuiDarkFlat>();dark->open();auto unmix=std::make_unique<detail::GuiLinearUnmixing>();unmix->open();auto marker=std::make_unique<detail::GuiMarkerRules>();marker->open();
        fluo->poll(*doc);fluo->set_correction(replay_dark_flat_recipe(*doc,recipe_rows.correction));
        ui::PathContext context{dir.u8string()};auto mark=[](GuiInputTarget){};auto transform=[](bool){return StainTransform({{1,0,0},{0,1,0}});};
        const auto catalog=project.images();
        ui::RecordRows empty_rows;
        std::vector<std::pair<std::string,std::function<void()>>> draws{
            {"empty-list",[&]{ImGui::SetNextWindowSize({640,240},ImGuiCond_Always);ImGui::Begin("Empty kept list");ui::draw_record_list("Kept raw files",empty_rows,0);ImGui::End();}},
            {"tma",[&]{tma->draw(*doc,selection,&project,false,true,mark,nullptr,context);}},
            {"raw",[&]{raw->draw(*doc,selection,&project,false,mark,nullptr,0,nullptr,context);}},
            {"raw-records",[&]{raw_records->draw(*doc,selection,&project,false,mark,nullptr,0,nullptr,context);}},
            {"batch",[&]{batch->draw(&project,1,&selection,false,{TissueParameters{},{},{},{},CellParameters{},{},{},{},{},GridCellParameters{},AnalysisKind{},RoiSamplingPolicy{},false,{},false,std::vector<double>(6),std::vector<double>(3,255)},transform,context);}},
            {"density",[&]{density->draw(&project,catalog,false,context);}},
            {"spatial",[&]{spatial->draw(*doc,selection,&project,&catalog,false,true,mark,nullptr,0,nullptr,context);}},
            {"object",[&]{object->draw(*doc,selection,false,true,mark,nullptr,context);}},
            {"pixel",[&]{pixel->update(*doc,selection);pixel->draw(*doc,selection,false,true,mark,transform,{0,0,7,5},nullptr,0,nullptr,context);}},
            {"pixel-browser",[&]{browser->update();browser->draw(*doc,false,mark,nullptr,0,context);}},
            {"raw-pixel",[&]{rp->update(*doc);rp->draw(*doc,selection,&project,false,true,mark,nullptr,0,nullptr,context);}},
            {"raw-pixel-records",[&]{rp->update(*doc);rp->draw(*doc,selection,&project,false,true,mark,nullptr,0,nullptr,context);}},
            {"fluorescence",[&]{fluo->draw(*doc,0,false,context);}},
            {"fluorescence-records",[&]{fluo->draw(*doc,0,false,context);}},
            {"dark-flat",[&]{dark->draw(*doc,false,context);}},
            {"unmixing",[&]{unmix->draw(*doc,false,context);}},
            {"marker",[&]{marker->draw(*doc,0,false,context);}}
        };
        std::vector<ui::RecordListItem> visible_rows;ui::record_list_trace=&visible_rows;struct RowReset{~RowReset(){ui::record_list_trace=nullptr;}} row_reset;
        std::set<std::string> seeded_seen;
        for(auto& [name,draw]:draws)for(int lower:{0,1}) {
            if(name=="fluorescence" || name=="fluorescence-records")fluo->set_tiling(name=="fluorescence",{},"capture-tiled");
            if(name=="raw-pixel" || name=="raw-pixel-records"){host.settle(draw);auto* tab_window=ImGui::FindWindowByName(ui::i18n::label("Raw pixel classifier").c_str());check(tab_window,"Restored Raw pixel tab window missing");ImGui::SetScrollY(tab_window,0);host.settle(draw);host.click(rp->bounds(name=="raw-pixel"?GuiInputTarget::RawPixelFilesTab:GuiInputTarget::RawPixelRecordsTab),draw);}
            if(name=="pixel" && lower)pixel->reveal_target(GuiInputTarget::ExportPixelStream);
            for(int frame=0;frame<5;++frame) {
                glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ui::begin_frame();visible_rows.clear();draw();
                if(!visible_rows.empty())seeded_seen.insert(name);
                if(lower)for(auto* win:ImGui::GetCurrentContext()->Windows)if(win->Active && !win->ParentWindow && !(win->Flags&ImGuiWindowFlags_Modal))ImGui::SetScrollY(win,win->ScrollMax.y);
                for(auto list:{tma->paths(),raw->paths(),raw_records->paths(),batch->paths(),density->paths(),spatial->paths(),object->paths(),pixel->paths(),browser->paths(),rp->paths(),fluo->paths(),dark->paths(),unmix->paths(),marker->paths()})for(auto* field:list)check(A::draws(*field)<=1,"PathField drawn twice in one frame");
                check(ui::layout_stats.overflow_items==0,"Changed tool window normal/French200 overflow: "+name+" / "+ui::layout_stats.first_overflow);
                ImGui::Render();int width,height;glfwGetFramebufferSize(window,&width,&height);glViewport(0,0,width,height);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                if(frame==4 && !lower) {
                    if(name=="raw")check(A::draws(core_path_checks::find(raw->paths(),"##raw NumPy path"))==1,"Restored Raw Data export path is not drawn");
                    if(name=="raw-pixel")check(A::draws(core_path_checks::find(rp->paths(),"Model path"))==1 && A::draws(core_path_checks::find(rp->paths(),"Export model path"))==1,"Restored Raw pixel model rows are not drawn");
                    if(name=="fluorescence")check(A::draws(core_path_checks::find(fluo->paths(),"Tiled results folder"))==1,"Restored tiled fluorescence path is not drawn");
                }
                if(frame==4)dialog_checks::capture(dir/("t24-"+name+(locale==2?"-fr200":"-normal")+(lower?"-lower.ppm":".ppm")),window);
                glfwSwapBuffers(window);
            }
        }
        for(const auto* family:{"raw-records","spatial","object","pixel","raw-pixel-records","fluorescence-records","dark-flat","unmixing","marker"})check(seeded_seen.count(family),std::string("Capture has no native row: ")+family);
        }
    }
    glfwDestroyWindow(window);glfwTerminate();std::cout<<"Changed tool windows normal/French200: ui_overflow_items=0 and focused control captures passed\n";
}
}
