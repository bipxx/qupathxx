#include "qupathxx/core.hpp"
#include "../selection_ranges.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("selection check failed"); }
int main() {
    try {
        {
            Document doc(Image::demo());const auto a=doc.add_rectangle({0,0,5,5});const auto detection=doc.add_detection(Roi::rectangle({10,0,5,5}));const auto b=doc.add_rectangle({20,0,5,5}),c=doc.add_rectangle({30,0,5,5});
            ObjectSelection range(doc);const auto revision=doc.revision();const std::vector<std::uint64_t> shown{c,a,b};
            detail::select_ordered_range(range,shown,c,b);check(range.size()==3 && !range.contains(detection) && range.primary()==b);
            range.replace({detection});detail::select_ordered_range(range,shown,a,b,true);check(range.ids()==std::vector<std::uint64_t>({a,detection,b}) && range.primary()==b);
            detail::select_ordered_range(range,shown,999,c);check(range.ids()==std::vector<std::uint64_t>{c});
            bool refused=false;try {detail::select_ordered_range(range,shown,c,detection);}catch(const std::out_of_range&){refused=true;}check(refused && range.ids()==std::vector<std::uint64_t>{c} && doc.revision()==revision);
            std::cout<<"Ordered filtered ranges: non-monotonic order, additive hidden selection, missing anchor and unchanged document passed\n";
        }
        Document d(Image::demo());
        auto a=d.add_rectangle({0,0,5,5}),b=d.add_rectangle({10,0,5,5}),c=d.add_rectangle({20,0,5,5});
        ObjectSelection s(d); s.replace({a,b,a}); check(s.size()==2 && s.primary()==a);
        s.toggle(a); check(s.primary()==b && !s.contains(a));
        s.select_range(c,a); check(s.size()==3 && s.primary()==a);
        s.select_region({0,0,15,5}); check(s.ids()==std::vector<std::uint64_t>{a,b});
        d.classify_objects(s.ids(),"Tumor"); check(d.class_count("Tumor")==2);
        d.remove_objects({a,b,a}); check(s.size()==0 && d.object_count()==1);
        check(d.undo() && d.class_count("Tumor")==2 && s.size()==0);
        s.select_all(); check(s.size()==3);
        const auto revision=d.revision();
        try { d.reparent_annotation(a,b); throw std::runtime_error("invalid parent accepted"); }
        catch(const std::invalid_argument&) {}
        check(d.revision()==revision && s.size()==3);
        std::cout<<"Selection, classification and atomic delete passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
