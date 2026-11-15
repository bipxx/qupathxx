#pragma once
#include "../gui_kept_counts.hpp"
#include "../gui_raw_pixel.hpp"
#include "../gui_pixel_classifier.hpp"
namespace qpx::detail {
struct GuiBatchTestAccess {
    static void raw_mode(GuiBatch& panel){panel.options_.kind=ProjectBatchKind::RawPixels;}
    static auto& options(GuiBatch& panel){return panel.options_;}
    static void image_settings(GuiBatch& panel,bool value){panel.use_image_settings_=value;}
    static auto& raw_x(GuiBatch& panel){return panel.raw_x_;}
    static auto& raw_y(GuiBatch& panel){return panel.raw_y_;}
    static bool selected_only(const GuiBatch& panel){return panel.selected_only_;}
    static auto sampling_target(const GuiBatch& panel){return panel.sampling_target_;}
    static auto sampling_walks(const GuiBatch& panel){return panel.sampling_target_walks_;}
    static auto sampling_visits(const GuiBatch& panel){return panel.sampling_target_visits_;}
    static auto effective_calls(const GuiBatch& panel){return panel.effective_calls_;}
};
struct GuiPixelClassifierTestAccess {static void provenance(GuiPixelClassifier& panel,std::uint64_t id){panel.session_published_ids_.insert(id);}};
struct GuiRawPixelTestAccess {
    static auto palette(GuiRawPixel& panel,const Document& doc,std::size_t size,std::shared_ptr<const RawPixelClassifier> model={}){return panel.palette(doc,std::vector<std::uint64_t>(size),model);}
};
}
namespace review_fix1_checks {
inline void cached_counts() {
    auto active=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80))),other=std::make_shared<Document>(Image(8,8,std::vector<std::uint8_t>(8*8*3,80)));
    std::size_t visits=0;
    struct Pointer {std::shared_ptr<Document> value;std::size_t* visits;const Document* get() const{++*visits;return value.get();}};
    struct Entry {Pointer document;};std::map<std::uint64_t,Entry> entries;
    entries.emplace(1,Entry{{active,&visits}});entries.emplace(2,Entry{{other,&visits}});
    for(std::uint64_t i=3;i<65;++i)entries.emplace(i,Entry{{nullptr,&visits}});
    detail::GuiKeptCounts cache;check(cache.update(entries,"project",0,0,1)==std::array<std::size_t,3>{0,0,0},"Initial kept counts/null entry");
    auto id=other->add_rectangle({0,0,4,4});StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*other,id,transform,AnalysisKind::StainMeans);auto record=other->keep_analysis(request,analyze(request));
    std::array<std::size_t,3> counts{};
    for(std::size_t i=0;i<entries.size();++i){visits=0;counts=cache.update(entries,"project",0,0,1);check(visits<=2,"Count cache walked the project per frame");}
    check(counts[0]==1,"Non-active image Keep did not refresh project totals");other->remove_analysis(record);
    for(std::size_t i=0;i<entries.size();++i){visits=0;counts=cache.update(entries,"project",0,0,1);check(visits<=2,"Removal refresh walked every image");}
    check(counts[0]==0,"Non-active image removal did not refresh project totals");
    std::cout<<"Review cache: non-active Keep/remove, null entries and at most two visits per unchanged frame passed\n";
}
inline void training_and_palette() {
    simplification_tools_checks::NativeUi host;Document doc(Image(32,32,std::vector<std::uint8_t>(32*32*3,80)));
    const auto area=doc.add_roi(Roi::rectangle({1,1,5,5}),"label"),detection=doc.add_detection(Roi::rectangle({10,1,5,5}),"detection",0,"A"),cell=doc.add_cell(Roi::rectangle({20,1,5,5}),Roi::ellipse({21,2,2,2}),"cell",0,"A");
    doc.classify_annotation(area,"A");ObjectSelection selection(doc);selection.replace({area,detection,cell});GuiOptions options;options.pixel_open=true;options.pixel_selected_only=true;
    detail::GuiPixelClassifier panel(options);
    auto draw=[&]{panel.update(doc,selection);panel.draw(doc,selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{});};
    host.fit("Pixel classifiers",900,1200,draw);detail::GuiPixelClassifierTestAccess::provenance(panel,area);ImGui::NewFrame();ImGui::LogToBuffer();draw();const std::string text=host.context->LogBuffer.c_str();ImGui::LogFinish();ImGui::Render();
    check(text.find("Training uses 1 labelled areas (1 added by a classifier in this session).")!=std::string::npos,"Selected training counts included cells/detections or lost provenance");
    detail::GuiRawPixel raw(GuiOptions{});const auto colors=detail::GuiRawPixelTestAccess::palette(raw,doc,3);
    check(colors.size()==3 && colors[0]!=colors[1] && colors[1]!=colors[2] && (colors[0]>>24)==115,"Null-model raw classes collapsed to grey");
    std::cout<<"Review training annotation/provenance counts and raw null-model categorical fallback passed\n";
}
inline void headless(){cached_counts();training_and_palette();}
}
