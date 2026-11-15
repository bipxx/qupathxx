#pragma once
#include "gui_dispatch_checks.hpp"
#include "../gui_help.hpp"
namespace reasons_help_checks {
using namespace qpx;
inline constexpr const char* reason_keys[]={
    "This recipe is already kept.",
    "This recipe is used by kept results.",
    "Run again for this image view before keeping results.",
    "Fit a new preview for the current expression source and image calibration.",
    "Select an expression source first.",
    "Remove a landmark before adding another.",
    "Clear the existing TMA grid before creating another.",
    "The grid has no present cores.",
    "Create or import a TMA grid first.",
    "Resolve the grid export error shown above.",
    "Acknowledge the omitted metadata first.",
    "Open the containing project first.",
    "Run normalization first.",
    "Choose a full indexed raw-count matrix first.",
    "QC is already available.",
    "This object has no properties.",
    "There is nothing to apply.",
    "Keep a cell run for the selected annotation before exporting kept cells.",
    "Open an OME-TIFF image to open another series.",
    "Train again to keep its training data.",
    "Keep a result first.",
    "Hash the file first.",
    "White reference kept. Native RGB8 stain application requires each white channel in 1..255.",
    "Choose a correction in Dark/flat correction, or turn off Use selected dark/flat correction.",
    "Choose a file path first.",
    "Choose an unmixing recipe in Linear unmixing, or turn off Use selected unmixing.",
    "In Tables, choose kept runs in Cell source.",
    "Close the kept mask first.",
    "Enter an output folder.",
    "Finish the current dialog first.",
    "Fluorescence analysis needs a quantitative TIFF image.",
    "Keep a cell run before exporting.",
    "Keep a grid measurement in Results, or switch to natural sampling for original-sample NumPy export.",
    "Load or edit the inputs first.",
    "Open a project first.",
    "Open a project to run a batch.",
    "Open a quantitative OME-TIFF or local OME-Zarr image.",
    "Open an image first.",
    "Open an image to run a batch.",
    "Open another series as a new image.",
    "Recheck the kept cell run before exporting current measurements.",
    "Recheck the kept result first.",
    "Restore the annotation before rechecking.",
    "Run a preview first.",
    "Run again for a new result.",
    "Run without tiles to keep results in the project.",
    "Select a kept result first.",
    "Select an annotation with a kept cell run.",
    "Select an area annotation or turn off Use selected ROI",
    "The selected correction no longer matches this image. Choose it again in Dark/flat correction.",
    "Trace contours",
    "Train a model first.",
    "The kept result's image source is unavailable.",
    "Train or load a model first.",
    "Use a build with this feature enabled.",
    "Wait for the current work to finish.",
};
inline void inspect(const ui::WorkflowTrace& trace,const std::string& scene) {
    check(!trace.actions.empty(),"No rendered actions: "+scene);
    for(const auto& [key,a]:trace.actions)if(a.disabled) {
        check(!a.enabled && !a.reason.empty(),"Disabled action has no reason: "+scene+" / "+key);
        if(!a.inline_reason.empty())check(a.inline_reason==a.reason,"Inline/tooltip reason mismatch: "+scene+" / "+key);
    }
}
template<class Host,class Draw>inline void press(Host& host,const char* original,Draw& draw) {
    const auto a=host.action(original);check(a.enabled,"Start action unavailable: "+std::string(original));
    auto& io=ImGui::GetIO();io.AddMousePosEvent(float(a.bounds.x+a.bounds.width/2),float(a.bounds.y+a.bounds.height/2));host.settle(draw);
    io.AddMouseButtonEvent(0,true);host.frame(draw);io.AddMouseButtonEvent(0,false);host.frame(draw);
}
inline void tooltip(model_safety_checks::GlHost& host,const char* key,std::function<void()>& draw) {
    const auto a=host.action(key);check(a.disabled && !a.reason.empty(),"Tooltip probe needs a disabled action");
    ImGui::GetIO().AddMousePosEvent(float(a.bounds.x+a.bounds.width/2),float(a.bounds.y+a.bounds.height/2));
    std::string text;bool tooltip_open=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!tooltip_open || model_safety_checks::occurrences(text,a.reason)<2) {
        check(std::chrono::steady_clock::now()<deadline,"Disabled tooltip did not render its translated reason: "+std::string(key));
        // The host renders without GLFW NewFrame here; keep its explicit test clock
        // independent of the tiny real-time delta left by the preceding capture.
        ImGui::GetIO().DeltaTime=1.f/60;
        text=model_safety_checks::logged(host,draw);tooltip_open=false;
        for(const auto* window:host.context->Windows)if(window->Active && (window->Flags&ImGuiWindowFlags_Tooltip))tooltip_open=true;
    }
    ImGui::GetIO().AddMousePosEvent(-100,-100);host.settle(draw);
}
inline void catalogue_reasons(GLFWwindow* window) {
    model_safety_checks::GlHost host(window);
    constexpr const char* action="Analyze region (Ctrl+M)";
    for(int locale:{0,2})for(const auto* key:reason_keys) {
        ui::i18n::choose(locale);const ui::Availability available{false,key};
        std::function<void()> draw=[&]{ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({1000,400});ImGui::Begin("Reason catalogue");ui::WorkflowAction(action,ui::design::Action::Primary,available);ui::WorkflowReason(action,available);ImGui::End();};
        host.settle(draw);const auto rendered=model_safety_checks::logged(host,draw);
        const auto translated=std::string(ui::i18n::text(key));
        check(!translated.empty() && (locale==0 || translated!=key),"Reason translation missing: "+std::string(key));
        check(rendered.find(translated)!=std::string::npos && host.action(action).inline_reason==translated,"Reason did not render: "+std::string(key));
        tooltip(host,action,draw);inspect(host.trace,"reason catalogue");
    }
    std::cout<<"All "<<std::size(reason_keys)<<" reason catalogue keys rendered inline and in disabled tooltips in English/French\n";
}
inline void tools(const std::filesystem::path& dir,GLFWwindow* window) {
    const auto doc=record_fixture::source(dir);const auto recipes=record_fixture::seed(*doc,dir);
    const auto recorded=replay_fluorescence_cell_record(*doc,recipes.fluorescence);const auto detected=detect_fluorescence_cells(recorded);
    const auto traced=trace_fluorescence_cells(detected);publish_fluorescence_cells(*doc,recorded,*traced,"Reasons cell");
    Project project;project.add_document(doc);
    const auto roi=doc->add_rectangle({0,0,15,9});ObjectSelection selection(*doc);
    const auto mark=[](GuiInputTarget){};const auto transform=[](bool){return StainTransform({{1,0,0},{0,1,0}});};ui::PathContext context{dir.u8string()};
    const auto object_model=dir/"reasons-object.qpxforest";save_forest_model(ForestModel::from_bytes(qpx_test::object_forest_fixture()),object_model.u8string());
    PixelFeatureOptions features;features.sigmas={0};const auto stain=transform(false);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:pixel_feature_schema(features,stain))schema.emplace_back(f.name,f.unit);
    PixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),features,stain,PixelResolution{});const auto pixel_model=dir/"reasons-pixel.qpc";save_pixel_classifier(model,pixel_model.u8string());
    // Each scene owns a fresh controller; native work is submitted by its widget.
    for(int tool=0;tool<8;++tool) {
        model_safety_checks::GlHost host(window);bool image=false,blocked=false;
        GuiOptions options;options.classifier_open=options.pixel_open=options.raw_open=true;options.raw_tab=1;
        options.classifier_model_path=object_model.u8string();options.pixel_model_path=pixel_model.u8string();
        auto raw_options=std::make_shared<GuiRawPixelOptions>();raw_options->open=true;raw_options->show_overlay=false;raw_options->training.forest.trees=3;raw_options->training.forest.max_depth=3;options.raw_pixel=raw_options;
        detail::GuiObjectClassifier object(options);detail::GuiPixelClassifier pixel(options);detail::GuiRaw raw(options);detail::GuiRawPixel raw_pixel(options);
        detail::GuiFluorescence fluo;fluo.open();detail::GuiDarkFlat dark;dark.open();detail::GuiLinearUnmixing unmix;unmix.open();detail::GuiMarkerRules marker;marker.open();
        dark.set_fields((dir/"list-dark.tif").u8string(),(dir/"list-flat.tif").u8string(),DarkFlatOptions{{10,20},0});
        unmix.set_saved_record(recipes.unmix);
        RawRegionRequest region;region.region={0,0,0,15,9};region.channels=2;fluo.set_request(region,{}, {0,0},false);
        const char* names[]={"object","pixel","raw","raw-pixel","fluorescence","dark-flat","unmixing","marker"};
        const char* originals[]={"Load model","Load pixel model","Measure raw channels","Train raw classifier","Run fluorescence","Load correction fields","Load kept unmixing###Load saved unmixing","Preview markers"};
        const char* primaries[]={"Train from labelled objects","Train labelled areas","Measure raw channels","Train raw classifier","Run fluorescence","Load correction fields","Load matrix JSON","Preview markers"};
        const char* title=tool==0?ui::tool_window_label(ui::ToolId::ObjectClassifier):tool==1?ui::tool_window_label(ui::ToolId::PixelClassifier):tool==2?ui::tool_window_label(ui::ToolId::Channels):tool==3?ui::tool_window_label(ui::ToolId::RawPixelClassifier):tool==4?ui::tool_window_label(ui::ToolId::Fluorescence):tool==5?ui::tool_window_label(ui::ToolChild::DarkFlat):tool==6?ui::tool_window_label(ui::ToolChild::Unmixing):ui::tool_window_label(ui::ToolChild::MarkerRules);
        const auto window_id=ImHashStr(title);
        std::function<void()> draw=[&]{
            switch(tool) {
            case 0:object.update(*doc,selection);object.draw(*doc,selection,blocked,image,mark,nullptr,context);break;
            case 1:pixel.update(*doc,selection);pixel.draw(*doc,selection,blocked,image,mark,transform,{0,0,15,9},nullptr,0,nullptr,context);break;
            case 2:raw.draw(*doc,selection,&project,blocked,mark,nullptr,0,nullptr,context,image);break;
            case 3:raw_pixel.update(*doc);raw_pixel.draw(*doc,selection,&project,blocked,image,mark,nullptr,0,nullptr,context);break;
            case 4:fluo.draw(*doc,0,blocked,context,image);break;
            case 5:dark.draw(*doc,blocked,context,image);break;
            case 6:unmix.draw(*doc,blocked,context,image);break;
            case 7:marker.draw(*doc,0,blocked,context,image);break;
            }
        };
        const auto style=ImGui::GetStyle();
        const auto visit_tabs=[&](const char* state){
            if(tool==2){for(auto tab:{GuiInputTarget::RawDisplayTab,GuiInputTarget::RawResultsTab,GuiInputTarget::RawRecordsTab,GuiInputTarget::RawDataTab}){host.click(raw.bounds(tab),draw);inspect(host.trace,std::string(names[tool])+" / "+state);if(tab==GuiInputTarget::RawDisplayTab && !image){check(host.action("Apply display channels / Z / time").disabled,"Raw Apply remains hidden without metadata");check(host.action("Open series as a new image").disabled,"Raw Open series remains hidden without metadata");}}}
            if(tool==3)for(auto tab:{GuiInputTarget::RawPixelFilesTab,GuiInputTarget::RawPixelRecordsTab,GuiInputTarget::RawPixelContoursTab,GuiInputTarget::RawPixelTrainingTab}){host.click(raw_pixel.bounds(tab),draw);inspect(host.trace,std::string(names[tool])+" / "+state);}
        };
        for(int locale:{0,2}) {
            ui::i18n::choose(locale);ImGui::GetStyle()=style;if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
            image=false;selection.clear();host.settle(draw);
            // Stable title IDs survive the language change.
            auto* pane=ImGui::FindWindowByID(window_id);check(pane,"Reasons tool window missing");ImGui::SetWindowPos(pane,{24,24});ImGui::SetWindowSize(pane,locale==2?ImVec2{1550,1500}:ImVec2{1100,1500});ImGui::SetScrollY(pane,0);host.settle(draw);
            inspect(host.trace,std::string(names[tool])+" no image");
            if(tool==0)check(host.action("Keep training record in project").disabled,"Object Keep training remains hidden");
            if(tool==1){check(host.action("Keep training cohort").disabled,"Pixel Keep training remains hidden");check(!host.trace.actions.count("Close kept mask###Close kept pixel mask"),"Close kept mask appears outside kept mode");}
            if(tool==2){check(host.action("Measure raw channels").disabled,"Raw Measure remains hidden without metadata");check(host.action("Export raw rectangle (.npy)").disabled,"Raw Export remains hidden without metadata");}
            if(tool==3)check(host.action("Keep raw training").disabled,"Raw Keep training remains hidden");
            if(tool==4){check(host.action("Keep fluorescence cells").disabled,"Fluorescence Keep cells remains hidden");check(host.action("Keep fluorescence run").disabled,"Fluorescence Keep run remains hidden");}
            const auto before_revision=doc->revision();
            std::vector<std::string> unavailable;for(const auto& [original,action]:host.trace.actions)if(action.disabled && action.bounds.y>=pane->InnerClipRect.Min.y && action.bounds.y+action.bounds.height<=pane->InnerClipRect.Max.y)unavailable.push_back(original);
            for(const auto& original:unavailable) {
                const auto bounds=host.action(original.c_str()).bounds;auto& io=ImGui::GetIO();io.AddMousePosEvent(float(bounds.x+bounds.width/2),float(bounds.y+bounds.height/2));host.settle(draw);
                io.AddMouseButtonEvent(0,true);host.frame(draw);io.AddMouseButtonEvent(0,false);host.frame(draw);
                check(host.action(original.c_str()).disabled && doc->revision()==before_revision,"Disabled tool click changed availability or document: "+original);
            }
            const auto primary=host.action(primaries[tool]);check(primary.disabled && primary.reason==primary.inline_reason,"Missing inline primary reason: "+std::string(names[tool]));
            tooltip(host,primaries[tool],draw);
            visit_tabs("no image");
            image=true;host.settle(draw);inspect(host.trace,std::string(names[tool])+" empty selection");visit_tabs("empty selection");
            check(ui::layout_stats.overflow_items==0,"Reasons scene overflow: "+std::string(names[tool])+" / "+ui::layout_stats.first_overflow);
            host.capture(dir/(std::string("t43-")+names[tool]+(locale==2?"-fr200.ppm":"-normal.ppm")),draw);
        }
        ui::i18n::choose(0);ImGui::GetStyle()=style;image=true;selection.replace({roi});host.settle(draw);
        auto* pane=ImGui::FindWindowByID(window_id);ImGui::SetWindowSize(pane,{1100,1500});ImGui::SetScrollY(pane,0);host.settle(draw);
        // Provide two labelled areas for the raw training job, without changing its scientific options.
        if(tool==3){const auto a=doc->add_rectangle({0,0,7,9}),b=doc->add_rectangle({8,0,7,9});doc->classify_annotation(a,"A");doc->classify_annotation(b,"B");host.settle(draw);}
        if(tool==2)host.until(draw,[&]{return raw.ready(*doc);},"Raw metadata did not become ready");
        press(host,originals[tool],draw);
        const bool running=tool==0?object.io_state()=="loading":tool==1?pixel.io_state()=="loading":tool==2?raw.analysis_state()=="running":tool==3?detail::RecordListTestAccess::busy(raw_pixel):tool==4?fluo.busy():tool==5?dark.busy():tool==6?unmix.busy():marker.busy();
        check(running,"Widget did not submit native work: "+std::string(names[tool]));
        inspect(host.trace,std::string(names[tool])+" running");
        host.until(draw,[&]{return tool==0?object.io_state()!="loading":tool==1?pixel.io_state()!="loading":tool==2?raw.analysis_state()!="running":tool==3?!detail::RecordListTestAccess::busy(raw_pixel):tool==4?!fluo.busy():tool==5?!dark.busy():tool==6?!unmix.busy():!marker.busy();},"Reasons native work did not finish");
        std::cout<<"Reasons "<<names[tool]<<": no image, empty selection, native job; two languages and primary tooltip/inline; normal/French200 overflow0 passed\n";
    }
}
inline void object_training_chain(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);Document doc(Image(32,32,std::vector<std::uint8_t>(32*32*3,128)));ObjectSelection selection(doc);
    std::vector<std::uint64_t> ids;
    for(int i=0;i<8;++i){const auto id=doc.add_rectangle({double(i*3),0,double(2+i%2),double(3+i%2)});doc.classify_annotation(id,i%2?"A":"B");ids.push_back(id);}
    selection.replace(ids);GuiOptions options;options.classifier_open=true;options.classifier_selected_only=true;options.classifier_unclassified_only=false;
    auto forest=std::make_shared<ForestParameters>();forest->trees=3;forest->max_depth=3;options.classifier_parameters=forest;
    detail::GuiObjectClassifier panel(options);
    std::function<void()> draw=[&]{panel.update(doc,selection);panel.draw(doc,selection,false,true,[](GuiInputTarget){},nullptr,{dir.u8string()});};
    host.fit("Object classifiers",1100,1500,draw);
    check(host.action("Keep training record in project").reason==ui::i18n::text("Train a model first."),"Object training reason suggests loading a model");
    press(host,"Train from labelled objects",draw);host.until(draw,[&]{return panel.trained();},"Object training chain failed");host.settle(draw);
    check(host.action("Keep training record in project").enabled && host.action("Keep training record in project").reason.empty(),"Training did not enable Object Keep training");
    press(host,"Predict object classes",draw);host.until(draw,[&]{return panel.predicted();},"Object prediction chain failed");host.settle(draw);
    check(host.action("Keep training record in project").disabled && host.action("Keep training record in project").reason==ui::i18n::text("Train again to keep its training data."),"Prediction loses training data without an accurate reason");
    press(host,"Train from labelled objects",draw);host.until(draw,[&]{return panel.trained();},"Object retraining chain failed");host.settle(draw);
    check(host.action("Keep training record in project").enabled,"Retraining did not enable Keep training");
    std::cout<<"Object Keep training reason chain passed\n";
}
inline void help(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);const auto style=ImGui::GetStyle();
    for(int locale:{0,2}) {
        ui::i18n::choose(locale);ImGui::GetStyle()=style;if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        std::map<ui::CommandId,int> seen;std::set<std::string> chords,captions;
        for(std::size_t scope=0;scope<3;++scope)for(const auto& line:detail::help_detail::shortcut_lines()[scope]) {
            const auto& row=ui::command(line.command);++seen[row.id];check(row.chord && row.chord_owner==ui::CommandId::Count && static_cast<std::size_t>(row.scope)==scope,"Help lists an alias, chordless row or wrong scope");
            check(line.text==ui::chord_text(row.chord)+": "+detail::help_detail::shortcut_caption(row),"Help line differs from table caption/chord");check(chords.insert(ui::chord_text(row.chord)).second,"Help lists a chord twice");
            check(captions.insert(detail::help_detail::shortcut_caption(row)).second || row.id==ui::CommandId::Key_RedoAlternate,"Help has an ambiguous caption");
        }
        for(const auto& row:ui::command_table)check(seen[row.id]==(row.chord?1:0),"Help omitted a chord or lists a chordless row");
        check(seen[ui::CommandId::Header_Redo]==1 && seen[ui::CommandId::Key_RedoAlternate]==1,"Help omitted a Redo chord");
        detail::GuiHelpWindow panel;bool open=false;panel.request(detail::HelpPage::QuickReference);std::function<void()> draw=[&]{panel.draw(*build_environment());open=panel.is_open();};host.settle(draw);
        auto* pane=ImGui::FindWindowByName(ui::dialog_definition(ui::DialogKind::Help).name);check(pane && open,"Help escaped registered dialog");
        host.capture(dir/(locale==2?"t43-help-fr200.ppm":"t43-help-normal.ppm"),draw);
        const auto rendered=model_safety_checks::logged(host,draw);
        for(const auto& group:detail::help_detail::shortcut_lines())for(const auto& line:group)check(rendered.find(line.text)!=std::string::npos,"Generated Help line was not rendered");
        bool scroll=false;for(auto* child:host.context->Windows)if(child->Active && child->RootWindow==pane && child->ScrollMax.y>0){scroll=true;ImGui::SetScrollY(child,child->ScrollMax.y*.5f);}
        check(scroll,"Complete Help body does not scroll");host.capture(dir/(locale==2?"t43-help-fr200-middle.ppm":"t43-help-normal-middle.ppm"),draw);
        for(auto* child:host.context->Windows)if(child->Active && child->RootWindow==pane && child->ScrollMax.y>0)ImGui::SetScrollY(child,child->ScrollMax.y);
        host.capture(dir/(locale==2?"t43-help-fr200-bottom.ppm":"t43-help-normal-bottom.ppm"),draw);
        // Keep overlapping viewport captures as well as the named overview
        // positions: long translated references can need more than three pages.
        for(auto* child:host.context->Windows)if(child->Active && child->RootWindow==pane && child->ScrollMax.y>0) {
            const float step=std::max(1.f,child->InnerClipRect.GetHeight()*.8f),last=child->ScrollMax.y;int page=0;
            for(float y=0;;y=std::min(last,y+step)) {
                ImGui::SetScrollY(child,y);host.capture(dir/(std::string("t43-help-")+(locale==2?"fr200":"normal")+"-page-"+std::to_string(page++)+".ppm"),draw);
                if(y==last)break;
            }
            std::cout<<"Help continuous viewport captures: "<<page<<"\n";
        }
        std::cout<<"Help "<<locale<<": "<<chords.size()<<" owning chords exactly once; registered/scrolling/overflow0 passed\n";
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);host.frame(draw);ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);host.settle(draw);
    }
}
inline void empty_tables(const std::filesystem::path& dir) {
    auto project=fixture(dir/"t43-empty-tables.qpxproj",32,32);auto doc=project->document(1);
    CellTableOptions table_options;const CellTableRequest request(*doc,{},table_options);check(request.row_count()==0,"Empty all-kept cell request unexpectedly has rows");
    const auto csv=cell_measurements_csv(request);check(csv.find('\n')==csv.size()-1,"Empty all-kept cell export is not header-only");
    auto options=::options(dir,"t43-empty-tables","workflow","tables");dispatch_checks::TraceScope observed;bool seen=false;int stage=0;
    observed.trace.after_frame=[&](const auto&,const auto&){
        auto* body=dispatch_checks::task_body();if(!body)return;
        if(stage==0){ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr("Scope",0,body->ID)));stage=1;return;}
        if(stage==1){auto* popup=ImGui::FindWindowByName("##Combo_00");if(popup && popup->Active){const int index=2;const auto seed=ImHashData(&index,sizeof(index),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text("Current image (all kept runs)"),0,seed));stage=2;}return;}
        const auto found=observed.workflow.trace.actions.find("Export all kept image cells");if(found==observed.workflow.trace.actions.end())return;
        const auto& action=found->second;check(action.disabled && action.reason==ui::i18n::text("Keep a cell run before exporting.") && action.inline_reason==action.reason,"Empty all-kept Export has no matching disabled/inline reason");seen=true;observed.trace.end_session=true;
    };
    const auto report=run_project_gui(project,1000,"",options);check(seen && report.cell_table_state=="idle" && doc->cell_record_count()==0,"Empty all-kept Export started work");
}

inline void table_keys(const std::filesystem::path& dir) {
    using Id=ui::CommandId;std::size_t pairs=0;
    for(const auto command:{Id::Action_Export_image_cells_Ctrl_Alt_X,Id::Action_Export_project_cells_Ctrl_Shift_X,Id::Action_Export_image_Ctrl_Alt_Q,Id::Action_Export_project_Ctrl_Shift_Q})for(bool image:{false,true}) {
        const bool cells=command==Id::Action_Export_image_cells_Ctrl_Alt_X || command==Id::Action_Export_project_cells_Ctrl_Shift_X;
        const bool project_scope=command==Id::Action_Export_project_cells_Ctrl_Shift_X || command==Id::Action_Export_project_Ctrl_Shift_Q;
        const auto name="t43-table-pair-"+std::to_string(static_cast<unsigned>(command))+(image?"-image":"-empty");
        auto project=std::make_shared<Project>();
        if(image){const auto source=dir/(name+"-source.ppm");write(source,"P6\n32 32\n255\n"+std::string(32*32*3,char(80)));project->add_image(source.u8string());auto doc=project->document(1);const auto roi=doc->add_rectangle({0,0,16,16});const AnalysisRequest request(*doc,roi,StainTransform({{1,0,0},{0,1,0}}),AnalysisKind::OpticalDensityMeans);doc->keep_analysis(request,analyze(request));}
        using Outcome=std::tuple<bool,std::string,std::uint64_t,std::string>;std::optional<Outcome> reference;
        for(bool keyboard:{false,true}) {
            const auto stem=name+(keyboard?"-key":"-button");const auto path=dir/(stem+".csv");auto options=::options(dir,stem,"workflow","tables");options.width=1440;options.height=1200;options.settle_tiles=false;options.cell_table_path=options.measurement_table_path=path.u8string();
            dispatch_checks::TraceScope observed;int stage=0,before_task=-1;bool available=false,done=false,activate_button=false,timed_out=false;std::string last_text;std::optional<ImGuiKeyChord> pending,down;
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            observed.workflow.trace.before_frame=[&](int){if(std::chrono::steady_clock::now()>=deadline){timed_out=true;observed.trace.end_session=true;return;}if(down){dispatch_checks::physical(*down,false);down.reset();}if(pending){dispatch_checks::point_at_canvas();dispatch_checks::physical(*pending,true);down=pending;pending.reset();}};
            observed.trace.before_context=[&](int){if(activate_button){dispatch_checks::activate(dispatch_checks::task_body(),ui::command_label(command));activate_button=false;}ImGui::LogToBuffer();ImGui::GetCurrentContext()->LogWindow=nullptr;};
            observed.trace.after_frame=[&](const auto& context,const auto&){
                last_text=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();if(timed_out)return;
                auto* body=dispatch_checks::task_body();if(!body)return;
                const auto combo=[&](const char* original){ImGui::ActivateItemByID(ImHashStr("##value",0,ImHashStr(original,0,body->ID)));};
                const auto choose=[&](int index,const char* caption){auto* popup=ImGui::FindWindowByName("##Combo_00");if(!popup || !popup->Active)return false;const auto seed=ImHashData(&index,sizeof(index),popup->ID);ImGui::ActivateItemByID(ImHashStr(ui::i18n::text(caption),0,seed));return true;};
                if(stage==0){combo("Table");stage=1;return;}
                if(stage==1){if(choose(cells?0:1,cells?"Cells":"ROI channel means"))stage=2;return;}
                if(stage==2){combo("Scope");stage=3;return;}
                if(stage==3){if(choose(project_scope?1:0,project_scope?"Project":"Current image"))stage=4;return;}
                if(stage==5 && !pending && !down && !activate_button){if(available && !std::filesystem::exists(path))return;if(!available)check(observed.trace.task==before_task,"Disabled table chord changed task: "+stem);done=true;observed.trace.end_session=true;stage=6;return;}
                const auto found=observed.workflow.trace.actions.find(ui::command(command).legacy_id);if(found==observed.workflow.trace.actions.end())return;
                if(stage==4){const auto& a=found->second;if(a.bounds.y<body->InnerClipRect.Min.y || a.bounds.y+a.bounds.height>body->InnerClipRect.Max.y){ImGui::SetScrollY(body,float(a.bounds.y-body->Pos.y+body->Scroll.y-body->Size.y*.5));return;}dispatch_checks::point_at_canvas();if(context.dialog_open || context.text_input)return;available=!a.disabled;before_task=observed.trace.task;check(available==(project_scope || image && !cells),"Table pair availability changed: "+stem);if(a.disabled)check(!a.reason.empty() && a.reason==a.inline_reason,"Disabled table has no matching reason: "+stem);else check(a.enabled,"Available Table button is clipped: "+stem);if(keyboard)pending=ui::command(command).chord;else activate_button=true;stage=5;return;}
            };
            const auto report=run_project_gui(project,100000,"",options);check(done,"Table pair did not finish: "+stem+" stage "+std::to_string(stage)+" / cells "+report.cell_table_state+" "+report.cell_table_error+" / measurements "+report.measurement_table_state+" "+report.measurement_table_error+"\n"+last_text);
            const auto state=cells?report.cell_table_state:report.measurement_table_state,bytes=std::filesystem::exists(path)?read(path):std::string{};
            const auto rows=cells?report.cell_table_rows:report.measurement_table_rows;
            check(state==(available?"succeeded":"idle") && (!available || !bytes.empty()),"Table pair did not preserve export behavior: "+stem+" / "+state);
            if(available && cells)check(std::filesystem::equivalent(std::filesystem::u8path(report.cell_table_path),path),"Table native output identifies another file");
            const Outcome actual{available,state,rows,bytes};if(!keyboard)reference=actual;else{check(reference && *reference==actual,"Table physical chord differs from button: "+stem);++pairs;}
        }
    }
    std::cout<<"Tables key/button pairs: "<<pairs<<"; no image and empty selection; exact CSV bytes/rows and native states equal\n";
}
inline void task_keys(const std::filesystem::path& dir,bool estimates_only=false) {
    using Id=ui::CommandId;
    const std::array<std::vector<Id>,3> commands{{
        {Id::Action_Analyze_region_Ctrl_M,Id::Action_Keep_result_Ctrl_K,Id::Action_Recheck_saved_Ctrl_Shift_K,Id::Action_Estimate_stains_F3,Id::Action_Use_estimated_stains_Shift_F3},
        {Id::Action_Detect_tissue_Ctrl_D,Id::Action_Keep_tissue_mask_Ctrl_U,Id::Action_Recheck_saved_mask_Ctrl_Shift_U,Id::Action_Trace_contours_Ctrl_J,Id::Action_Add_annotations_Ctrl_Shift_J},
        {Id::Action_Detect_cells_Ctrl_G,Id::Action_Keep_cell_run_Ctrl_Shift_C,Id::Action_Recheck_cells_Ctrl_Alt_C,Id::Action_Add_cells_Ctrl_Shift_G,Id::Action_Export_cell_table_Ctrl_Alt_X}}};
    std::size_t pairs=0,disabled=0,enabled=0,absent=0;
    for(int family=0;family<3;++family)for(const std::string situation:{"no-result","running","preview","kept","stale","cancelled","failed","recheck","live-kept","contours"})for(const auto command:commands[family]) {
        if(situation=="contours" && (family!=1 || command!=Id::Action_Add_annotations_Ctrl_Shift_J))continue;
        if(situation=="live-kept" && command!=Id::Action_Export_cell_table_Ctrl_Alt_X)continue;
        const bool estimate_command=command==Id::Action_Estimate_stains_F3 || command==Id::Action_Use_estimated_stains_Shift_F3;
        if(estimates_only && !estimate_command)continue;
        using Outcome=std::tuple<bool,bool,std::uint64_t,std::size_t,std::size_t,std::uint64_t,std::string,std::vector<double>,std::uint64_t,std::size_t,std::uint64_t,std::size_t,std::size_t,std::string,bool,std::string,std::uint64_t,std::string>;
        std::optional<Outcome> reference;
        for(bool keyboard:{false,true}) {
            const auto stem="t43-pair-"+std::to_string(family)+"-"+situation+"-"+std::to_string(static_cast<unsigned>(command))+(keyboard?"-key":"-button");
            auto server=std::make_shared<analysis_tasks_checks::Server>();
            std::vector<std::uint8_t> rgb(128*96*3,255);for(int y=0;y<96;++y)for(int x=0;x<128;++x)if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100)for(int c=0;c<3;++c)rgb[(y*128+x)*3+c]=0;
            if(estimate_command){const StainTransform truth({{.65,.70,.29},{.07,.99,.11}});for(int y=0;y<96;++y)for(int x=0;x<128;++x)for(int c=0;c<3;++c){const auto a=x%8==0?0.:.1+.6*x/127,b=x%8==1?0.:.1+.7*y/95;rgb[(y*128+x)*3+c]=std::uint8_t(std::round(255*std::pow(10.,-a*truth.stains()[0][c]-b*truth.stains()[1][c])));}}
            const auto source=dir/(stem+".ppm");write(source,"P6\n128 96\n255\n"+std::string(reinterpret_cast<const char*>(rgb.data()),rgb.size()));
            server->inner=dense_image_server(Image(128,96,std::move(rgb)));
            auto doc=std::make_shared<Document>(server);const auto roi=doc->add_rectangle({8,8,44,60});auto project=std::make_shared<Project>();project->add_document(doc,"Reasons pair",source.u8string());
            auto options=::options(dir,stem,"workflow",family==0?"region":family==1?"tissue":"cells");options.width=1440;options.height=1200;options.settle_tiles=false;options.inputs={key(2,GuiInputKind::SelectAllKey)};
            if(estimate_command)options.analysis_kind=1;
            options.cell_table_published_objects=situation=="live-kept";options.cell_table_path=(dir/(stem+".csv")).u8string();
            CellParameters cells;cells.sigma=0;options.cell_parameters=std::make_shared<const CellParameters>(cells);
            dispatch_checks::TraceScope observed;observed.trace.observe_task_jobs=true;
            struct Release {analysis_tasks_checks::Server& server;~Release(){server.hold=false;}} release{*server};
            int stage=0;std::optional<ImGuiKeyChord> down,pending;std::optional<Id> pending_command;std::optional<std::string> prepare_button,activate_button;bool await_acceptance=false,selection_ready=false;
            std::uint64_t prepared_generation=0,before_generation=0,before_revision=0;std::size_t before_records=0,before_objects=0;bool present=false,available=false,done=false;
            const auto count=[&]{return family==0?doc->analysis_record_count():family==1?doc->tissue_record_count():doc->cell_record_count();};
            const auto requested=[&](Id id,bool acknowledge=false) {const auto& row=ui::command(id);pending_command=row.chord_owner==Id::Count?id:row.chord_owner;pending=ui::command(*pending_command).chord;await_acceptance=acknowledge;};
            const auto button=[&](const char* original) {const auto found=observed.workflow.trace.actions.find(original);check(found!=observed.workflow.trace.actions.end() && !found->second.disabled,"Pair preparation action unavailable: "+stem+" / "+original);prepare_button=original;};
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
            observed.workflow.trace.before_frame=[&](int){
                check(std::chrono::steady_clock::now()<deadline,"State-driven key/button pair timed out: "+stem+" stage "+std::to_string(stage));
                if(down){dispatch_checks::physical(*down,false);down.reset();return;}
                if(pending){dispatch_checks::point_at_canvas();dispatch_checks::physical(*pending,true);down=pending;if(!await_acceptance){pending.reset();pending_command.reset();}}
            };
            observed.trace.before_context=[&](int){if(activate_button){dispatch_checks::activate(dispatch_checks::task_body(),ui::command_label(ui::workflow_command(activate_button->c_str()).id));activate_button.reset();}};
            observed.trace.after_frame=[&](const auto& context,const auto& fired) {
                if(fired.test(static_cast<std::size_t>(Id::Key_SelectAll)))selection_ready=true;
                if(pending_command && fired.test(static_cast<std::size_t>(*pending_command))){pending.reset();pending_command.reset();}
                const auto& job=observed.trace.task_jobs[family];
                if(prepare_button) {
                    const auto action=observed.workflow.trace.actions.find(*prepare_button);
                    check(action!=observed.workflow.trace.actions.end(),"Preparation control disappeared: "+stem);
                    const auto& bounds=action->second.bounds;auto* pane=dispatch_checks::task_body();
                    if(pane && (bounds.y<pane->InnerClipRect.Min.y || bounds.y+bounds.height>pane->InnerClipRect.Max.y)) {
                        ImGui::SetScrollY(pane,float(bounds.y-pane->Pos.y+pane->Scroll.y-pane->Size.y*.5));return;
                    }
                    if(action->second.disabled || !action->second.enabled || context.dialog_open)return;
                    activate_button=*prepare_button;prepare_button.reset();return;
                }
                const auto run=ui::command(commands[family][0]).legacy_id,keep=ui::command(commands[family][1]).legacy_id,recheck=ui::command(commands[family][2]).legacy_id;
                auto found=observed.workflow.trace.actions.find(run);if(found==observed.workflow.trace.actions.end())return;
                ui::WorkflowTrace task_trace;for(const auto id:commands[family]) {
                    const auto item=observed.workflow.trace.actions.find(ui::command(id).legacy_id);
                    if(item!=observed.workflow.trace.actions.end())task_trace.actions.insert(*item);
                }
                inspect(task_trace,stem);
                if(stage==0) {
                    if(!selection_ready)return;
                    if(situation=="no-result"){stage=10;return;}
                    if(situation=="running" || situation=="cancelled"){server->hold=true;server->entered=false;}
                    if(situation=="failed")server->fail=true;
                    if(found->second.disabled || !found->second.enabled || context.dialog_open || context.text_input || context.control_active)return;
                    requested(commands[family][0],true);stage=1;return;
                }
                if(stage==1) {
                    if(situation=="running" || situation=="cancelled") {
                        if(!job.busy || !server->entered)return;
                        if(situation=="running"){stage=10;return;}
                        button(family==0?"Cancel analysis":family==1?"Cancel tissue":"Cancel cells (Ctrl+Alt+G)");stage=2;return;
                    }
                    if(situation=="failed"){if(job.state=="failed" && !job.busy)stage=10;return;}
                    if(job.state!="ready" || job.busy)return;
                    if(situation=="preview"){stage=10;return;}
                    if(situation=="contours") {
                        const auto add=observed.workflow.trace.actions.find(ui::command(command).legacy_id);
                        if(add==observed.workflow.trace.actions.end())return;
                        check(add->second.disabled && add->second.reason==ui::i18n::text("Trace contours"),"Tissue Add did not name its next step");
                        requested(Id::Action_Trace_contours_Ctrl_J,true);stage=6;return;
                    }
                    const auto kept=observed.workflow.trace.actions.find(keep);if(kept==observed.workflow.trace.actions.end() || kept->second.disabled)return;
                    button(keep);stage=3;return;
                }
                if(stage==2){if(activate_button)return;server->hold=false;if(!job.busy && job.state=="cancelled")stage=10;return;}
                if(stage==3) {
                    if(activate_button || count()!=1)return;
                    if(situation=="kept" || situation=="live-kept"){stage=10;return;}
                    if(situation=="stale"){doc->update_rectangle(roi,{8,8,40,56});stage=4;return;}
                    prepared_generation=job.generation;button(recheck);stage=5;return;
                }
                if(stage==4){if(job.state=="stale")stage=10;return;}
                if(stage==5){if(!activate_button && job.generation!=prepared_generation && job.state=="ready" && !job.busy)stage=10;return;}
                if(stage==6){const auto add=observed.workflow.trace.actions.find(ui::command(command).legacy_id);if(!pending && !job.busy && add!=observed.workflow.trace.actions.end() && !add->second.disabled){check(add->second.reason.empty(),"Tissue Add retained its old reason after tracing");stage=10;}return;}
                if(stage==10) {
                    const auto action=observed.workflow.trace.actions.find(ui::command(command).legacy_id);
                    present=action!=observed.workflow.trace.actions.end();available=present && !action->second.disabled;
                    if(present) {
                        const auto& a=action->second;auto* pane=dispatch_checks::task_body();
                        if(pane && (a.bounds.y<pane->InnerClipRect.Min.y || a.bounds.y+a.bounds.height>pane->InnerClipRect.Max.y)) {
                            ImGui::SetScrollY(pane,float(a.bounds.y-pane->Pos.y+pane->Scroll.y-pane->Size.y*.5));return;
                        }
                    }
                    before_generation=job.generation;before_revision=doc->revision();before_records=count();before_objects=doc->object_count();
                    if(keyboard)requested(command);else if(present)activate_button=ui::command(command).legacy_id;
                    stage=11;return;
                }
                if(stage==11) {
                    if(pending || down || activate_button)return;
                    if(available && job.busy && !estimate_command)return;
                    if(!available)check(doc->revision()==before_revision && count()==before_records && doc->object_count()==before_objects && job.generation==before_generation,"Disabled/absent action changed native work: "+stem);
                    server->hold=false;stage=12;return;
                }
                if(stage==12 && !job.busy){done=true;observed.trace.end_session=true;stage=13;}
            };
            const auto report=run_project_gui(project,100000,"",options);check(done,"Key/button pair did not finish: "+stem);
            Outcome actual{present,available,doc->revision(),count(),doc->object_count(),observed.trace.task_jobs[family].generation,
                family==0?report.analysis_state:family==1?report.tissue_state:report.cell_state,report.analysis_means,report.analysis_samples,report.tissue_components,report.tissue_foreground_pixels,report.tissue_contour_vertices,report.cell_result?report.cell_result->features().size():0,report.stain_estimate_state,report.stain_estimate_applied,report.cell_table_state,report.cell_table_rows,std::filesystem::exists(std::filesystem::u8path(options.cell_table_path))?read(std::filesystem::u8path(options.cell_table_path)):std::string{}};
            if(!keyboard)reference=actual;else {
                if(reference && actual!=*reference){const auto dump=[&](const char* name,const Outcome& value){std::cerr<<name<<": present="<<std::get<0>(value)<<" available="<<std::get<1>(value)<<" revision="<<std::get<2>(value)<<" records="<<std::get<3>(value)<<" objects="<<std::get<4>(value)<<" generation="<<std::get<5>(value)<<" state="<<std::get<6>(value)<<" means=";for(auto mean:std::get<7>(value))std::cerr<<std::setprecision(17)<<mean<<",";std::cerr<<" samples="<<std::get<8>(value)<<" tissue="<<std::get<9>(value)<<","<<std::get<10>(value)<<","<<std::get<11>(value)<<" cells="<<std::get<12>(value)<<" estimate="<<std::get<13>(value)<<" applied="<<std::get<14>(value)<<"\n";};dump("button",*reference);dump("key",actual);}
                if(reference && actual!=*reference)std::cerr<<"Cell export: button "<<std::get<15>(*reference)<<"/"<<std::get<16>(*reference)<<"/"<<std::get<17>(*reference).size()<<" bytes; key "<<std::get<15>(actual)<<"/"<<std::get<16>(actual)<<"/"<<std::get<17>(actual).size()<<" bytes\n";
                check(reference && actual==*reference,"Physical chord differs from its rendered button: "+stem);
                ++pairs;if(!present)++absent;else if(available)++enabled;else ++disabled;
            }
        }
    }
    std::cout<<"State-driven task key/button pairs: "<<pairs<<"; enabled "<<enabled<<", disabled "<<disabled<<", presenter-absent "<<absent<<"; native effects/generation/records/counts equal\n";
}
inline void run(const std::filesystem::path& out,const std::string& part="all") {
    const auto dir=std::filesystem::absolute(out);std::filesystem::create_directories(dir);
    check(glfwInit()!=0,"Reasons GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Reasons and Help",nullptr,nullptr);check(window,"Reasons window");glfwMakeContextCurrent(window);
    if(part=="all" || part=="tools"){catalogue_reasons(window);tools(dir,window);object_training_chain(dir,window);}
    if(part=="all" || part=="help")help(dir,window);
    glfwDestroyWindow(window);glfwTerminate();
    if(part=="all" || part=="tasks"){empty_tables(dir);table_keys(dir);task_keys(dir);}
    if(part=="tables")table_keys(dir);
    if(part=="estimate")task_keys(dir,true);
}
}
