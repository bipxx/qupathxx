#pragma once
#include "qupathxx/source_identity.hpp"
// Included after the organization fixture helpers; exercises real native handlers.
static void dialog_workflows(const std::filesystem::path& directory) {
    auto click=[](std::vector<GuiInput>& inputs,int frame,GuiInputTarget target) {
        GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=target;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;inputs.push_back(down);inputs.push_back(up);
    };
    {
        auto project=fixture(directory/"published-cleanup.qpxproj",128,96);const auto saved=read(std::filesystem::u8path(project->path()));
        project->document(project->active_image())->add_rectangle({10,12,30,24},"Published recovery object");
        const auto recovery=directory/"published-cleanup.qpxrecover";ProjectAutosave writer;
        check(writer.request(*project,recovery.u8string()),"Request published cleanup fixture");const auto ready=writer.wait();
        check(ready.state=="ready" && ready.completed==1 && Project::recover(recovery.u8string())->document(1)->annotation(1).name=="Published recovery object","Cleanup fixture must actually publish recoverable state");
        writer.clear();const auto cleared=writer.wait();check(cleared.error.empty() && cleared.completed==1 && !std::filesystem::exists(recovery) && read(std::filesystem::u8path(project->path()))==saved,"Published owned recovery cleanup did not preserve primary bytes");
    }
    {
        auto project=fixture(directory/"metadata-workbench.qpxproj",128,96);auto doc=project->document(project->active_image());
        const auto object=doc->add_rectangle({10,12,30,24},"Original");project->save(project->path());doc->rename_annotation(object,"Captured");
        const auto saved=read(std::filesystem::u8path(project->path()));auto o=options(directory,"metadata-workbench","project");
        GuiInput down;down.frame=4;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=360./o.width;down.y=500./o.height;
        auto up=down;up.frame=5;up.kind=GuiInputKind::LeftUp;o.inputs={down,up,key(9,GuiInputKind::UndoKey)};
        const auto report=run_project_gui(project,20,(directory/"dialog-metadata-workbench.ppm").u8string(),o);
        check(report.replay_complete && report.project_error.empty() && doc->annotation(object).name=="Captured" && read(std::filesystem::u8path(project->path()))==saved,"Metadata modal allowed an underlying Undo or changed saved bytes");
    }
    for(const bool cancel:{true,false}) {
        const std::string name=cancel?"source-cancel":"source-apply";const auto source=directory/(name+".ppm"),path=directory/(name+".qpxproj");
        write(source,std::string("P6\n16 16\n255\n")+std::string(16*16*3,char(90)));
        auto project=std::make_shared<Project>();const auto image=project->add_image(source.u8string());project->save_new(path.u8string());
        const auto saved=read(path);auto o=options(directory,name,"workflow","files");o.file_fingerprint_path=source.u8string();o.file_fingerprint_associate_source=true;
        auto review=key(8,GuiInputKind::ReviewSourceVerificationKey);review.wait_for=GuiInputWait::FileFingerprintReady;o.inputs={key(3,GuiInputKind::FingerprintFileKey),review};
        if(cancel)o.inputs.push_back(key(16,GuiInputKind::EscapeKey));else click(o.inputs,16,GuiInputTarget::ApplySourceVerification);
        const auto report=run_project_gui(project,60,(directory/("dialog-"+name+".ppm")).u8string(),o);
        check(report.replay_complete && report.project_error.empty() && report.file_fingerprint_error.empty() && read(path)==saved,"Source dialog failed or published without an explicit project Save");
        const auto verification=project->source_verification(image);check(bool(verification)==!cancel,"Source dialog Cancel/Confirm dispatched the wrong native action");
        check(project->document(image)->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({90,90,90,255}),"Source review changed raw pixels");
        if(!cancel){check(verification->fingerprint.sha256.size()==64,"Source review lost its checksum");project->save(path.u8string());check(Project::open(path.u8string())->source_verification(image)->fingerprint.sha256==verification->fingerprint.sha256,"Source verification did not survive Save/reopen");}
    }
    for(const bool cancel:{true,false}) {
        const std::string name=cancel?"checkpoint-cancel":"checkpoint-delete";auto project=fixture(directory/(name+".qpxproj"),128,96);
        project->document(project->active_image())->add_rectangle({10,12,30,24},"Unsaved recovery object");
        const auto primary=read(std::filesystem::u8path(project->path()));const auto generation=project->generation();const auto recovery=directory/(name+".qpxrecover");
        {ProjectAutosave writer;check(writer.request(*project,recovery.u8string()) && writer.wait().state=="ready","Prepare native recovery checkpoint");}
        const auto bytes=read(recovery);auto o=options(directory,name,"project");o.recovery_path=recovery.u8string();o.inputs={key(3,GuiInputKind::ReviewRecoveryKey)};
        click(o.inputs,12,cancel?GuiInputTarget::CancelDiscardCheckpoint:GuiInputTarget::ConfirmDiscardCheckpoint);
        const auto report=run_project_gui(project,60,(directory/("dialog-"+name+".ppm")).u8string(),o);
        check(report.replay_complete && report.recovery_maintenance_error.empty() && project->generation()==generation && read(std::filesystem::u8path(project->path()))==primary,"Checkpoint dialog changed primary state or failed its native handler");
        if(cancel)check(report.checkpoints_deleted==0 && read(recovery)==bytes,"Checkpoint Cancel deleted or changed recovery bytes");
        else check(report.checkpoints_deleted==1 && !std::filesystem::exists(recovery),"Checkpoint Confirm did not await checked deletion");
    }
    std::cout<<"Registered native dialogs: metadata blocks Undo, source review Cancel/Apply and Save/reopen, checkpoint Cancel/checked delete passed\n";
}
