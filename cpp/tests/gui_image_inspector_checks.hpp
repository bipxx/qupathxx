#pragma once
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/image_analysis_settings.hpp"
static void image_inspector_workflows(const std::filesystem::path& directory) {
    for(const std::string mode:{"normal","scaled","unknown","information","information-escape","calibration-edit","calibration-cancel","workflow-defaults"}) {
        const auto project=fixture(directory/(mode+".qpxproj"),128,96);const auto doc=project->document(1);
        if(mode!="unknown")doc->set_calibration(.5,.25);
        doc->set_image_properties(std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"Case","A###literal"}},std::map<std::string,double>{{"Imported area",12.25}}));
        ImageDataValue value;value.value=std::string("Research <sample>");
        doc->set_image_data_properties(std::make_shared<const ImageDataProperties>(ImageDataValue::Map{{"Description",value}}));
        doc->set_image_analysis_settings(ImageAnalysisSettings("BRIGHTFIELD_H_E",{{.65,.70,.29},{.07,.99,.11}},{255,255,255},"H&E",{"Hematoxylin","Eosin"}));
        project->save(project->path());const auto before=read(std::filesystem::u8path(project->path()));const auto revision=doc->revision();
        auto o=options(directory,mode,mode=="workflow-defaults"?"workflow":"image","region",mode=="scaled"?2:1);
        if(mode=="scaled") {o.width=900;o.height=1000;}
        if(mode=="information" || mode=="information-escape") {
            GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=358./o.width;down.y=80./o.height;
            auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};
            if(mode=="information-escape")o.inputs.push_back(key(13,GuiInputKind::EscapeKey));
        }
        if(mode=="calibration-edit" || mode=="calibration-cancel") {GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=358./o.width;down.y=300./o.height;auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;o.inputs={down,up};if(mode=="calibration-cancel"){o.inputs.push_back(key(13,GuiInputKind::EscapeKey));}}
        if(mode=="workflow-defaults") {
            GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::UseImageStains;down.wait_for=GuiInputWait::TargetReady;
            auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs={down,up};
        }
        const auto r=run_project_gui(project,22,(directory/(mode+".ppm")).u8string(),o);
        check(r.replay_complete && r.ui_overflow_items==0,"Image inspector replay/overflow: "+mode+" / "+r.ui_overflow_label);
        check(doc->revision()==revision && before==read(std::filesystem::u8path(project->path())),"Image inspection changed persisted inputs");
        check(doc->pixel_width_um()==(mode=="unknown"?std::optional<double>{}:std::optional<double>{.5}),"Image inspector changed calibration");
        check(doc->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({30,120,210,255}),"Image inspector changed raw RGB");
    }
    std::cout<<"Flat Image inspector: normal/scaled/unknown, read-only information/Escape and explicit saved stains without ROI passed\n";
}
