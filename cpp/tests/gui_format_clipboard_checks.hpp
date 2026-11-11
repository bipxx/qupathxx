#pragma once
#include "qupathxx/interchange.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/raw_image.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <limits>

namespace format_clipboard_checks {
// GLFW exposes text only. Preserve that supported clipboard state around the
// replay; arbitrary native clipboard formats are outside its transport API.
struct ClipboardWindow {
    GLFWwindow* window=nullptr;
    ClipboardWindow() {
        check(glfwInit()!=0,"Clipboard GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
        window=glfwCreateWindow(1,1,"Clipboard transport",nullptr,nullptr);
        if(!window) {glfwTerminate();throw std::runtime_error("Clipboard transport window failed");}
    }
    ~ClipboardWindow() {if(window)glfwDestroyWindow(window);glfwTerminate();}
};
struct RestoreClipboardText {
    std::string previous;
    RestoreClipboardText() {ClipboardWindow native;if(const auto* text=glfwGetClipboardString(native.window))previous=text;}
    ~RestoreClipboardText() {try {ClipboardWindow native;glfwSetClipboardString(native.window,previous.c_str());}catch(...) {}}
};
static void check_rectangle(const Roi& geometry,Rect expected,const std::string& label) {
    const auto value=geometry.bounds();
    check(geometry.kind()==RoiKind::Rectangle && value.x==expected.x && value.y==expected.y && value.width==expected.width && value.height==expected.height,
        "Clipboard changed exact pixel geometry: "+label);
}
static void check_graph(const Document& document,std::uint64_t parent,std::uint64_t child) {
    const auto a=document.annotation(parent),b=document.annotation(child);
    check(a.name=="Parent" && b.name=="Cell" && b.parent_id==parent && a.parent_id==0 && b.kind==ObjectKind::Cell,
        "Clipboard changed native object graph");
    check_rectangle(a.geometry,{1,1,40,40},"parent");check_rectangle(b.geometry,{4,5,10,12},"cell");
    check(b.nucleus.has_value(),"Clipboard lost cell nucleus");check_rectangle(*b.nucleus,{6,7,2,3},"nucleus");
    check(a.plane==ObjectPlane{} && b.plane==ObjectPlane{} && b.class_name=="Tumor" && document.object_class("Tumor").color_argb()==0x80ff2030,
        "Clipboard changed class ARGB or source plane");
    check(b.properties && b.properties->metadata()==std::map<std::string,std::string>{{"note","clipboard"}} &&
        b.properties->measurements().at("Area")==3.5 && std::isnan(b.properties->measurements().at("missing")) &&
        std::signbit(b.properties->measurements().at("negative zero")) && b.locked() && b.color_rgb()==0x123456 && b.class_probability()==.875,
        "Clipboard changed historical properties or permitted probability extension");
}
static void run(const std::filesystem::path& directory) {
    check(has_interchange(),"Native format clipboard replay requires interchange support");
    RestoreClipboardText restore;
    const auto project=fixture(directory/"clipboard-source.qpxproj",80,60);const auto source=project->document(1);
    source->set_class_definition(ObjectClass::definition("Tumor",0x80ff2030));
    const auto parent=source->add_rectangle({1,1,40,40},"Parent");
    const auto child=source->add_cell(Roi::rectangle({4,5,10,12}),Roi::rectangle({6,7,2,3}),"Cell",parent,"Tumor");
    source->set_object_properties(child,std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","clipboard"}},
        std::map<std::string,double>{{"Area",3.5},{"missing",std::numeric_limits<double>::quiet_NaN()},{"negative zero",-0.0}},true,0x123456,.875));
    source->set_calibration(.25,.5);project->save(project->path());const auto original_project=read(std::filesystem::u8path(project->path()));
    auto o=options(directory,"clipboard-graph","annotations");
    o.inputs={key(4,GuiInputKind::SelectAllKey),key(8,GuiInputKind::CopyObjectsKey),key(12,GuiInputKind::PasteObjectsKey)};
    const auto copied=run_project_gui(project,20,(directory/"clipboard-graph.ppm").u8string(),o);
    check(copied.replay_complete && copied.clipboard_error.empty() && copied.clipboard_copies==1 && copied.clipboard_pastes==1 &&
        copied.clipboard_copied_objects==2 && copied.clipboard_pasted_objects==2 && copied.selected_ids.size()==2 && source->object_count()==4,
        "Ctrl+C/Ctrl+V did not dispatch exactly one two-object clipboard transfer: "+copied.clipboard_error);
    const auto clone_parent=copied.selected_ids[0],clone_child=copied.selected_ids[1];
    check(clone_parent>child && clone_child>clone_parent && clone_parent!=parent && clone_child!=child,"Clipboard reused existing native IDs");
    check_graph(*source,clone_parent,clone_child);
    check(source->pixel_width_um()==.25 && source->pixel_height_um()==.5 && source->undo_label()=="Paste objects",
        "Clipboard changed source calibration or compound history");
    const auto revision=source->revision();
    o.inputs={key(4,GuiInputKind::UndoKey),key(8,GuiInputKind::RedoKey)};
    const auto history=run_project_gui(project,16,(directory/"clipboard-history.ppm").u8string(),o);
    check(history.replay_complete && history.clipboard_error.empty() && source->revision()==revision+2 && source->object_count()==4,
        "Clipboard compound undo/redo did not replay");
    check_graph(*source,clone_parent,clone_child);
    check(read(std::filesystem::u8path(project->path()))==original_project,"Clipboard replay wrote source project bytes");

    // The source window is closed. Paste pixel geometry into an independently
    // calibrated target, retaining its existing object and allocating fresh IDs.
    Document target(Image(80,60,std::vector<std::uint8_t>(80*60*3,40)));target.set_calibration(2,3);
    const auto sentinel=target.add_rectangle({65,45,2,2},"Existing");
    auto destination=options(directory,"clipboard-target","annotations");destination.inputs={key(5,GuiInputKind::PasteObjectsKey)};
    const auto pasted=run_gui(target,15,(directory/"clipboard-target.ppm").u8string(),destination);
    check(pasted.replay_complete && pasted.clipboard_error.empty() && pasted.clipboard_copies==0 && pasted.clipboard_pastes==1 &&
        pasted.clipboard_pasted_objects==2 && pasted.selected_ids==std::vector<std::uint64_t>({sentinel+1,sentinel+2}) && target.object_count()==3,
        "Cross-window paste did not allocate the target's new native IDs: "+pasted.clipboard_error);
    check_graph(target,pasted.selected_ids[0],pasted.selected_ids[1]);check_rectangle(target.annotation(sentinel).geometry,{65,45,2,2},"existing target");
    check(target.pixel_width_um()==2 && target.pixel_height_um()==3,"Clipboard rescaled target calibration");

    Document collision(Image(80,60,std::vector<std::uint8_t>(80*60*3,40)));collision.set_class("Tumor",0x123456);
    const auto collision_revision=collision.revision();
    destination=options(directory,"clipboard-refusal","annotations");destination.inputs={key(5,GuiInputKind::PasteObjectsKey)};
    const auto refused=run_gui(collision,15,(directory/"clipboard-refusal.ppm").u8string(),destination);
    check(refused.replay_complete && refused.clipboard_error.find("import class conflicts with existing definition")!=std::string::npos &&
        refused.clipboard_pastes==0 && collision.revision()==collision_revision &&
        collision.object_count()==0 && collision.classes().size()==1 && collision.object_class("Tumor").color==0x123456,
        "Clipboard class collision did not refuse atomically");

    if(has_tiff()) {
        const auto path=directory/"clipboard-plane.tiff";qpx_test::raw_pixel_tiff_fixture(path,80,60);const auto original_tiff=read(path);
        TiffOpenOptions input;input.view.channels={1,-1,-1};input.view.lower={0,0,0};input.view.upper={65535,65535,65535};
        Document plane(open_tiff(path.u8string(),input));const auto original=plane.add_rectangle({7,9,11,13},"Plane object");plane.set_object_plane(original,{1,0,0});
        auto plane_options=options(directory,"clipboard-plane-copy","annotations");plane_options.inputs={key(4,GuiInputKind::SelectAllKey),key(8,GuiInputKind::CopyObjectsKey)};
        const auto before=run_gui(plane,16,"",plane_options);
        check(before.replay_complete && before.clipboard_copies==1 && before.clipboard_copied_objects==1 && before.clipboard_error.empty(),"Channel-specific clipboard copy failed");
        plane_options=options(directory,"clipboard-plane-paste","annotations");plane_options.inputs={key(5,GuiInputKind::PasteCurrentPlaneKey)};
        const auto after=run_gui(plane,15,(directory/"clipboard-plane.ppm").u8string(),plane_options);
        check(after.replay_complete && after.clipboard_pastes==1 && after.clipboard_error.empty() && after.selected_ids.size()==1 && after.selected_ids.front()!=original &&
            plane.annotation(original).plane==ObjectPlane(1,0,0) && plane.annotation(after.selected_ids.front()).plane==ObjectPlane(-1,0,0),
            "Ctrl+Shift+V changed source plane or failed to adopt displayed plane");
        check_rectangle(plane.annotation(after.selected_ids.front()).geometry,{7,9,11,13},"displayed plane");
        check(read(path)==original_tiff,"Clipboard replay wrote quantitative source bytes");
    }
    check(read(std::filesystem::u8path(project->path()))==original_project,"Cross-window clipboard replay wrote source project bytes");
    std::cout<<"Native format clipboard: Ctrl+C/V graph, exact pixels/classes/properties/calibration, new IDs, history and atomic refusal passed; displayed-plane route "
        <<(has_tiff()?"passed":"unavailable (TIFF disabled)")<<'\n';
}
}
