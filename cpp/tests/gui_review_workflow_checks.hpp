#pragma once
static void review_ux_workflows(const std::filesystem::path& directory) {
    GuiOptions fresh;fresh.docked_workspace=true;GuiWorkspace new_workspace(fresh);check(new_workspace.annotations_only && new_workspace.objects_visible,"fresh workbench defaults");
    GuiOptions compact;GuiWorkspace compact_workspace(compact);check(!compact_workspace.annotations_only,"compact viewer retains all objects");
    auto image_click=[](std::vector<GuiInput>& events,int frame,double x,double y,bool shift=false) {
        GuiInput down;down.frame=frame;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=x;down.y=y;down.shift=shift;
        auto up=down;up.frame=frame+1;up.kind=GuiInputKind::LeftUp;events.push_back(down);events.push_back(up);
    };
    for(const std::string mode:{"hidden-pick","hidden-marquee","delete-restores","eye-toggle","filtered-selection","measurements"}) {
        const auto project=fixture(directory/(mode+".qpxproj"),128,96);const auto doc=project->document(1);
        const auto annotation=doc->add_rectangle({8,8,44,60},"ROI###Literal");
        const auto detection=doc->add_detection(Roi::rectangle({70,10,30,30}),"Detection",0,"Tumor");doc->add_rectangle({104,48,16,24},"Second ROI");
        doc->set_calibration(.5,.25);project->save(project->path());const auto bytes=read(std::filesystem::u8path(project->path()));const auto revision=doc->revision();
        auto o=options(directory,mode,"annotations");const bool hidden=mode=="hidden-pick" || mode=="hidden-marquee" || mode=="delete-restores";
        auto ini=read(std::filesystem::u8path(o.layout_path));ini+="; qupathxx objects-visible="+std::string(hidden?"0":"1")+"\n; qupathxx object-filter=annotations\n";write(std::filesystem::u8path(o.layout_path),ini);
        if(mode=="hidden-pick")image_click(o.inputs,5,25,25);
        if(mode=="hidden-marquee") {GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Image;down.x=2;down.y=2;down.shift=true;auto up=down;up.frame=8;up.kind=GuiInputKind::LeftUp;up.x=120;up.y=80;o.inputs={down,up};}
        if(mode=="delete-restores")o.inputs={key(3,GuiInputKind::SelectAllKey),key(8,GuiInputKind::DeleteKey)};
        if(mode=="eye-toggle") {GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;down.x=782./o.width;down.y=46./o.height;auto up=down;up.frame=6;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;o.inputs={down,up};}
        const auto output=directory/(mode+".geojson");
        if(mode=="filtered-selection") {image_click(o.inputs,4,80,20);o.interchange_path=output.u8string();o.inputs.push_back(key(10,GuiInputKind::ExportKey));}
        if(mode=="measurements")o.inputs={key(4,GuiInputKind::SelectAllKey)};
        const auto result=run_project_gui(project,24,(directory/(mode+".ppm")).u8string(),o);
        check(result.replay_complete && result.ui_overflow_items==0,"review UX replay/overflow: "+mode+" / "+result.ui_overflow_label);
        check(doc->object_count()==3 && doc->revision()==revision && read(std::filesystem::u8path(project->path()))==bytes,"presentation changed document/project: "+mode);
        check(doc->server()->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>({30,120,210,255}),"presentation changed raw image bytes");
        const auto saved=read(std::filesystem::u8path(o.layout_path));const bool should_hide=mode=="hidden-pick" || mode=="hidden-marquee" || mode=="eye-toggle";
        check(saved.find(std::string("; qupathxx objects-visible=")+(should_hide?"0":"1"))!=std::string::npos,"visibility state did not persist: "+mode);
        check(saved.find("; qupathxx object-filter=annotations")!=std::string::npos,"canvas selection changed object filter");
        if(mode=="hidden-pick" || mode=="hidden-marquee")check(result.selected_ids.empty(),"hidden picking/marquee changed selection");
        if(mode=="delete-restores")check(result.selected_ids.size()==3,"first hidden Delete changed selection");
        if(mode=="filtered-selection") {check(result.selected_ids==std::vector<std::uint64_t>{detection},"canvas detection selection must remain independent of filter");check(read(output)==export_geojson(*doc).text,"list filter changed export scope");}
        if(mode=="measurements") {const auto shape=doc->shape_measurements(annotation);check(shape.roi.area_px2==2640 && shape.roi.area_um2==330,"primary shape values are not native calibrated measurements");}
    }
    std::cout<<"Review UX: hidden pick/marquee, first Delete restore, actual Eye toggle, filter/selection/export independence, native measurements and persistence passed\n";
}
