#include "qupathxx/interchange.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("interchange check failed"); }
int main() {
    try {
        Document source(Image::demo());
        auto parent=source.add_rectangle({0,0,100,100},"Parent");
        source.add_cell(Roi::rectangle({10,10,20,20}),Roi::rectangle({15,15,3,3}),"Cell",parent,"Tumor");
        check(measurements_csv(source).find("Cell")!=std::string::npos);
        if(!has_interchange()) {
            bool rejected=false; try { export_geojson(source); } catch(const std::runtime_error&) { rejected=true; }
            check(rejected); return 0;
        }
        auto exported=export_geojson(source);
        check(exported.text==export_geojson(source).text);
        Document target(Image::demo()); target.add_rectangle({200,200,1,1});
        auto imported=import_geojson(target,exported.text);
        check(imported.imported_ids==std::vector<std::uint64_t>({2,3}));
        check(target.annotation(3).parent_id==2 && target.annotation(3).nucleus->area()==9);
        check(target.undo() && target.objects().size()==1);
        bool rejected=false; try { import_geojson(target,"{bad}"); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected && target.redo() && target.objects().size()==3);
        std::cout<<"Native interchange atomicity and exact rectangle round trip passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
