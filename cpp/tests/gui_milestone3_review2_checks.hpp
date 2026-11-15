#pragma once
#include "gui_milestone3_review_checks.hpp"
namespace milestone3_review2_checks {
using namespace qpx;
using A=detail::ModelSafetyTestAccess;
using model_safety_checks::click;
inline void pixel(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);
    auto image=[] {std::vector<std::uint8_t> pixels(32*16*3);for(int y=0;y<16;++y)for(int x=0;x<32;++x){auto i=(y*32+x)*3;pixels[i]=x<16?220:30;pixels[i+1]=x<16?30:220;pixels[i+2]=50;}return Image(32,16,std::move(pixels));};
    auto first=std::make_unique<Document>(image());Document second(image());Document* current=first.get();
    first->set_class("Old A",0xff0000);first->set_class("Old B",0x030303);
    const auto a=first->add_rectangle({0,0,14,16}),b=first->add_rectangle({18,0,14,16}),roi=first->add_rectangle({0,0,32,16});first->classify_annotation(a,"Old A");first->classify_annotation(b,"Old B");
    auto selection=std::make_unique<ObjectSelection>(*current);selection->replace({roi});
    GuiOptions o;o.pixel_open=true;o.pixel_object_id=roi;o.pixel_preview_budget=1024*1024;
    auto settings=std::make_shared<PixelTrainingOptions>();settings->calibrated=false;settings->features.sigmas={0};settings->max_samples_per_class=64;o.pixel_training_options=settings;
    auto forest=std::make_shared<ForestParameters>();forest->trees=3;forest->max_depth=3;o.pixel_parameters=forest;
    detail::GuiPixelClassifier panel(o);detail::RecordRemovalDialog removal;detail::OutputGate gate;std::string error;
    auto draw=[&]{panel.update(*current,*selection);panel.update_preview();panel.draw(*current,*selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,32,16},&removal,1,&gate,{dir.u8string()});model_safety_checks::mask(panel.preview());removal.draw(*current,1,error);gate.draw([](const auto&){return true;});};
    host.fit("Pixel classifiers",1100,1500,draw);
    auto train=[&]{++A::parameters(panel).seed;const auto old=panel.model();click(host,panel,GuiInputTarget::TrainPixelClassifier,draw);host.until(draw,[&]{return panel.model()!=old && panel.trained();},"Review2 train failed");check(A::unsaved(panel),"Review2 trained model warning absent");};
    auto keep=[&]{click(host,panel,GuiInputTarget::KeepPixelTraining,draw);host.until(draw,[&]{return !panel.archives().busy();},"Review2 Keep failed");check(!A::unsaved(panel),"Review2 kept model warning remains");return panel.archives().kept();};
    auto change=[&](Document& doc){current=&doc;selection=std::make_unique<ObjectSelection>(doc);host.settle(draw);};
    train();auto record=keep();const auto held=panel.model();change(second);check(panel.model()==held && !A::unsaved(panel),"Switch to B lost the open A model holder");
    change(*first);A::select(panel,record);click(host,panel,GuiInputTarget::RemovePixelRecord,draw);check(removal.pending(),"Review2 removal confirmation absent");record_list_checks::confirm_removal();host.settle(draw);check(error.empty() && A::unsaved(panel),"Returning to A did not recheck removed holder");
    // Removal invalidates the native capture: obtain a fresh training capture.
    train();record=keep();change(second);check(!A::unsaved(panel),"Second switch lost live holder");first.reset();host.settle(draw);check(A::unsaved(panel),"Destroyed holder document still counts as kept");
    core_path_checks::find(panel.paths(),"Export pixel model file").set((dir/"two-document-model.qpc").u8string());click(host,panel,GuiInputTarget::SavePixelModel,draw);host.until(draw,[&]{return !A::io(panel);},"Review2 Save failed");check(panel.io_state()=="saved" && !A::unsaved(panel),"Save did not remain a session fact after holder close");
    std::cout<<"K: train/keep A, B, A remove, keep B, destroy A, Save retention facts passed\n";
}
inline void opacity(const std::filesystem::path& dir,GLFWwindow* window) {
    model_safety_checks::GlHost host(window);std::vector<std::uint8_t> pixels(32*16*3);for(int y=0;y<16;++y)for(int x=0;x<32;++x){auto i=(y*32+x)*3;pixels[i]=x<16?220:30;pixels[i+1]=x<16?30:220;pixels[i+2]=50;}
    Document doc(Image(32,16,std::move(pixels)));doc.set_class("Old A",0xff0000);doc.set_class("Old B",0x030303);
    const auto a=doc.add_rectangle({0,0,14,16}),b=doc.add_rectangle({18,0,14,16}),roi=doc.add_rectangle({0,0,32,16});doc.classify_annotation(a,"Old A");doc.classify_annotation(b,"Old B");ObjectSelection selection(doc);selection.replace({roi});
    GuiOptions o;o.pixel_open=true;o.pixel_object_id=roi;o.pixel_preview_budget=1024*1024;
    auto settings=std::make_shared<PixelTrainingOptions>();settings->calibrated=false;settings->features.sigmas={0};settings->max_samples_per_class=64;o.pixel_training_options=settings;
    auto params=std::make_shared<ForestParameters>();params->trees=3;params->max_depth=3;o.pixel_parameters=params;
    std::shared_ptr<const PixelClassifier> model;std::uint64_t record=0;
    {
        detail::GuiPixelClassifier trainer(o);
        auto draw=[&]{trainer.update(doc,selection);trainer.draw(doc,selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,32,16});};
        host.fit("Pixel classifiers",1100,1500,draw);click(host,trainer,GuiInputTarget::TrainPixelClassifier,draw);host.until(draw,[&]{return trainer.trained();},"Opacity fixture train failed");model=trainer.model();
        click(host,trainer,GuiInputTarget::PredictPixelClasses,draw);host.until(draw,[&]{return trainer.predicted();},"Opacity fixture predict failed");click(host,trainer,GuiInputTarget::KeepPixelPrediction,draw);host.until(draw,[&]{return !trainer.archives().busy();},"Opacity fixture Keep failed");record=trainer.archives().kept();check(record,"Opacity fixture record absent");
    }
    const auto style=ImGui::GetStyle();
    for(bool working:{true,false}) {
        o.pixel_model=working?model:nullptr;detail::GuiPixelClassifier panel(o);
        auto draw=[&]{panel.update(doc,selection);panel.update_preview();panel.draw(doc,selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,32,16});model_safety_checks::mask(panel.preview());};
        host.fit("Pixel classifiers",1100,1500,draw);A::select(panel,record);click(host,panel,GuiInputTarget::PreviewPixelRecord,draw);host.settle(draw);
        check(bool(panel.model())==working && panel.historical_result(),"Opacity working/no-model setup failed");
        for(int locale:{0,2}) {
            ui::i18n::choose(locale);ImGui::GetStyle()=style;if(locale){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
            host.settle(draw);auto* w=ImGui::FindWindowByName(ui::i18n::label("Pixel classifiers").c_str());check(w,"Opacity window absent");ImGui::SetWindowPos(w,{24,24});ImGui::SetWindowSize(w,locale?ImVec2{1500,1500}:ImVec2{1100,1500});ImGui::SetScrollY(w,w->ScrollMax.y);host.settle(draw);
            const auto text=model_safety_checks::logged(host,draw);check(model_safety_checks::occurrences(text,ui::i18n::text("Opacity"))==1,"Kept mask must draw exactly one Opacity slider");
            const auto before=A::opacity(panel);const auto bounds=A::opacity_bounds(panel);const float fraction=before>.65f?.55f:.9f;
            host.click(Rect{bounds.x+bounds.width*fraction,bounds.y+ bounds.height/2,1,1},draw);host.settle(draw);
            check(A::opacity(panel)!=before,"Moving historical Opacity did not change it");const auto alpha=std::uint32_t(std::lround(A::opacity(panel)*255));for(auto color:A::palette(panel))check((color>>24)==alpha,"Historical palette ignored Opacity");
            host.capture(dir/(std::string("opacity-")+(working?"working":"no-model")+(locale?"-fr200.ppm":"-normal.ppm")),draw);
        }
        ui::i18n::choose(0);ImGui::GetStyle()=style;host.settle(draw);auto* w=ImGui::FindWindowByName("Pixel classifiers");ImGui::SetScrollY(w,w->ScrollMax.y);host.settle(draw);host.click("Close kept mask###Close kept pixel mask",draw);check(!panel.historical_result(),"Opacity test Close failed");
        if(!working){A::select(panel,record);click(host,panel,GuiInputTarget::UsePixelRecordModel,draw);}
        check(panel.model()==model || panel.model()->sha256()==model->sha256(),"Opacity changed model");for(auto color:A::live_palette(panel))check((color>>24)==std::uint32_t(std::lround(A::opacity(panel)*255)),"Close/adopt lost shared live Opacity");
    }
    ui::i18n::choose(0);ImGui::GetStyle()=style;
    std::cout<<"L: historical Opacity with/without working model, actual mouse changes, shared live alpha, normal/French200 overflow0 passed\n";
}
inline void run(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    record_list_checks::raw_pixel(dir);
    check(glfwInit()!=0,"Review2 GLFW init");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);auto* window=glfwCreateWindow(2200,1600,"Milestone 3 review2",nullptr,nullptr);check(window,"Review2 window");glfwMakeContextCurrent(window);pixel(dir,window);opacity(dir,window);glfwDestroyWindow(window);glfwTerminate();
}
}
