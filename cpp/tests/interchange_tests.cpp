#include "qupathxx/interchange.hpp"
#include "../object_properties_codec.hpp"
#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("interchange check failed"); }
int main() {
    try {
        const ObjectProperties facts({{"note","native"}},{{"Area",3.5},{"negative zero",-0.0},{"missing",std::numeric_limits<double>::quiet_NaN()},{"infinity",std::numeric_limits<double>::infinity()}});
        auto bytes=detail::encode_object_properties(facts);const auto decoded=detail::decode_object_properties(bytes.data(),bytes.size());
        check(facts.equals(*decoded) && std::isnan(decoded->measurements().at("missing")) && std::signbit(decoded->measurements().at("negative zero")));
        bytes.back()^=1;bool bad_checksum=false;try {detail::decode_object_properties(bytes.data(),bytes.size());}catch(const std::invalid_argument&){bad_checksum=true;}check(bad_checksum);
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
        Document properties(Image::demo());auto item=properties.add_rectangle({1,1,10,10});
        properties.set_object_properties(item,std::make_shared<const ObjectProperties>(facts));const auto owned=properties.annotation(item).properties;
        properties.rename_annotation(item,"Preserved");check(properties.annotation(item).properties==owned);
        const auto exported_properties=export_geojson(properties);Document reimported(Image::demo());import_geojson(reimported,exported_properties.text);
        check(reimported.annotation(1).properties->equals(facts));
        properties.set_object_properties(item,nullptr);check(!properties.annotation(item).properties && properties.undo() && properties.annotation(item).properties==owned);
        std::cout<<"Object properties: immutable history, special numbers, checksummed codec and lossless GeoJSON round trip passed\n";
        auto nested=export_geojson(source,{},false,true);
        check(nested.features==2 && nested.issues.empty());
        check(nested.text.find("childObjects")!=std::string::npos);
        Document nested_target(Image::demo());
        auto nested_import=import_geojson(nested_target,nested.text);
        check(nested_import.features==2 && nested_import.issues.empty());
        check(nested_target.annotation(2).parent_id==1 && nested_target.annotation(2).nucleus->area()==9);
        check(export_geojson(source,{},false,false).text==exported.text);
        Document deep(Image::demo()); std::uint64_t previous=0;
        for(int i=0;i<32;++i) previous=deep.add_detection(Roi::rectangle({0,0,100,100}),"Level",previous);
        Document restored(Image::demo());
        check(import_geojson(restored,export_geojson(deep,{},false,true).text).features==32);
        deep.add_detection(Roi::rectangle({0,0,100,100}),"Too deep",previous);
        rejected=false;try {export_geojson(deep,{},false,true);} catch(const std::length_error&) {rejected=true;}
        check(rejected && export_geojson(deep).features==33);
        std::cout<<"Native nested GeoJSON hierarchy, nucleus, flat compatibility and depth bounds passed\n";
        std::cout<<"Native interchange atomicity and exact rectangle round trip passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
