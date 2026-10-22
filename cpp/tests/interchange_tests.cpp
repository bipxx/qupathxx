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
            check(rejected);
            rejected=false;try {import_geojson(source,"{}",true,true,true);} catch(const std::runtime_error&) {rejected=true;}
            check(rejected); return 0;
        }
        const std::string foreign=R"({"type":"Feature","geometry":{"type":"Polygon","isEllipse":true,"coordinates":[[[1.123,2.234],[3.987,2.234],[3.987,5.876],[1.123,5.876],[1.123,2.234]]]},"properties":{}})";
        Document precision(Image::demo());bool precision_rejected=false;
        if(has_geos()) {
            try {import_geojson(precision,foreign,true,false,true);} catch(const std::invalid_argument&) {precision_rejected=true;}
            check(precision_rejected && precision.objects().empty());
            auto converted=import_geojson(precision,foreign,true,true,true);
            const auto bounds=precision.annotation(converted.imported_ids.front()).geometry.bounds();
            const double x=static_cast<float>(1.12),y=static_cast<float>(2.23);
            check(bounds.x==x && bounds.y==y && bounds.width==double(float(3.99))-x && bounds.height==double(float(5.88))-y);
            check(converted.issues.size()==2 && converted.issues.back().code=="qupath_ellipse_precision_changed");
        } else {
            try {import_geojson(precision,foreign,true,true,true);} catch(const std::runtime_error&) {precision_rejected=true;}
            check(precision_rejected && precision.objects().empty());
            check(import_geojson(precision,foreign,true).imported_ids.size()==1);
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
