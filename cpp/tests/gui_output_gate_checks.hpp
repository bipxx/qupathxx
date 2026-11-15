#pragma once
#include "../gui_output_gate.hpp"
#include "../gui_stain_model.hpp"
#include "../gui_raw_pixel.hpp"
#include "../gui_pixel_classifier.hpp"
#include "../gui_object_classifier.hpp"
#include "../gui_spatial.hpp"
#include "../sha256.hpp"
#include "../annotation_format_registry.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/raw_pixel_training_checkpoint.hpp"
#include "qupathxx/raw_pixel_stream_file.hpp"
#include "qupathxx/stain_model_io.hpp"
#include <thread>
namespace qpx::detail {
struct OutputGateTestAccess {
    static const std::string& path(const OutputGate& gate){return gate.pending_->path;}
    static const std::string& scope(const OutputGate& gate){return gate.pending_->scope;}
    static const std::string& success(const OutputGate& gate){static const std::string empty;if(gate.notifications_)for(std::size_t i=0;i<gate.notifications_->size();++i){const auto& entry=gate.notifications_->newest(i);if(entry.notice.severity==ui::Severity::Success)return entry.notice.path;}return empty;}
    static void clear_success(OutputGate& gate){if(gate.notifications_)*gate.notifications_={};}
    static bool confirm(OutputGate& gate,const std::function<bool(const ExportOwnerStamp&)>& valid){return gate.confirm(valid);}
};
}
namespace output_gate_checks {
using namespace qpx;
inline std::string hash(const std::filesystem::path& path) {const auto bytes=read(path);Sha256 h;h.update(bytes.data(),bytes.size());return h.finish();}
struct Host: simplification_tools_checks::NativeUi {
    ui::Notifications notices;
    detail::OutputGate gate;bool valid=true;std::map<ui::DialogAction,Rect> actions;
    Host(){gate.notifications(&notices);}
    std::function<void()> panel;
    void frame() {
        ImGui::NewFrame();if(panel)panel();ui::begin_frame();ImGui::Begin("Output gate host");
        gate.draw([&](const detail::ExportOwnerStamp&){return valid;},[&](ui::DialogAction action){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();actions[action]={a.x,a.y,b.x-a.x,b.y-a.y};});
        ImGui::End();ImGui::Render();
        check(ui::layout_stats.overflow_items==0,"Output dialog overflow: "+ui::layout_stats.first_overflow);
    }
    void click(ui::DialogAction action) {
        frame();frame();const auto b=actions.at(action);check(b.width>0 && b.height>0,"Output dialog action absent");
        auto& io=ImGui::GetIO();io.AddMousePosEvent(float(b.x+b.width/2),float(b.y+b.height/2));frame();
        io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
    }
};
inline void decisions(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    for(const bool french:{false,true}) {
        Host host;if(french){ui::i18n::choose(2);ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=ui::design::metrics.font_size*2;}
        const auto path=dir/std::filesystem::u8path(french?u8"résultat-雪.csv":"gate.csv");write(path,"sentinel");const auto before=hash(path);
        std::size_t writes=0;std::string error;ExportWritePolicy used=ExportWritePolicy::CreateNew;
        auto submit=[&](bool preauthorized=false) {host.gate.submit(std::filesystem::relative(path).u8string(),"Current image / CSV",
            [&](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>&){++writes;used=policy;detail::AtomicExportFile f(output,1024);f.write("replacement",11);f.commit(policy==ExportWritePolicy::ReplaceExisting);host.gate.succeeded(output);},
            [&](const std::string& message){error=message;},preauthorized);};
        submit();check(host.gate.pending() && detail::OutputGateTestAccess::path(host.gate)==path.u8string(),"Relative/non-ASCII dialog path is not absolute");
        host.click(ui::DialogAction::Cancel);check(!host.gate.pending() && !writes && hash(path)==before,"Cancel wrote bytes");
        submit();host.click(ui::DialogAction::Confirm);check(!host.gate.pending() && writes==1 && used==ExportWritePolicy::ReplaceExisting && read(path)=="replacement","Confirm did not replace once");
        check(detail::OutputGateTestAccess::success(host.gate)==path.u8string(),"Success path is not absolute");
        write(path,"sentinel");submit(true);check(!host.gate.pending() && writes==2,"Script preauthorization asked");
        const auto fresh=dir/(french?"gate-new-fr.csv":"gate-new.csv");
        host.gate.submit(fresh.u8string(),"new",[&](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>&){check(policy==ExportWritePolicy::CreateNew,"New output used replacement");detail::AtomicExportFile f(output,64);f.write("new",3);f.commit(false);},[&](const std::string& message){error=message;});
        check(!host.gate.pending() && read(fresh)=="new","New output asked or failed");
        write(path,"sentinel");submit();host.valid=false;
        check(!detail::OutputGateTestAccess::confirm(host.gate,[&](const detail::ExportOwnerStamp&){return host.valid;}) && hash(path)==before && writes==2 && error==ui::i18n::text("Export context changed. Cancel and export again."),"Stale owner wrote");
        host.click(ui::DialogAction::Cancel);host.valid=true;error.clear();submit();write(path,"externally changed");
        check(!detail::OutputGateTestAccess::confirm(host.gate,[](const detail::ExportOwnerStamp&){return true;}) && read(path)=="externally changed" && writes==2,"Destination identity change accepted");host.click(ui::DialogAction::Cancel);
        Document changed(Image(4,4,std::vector<std::uint8_t>(4*4*3,80)));Project changed_project;
        auto stamp=[&](bool,bool){detail::ExportOwnerStamp value;value.document=&changed;value.revision=changed.revision();value.project=&changed_project;for(const auto& image:changed_project.images()){const auto member=changed_project.document(image.id);value.members.emplace_back(image.id,member.get(),member->revision(),member->calibration_revision(),member->server().get());}return value;};
        host.gate.bind(stamp,[] {return std::vector<std::string>{};});
        for(bool project_change:{false,true}) {
            error.clear();submit();const auto captured=stamp(false,false);
            if(project_change)changed_project.add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,80))));else changed.add_rectangle({0,0,1,1});
            check(!(captured==stamp(false,false)) && !detail::OutputGateTestAccess::confirm(host.gate,[&](const detail::ExportOwnerStamp& owner){return owner==stamp(false,false);}) && writes==2,"Native document/project edit bypassed owner check");host.click(ui::DialogAction::Cancel);
        }
        for(const auto& file:{"project.qpxproj","image.ppm","workspace.ini","recovery.qpxrecover","project.qpxproj-wal"}) {
            const auto protected_path=dir/file;write(protected_path,"protected");const auto digest=hash(protected_path);error.clear();
            detail::OutputIntent intent{protected_path.u8string(),"protected",{protected_path.u8string()},{},true,[&](ExportWritePolicy){++writes;},[&](const std::string& message){error=message;}};
            host.gate.submit(std::move(intent));check(!host.gate.pending() && writes==2 && !error.empty() && hash(protected_path)==digest,"Protected output started work");
        }
        const auto raced=dir/(french?"race-fr.csv":"race.csv");error.clear();
        host.gate.submit(raced.u8string(),"race",[&](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>&){check(policy==ExportWritePolicy::CreateNew,"Race changed policy");write(raced,"other writer");detail::AtomicExportFile f(output,64);f.write("bad",3);f.commit(false);},[&](const std::string& message){error=message;});
        check(!host.gate.pending() && !error.empty() && read(raced)=="other writer","CreateNew race replaced or asked");
        const auto asynchronous=dir/(french?"async-race-fr.csv":"async-race.csv");std::future<std::string> worker;std::string native_error,reported;int failures=0;bool completed=false;
        host.gate.submit(asynchronous.u8string(),"async race",[&](const std::string& output,ExportWritePolicy policy,const std::vector<std::string>&){
            check(policy==ExportWritePolicy::CreateNew,"Async race changed policy");
            worker=std::async(std::launch::async,[output]{write(std::filesystem::u8path(output),"other writer");try {detail::AtomicExportFile f(output,64);f.write("bad",3);f.commit(false);}catch(const std::exception& e){return std::string(e.what());}return std::string{};});
            host.gate.watch(output,[&]()->std::optional<bool>{if(worker.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return {};native_error=worker.get();completed=true;return native_error.empty();},[&]{return native_error;});
        },[&](const std::string& message){reported=message;++failures;});
        for(int i=0;i<1000 && !completed;++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(completed && failures==1 && !reported.empty() && reported==native_error && read(asynchronous)=="other writer" && !host.gate.pending(),"Async native CreateNew failure bypassed fail or replaced bytes");
        host.gate.submit((dir/(french?"cancelled-fr.csv":"cancelled.csv")).u8string(),"cancelled",[&](const std::string& output,ExportWritePolicy,const std::vector<std::string>&){host.gate.watch(output,[]()->std::optional<bool>{return false;},[]{return std::string{};});},[&](const std::string&){++failures;});host.frame();check(failures==1,"Cancellation was turned into a failure");
        bool launched=false;ui::shell_open_override=[&](const std::string& value){check(value==dir.u8string(),"Shell path changed");launched=true;};
        ui::open_in_shell(dir.u8string());ui::shell_open_override={};check(launched,"Shell override was bypassed");
    }
    std::cout<<"Output gate: new/Cancel/hash/Confirm/preauthorization/protection/stale owner/stale destination/race/UTF-8/relative/normal/French200/shell interception passed\n";
}
// Exercise each retained native writer independently. A failing publication is
// never counted as a pass; the writer's phase can be rerun outside the sandbox.
inline const std::vector<std::string> writer_names={"geojson","object-csv","saved-cells","image-cells","project-cells","image-roi","project-roi","image-tissue","project-tissue","fingerprint","stain","tma-metadata","qptma","tma-case","tma-cohort","raw-numpy","raw-csv","spatial","pixel-model","pixel-stream","object-model","raw-model","raw-checkpoint","raw-prediction"};
inline ForestModel forest(const std::vector<std::pair<std::string,std::string>>& schema,bool old=false) {
    auto bytes=qpx_test::object_forest_fixture(schema);
    // The training digest is independently supplied by the fixture codec.
    if(old)for(std::size_t i=0;i+64<=bytes.size();++i)if(std::all_of(bytes.begin()+i,bytes.begin()+i+64,[](auto c){return c=='a';})){bytes[i]='b';break;}
    return ForestModel::from_bytes(bytes);
}
inline void writer(const std::filesystem::path& directory,const std::string& name) {
    check(std::find(writer_names.begin(),writer_names.end(),name)!=writer_names.end(),"Unknown output writer");
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    auto doc=std::make_shared<Document>(open_ome_zarr(source.u8string()));Project project;
    const auto a=doc->add_rectangle({0,0,3,5},"A"),b=doc->add_rectangle({4,0,3,5},"B");doc->classify_annotation(a,"A");doc->classify_annotation(b,"B");
    const StainTransform transform({{1,0,0},{0,1,0}});const AnalysisRequest analysis(*doc,a,transform);doc->keep_analysis(analysis,analyze(analysis));
    const CellRequest cells(*doc,a,transform);const auto cell=doc->keep_cells(cells,detect_cells(cells));
    TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={1,2};grid.column_step={4,0};grid.row_step={0,2};grid.width=grid.height=1;if(name.rfind("tma-",0)==0 || name=="qptma")doc->create_tma_grid(grid);const auto tma=doc->tma_grid();
    auto member=std::make_shared<Document>(Image(7,5,std::vector<std::uint8_t>(7*5*3,80)));const auto member_roi=member->add_rectangle({0,0,3,5});
    const AnalysisRequest member_analysis(*member,member_roi,transform);member->keep_analysis(member_analysis,analyze(member_analysis));
    const CellRequest member_cells(*member,member_roi,transform);member->keep_cells(member_cells,detect_cells(member_cells));project.add_document(member);
    RawRegionRequest raw;raw.region={0,0,0,7,5};raw.channels=3;const RawAnalysisRequest raw_request(*doc,raw);doc->keep_raw_analysis(raw_request,analyze_raw(raw_request));
    PixelFeatureOptions features;features.sigmas={0};std::vector<std::pair<std::string,std::string>> pixel_schema;for(const auto& f:pixel_feature_schema(features,transform))pixel_schema.emplace_back(f.name,f.unit);
    PixelClassifier pixel(forest(pixel_schema),features,transform,PixelResolution{}),old_pixel(forest(pixel_schema,true),features,transform,PixelResolution{});
    const auto object=ForestModel::from_bytes(qpx_test::object_forest_fixture());auto object_schema=object.features();std::vector<std::pair<std::string,std::string>> os;for(const auto& f:object_schema)os.emplace_back(f.name,f.unit);const auto old_object=forest(os,true);
    ScientificRawLattice lattice(7,5,{0,7,7},{0,5,5});ScientificRawOptions selection;selection.channels=3;RawPixelFeatureOptions rf;rf.offsets={0,0,0};rf.scales={1,1,1};
    RawPixelRecipe recipe(*doc->server()->raw_metadata(nullptr),lattice,selection,rf);std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema())schema.emplace_back(f.name,f.unit);
    RawPixelClassifier model(forest(schema),recipe),old_model(forest(schema,true),recipe);
    const RawPixelTiledTrainingRequest training(*doc,{a,b},lattice,selection,rf);RawPixelStreamOptions stream_options;const RawPixelTiledPredictionRequest prediction(*doc,model,lattice,stream_options);
    const auto input=dir/"fingerprint-input.bin";write(input,"input bytes");
    const bool only_new=name=="tma-case" || name=="tma-cohort" || name=="spatial";
    const bool scripted=name=="pixel-model" || name=="pixel-stream" || name=="object-model" || name=="raw-model" || name=="raw-checkpoint" || name=="raw-prediction";
    auto native=[&](const std::string& path,ExportWritePolicy policy,bool old=false) {
        const bool replace=policy==ExportWritePolicy::ReplaceExisting;
        if(name=="geojson") {detail::AnnotationFileExportRequest r{"geojson",path,detail::AnnotationGeoJsonExportOptions{},policy};detail::export_annotations(*doc,r);}
        else if(name=="object-csv") {detail::AnnotationFileExportRequest r{"measurements-csv",path,detail::AnnotationMeasurementsCsvOptions{},policy};detail::export_annotations(*doc,r);}
        else if(name=="saved-cells" || name=="image-cells" || name=="project-cells") {CellTableOptions o;o.published_objects=name=="image-cells";auto request=name=="project-cells"?CellTableRequest(project,{},o):CellTableRequest(*doc,name=="saved-cells"?std::vector<std::uint64_t>{cell}:std::vector<std::uint64_t>{},o);CellTableExportJob job;job.request_with_policy(request,path,policy);const auto result=job.wait();check(result.state=="succeeded",result.error);}
        else if(name.find("-roi")!=std::string::npos || name.find("-tissue")!=std::string::npos) {const auto kind=name.find("-roi")!=std::string::npos?MeasurementTableKind::Roi:MeasurementTableKind::Tissue;const auto request=name.rfind("project-",0)==0?MeasurementTableRequest(project,kind):MeasurementTableRequest(*doc,kind);MeasurementTableExportJob job;job.request_with_policy(request,path,policy);const auto result=job.wait();check(result.state=="succeeded",result.error);}
        else if(name=="fingerprint") {FileFingerprintJob job;job.request_with_policy(input.u8string(),path,policy);const auto result=job.wait();check(result.error.empty(),result.error);}
        else if(name=="stain") {detail::GuiStainModel io;io.save(transform,path,{},policy);while(io.busy()){io.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(io.state()=="saved",io.error());}
        else if(name=="tma-metadata" || name=="qptma") {const auto bytes=name=="qptma"?qptma_text(qptma_from_grid(*tma,"","grid").data):tma_metadata_csv(*tma);detail::AtomicExportFile f(path,1024*1024);f.write(bytes.data(),bytes.size());f.commit(replace);}
        else if(name=="tma-case")save_tma_case_summary_csv(*tma,path);
        else if(name=="tma-cohort")save_tma_cohort_case_summary_csv({{1,tma}},path);
        else if(name=="raw-numpy")save_raw_numpy(doc->server()->read_raw(raw,nullptr),path,replace);
        else if(name=="raw-csv")save_raw_analysis_csv(RawTableRequest(*doc),path,replace);
        else if(name=="spatial") {detail::SpatialFrontend frontend;frontend.measure(*doc,{a},{b});frontend.finish(*doc,true);frontend.export_csv(*doc,path,65536);frontend.finish(*doc,true);check(frontend.report().export_committed,frontend.report().export_error);}
        else if(name=="pixel-model")save_pixel_classifier(old?old_pixel:pixel,path,replace);
        else if(name=="pixel-stream") {PixelPredictionStream stream(*doc,a,old?old_pixel:pixel);save_pixel_stream(stream,path,replace);}
        else if(name=="object-model")save_forest_model(old?old_object:object,path,replace);
        else if(name=="raw-model")save_raw_pixel_classifier(old?old_model:model,path,replace);
        else if(name=="raw-checkpoint") {RawPixelTrainingCheckpointOptions options;options.replace_existing=replace;options.max_new_windows=old?1:0;save_raw_pixel_training_checkpoint(training,path,options);}
        else if(name=="raw-prediction") {RawPixelStreamSaveOptions options;options.replace_existing=replace;options.max_new_windows=old?1:0;save_raw_pixel_stream(prediction,path,options);}
    };
    Host host;std::string error;std::size_t calls=0;
    auto submit=[&](const std::filesystem::path& path,bool preauthorized=false,std::vector<std::string> protected_paths={}) {
        host.gate.submit(detail::OutputIntent{path.u8string(),name,std::move(protected_paths),{},preauthorized,
            [&,path](ExportWritePolicy policy){++calls;native(path.u8string(),policy);host.gate.succeeded(path.u8string());},[&](const std::string& message){error=message;},only_new?ui::i18n::text("This file already exists and this export never replaces a file. Choose a new file name."):""});
    };
    // Negative paths complete before any native job, even on restricted hosts.
    for(const auto& file:{"project.qpxproj","image-source.bin","workspace.ini","recovery.qpxrecover"}) {const auto path=dir/file;write(path,"protected");const auto before=hash(path);error.clear();submit(path,true,{path.u8string()});check(calls==0 && !host.gate.pending() && !error.empty() && hash(path)==before,"Writer protection failed: "+name);}
    const auto existing=dir/"existing.output";
    if(only_new)write(existing,"sentinel");else if(scripted)native(existing.u8string(),ExportWritePolicy::CreateNew,true);else write(existing,"sentinel");
    const auto before=hash(existing);error.clear();submit(existing);
    if(only_new){check(!host.gate.pending() && calls==0 && error==ui::i18n::text("This file already exists and this export never replaces a file. Choose a new file name.") && hash(existing)==before,"Create-only writer started work");}
    else {check(host.gate.pending() && calls==0,"Existing writer did not ask");host.click(ui::DialogAction::Cancel);check(hash(existing)==before && calls==0,"Writer Cancel changed hash");submit(existing);host.click(ui::DialogAction::Confirm);check(!host.gate.pending() && error.empty() && calls==1 && hash(existing)!=before,"Writer Confirm failed: "+name+" / "+error);}
    const auto fresh=dir/"new.output";error.clear();const auto old_calls=calls;submit(fresh);check(!host.gate.pending() && error.empty() && calls==old_calls+1 && std::filesystem::exists(fresh),"Writer new file failed: "+name+" / "+error);
    if(scripted){const auto authorized=dir/"authorized.output";native(authorized.u8string(),ExportWritePolicy::CreateNew,true);const auto prior=hash(authorized);submit(authorized,true);check(!host.gate.pending() && error.empty() && hash(authorized)!=prior,"Scripted writer asked or failed: "+name+" / "+error);}
    std::cout<<"Native writer "<<name<<": protected/new/existing/Cancel/hash/Confirm/script policy passed (create-only exemptions explicit)\n";
}

inline void interactive(const std::filesystem::path& directory,const std::string& phase="all") {
    const bool browsing=phase.rfind("browse-",0)==0;const auto selected_phase=browsing?phase.substr(7):phase;
    const bool missing=selected_phase.rfind("missing-",0)==0;const auto group=missing?selected_phase.substr(8):selected_phase;
    std::function<std::string()> own_error;
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    auto doc=std::make_shared<Document>(open_ome_zarr(source.u8string()));Project project;ObjectSelection selected(*doc);
    const auto a=doc->add_rectangle({0,0,3,5},"A"),b=doc->add_rectangle({4,0,3,5},"B");doc->classify_annotation(a,"A");doc->classify_annotation(b,"B");selected.replace({a});
    RawRegionRequest request;request.region={0,0,0,7,5};request.channels=3;const RawAnalysisRequest raw_request(*doc,request);doc->keep_raw_analysis(raw_request,analyze_raw(raw_request));
    TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={1,2};grid.column_step={4,0};grid.row_step={0,2};grid.width=grid.height=1;doc->create_tma_grid(grid);
    PixelFeatureOptions features;features.sigmas={0};StainTransform transform({{1,0,0},{0,1,0}});std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& feature:pixel_feature_schema(features,transform))schema.emplace_back(feature.name,feature.unit);
    const auto pixel=std::make_shared<PixelClassifier>(forest(schema),features,transform,PixelResolution{});
    ScientificRawLattice lattice(7,5,{0,7,7},{0,5,5});ScientificRawOptions selection;selection.channels=3;RawPixelFeatureOptions rf;rf.offsets={0,0,0};rf.scales={1,1,1};RawPixelRecipe recipe(*doc->server()->raw_metadata(nullptr),lattice,selection,rf);schema.clear();for(const auto& feature:recipe.feature_schema())schema.emplace_back(feature.name,feature.unit);
    const auto raw_model=std::make_shared<RawPixelClassifier>(forest(schema),recipe);
    auto path=dir/"interactive-existing.output";if(!browsing)write(path,"sentinel");const auto digest=browsing?std::string{}:hash(path);
    struct PreviewContext {
        GLFWwindow* window=nullptr;
        explicit PreviewContext(bool needed){if(!needed)return;check(glfwInit()!=0,"Browse preview GLFW initialization");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);window=glfwCreateWindow(64,64,"Browse preview",nullptr,nullptr);check(window,"Browse preview GL context");glfwMakeContextCurrent(window);}
        ~PreviewContext(){if(window){glfwMakeContextCurrent(nullptr);glfwDestroyWindow(window);glfwTerminate();}}
    } preview_context(browsing && (group=="pixel" || group=="raw-pixel"));
    Host host;ImGui::GetIO().DisplaySize={2400,2600};auto mark=[](GuiInputTarget){};
    auto press=[&](auto& panel,GuiInputTarget target) {
        for(int i=0;i<5;++i)host.frame();const auto rect=panel.bounds(target);check(rect.width>0 && rect.height>0,"Writer action absent: "+std::to_string(int(target)));
        auto& io=ImGui::GetIO();io.AddMousePosEvent(float(rect.x+rect.width/2),float(rect.y+rect.height/2));host.frame();io.AddMouseButtonEvent(0,true);host.frame();io.AddMouseButtonEvent(0,false);host.frame();host.frame();
    };
    auto cancel=[&](const char* name) {
        if(browsing) {
            for(int i=0;i<5000 && detail::OutputGateTestAccess::success(host.gate)!=path.u8string();++i){host.frame();if(own_error && !own_error().empty())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            check(!host.gate.pending() && std::filesystem::is_regular_file(path) && detail::OutputGateTestAccess::success(host.gate)==path.u8string(),std::string("Browse-only writer did not finish: ")+name+" / "+(own_error?own_error():std::string{}));return;
        }
        if(missing){check(!host.gate.pending() && own_error && own_error()=="Output gate is unavailable" && hash(path)==digest,std::string("Missing gate did not report on its window: ")+name);return;}
        check(host.gate.pending(),std::string("Interactive writer bypassed dialog: ")+name);
        check(detail::OutputGateTestAccess::path(host.gate)==path.u8string() && !detail::OutputGateTestAccess::scope(host.gate).empty(),"Interactive dialog lacks absolute path/scope");
        host.click(ui::DialogAction::Cancel);check(!host.gate.pending() && hash(path)==digest,std::string("Interactive Cancel changed bytes: ")+name);
    };
    auto choose=[&](auto& panel,const char* label,const std::string& existing=std::string{}) {
        if(!browsing)return;
        auto fields=panel.paths();auto& field=core_path_checks::find(fields,label);const auto& spec=ui::PathFieldTestAccess::spec(field);
        if(existing.empty()){std::string name=label;for(auto& c:name)if(!std::isalnum(static_cast<unsigned char>(c)))c='-';path=dir/("browse-"+name+".output");}
        const auto chosen=existing.empty()?path.u8string():existing;
        field.set("");field.open_picker({},chosen);
        const auto mode=spec.role==ui::PathRole::OpenFile?detail::PickerMode::OpenFile:detail::PickerMode::NewFile;
        ui::PathFieldTestAccess::picker(field).open(chosen,spec.extensions,mode);
        const auto draw=host.panel;host.panel=[&]{draw();field.draw_picker({});};host.frame();host.frame();
        core_path_checks::picker_action(mode==detail::PickerMode::OpenFile?"Choose file":"Choose new output");host.frame();host.frame();host.panel=draw;
        check(!field.picker_active() && field.state().resolved==chosen,"Browse-only writer path selection failed");
    };
    auto load_split=[&](auto& panel,const char* input,const char* output,GuiInputTarget target) {
        if(!browsing)return;auto fields=panel.paths();auto& read=core_path_checks::find(fields,input);auto& save=core_path_checks::find(fields,output);
        check(std::string(read.text())==(dir/"interactive-existing.output").u8string(),"Saving split model changed read path");const auto original=std::string(save.text());choose(panel,input,path.u8string());press(panel,target);
        for(int i=0;i<5000 && panel.io_state()!="loaded" && panel.io_state()!="failed";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(panel.io_state()=="loaded" && std::string(save.text())==original,"Loading split model changed save path or failed");
    };
    auto size=[](const char* name){if(ImGui::FindWindowByName(ui::i18n::label(name).c_str()))ImGui::SetWindowSize(ui::i18n::label(name).c_str(),{900,2200},ImGuiCond_Always);};
    if(group=="all" || group=="object") {
        GuiOptions options;options.classifier_open=true;options.classifier_model_path=path.u8string();options.classifier_model=std::make_shared<ForestModel>(ForestModel::from_bytes(qpx_test::object_forest_fixture()));detail::GuiObjectClassifier panel(options);own_error=[&]{return panel.io_error();};
        host.panel=[&]{size("Object classifiers");panel.update(*doc,selected);panel.draw(*doc,selected,host.gate.pending(),true,mark,missing?nullptr:&host.gate);};choose(panel,"Export model file");press(panel,GuiInputTarget::SaveClassifierModel);cancel("object model");load_split(panel,"Model file","Export model file",GuiInputTarget::LoadClassifierModel);host.panel={};
    }
    if(group=="all" || group=="pixel") {
        GuiOptions options;options.pixel_open=true;options.pixel_model_path=path.u8string();options.pixel_stream_path=path.u8string();options.pixel_model=pixel;detail::GuiPixelClassifier panel(options);own_error=[&]{return panel.io_error().empty()?panel.error():panel.io_error();};
        host.panel=[&]{size("Pixel classifier");panel.update(*doc,selected);panel.draw(*doc,selected,host.gate.pending(),true,mark,[&](bool){return transform;},{0,0,7,5},nullptr,0,missing?nullptr:&host.gate);};
        choose(panel,"Export pixel model file");press(panel,GuiInputTarget::SavePixelModel);cancel("pixel model");load_split(panel,"Pixel model file","Export pixel model file",GuiInputTarget::LoadPixelModel);panel.reveal_target(GuiInputTarget::ExportPixelStream);choose(panel,"Pixel stream file");press(panel,GuiInputTarget::ExportPixelStream);cancel("pixel stream");if(browsing){const auto output=std::string(core_path_checks::find(panel.paths(),"Pixel stream file").text());panel.open_saved_streams();choose(panel,"Saved stream file",path.u8string());press(panel,GuiInputTarget::OpenPixelStreamFile);for(int i=0;i<5000 && panel.browser().busy();++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}host.frame();check(panel.browser().stats().declared_summary && panel.browser().error().empty() && std::string(core_path_checks::find(panel.paths(),"Pixel stream file").text())==output,"Browse-only stream read changed export path or failed: "+panel.browser().stats().state+" / "+panel.browser().error());}host.panel={};
    }
    if(group=="all" || group=="raw-numpy" || group=="raw-csv") {
        GuiOptions options;options.raw_open=true;options.raw_tab=1;options.raw_request=std::make_shared<RawRegionRequest>(request);options.raw_numpy_path=options.raw_csv_path=path.u8string();detail::GuiRaw panel(options);own_error=[&]{return panel.error();};
        host.panel=[&]{size("Quantitative channels");panel.draw(*doc,selected,&project,host.gate.pending(),mark,nullptr,0,missing?nullptr:&host.gate);};
        for(int i=0;i<3000 && panel.state()!="ready";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(panel.state()=="ready",panel.error());if(group!="raw-csv"){choose(panel,"##raw NumPy path");press(panel,GuiInputTarget::ExportRawRegion);cancel("NumPy");}if(group!="raw-numpy"){press(panel,GuiInputTarget::RawRecordsTab);choose(panel,"##raw CSV path");press(panel,GuiInputTarget::ExportRawMeasurements);cancel("raw CSV");}host.panel={};
    }
    if(group=="all" || group=="tma") {
        GuiOptions options;options.tma_open=true;options.tma_tab=2;options.tma_metadata_path=options.qptma_output_path=path.u8string();detail::GuiTma panel(options);own_error=[&]{return panel.error();};
        host.panel=[&]{size("Tissue microarray");panel.draw(*doc,selected,&project,host.gate.pending(),true,mark,missing?nullptr:&host.gate);};
        panel.reveal_target(GuiInputTarget::ExportTmaMetadata);choose(panel,"##TMA metadata path");press(panel,GuiInputTarget::ExportTmaMetadata);cancel("TMA metadata");
        panel.reveal_target(GuiInputTarget::AcknowledgeQptmaLosses);press(panel,GuiInputTarget::AcknowledgeQptmaLosses);panel.reveal_target(GuiInputTarget::ExportQptma);choose(panel,"##QPTMA output");press(panel,GuiInputTarget::ExportQptma);cancel("QPTMA");host.panel={};
    }
    if(group=="all" || group=="raw-pixel") {
        GuiOptions options;auto settings=std::make_shared<GuiRawPixelOptions>();settings->open=true;settings->model=*raw_model;settings->model_path=settings->stream_path=settings->training.checkpoint_path=path.u8string();settings->training.selection=selection;settings->training.features=rf;settings->training.lattice=lattice;settings->training.checkpoint.max_new_windows=0;options.raw_pixel=settings;detail::GuiRawPixel panel(options);own_error=[&]{return panel.report().error;};
        host.panel=[&]{size("Raw pixel classifier");panel.update(*doc);panel.draw(*doc,selected,&project,host.gate.pending(),true,mark,nullptr,0,missing?nullptr:&host.gate);};
        choose(panel,"Training checkpoint");press(panel,GuiInputTarget::RawPixelTrain);cancel("raw checkpoint");press(panel,GuiInputTarget::RawPixelFilesTab);choose(panel,"Export model path");press(panel,GuiInputTarget::RawPixelSaveModel);cancel("raw model");if(browsing){check(std::string(core_path_checks::find(panel.paths(),"Model path").text())==(dir/"interactive-existing.output").u8string(),"Saving raw model changed read path");const auto save=std::string(core_path_checks::find(panel.paths(),"Export model path").text());choose(panel,"Model path",path.u8string());press(panel,GuiInputTarget::RawPixelLoadModel);for(int i=0;i<5000 && panel.report().state!="loaded" && panel.report().state!="failed";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(panel.report().state=="loaded" && std::string(core_path_checks::find(panel.paths(),"Export model path").text())==save,"Raw model load changed write path or failed");}choose(panel,"Prediction file");press(panel,GuiInputTarget::RawPixelExport);cancel("raw prediction");if(browsing){check(std::string(core_path_checks::find(panel.paths(),"Open prediction file").text())==(dir/"interactive-existing.output").u8string(),"Saving raw prediction changed read path");const auto save=std::string(core_path_checks::find(panel.paths(),"Prediction file").text());choose(panel,"Open prediction file",path.u8string());press(panel,GuiInputTarget::RawPixelOpenFile);for(int i=0;i<5000 && !panel.report().historical && panel.report().state!="verified" && panel.report().state!="failed";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(panel.report().error.empty() && panel.report().link && (panel.report().historical || panel.report().state=="verified") && std::string(core_path_checks::find(panel.paths(),"Prediction file").text())==save,"Raw prediction load changed write path or failed");}host.panel={};
    }
    if(group=="all" || group=="spatial") {
        GuiOptions options;auto settings=std::make_shared<GuiSpatialOptions>();settings->open=settings->run=true;settings->sources={a};settings->targets={b};settings->csv_path=path.u8string();options.spatial=settings;detail::GuiSpatial panel(options);own_error=[&]{return panel.report().error;};
        host.panel=[&]{size("Spatial analysis");panel.draw(*doc,selected,&project,nullptr,false,true,mark,nullptr,0,missing?nullptr:&host.gate);};
        for(int i=0;i<3000 && panel.report().state!="ready";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(panel.report().state=="ready",panel.report().error);choose(panel,"CSV path");press(panel,GuiInputTarget::SpatialExport);
        if(browsing)cancel("spatial CSV");else check(!host.gate.pending() && panel.report().export_state=="idle" && (missing?panel.report().error=="Output gate is unavailable":!panel.report().error.empty()) && hash(path)==digest,"Create-only spatial button started a job or hid refusal");host.panel={};
    }
    if(group=="all" || group=="tma-summary") {
        GuiOptions options;options.tma_open=true;options.tma_tab=2;options.tma_case_summary=true;options.tma_metadata_path=path.u8string();detail::GuiTma panel(options);own_error=[&]{return panel.error();};
        host.panel=[&]{size("Tissue microarray");panel.draw(*doc,selected,&project,false,true,mark,missing?nullptr:&host.gate);};panel.reveal_target(GuiInputTarget::ExportTmaMetadata);choose(panel,"##TMA metadata path");press(panel,GuiInputTarget::ExportTmaMetadata);
        if(browsing)cancel("TMA summary");else check(!host.gate.pending() && panel.export_state()=="idle" && (missing?panel.error()=="Output gate is unavailable":!panel.error().empty()) && hash(path)==digest,"Create-only TMA summary button started a job or hid refusal");host.panel={};
    }
    std::cout<<(missing?"Missing gate errors: ":"Actual tool buttons: ")<<" object/pixel models, pixel stream, NumPy/raw CSV, TMA metadata/QPTMA, raw checkpoint/model/prediction ask once and Cancel preserves SHA256\n";
}
inline void partial_prediction(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    Document doc(open_ome_zarr(source.u8string()));ObjectSelection selected(doc);
    ScientificRawLattice lattice(7,5,{0,7,7},{0,5,5});ScientificRawOptions channels;channels.channels=3;
    RawPixelFeatureOptions features;features.offsets={0,0,0};features.scales={1,1,1};RawPixelRecipe recipe(*doc.server()->raw_metadata(nullptr),lattice,channels,features);
    std::vector<std::pair<std::string,std::string>> schema;for(const auto& f:recipe.feature_schema())schema.emplace_back(f.name,f.unit);
    const RawPixelClassifier model(forest(schema),recipe);auto settings=std::make_shared<GuiRawPixelOptions>();settings->open=true;settings->model=model;
    settings->stream.tile_width=settings->stream.tile_height=2;settings->save.max_new_windows=1;settings->save.replace_existing=true;
    const auto path=(dir/"partial.qws").u8string();settings->stream_path=path;GuiOptions options;options.raw_pixel=settings;
    auto panel=std::make_unique<detail::GuiRawPixel>(options);Host host;ImGui::GetIO().DisplaySize={2400,2600};
    host.panel=[&]{if(ImGui::FindWindowByName("Raw pixel classifier"))ImGui::SetWindowSize("Raw pixel classifier",{900,2200},ImGuiCond_Always);panel->update(doc);panel->draw(doc,selected,nullptr,false,true,[](GuiInputTarget){},nullptr,0,&host.gate);};
    auto press=[&](GuiInputTarget target){for(int i=0;i<4;++i)host.frame();const auto r=panel->bounds(target);check(r.width>0 && r.height>0,"Partial save action missing");auto& io=ImGui::GetIO();io.AddMousePosEvent(float(r.x+r.width/2),float(r.y+r.height/2));host.frame();io.AddMouseButtonEvent(0,true);host.frame();io.AddMouseButtonEvent(0,false);host.frame();};
    auto settle=[&]{for(int i=0;i<3000 && panel->report().state=="running";++i){host.frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}for(int i=0;i<4;++i)host.frame();};
    press(GuiInputTarget::RawPixelFilesTab);press(GuiInputTarget::RawPixelExport);settle();
    check(panel->report().state=="partial" && panel->report().link && detail::OutputGateTestAccess::success(host.gate).empty(),"Partial file posted a completion receipt or did not retain partial state: "+panel->report().state+" / "+panel->report().error);
    core_path_checks::find(panel->paths(),"Resume prediction from").set_option(path);
    int resumes=0;while(panel->report().state=="partial" && resumes++<40){press(GuiInputTarget::RawPixelExport);settle();if(panel->report().state=="partial")check(detail::OutputGateTestAccess::success(host.gate).empty(),"Partial resume posted completion");}
    check(panel->report().state=="saved" && detail::OutputGateTestAccess::success(host.gate)==path,"Complete resumed prediction did not post completion: "+panel->report().state+" / "+panel->report().error);
    check(host.notices.posted()==1 && host.notices.newest(0).notice.source==ui::command(ui::CommandId::Tool_RawPixelClassifier).caption,"Resumed raw prediction did not post exactly one receipt from its tool");
    std::cout<<"Partial prediction: incomplete file has no receipt; "<<resumes<<" bounded resumes reach saved and one completion receipt\n";
}
inline void core_controls(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    for(const std::string kind:{"fingerprint","stain"}) {
        auto project=fixture(dir/(kind+"-controls.qpxproj"),32,32);project->document(1)->add_rectangle({2,2,20,20});const auto path=dir/(kind+"-existing.json");write(path,"sentinel");const auto before=hash(path);
        auto settings=options(dir,kind+"-controls","workflow",kind=="stain"?"region":"files");settings.width=1600;settings.height=1400;
        settings.file_fingerprint_path=project->path();settings.file_fingerprint_output=path.u8string();settings.file_fingerprint_associate_source=false;settings.stain_model_path=path.u8string();
        bool activated=false;ui::WorkflowTrace trace;auto* previous=ui::workflow_trace;ui::workflow_trace=&trace;
        if(kind=="fingerprint")settings.inputs={key(4,GuiInputKind::FingerprintFileKey)};
        else {settings.inputs={key(3,GuiInputKind::SelectAllKey)};trace.before_frame=[&](int frame) {
            if(activated || frame<7)return;
            for(auto* window:ImGui::GetCurrentContext()->Windows)if(std::string(window->Name).find("Workflow task body")!=std::string::npos && !window->SkipItems) {
                ImGui::ActivateItemByID(ImHashStr("###Save stain model",0,window->ID));activated=true;break;
            }
        };}
        workflow_safety_checks::click(settings.inputs,12,GuiInputTarget::CancelReplaceExport,GuiInputWait::TargetReady);
        const auto result=run_project_gui(project,32,(dir/(kind+"-controls.ppm")).u8string(),settings);ui::workflow_trace=previous;
        check((kind!="stain" || activated) && result.replay_complete && hash(path)==before,"Core writer did not ask/Cancel through the gate: "+kind);
    }
    std::cout<<"Actual fingerprint and stain export controls: ReplaceExport/Cancel preserves SHA256\n";
    {
        auto project=fixture(dir/"picker-existing.qpxproj",32,32);project->document(1)->add_rectangle({2,2,20,20});const auto path=dir/std::filesystem::u8path(u8"picker-雪-é.geojson");write(path,"sentinel");const auto before=hash(path);
        auto settings=options(dir,"picker-existing","annotations");settings.inputs={key(4,GuiInputKind::ExportKey)};workflow_safety_checks::click(settings.inputs,16,GuiInputTarget::CancelReplaceExport,GuiInputWait::TargetReady);
        bool chosen=false,was_open=false;int asks=0;core_path_checks::TraceScope picker_trace;
        picker_trace.trace.before_frame=[&](detail::GuiProjectForms&,const std::vector<ui::PathField*>& fields,std::vector<detail::GuiPathDrop>&) {
            const auto* modal=ImGui::FindWindowByName(ui::i18n::label(ui::dialog_definition(ui::DialogKind::ReplaceExport).name).c_str());const bool open=modal && modal->WasActive;if(open && !was_open)++asks;was_open=open;
            auto& field=core_path_checks::find(fields,"##Interchange path");if(chosen || !field.picker_active() || !ImGui::FindWindowByName("Choose import file"))return;
            core_path_checks::Access::picker(field).open(path.u8string(),{".geojson"},detail::PickerMode::NewFile);core_path_checks::picker_action("Choose new output");chosen=true;
        };
        const auto result=run_project_gui(project,32,(dir/"picker-existing.ppm").u8string(),settings);
        check(chosen && asks==1 && result.replay_complete && hash(path)==before,"Picker decided replacement or existing choice did not ask exactly once");
    }
    std::cout<<"Empty export / NewFile picker / existing UTF-8 file asks exactly once; Cancel preserves SHA256\n";
}
inline void scripted(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"tests/fixtures/ome-zarr/plain";
    auto doc=std::make_shared<Document>(open_ome_zarr(source.u8string()));Project project;ObjectSelection selected(*doc);
    const auto a=doc->add_rectangle({0,0,3,5},"A"),b=doc->add_rectangle({4,0,3,5},"B");doc->classify_annotation(a,"A");doc->classify_annotation(b,"B");selected.replace({a});
    StainTransform transform({{1,0,0},{0,1,0}});PixelFeatureOptions features;features.sigmas={0};std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& feature:pixel_feature_schema(features,transform))schema.emplace_back(feature.name,feature.unit);
    auto pixel=std::make_shared<PixelClassifier>(forest(schema),features,transform,PixelResolution{});const PixelClassifier old_pixel(forest(schema,true),features,transform,PixelResolution{});
    ScientificRawLattice lattice(7,5,{0,7,7},{0,5,5});ScientificRawOptions selection;selection.channels=3;RawPixelFeatureOptions rf;rf.offsets={0,0,0};rf.scales={1,1,1};RawPixelRecipe recipe(*doc->server()->raw_metadata(nullptr),lattice,selection,rf);schema.clear();for(const auto& feature:recipe.feature_schema())schema.emplace_back(feature.name,feature.unit);
    const RawPixelClassifier model(forest(schema),recipe),old_model(forest(schema,true),recipe);
    Host host;ImGui::GetIO().DisplaySize={2400,2600};auto mark=[](GuiInputTarget){};
    auto size=[](const char* name){if(ImGui::FindWindowByName(name))ImGui::SetWindowSize(name,{900,2200},ImGuiCond_Always);};
    auto press=[&](auto& panel,GuiInputTarget target) {
        for(int i=0;i<5;++i)host.frame();const auto rect=panel.bounds(target);check(rect.width>0 && rect.height>0,"Script writer action absent");auto& io=ImGui::GetIO();io.AddMousePosEvent(float(rect.x+rect.width/2),float(rect.y+rect.height/2));host.frame();io.AddMouseButtonEvent(0,true);host.frame();io.AddMouseButtonEvent(0,false);host.frame();
        check(!host.gate.pending(),"Scripted replace flag opened a dialog");
    };
    auto settle=[&](auto ready,const std::filesystem::path& path,const std::string& before,bool complete=true) {
        for(int i=0;i<3000 && !ready();++i){host.frame();check(!host.gate.pending(),"Scripted writer asked after its job started");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(ready() && hash(path)!=before,"Scripted option failed to replace native bytes: "+path.u8string());host.frame();if(complete)check(detail::OutputGateTestAccess::success(host.gate)==path.u8string(),"Scripted success lost its absolute receipt");else check(detail::OutputGateTestAccess::success(host.gate).empty(),"Scripted partial prediction posted a completion receipt");detail::OutputGateTestAccess::clear_success(host.gate);
    };
    {
        const auto path=dir/"object.model";const auto native=ForestModel::from_bytes(qpx_test::object_forest_fixture());schema.clear();for(const auto& f:native.features())schema.emplace_back(f.name,f.unit);save_forest_model(forest(schema,true),path.u8string());const auto before=hash(path);
        GuiOptions options;options.classifier_open=true;options.classifier_replace_model=true;options.classifier_model_path=path.u8string();options.classifier_model=std::make_shared<ForestModel>(native);detail::GuiObjectClassifier panel(options);
        host.panel=[&]{size("Object classifiers");panel.update(*doc,selected);panel.draw(*doc,selected,false,true,mark,&host.gate);};press(panel,GuiInputTarget::SaveClassifierModel);settle([&]{return panel.io_state()=="saved";},path,before);host.panel={};
    }
    {
        const auto path=dir/"pixel.model",stream_path=dir/"pixel.stream";save_pixel_classifier(old_pixel,path.u8string());PixelPredictionStream old_stream(*doc,a,old_pixel);save_pixel_stream(old_stream,stream_path.u8string());const auto before=hash(path),before_stream=hash(stream_path);
        GuiOptions options;options.pixel_open=true;options.pixel_replace_model=options.pixel_stream_replace=true;options.pixel_model_path=path.u8string();options.pixel_stream_path=stream_path.u8string();options.pixel_model=pixel;detail::GuiPixelClassifier panel(options);
        host.panel=[&]{size("Pixel classifier");panel.update(*doc,selected);panel.draw(*doc,selected,false,true,mark,[&](bool){return transform;},{0,0,7,5},nullptr,0,&host.gate);};
        press(panel,GuiInputTarget::SavePixelModel);settle([&]{return panel.io_state()=="saved";},path,before);panel.reveal_target(GuiInputTarget::ExportPixelStream);press(panel,GuiInputTarget::ExportPixelStream);settle([&]{return panel.stream_stats().state=="saved";},stream_path,before_stream);host.panel={};
    }
    {
        const auto checkpoint=dir/"raw.checkpoint",path=dir/"raw.model",stream_path=dir/"raw.stream";
        const RawPixelTiledTrainingRequest training(*doc,{a,b},lattice,selection,rf);RawPixelTrainingCheckpointOptions checkpoint_options;checkpoint_options.max_new_windows=1;save_raw_pixel_training_checkpoint(training,checkpoint.u8string(),checkpoint_options);
        save_raw_pixel_classifier(old_model,path.u8string());RawPixelStreamSaveOptions stream_save;stream_save.max_new_windows=1;save_raw_pixel_stream(RawPixelTiledPredictionRequest(*doc,model,lattice),stream_path.u8string(),stream_save);const auto before=hash(path),before_checkpoint=hash(checkpoint),before_stream=hash(stream_path);
        GuiOptions options;auto settings=std::make_shared<GuiRawPixelOptions>();settings->open=true;settings->model=model;settings->replace_model=settings->training.checkpoint.replace_existing=settings->save.replace_existing=true;settings->model_path=path.u8string();settings->stream_path=stream_path.u8string();settings->training.checkpoint_path=checkpoint.u8string();settings->training.selection=selection;settings->training.features=rf;settings->training.lattice=lattice;settings->training.checkpoint.max_new_windows=settings->save.max_new_windows=0;options.raw_pixel=settings;detail::GuiRawPixel panel(options);
        host.panel=[&]{size("Raw pixel classifier");panel.update(*doc);panel.draw(*doc,selected,&project,false,true,mark,nullptr,0,&host.gate);};
        press(panel,GuiInputTarget::RawPixelTrain);settle([&]{return panel.report().state=="partial";},checkpoint,before_checkpoint);press(panel,GuiInputTarget::RawPixelFilesTab);press(panel,GuiInputTarget::RawPixelSaveModel);settle([&]{return panel.report().state=="saved";},path,before);press(panel,GuiInputTarget::RawPixelExport);settle([&]{return panel.report().state=="partial" && bool(panel.report().link);},stream_path,before_stream,false);host.panel={};
    }
    std::cout<<"All six real GuiOptions replace flags: actual Save/Train/Export buttons replace verified files without dialogs; complete-only absolute receipts pass; partial prediction has no receipt\n";
}
}
