#pragma once
#include "qupathxx/tma.hpp"
static void panel_refinement_workflows(const std::filesystem::path& directory,bool details_only=false) {
    for(const std::string panel:{"region","tissue","cells","annotations","tables","tma-detect","tma-manual","tma-import","pixel","object-information","active-information","active-stain-information","pixel-reveal"})for(const bool scaled:{false,true}) {
        if(details_only && panel!="active-information" && panel!="active-stain-information")continue;
        const auto name=panel+(scaled?"-fr-2x":"");const auto project=fixture(directory/(name+".qpxproj"),128,96);const auto doc=project->document(1);
        const auto id=doc->add_rectangle({8,8,44,60},"ROI###literal");doc->set_calibration(.5,.25);
        doc->set_object_properties(id,std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"Long imported field / source identity","A long historical value that must wrap and remain readable in the information view."}},std::map<std::string,double>{{"Reported area",12.123456789012345}}));
        doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},{255,255,255},"H&E",{"Hematoxylin","Eosin"}));
        auto o=options(directory,name,panel=="annotations" || panel=="object-information"?"annotations":"workflow",(panel=="active-information" || panel=="active-stain-information")?"tissue":panel=="region"||panel=="tissue"||panel=="cells"||panel=="tables"?panel.c_str():"tools",scaled?2:1);
        if(scaled) {o.width=1000;o.height=1000;auto ini=read(std::filesystem::u8path(o.layout_path));ini.replace(ini.find("language=en"),11,"language=fr");write(std::filesystem::u8path(o.layout_path),ini);}
        o.inputs={key(3,GuiInputKind::SelectAllKey)};
        if(panel.rfind("tma-",0)==0) {
            o.tma_open=true;if(panel=="tma-manual")o.tma_options=std::make_shared<const TmaGridOptions>();
            if(panel=="tma-import")o.qptma_input_path="Review grid.qptma";
        }
        if(panel=="pixel" || panel=="pixel-reveal")o.pixel_open=true;
        if(panel=="pixel-reveal") {GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::OpenPixelStreamBrowser;down.wait_for=GuiInputWait::TargetReady;auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs.push_back(down);o.inputs.push_back(up);}
        if(panel=="active-stain-information")o.tissue_feature=1;
        // Identical native basis values: labels must follow the captured mode.
        if(panel=="active-information" || panel=="active-stain-information")o.analysis_vectors={1,0,0,0,1,0};
        if(panel=="object-information" || panel=="active-information" || panel=="active-stain-information") {
            o.ui_scale=1;
            GuiInput down;down.frame=8;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;
            down.x=(panel=="object-information"?120.:358.)/o.width;down.y=(panel=="object-information"?440.:132.)/o.height;
            auto up=down;up.frame=9;up.kind=GuiInputKind::LeftUp;o.inputs.push_back(down);o.inputs.push_back(up);
            if(scaled){auto scale=key(14,GuiInputKind::SetUiScale);scale.x=2;o.inputs.push_back(scale);}
        }
        project->save(project->path());const auto revision=doc->revision();const auto bytes=read(std::filesystem::u8path(project->path()));
        const auto r=run_project_gui(project,20,(directory/(name+".ppm")).u8string(),o);
        check(r.replay_complete && r.ui_overflow_items==0,"Panel refinement bounds/replay: "+name+" / "+r.ui_overflow_label);
        check(doc->revision()==revision && read(std::filesystem::u8path(project->path()))==bytes,"Panel inspection changed native data: "+name);
        check(doc->pixel_width_um()==.5 && doc->pixel_height_um()==.25 && doc->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({30,120,210,255}),"Panel inspection changed calibration/RGB");
    }
    if(!details_only) {
        const auto project=fixture(directory/"manual-create.qpxproj",128,96);const auto doc=project->document(1);
        TmaGridOptions grid;grid.rows=1;grid.columns=2;grid.origin={24,32};grid.column_step={48,0};grid.width=grid.height=20;
        auto o=options(directory,"manual-create","workflow","tools");o.height=1100;o.tma_open=true;o.tma_options=std::make_shared<const TmaGridOptions>(grid);
        GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::CreateTmaGrid;down.wait_for=GuiInputWait::TargetReady;
        auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs={down,up};
        const auto r=run_project_gui(project,30,(directory/"manual-create.ppm").u8string(),o);
        check(r.replay_complete && r.tma_error.empty() && doc->tma_core_count()==2,"TMA Manual entry lost Create target/native edit");
        check(doc->undo() && doc->tma_core_count()==0,"TMA regrouping lost native undo");
    }
    if(details_only)std::cout<<"OD and stain details: four normal/French-200% views preserve native data with identical native basis values\n";
    else std::cout<<"Panel refinement: 26 normal/French-200% views preserve native data; TMA Manual Create/Undo passed\n";
}
