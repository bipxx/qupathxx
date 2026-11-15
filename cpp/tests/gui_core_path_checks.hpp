#pragma once
#include "../gui_project_forms.hpp"
#include "../gui_core_paths.hpp"
namespace core_path_checks {
using Access=qpx::ui::PathFieldTestAccess;
struct TraceScope {
    detail::GuiProjectFormsTrace trace;detail::GuiProjectFormsTrace* previous=detail::project_forms_trace;
    TraceScope(){detail::project_forms_trace=&trace;}
    ~TraceScope(){detail::project_forms_trace=previous;}
};
inline ui::PathField& find(const std::vector<ui::PathField*>& fields,const char* label) {
    for(auto* field:fields)if(std::string(Access::spec(*field).label)==label)return *field;
    throw std::runtime_error(std::string("Missing path field: ")+label);
}
inline void controls(const std::filesystem::path& directory) {
    namespace fs=std::filesystem;const auto dir=fs::absolute(directory).lexically_normal();
    const auto project_dir=dir/"core-path-project";fs::create_directories(project_dir);
    GuiOptions opts;opts.project_open_path="relative.qpxproj";opts.project_image_path="relative.ppm";opts.interchange_path="relative.geojson";opts.measurement_table_path="relative.csv";
    auto forms=std::make_unique<detail::GuiProjectForms>(opts);auto paths=std::make_unique<detail::GuiCorePaths>(opts);
    std::string last;ui::PathContext context{project_dir.u8string(),&last};
    auto fields=forms->paths();for(auto* field:paths->paths())fields.push_back(field);
    for(auto* field:{&forms->open_path,&forms->image,&paths->geojson,&paths->object_csv,&paths->import_source,&paths->roi,&paths->tissue}) {
        field->sync(context);check(field->state().resolved==(fs::current_path()/fs::u8path(field->text())).lexically_normal().u8string(),"Relative option no longer resolves from process directory");
    }
    check(std::string(forms->open_path.text())==opts.project_open_path && std::string(forms->image.text())==opts.project_image_path && std::string(paths->geojson.text())==opts.interchange_path && std::string(paths->roi.text())==opts.measurement_table_path,"Scripted option prefill changed text");
    paths->geojson.drop("user.geojson");check(paths->geojson.state().resolved==(project_dir/"user.geojson").u8string(),"Drop did not switch from option to project-relative origin");
    const auto good1=dir/"drop-one.ppm",bad=dir/"drop-bad.ppm",good2=dir/"drop-two.ppm";
    write(good1,"P6\n2 2\n255\n"+std::string(12,'x'));write(bad,"not a readable image");write(good2,"P6\n2 2\n255\n"+std::string(12,'y'));
    const std::vector<std::string> dropped{good1.u8string(),bad.u8string(),good2.u8string()};Project project;
    for(int locale:{0,2}) {
        dialog_contract_checks::Fixture host;ui::i18n::choose(locale);auto& io=ImGui::GetIO();io.DisplaySize={1440,1100};
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        bool open_requested=false;int reader=0;
        auto frame=[&](bool draw_all=false) {
            ImGui::NewFrame();ui::begin_frame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({620,1000});ImGui::Begin("Project forms");
            ImGui::LogToBuffer();
            if(draw_all)for(auto* field:fields) {
                const auto expected=ImHashStr("##value",0,ImHashStr(Access::spec(*field).label,0,ImGui::GetCurrentWindow()->IDStack.back()));
                ui::SetNextItemWidth(-1);field->draw(context,false,[&]{check(ImGui::GetItemID()==expected,"Path replay target left the exact original input ID");});
            }else {
                forms->draw_save(context,false,[]{});open_requested=forms->draw_open(context,false,false,false,nullptr,host.owner).open;
                forms->draw_image(context,false,true,{},reader,std::string("Auto\0PPM\0\0",10));
            }
            const std::string text=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();
            check(ui::layout_stats.overflow_items==0,"Core paths/forms overflow at normal/French 200%: "+ui::layout_stats.first_overflow);
            ImGui::End();ImGui::Render();return text;
        };
        forms->select(detail::ProjectForm::Save);frame();frame();forms->select(detail::ProjectForm::Open);frame();forms->select(detail::ProjectForm::Relink);frame();forms->select(detail::ProjectForm::Add);frame();frame();
        const auto position=Access::center(forms->image);
        check(detail::route_path_drop({dropped,position.x,position.y},*forms,fields,false),"Drop on field was not routed");
        check(std::string(forms->image.text())==dropped.front() && !forms->adding(),"Drop on field started Add or did not take first path");
        check(frame().find(ui::i18n::text("Using the first dropped path."))!=std::string::npos,"Multiple-path field drop lacks notice");
        check(!detail::route_path_drop({dropped,-100,-100},*forms,fields,true,123),"Modal accepted an outside drop");
        check(!forms->adding(),"Modal drop changed queue");
        check(!detail::route_path_drop({dropped,position.x,position.y},*forms,fields,true),"Load/save guard accepted a background field drop");
        const auto total=forms->total();const auto open=(project_dir/"selected.qpxproj").u8string();
        check(detail::route_path_drop({{open},-100,-100},*forms,fields,false) && forms->shown==detail::ProjectForm::Open && std::string(forms->open_path.text())==open && forms->total()==total,"Project drop did not only reveal/fill Open");
        frame();check(!open_requested,"Project drop opened without a user command");
        if(locale==0) {
            check(detail::route_path_drop({dropped,-100,-100},*forms,fields,false) && forms->total()==3,"Three dropped images did not queue three adds");
            for(std::size_t i=0;i<3;++i) {
                const auto text=frame();char expected[80];std::snprintf(expected,sizeof(expected),"Adding %zu of %zu",i+1,std::size_t(3));check(text.find(expected)!=std::string::npos,"Add progress missing");
                const auto before=forms->completed();forms->process_one([&](const std::string& path){project.add_image(path,"ppm");return true;});check(forms->completed()==before+1,"Add queue processed more than one file per frame");
            }
            check(project.images().size()==2 && forms->failures().size()==1 && forms->failures()[0].first=="drop-bad.ppm" && !forms->failures()[0].second.empty(),"Queue lost successful files or per-file error");
            check(frame().find("drop-bad.ppm")!=std::string::npos,"Queue error is hidden");
        }
        frame(true);
    }
    ui::i18n::choose(0);std::cout<<"Core path option origins/IDs/forms/drop hit testing/modal guard/three-file queue/progress/per-file failures passed\n";
}
inline void picker_action(const char* label) {
    auto* window=ImGui::FindWindowByName("Choose import file");check(window!=nullptr,"New-file picker not visible");ImGui::ActivateItemByID(ImHashStr(label,0,window->ID));
}
inline void workflows(const std::filesystem::path& directory,const std::string& phase="all",int family_filter=-1) {
    namespace fs=std::filesystem;const auto dir=fs::absolute(directory).lexically_normal();
    if(phase=="all" || phase=="drop") {
        {
            Document single(Image(64,48,std::vector<std::uint8_t>(64*48*3,80)));
            single.add_rectangle({4,4,20,20});
            auto standalone=options(dir,"standalone-drop","annotations");standalone.inputs={key(8,GuiInputKind::UndoKey)};
            TraceScope drop_trace;workflow_safety_checks::FeedbackTrace feedback;int frame=0;bool refused=false;
            ui::NotificationsTrace notices;const auto old_notices=ui::notifications_trace;
            struct RestoreNotices {ui::NotificationsTrace* old;~RestoreNotices(){ui::notifications_trace=old;}} restore_notices{old_notices};
            ui::notifications_trace=&notices;std::size_t refusal_notices=0;bool undo_quiet=false;
            notices.after_frame=[&](ui::Notifications& value){
                if(frame==7){const auto* current=value.current();refused=current && current->notice.severity==ui::Severity::Warning && current->notice.source=="Project" && current->notice.message_key && current->notice.message=="Open a project to add images.";}
                refusal_notices=0;for(std::size_t i=0;i<value.size();++i)if(value.newest(i).notice.source=="Project" && value.newest(i).notice.message=="Open a project to add images.")++refusal_notices;
                if(const auto* current=value.current())undo_quiet=current->notice.severity==ui::Severity::Warning && current->notice.source=="Project" && value.posted()==1;
            };
            drop_trace.trace.before_frame=[&](auto&,const auto&,auto& drops) {
                ++frame;if(frame==3)drops.push_back({{"unused.ppm"},-100,-100});
                if(frame==12)feedback.text.clear();
            };
            run_gui(single,16,"",standalone);
            check(refused && single.object_count()==0,"Standalone refusal or subsequent successful Undo not exercised");
            check(refusal_notices==1 && undo_quiet,"Drop warning history changed or successful Undo posted a notice");
            check(feedback.text.find("Open a project to add images.")==std::string::npos && feedback.text.find("64 x 48 px")!=std::string::npos,"Successful action did not restore standalone image status");
            std::cout<<"Standalone drop warning retained after quiet Undo; normal image status preserved\n";
        }
        auto project=fixture(dir/"production-drop.qpxproj",128,96);auto o=options(dir,"production-drop","project");
        const auto a=dir/"production-one.ppm",b=dir/"production-bad.ppm",c=dir/"production-two.ppm";
        write(a,"P6\n2 2\n255\n"+std::string(12,'a'));write(b,"bad image");write(c,"P6\n2 2\n255\n"+std::string(12,'c'));
        TraceScope trace;int frame=0;bool completed=false,project_only=false;workflow_safety_checks::FeedbackTrace text;
        trace.trace.before_frame=[&](detail::GuiProjectForms& forms,const auto&,std::vector<detail::GuiPathDrop>& drops){
            ++frame;if(frame==3)drops.push_back({{a.u8string(),b.u8string(),c.u8string()},-100,-100});
            if(frame==10){completed=forms.completed()==3 && forms.failures().size()==1 && forms.failures()[0].first=="production-bad.ppm";drops.push_back({{(dir/"drop-open.qpxproj").u8string()},-100,-100});}
            if(frame==12)project_only=forms.shown==detail::ProjectForm::Open && forms.open_path.state().resolved==(dir/"drop-open.qpxproj").u8string();
        };
        const auto result=run_project_gui(project,18,(dir/"production-drop.ppm").u8string(),o);
        check(completed && project_only && project->images().size()==3 && result.project_replacements==0,"Production drop routing/queue opened a project or lost additions");
        check(text.text.find("Adding 1 of 3")!=std::string::npos && text.text.find("Adding 3 of 3")!=std::string::npos && text.text.find("production-bad.ppm")!=std::string::npos,"Production queue progress/error hidden");
        std::cout<<"Production drop router adds two/read-error/Open reveal without open passed\n";
    }
    if(phase=="all" || phase=="picker")for(bool choose:{false,true}) {
        const auto name=choose?"picked":"cancelled";auto project=fixture(dir/(std::string(name)+".qpxproj"),128,96);auto doc=project->document(1);doc->add_rectangle({8,8,40,40});
        auto o=options(dir,name,"annotations");o.inputs={key(5,GuiInputKind::ExportKey)};
        const auto output=dir/fs::u8path(u8"\u96ea-\u00e9.geojson");bool seen=false,activated=false;
        TraceScope trace;workflow_safety_checks::FeedbackTrace feedback;
        trace.trace.before_frame=[&](detail::GuiProjectForms&,const std::vector<ui::PathField*>& fields,std::vector<detail::GuiPathDrop>&) {
            auto& field=find(fields,"##Interchange path");
            if(!field.picker_active() || !ImGui::FindWindowByName("Choose import file"))return;
            using P=detail::GuiImportPickerTestAccess;auto& picker=Access::picker(field);
            if(!seen){check(P::mode(picker)==detail::PickerMode::NewFile && P::filename(picker)==std::string(name)+"-annotations.geojson","Empty export picker mode/suggestion differs");seen=true;}
            if(!activated && ImGui::GetFrameCount()>10) {
                if(choose)picker.open(output.u8string(),{".geojson"},detail::PickerMode::NewFile);
                picker_action(choose?"Choose new output":"Cancel file choice");activated=true;
            }
        };
        const auto result=run_project_gui(project,28,(dir/(std::string(name)+"-picker.ppm")).u8string(),o);
        check(result.replay_complete && seen && activated,"Empty export did not reach a cancellable new-file picker");
        write(dir/(std::string(name)+"-text.log"),feedback.text);
        check(choose?read(output)==export_geojson(*doc).text:!fs::exists(output),"Picker cancel wrote bytes or UTF-8 export differs: "+feedback.text.substr(feedback.text.size()>1800?feedback.text.size()-1800:0));
        check(!fs::exists(dir/(std::string(name)+"-annotations.geojson")),"Picker wrote the unchosen suggested name");
        check(result.ui_overflow_items==0,"Export picker layout overflow");
    }
    // Distinct fields feed the existing native export routes, in both directions.
    if(phase=="all" || phase=="isolation")for(int family=0;family<4;++family) {
        if(family_filter>=0 && family!=family_filter)continue;
        const auto name="isolated-"+std::to_string(family);auto project=fixture(dir/(name+".qpxproj"),64,48);auto doc=project->document(1);const auto id=doc->add_rectangle({4,4,40,30});
        StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*doc,id,transform);doc->keep_analysis(request,analyze(request));
        TissueParameters parameters;TissueRequest tissue(*doc,id,transform,parameters);doc->keep_tissue(tissue,detect_tissue(tissue));
        auto o=options(dir,name,family<2?"annotations":"workflow",family<2?"measurements":"tables");o.interchange_path=(dir/(name+"-geo.geojson")).u8string();o.measurement_table_kind=family==3?1:0;
        const auto object=dir/(name+"-objects.csv"),roi=dir/(name+"-roi.csv"),tissue_path=dir/(name+"-tissue.csv");
        bool initialized=false,activated=false;TraceScope trace;workflow_safety_checks::FeedbackTrace feedback;
        trace.trace.before_frame=[&](detail::GuiProjectForms&,const std::vector<ui::PathField*>& fields,std::vector<detail::GuiPathDrop>&) {
            if(!initialized){find(fields,"##Object CSV path").set(object.u8string());find(fields,"##Measurement destination").set(roi.u8string());find(fields,"##Tissue table destination").set(tissue_path.u8string());initialized=true;}
            if(family==1 && ImGui::GetFrameCount()>=5) {
                auto* window=ImGui::FindWindowByName(GuiWorkspace::objects_pane);check(window!=nullptr,"Annotations pane missing");
                ImGui::SetScrollY(window,window->ScrollMax.y);
                if(!activated && ImGui::GetFrameCount()>8){ImGui::ActivateItemByID(ImHashStr("Export measurements CSV",0,window->ID));activated=true;}
            }
        };
        if(family!=1)o.inputs={key(5,family==0?GuiInputKind::ExportKey:GuiInputKind::ExportMeasurementTableKey)};
        const auto result=run_project_gui(project,36,(dir/(name+".ppm")).u8string(),o);check(result.replay_complete,"Split export replay incomplete");
        write(dir/(name+"-text.log"),feedback.text);
        const fs::path geo=fs::u8path(o.interchange_path);
        if(family==0)check(read(geo)==export_geojson(*doc).text && !fs::exists(object),"GeoJSON wrote to object CSV destination: "+feedback.text.substr(feedback.text.size()>1800?feedback.text.size()-1800:0));
        if(family==1)check(read(object)==measurements_csv(*doc) && !fs::exists(geo),"Object CSV wrote to GeoJSON destination: "+feedback.text.substr(feedback.text.size()>1800?feedback.text.size()-1800:0));
        if(family>=2){const auto output=family==2?roi:tissue_path;check(result.measurement_table_state=="succeeded" && read(output)==measurements_csv(MeasurementTableRequest(*doc,family==2?MeasurementTableKind::Roi:MeasurementTableKind::Tissue)),"Split table differs from native bytes: "+result.measurement_table_error);check(!fs::exists(family==2?tissue_path:roi),"ROI/tissue shared a destination");}
        check(result.ui_overflow_items==0,"Split export form overflow: "+result.ui_overflow_label);
    }
    if(phase=="all" || phase=="layout")for(bool french:{false,true}) {
        const auto name=french?"forms-fr-200":"forms-normal";auto project=fixture(dir/(std::string(name)+".qpxproj"),128,96);auto o=options(dir,name,"project");if(french){o.width=1440;o.height=1000;o.ui_scale=2;auto ini=read(fs::u8path(o.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(fs::u8path(o.layout_path),ini);}
        int frame=0;TraceScope trace;trace.trace.before_frame=[&](detail::GuiProjectForms& forms,const auto&,auto&){++frame;if(frame==3)forms.select(detail::ProjectForm::Save);if(frame==8)forms.select(detail::ProjectForm::Open);if(frame==13)forms.select(detail::ProjectForm::Add);if(frame==18)forms.select(detail::ProjectForm::Relink);};
        const auto result=run_project_gui(project,24,(dir/(std::string(name)+".ppm")).u8string(),o);check(result.ui_overflow_items==0,"Production forms normal/French 200% overflow: "+result.ui_overflow_label);
        o=options(dir,std::string(name)+"-groups","annotations");o.interchange_path=(dir/"group-destination.geojson").u8string();if(french){o.width=1440;o.height=1000;o.ui_scale=2;auto ini=read(fs::u8path(o.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(fs::u8path(o.layout_path),ini);}
        trace.trace.before_frame={};const auto groups=run_project_gui(project,12,(dir/(std::string(name)+"-groups.ppm")).u8string(),o);check(groups.ui_overflow_items==0,"Import/export groups normal/French 200% overflow: "+groups.ui_overflow_label);
        GuiInput move;move.frame=3;move.kind=GuiInputKind::Move;move.target=GuiInputTarget::Window;move.x=.25;move.y=.55;
        auto wheel=move;wheel.frame=5;wheel.kind=GuiInputKind::Wheel;wheel.wheel=-30;o.inputs={move,wheel};
        const auto lower=run_project_gui(project,12,(dir/(std::string(name)+"-groups-lower.ppm")).u8string(),o);check(lower.ui_overflow_items==0,"Lower import/export groups normal/French 200% overflow: "+lower.ui_overflow_label);
    }
    std::cout<<"Core path phase "<<phase<<" passed\n";
}
}
