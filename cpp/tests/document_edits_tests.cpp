#include "qupathxx/core.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("document edit check failed"); }
int main() {
    try {
        Document d(Image::demo());
        auto parent=d.add_rectangle({0,0,200,200},"Parent");
        auto child=d.execute({ObjectEdit::create({10,10,20,20},"Child",parent,"Tumor")}).at(0);
        const auto revision=d.revision();
        try {
            d.execute({ObjectEdit::replace(child,{20,20,10,10},"Changed",parent),ObjectEdit::create({-1,0,1,1})});
            throw std::runtime_error("invalid batch was accepted");
        } catch(const std::invalid_argument&) {}
        check(d.revision()==revision && d.annotation(child).name=="Child");
        check(d.children(parent)==std::vector<std::uint64_t>{child});
        check(d.remove_annotation(parent) && d.object_count()==0);
        check(d.undo() && d.object_count()==2 && d.annotation(child).class_name=="Tumor");
        check(d.children(parent)==std::vector<std::uint64_t>{child});
        check(d.redo() && d.object_count()==0);
        check(d.undo()); d.set_calibration(.5,.25); check(d.area_um2(child)==50);
        check(d.undo() && !d.pixel_width_um()); check(d.redo() && d.pixel_height_um()==.25);
        d.clear_history(); d.set_history_limits(1,1024*1024);
        d.rename_annotation(child,"Renamed"); d.classify_annotation(child,"Stroma");
        check(d.undo() && d.annotation(child).class_name=="Tumor" && !d.undo());
        auto cell=d.add_cell(Roi::rectangle({30,30,10,10}),Roi::ellipse({60,60,4,4}),"Cell",parent);
        check(d.annotation(cell).kind==ObjectKind::Cell && d.annotation(cell).is_detection());
        check(d.pick({62,62})==std::vector<std::uint64_t>{parent,cell});
        const auto builds=d.spatial_stats().builds;
        d.update_nucleus(cell,Roi::rectangle({100,100,2,2}));
        check(d.pick({101,101})==std::vector<std::uint64_t>{parent,cell});
        check(d.spatial_stats().builds==builds);
        check(d.undo() && d.annotation(cell).nucleus->kind()==RoiKind::Ellipse);
        d.rename_annotation(cell,"Renamed cell");
        check(d.annotation(cell).kind==ObjectKind::Cell && d.annotation(cell).nucleus.has_value());
        d.set_history_limits(128,64*1024*1024);
        check(d.remove_annotation(parent) && d.object_count()==0);
        check(d.undo() && d.annotation(cell).nucleus.has_value());
        const auto saved=d.revision();
        try { d.update_nucleus(cell,Roi::rectangle({-1,0,1,1})); throw std::runtime_error("invalid nucleus accepted"); }
        catch(const std::invalid_argument&) {}
        check(d.revision()==saved);
        Document facts(Image::demo());auto roi=facts.add_rectangle({1,2,4,5});
        const auto object_revision=facts.annotation(roi).revision,render=facts.render_revision(),structure=facts.structure_revision();
        auto properties=std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","root"}},std::map<std::string,double>{{"Area",12.5}});
        facts.set_image_properties(properties);check(facts.image_properties()==properties && facts.object_count()==1);
        check(facts.annotation(roi).revision==object_revision && facts.render_revision()==render && facts.structure_revision()==structure);
        check(facts.undo() && !facts.image_properties() && facts.redo() && facts.image_properties()==properties);
        const auto property_revision=facts.revision();facts.set_image_properties(properties);check(facts.revision()==property_revision);
        bool invalid_state=false;try {facts.set_image_properties(std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{},std::map<std::string,double>{},true));}catch(const std::invalid_argument&){invalid_state=true;}
        check(invalid_state && facts.revision()==property_revision && facts.image_properties()==properties);
        std::cout<<"Native image facts: immutable history, ROI separation and unchanged render/spatial revisions passed\n";
        const ImageAnalysisSettings defaults("BRIGHTFIELD_OTHER",{{1,0,0},{0,1,0},{0,0,1}},{240,245,250},"Three",{"R","G","B"});
        const auto facts_render=facts.render_revision(),facts_structure=facts.structure_revision();facts.set_image_analysis_settings(defaults);
        check(facts.image_analysis_settings()->equals(defaults) && facts.image_analysis_settings()->rgb8_supported());
        check(facts.render_revision()==facts_render && facts.structure_revision()==facts_structure);
        check(facts.undo() && !facts.image_analysis_settings() && facts.redo() && facts.image_analysis_settings()->equals(defaults));
        facts.set_image_analysis_settings({});check(!facts.image_analysis_settings() && facts.undo() && facts.image_analysis_settings()->equals(defaults));
        std::cout<<"Native image defaults: owned values, history, clear and unchanged ROI/render authority passed\n";
        auto data=std::make_shared<const ImageDataProperties>(ImageDataValue::Map{{"exact",ImageDataValue{std::int64_t(9007199254740993LL)}},{"nested",ImageDataValue{ImageDataValue::List{ImageDataValue{true},ImageDataValue{}}}}});
        const auto data_render=facts.render_revision(),data_structure=facts.structure_revision();facts.set_image_data_properties(data);
        check(facts.image_data_properties()==data && facts.render_revision()==data_render && facts.structure_revision()==data_structure);
        check(facts.undo() && !facts.image_data_properties() && facts.redo() && facts.image_data_properties()==data);
        facts.set_image_data_properties({});check(!facts.image_data_properties() && facts.undo() && facts.image_data_properties()==data);
        std::cout<<"Native ImageData properties: typed values, immutable history, clear and unchanged render/ROI authority passed\n";
        std::cout<<"Atomic edits, subtree restore, calibration and bounded undo passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
