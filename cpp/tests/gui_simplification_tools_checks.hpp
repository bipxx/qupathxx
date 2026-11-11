#pragma once
#include "../gui_project_density.hpp"
#include "../gui_tma.hpp"
#include "qupathxx/density_workflow.hpp"
#include "qupathxx/spatial_density.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <set>
#include <thread>

namespace qpx::detail {
// Private GUI test access only. Production Run submission is exercised below
// through real controls; this accessor seeds the existing native image-event
// barrier solely to make a Cancel click deterministic.
struct GuiProjectDensityTestAccess {
    static ProjectDensityJob& job(GuiProjectDensity& panel) {return panel.job_;}
};
struct GuiTmaTestAccess {
    static bool pending(const GuiTma& panel) {return panel.exchange_.valid();}
    static bool cancel_requested(const GuiTma& panel) {return panel.exchange_cancel_.load();}
    static void pending_export(GuiTma& panel,const QptmaData& data,const std::string& path,std::shared_future<void> gate) {
        panel.exchange_cancel_.store(false);panel.exchange_exporting_=true;panel.exchange_state_="exporting";
        panel.exchange_=std::async(std::launch::async,[&panel,data,path,gate] {
            gate.wait();GuiTma::Exchange result;
            try {result.bytes=save_qptma(data,path,8*1024*1024,&panel.exchange_cancel_);result.state="saved";}
            catch(const std::exception& e) {result.state=panel.exchange_cancel_.load()?"cancelled":"failed";result.error=e.what();}
            return result;
        });
    }
};
}

namespace simplification_tools_checks {
struct NativeUi {
    ImGuiContext* previous_context=ImGui::GetCurrentContext();
    ui::i18n::State previous_language=ui::i18n::state;
    ui::design::State previous_theme=ui::design::state;
    ui::WorkflowTrace* previous_trace=ui::workflow_trace;
    ImGuiContext* context=ImGui::CreateContext();
    ui::WorkflowTrace trace;
    NativeUi() {
        ImGui::SetCurrentContext(context);ui::i18n::choose(0);ui::workflow_trace=&trace;
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.DisplaySize={1600,1400};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
        io.Fonts->AddFontDefault();unsigned char* pixels;int width,height;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);
        ImGui::GetStyle().FontSizeBase=16;
    }
    ~NativeUi() {
        ui::workflow_trace=previous_trace;ImGui::DestroyContext(context);
        ImGui::SetCurrentContext(previous_context);ui::i18n::state=previous_language;ui::design::state=previous_theme;
    }
    template<class Draw>void frame(Draw& draw) {
        ImGui::NewFrame();ui::begin_frame();ui::reset_workflow_trace();draw();ImGui::Render();
    }
    template<class Draw>void settle(Draw& draw) {for(int i=0;i<4;++i)frame(draw);}
    template<class Draw>void fit(const char* name,float width,float height,Draw& draw) {
        settle(draw);auto* window=ImGui::FindWindowByName(name);check(window,"tools fixture window missing");
        ImGui::SetWindowPos(window,{24,24});ImGui::SetWindowSize(window,{width,height});settle(draw);
    }
    const ui::WorkflowActionTrace& action(const char* key) const {
        const auto found=trace.actions.find(key);check(found!=trace.actions.end(),std::string("tools control missing: ")+key);
        return found->second;
    }
    template<class Draw>void click(Rect bounds,Draw& draw,bool field=false) {
        check(bounds.width>0 && bounds.height>0,"tools input target has no bounds");
        auto& io=ImGui::GetIO();
        io.AddMousePosEvent(float(field?bounds.x+20:bounds.x+bounds.width/2),float(field?bounds.y+12:bounds.y+bounds.height/2));settle(draw);
        io.AddMouseButtonEvent(0,true);frame(draw);io.AddMouseButtonEvent(0,false);settle(draw);
    }
    template<class Draw>void click(const char* key,Draw& draw) {
        const auto target=action(key);check(target.enabled,std::string("tools control disabled: ")+key);click(target.bounds,draw);
    }
    template<class Draw>void text(const char* key,const std::string& value,Draw& draw) {
        const auto target=action(key);check(target.enabled,std::string("tools input disabled: ")+key);
        click(target.bounds,draw,true);auto& io=ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame(draw);
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame(draw);
        io.AddInputCharactersUTF8(value.c_str());frame(draw);
        io.AddKeyEvent(ImGuiKey_Tab,true);frame(draw);io.AddKeyEvent(ImGuiKey_Tab,false);settle(draw);
    }
    template<class Draw>void toggle_tma(Draw& draw) {
        auto& io=ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiMod_Alt,true);io.AddKeyEvent(ImGuiKey_T,true);frame(draw);
        io.AddKeyEvent(ImGuiKey_T,false);io.AddKeyEvent(ImGuiMod_Alt,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);settle(draw);
    }
    template<class Draw,class Ready>void until(Draw& draw,Ready ready,const char* failure) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        while(!ready()) {
            check(std::chrono::steady_clock::now()<deadline,failure);frame(draw);std::this_thread::yield();
        }
        settle(draw);
    }
};

inline std::set<std::string> shown_omissions(const NativeUi& ui) {
    constexpr const char* prefix="qptma-omission-";std::set<std::string> result;
    for(const auto& item:ui.trace.actions)if(item.first.rfind(prefix,0)==0)result.insert(item.first.substr(std::strlen(prefix)));
    return result;
}
inline void tma_review(const std::filesystem::path& directory) {
    const auto project=fixture(directory/"review.qpxproj",128,96);const auto doc=project->document(1);
    TmaGridOptions grid;grid.rows=2;grid.columns=2;grid.origin={24,24};grid.row_step={0,32};grid.column_step={32,0};grid.width=grid.height=20;
    const auto cores=doc->create_tma_grid(grid);project->save(project->path());const auto saved=read(project->path());
    const auto output=directory/"review.qptma";GuiOptions options;options.tma_open=true;options.tma_tab=2;options.qptma_output_path=output.u8string();
    NativeUi ui;detail::GuiTma panel(options);ObjectSelection selection(*doc);selection.replace({cores.front()},cores.front());
    auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};
    ui.fit("Tissue microarray",600,1100,draw);
    const auto original=qptma_from_grid(*doc->tma_grid(),doc->server()->source_locator(),doc->tma_grid()->info()->name());
    check(shown_omissions(ui)==std::set<std::string>(original.omitted_metadata.begin(),original.omitted_metadata.end()),"QPTMA review omitted an actual native loss or invented a class/parent loss");
    check(panel.enabled(GuiInputTarget::AcknowledgeQptmaLosses) && !panel.enabled(GuiInputTarget::ExportQptma),"QPTMA export did not initially require loss acknowledgment");
    ui.click(panel.bounds(GuiInputTarget::AcknowledgeQptmaLosses),draw);ui.settle(draw);
    check(panel.enabled(GuiInputTarget::ExportQptma),"unchanged-frame QPTMA review reset acknowledgment or hid the export control");
    doc->set_class("Class###literal",0x123456);doc->classify_annotation(cores.front(),"Class###literal");ui.settle(draw);
    check(!panel.enabled(GuiInputTarget::ExportQptma) && shown_omissions(ui).count("class_assignments"),"core edit did not invalidate acknowledgment and refresh native class loss");
    ui.click(panel.bounds(GuiInputTarget::AcknowledgeQptmaLosses),draw);check(panel.enabled(GuiInputTarget::ExportQptma),"class-loss acknowledgment cannot be reapplied");
    const auto parent=doc->add_rectangle({0,0,100,90},"Parent###literal");doc->reparent_annotation(cores.front(),parent);ui.settle(draw);
    const auto with_parent=qptma_from_grid(*doc->tma_grid(),doc->server()->source_locator(),doc->tma_grid()->info()->name());
    check(!panel.enabled(GuiInputTarget::ExportQptma) && shown_omissions(ui)==std::set<std::string>(with_parent.omitted_metadata.begin(),with_parent.omitted_metadata.end()),"parent edit did not refresh exact native omissions and invalidate acknowledgment");
    ui.click(panel.bounds(GuiInputTarget::AcknowledgeQptmaLosses),draw);
    ui.click("Export QuPath grid",draw);ui.until(draw,[&]{return panel.qptma_state()=="saved";},"QPTMA UI export did not finish");
    check(panel.error().empty() && read(output)==qptma_text(with_parent.data),"reviewed QPTMA export differs from exact native bytes");
    doc->remove_annotation(cores.back());ui.settle(draw);
    check(!panel.enabled(GuiInputTarget::AcknowledgeQptmaLosses) && !panel.enabled(GuiInputTarget::ExportQptma),"vacant-slot native refusal did not disable acknowledgment/export");
    const auto refusal=ui.trace.actions.find("qptma-review-error");
    check(refusal!=ui.trace.actions.end() && refusal->second.caption.find("vacant")!=std::string::npos,"vacant-slot refusal is not visible in the native control");
    check(doc->undo(),"vacant core removal Undo missing");ui.settle(draw);
    check(panel.enabled(GuiInputTarget::AcknowledgeQptmaLosses) && !panel.enabled(GuiInputTarget::ExportQptma),"restored core did not require a fresh valid acknowledgment");
    check(read(project->path())==saved,"TMA review/export unexpectedly saved the source project");
}

struct ExportGate {
    std::promise<void> promise;
    std::shared_future<void> future=promise.get_future().share();
    bool released=false;
    void release() {if(!released) {promise.set_value();released=true;}}
    ~ExportGate() {release();}
};
inline void tma_close_lifecycle(const std::filesystem::path& directory) {
    {
        const auto project=fixture(directory/"hidden-export-source.qpxproj",128,96);const auto doc=project->document(1);
        TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={24,24};grid.column_step={32,0};grid.width=grid.height=20;
        doc->create_tma_grid(grid);project->save(project->path());const auto source=read(project->path());
        const auto data=qptma_from_grid(*doc->tma_grid(),doc->server()->source_locator(),doc->tma_grid()->info()->name()).data;
        GuiOptions options;options.tma_open=true;options.tma_tab=2;NativeUi ui;detail::GuiTma panel(options);
        ObjectSelection selection(*doc);Document* active=doc.get();
        auto draw=[&]{panel.draw(*active,selection,project.get(),false,true,[](GuiInputTarget){});};ui.fit("Tissue microarray",600,1100,draw);
        // The real Export action/ack/native writer bytes are covered above.
        // A gated native save keeps the controller pending while its actual
        // keyboard Close is exercised; it introduces no production writer flag.
        ExportGate gate;const auto output=directory/"hidden-export.qptma";
        detail::GuiTmaTestAccess::pending_export(panel,data,output.u8string(),gate.future);
        ui.toggle_tma(draw);
        const auto* window=ImGui::FindWindowByName("Tissue microarray");
        check(window && !window->Active && detail::GuiTmaTestAccess::pending(panel) && !detail::GuiTmaTestAccess::cancel_requested(panel),"closing the TMA panel cancelled its pending QPTMA export");
        gate.release();ui.until(draw,[&]{return !detail::GuiTmaTestAccess::pending(panel);},"hidden TMA export did not continue polling");
        check(panel.qptma_state()=="saved" && panel.error().empty() && read(output)==qptma_text(data),"hidden TMA export failed or changed exact native bytes");

        ExportGate invalidated;const auto refused=directory/"invalidated-export.qptma";
        detail::GuiTmaTestAccess::pending_export(panel,data,refused.u8string(),invalidated.future);
        Document replacement(Image(128,96,std::vector<std::uint8_t>(128*96*3,80)));active=&replacement;ui.settle(draw);
        check(detail::GuiTmaTestAccess::cancel_requested(panel),"TMA document-identity invalidation stopped cancelling a captured export");
        invalidated.release();ui.until(draw,[&]{return !detail::GuiTmaTestAccess::pending(panel);},"invalidated TMA export did not finish cancellation");
        check(panel.qptma_state()=="cancelled" && !std::filesystem::exists(refused) && read(project->path())==source,"invalidated export published a file or changed the source project");
    }
    {
        const auto project=fixture(directory/"import-close-source.qpxproj",128,96);const auto doc=project->document(1);
        const auto source=read(project->path());const auto revision=doc->revision();GuiOptions options;
        options.tma_open=true;options.qptma_input_path=(directory/"review.qptma").u8string();
        NativeUi ui;detail::GuiTma panel(options);ObjectSelection selection(*doc);
        auto draw=[&]{panel.draw(*doc,selection,project.get(),false,true,[](GuiInputTarget){});};ui.fit("Tissue microarray",600,1100,draw);
        ui.click("Load grid preview",draw);ui.until(draw,[&]{return panel.qptma_state()=="preview ready";},"real TMA import preview did not load");
        check(panel.qptma_cores()==4 && !doc->tma_core_count(),"import preview was applied before user acceptance");
        ui.toggle_tma(draw);
        check(panel.qptma_state()=="cancelled" && !panel.qptma_cores() && !doc->tma_core_count() && doc->revision()==revision && read(project->path())==source,"closing TMA failed to discard an unapplied import preview safely");
    }
}

inline std::shared_ptr<Project> density_fixture(const std::filesystem::path& path) {
    auto project=std::make_shared<Project>();
    for(int i=0;i<2;++i) {
        auto doc=std::make_shared<Document>(Image(24,24,std::vector<std::uint8_t>(24*24*3,80)));doc->set_calibration(.5,.25);
        for(const auto* name:{"Windows","Tissue","Tumor"})doc->set_class(name,0x008800);
        const auto window=doc->add_rectangle({0,0,12,12});doc->classify_annotation(window,"Windows");
        const auto tissue=doc->add_rectangle({0,0,10,10});doc->classify_annotation(tissue,"Tissue");
        for(int x:{2,5,8})doc->add_cell(Roi::rectangle({double(x)-1,double(x)-1,2,2}),{},"Cell",0,"Tumor");
        project->add_document(doc,"Image "+std::to_string(i)+", quoted \"literal\"");
    }
    project->save_new(path.u8string());return project;
}
inline std::string csv_quote(const std::string& value) {
    std::string result="\"";for(char c:value){result+=c;if(c=='\"')result+='\"';}return result+'\"';
}
inline std::string expected_density_csv(const ProjectDensityReport& report) {
    std::string expected;
    for(const auto& image:report.images)if(image.state=="saved" || image.state=="reused") {
        const auto checkpoint=Project::open(image.checkpoint);const auto doc=checkpoint->document(image.checkpoint_image);
        const auto record=doc->density_record(image.record);
        check(record && record->result && record->result->sha256()==image.result_sha256 && record->result->input_sha256()==image.result_input_sha256,"density GUI checkpoint lost exact native result identity");
        const auto& rows=record->result->rows();check(rows.size()==1 && rows.front().count==3 && std::abs(rows.front().area_mm2-.0000125)<1e-15 && rows.front().density_per_mm2 && std::abs(*rows.front().density_per_mm2-240000)<1e-8,"density GUI input selection/calibration differs from independent fixture facts");
        const auto native=spatial_density_csv(*record->result);const auto header_end=native.find('\n');check(header_end!=std::string::npos,"native density CSV header missing");
        if(expected.empty())expected="project_id,image_id,image_name,record_id,reuse,request_input_sha256,checkpoint,"+native.substr(0,header_end+1);
        const auto prefix=csv_quote(report.project_id)+","+std::to_string(image.id)+","+csv_quote(image.name)+","+std::to_string(image.record)+","+image.reuse+","+image.input_sha256+","+csv_quote(image.checkpoint)+",";
        for(std::size_t at=header_end+1;at<native.size();) {
            const auto end=native.find('\n',at);check(end!=std::string::npos,"native density CSV row missing newline");
            expected+=prefix+native.substr(at,end-at+1);at=end+1;
        }
    }
    return expected;
}
inline void density_controls(const std::filesystem::path& directory) {
    const auto project=density_fixture(directory/"source.qpxproj");const auto saved=read(project->path());const auto catalog=project->images();
    NativeUi ui;detail::GuiProjectDensity panel;panel.open();auto draw=[&]{panel.draw(project.get(),catalog,false);};
    ui.fit("Project-wide density",820,1280,draw);
    check(ui.action("Preview cohorts and calibration").caption=="Check inputs" && ui.action("Preview cohorts and calibration").enabled && !ui.action("Run project density").enabled,"density stage captions/availability are incorrect before Check inputs");
    ui.text("density-class-0","Windows",draw);ui.text("density-class-1","Tissue",draw);
    const auto output=directory/"ui-results";ui.text("density-output",output.u8string(),draw);
    ui.click("Preview cohorts and calibration",draw);
    ui.until(draw,[&]{return ui.action("Run project density").enabled;},"density Check inputs did not produce a runnable native preview");
    auto& job=detail::GuiProjectDensityTestAccess::job(panel);const auto preview=job.result();
    check(preview && preview->valid() && preview->recipe.images==std::vector<std::uint64_t>({1,2}) && preview->recipe.classes[0]==std::vector<std::string>{"Windows"} && preview->recipe.classes[1]==std::vector<std::string>{"Tissue"},"density text controls did not submit the exact native image/class selection");
    ui.click("Run project density",draw);ui.until(draw,[&]{return !job.busy() && job.result() && job.result()->state!="preview";},"density Run control did not finish the native job");
    const auto result=job.result();check(result && result->state=="completed" && result->computed==2 && result->completed==2 && result->failed==0,"normal density GUI Run failed");
    check(read(result->report)==result->json && read(result->csv)==expected_density_csv(*result),"density GUI CSV/report differ from native archived results");
    check(read(project->path())==saved,"density Check inputs/Run changed source project bytes");

    // Seed only this cancellation case with the existing native event barrier.
    // The ordinary GUI submission above has already proved real text/buttons.
    const auto manifest_path=directory/"cancel-manifest.json";
    generate_project_density_manifest(project->path(),preview->recipe,manifest_path.u8string());
    const auto manifest=load_project_density_manifest(manifest_path.u8string());ProjectDensityRunOptions run;run.pause_after_image=true;
    const auto cancelled_output=directory/"cancelled-results";
    check(job.run(manifest,cancelled_output.u8string(),run),"density cancellation fixture could not start native paused job");
    bool first_image=false;
    ui.until(draw,[&]{const auto events=job.take_images();first_image=first_image || !events.empty();return first_image;},"density image-event barrier was not reached");
    check(job.busy() && job.progress().completed.load()==1 && ui.action("Cancel density").enabled,"native event barrier did not hold one completed image for the real Cancel control");
    const auto checkpoint=cancelled_output/"image-1.qpxproj",receipt=cancelled_output/"image-1.json";
    const auto checkpoint_before=read(checkpoint),receipt_before=read(receipt);check(!checkpoint_before.empty() && !receipt_before.empty(),"completed density image was not published before cancellation");
    ui.click("Cancel density",draw);ui.until(draw,[&]{return !job.busy();},"real density Cancel control did not release the native image barrier");
    const auto cancelled=job.result();
    check(cancelled && cancelled->state=="cancelled" && cancelled->completed==1 && cancelled->not_run==1,"density UI Cancel did not retain the completed-image boundary");
    check(read(checkpoint)==checkpoint_before && read(receipt)==receipt_before && read(project->path())==saved,"density cancellation changed completed outputs or source bytes");
    check(read(cancelled->report)==cancelled->json && std::filesystem::exists(cancelled->csv),"density cancellation did not retain its partial report/CSV");
    panel.finish();
}
inline void headless(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory/"tma");std::filesystem::create_directories(directory/"density");
    tma_review(directory/"tma");tma_close_lifecycle(directory/"tma");density_controls(directory/"density");
    std::cout<<"Simplification tools: native QPTMA omission/ack/cache/refusal/export/hidden completion/import discard and density text/Check/Run/exact CSV/report/paused Cancel retention passed\n";
}
inline void workflows(const std::filesystem::path& directory) {
    // Existing tool_catalog_workflows covers actual GLFW TMA edit/Undo and exact
    // CSV/QPTMA captures. This packet targets the newly changed control guards.
    headless(directory);
}
}
