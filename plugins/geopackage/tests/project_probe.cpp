// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>

using namespace qpx;

void check(bool success,const char* what) {
    if (!success) throw std::runtime_error(what);
}

int main(int argc,char** argv) {
    try {
        if (argc!=4) throw std::invalid_argument("Expected project, reader manifest and oracle profile");
        configure_reader_plugins(argv[2]);
        const auto project=Project::open_without_journal_recovery(argv[1]);
        const auto document=project->document(1);
        const std::string_view profile(argv[3]);
        const bool wkb=profile=="wkb";
        std::map<std::string,const Annotation*> shapes;
        for (const auto& item:document->objects()) {
            const auto& object=item.second;
            check(object.properties && object.plane==ObjectPlane(-1,0,0),"Annotation properties or plane differ");
            const auto& props=object.properties->metadata();
            check(props.at("qpx.annotation-plugin.provider_id") ==
                  (wkb?"org.qupathxx.wkt-wkb":"org.qupathxx.geopackage"),"Wrong annotation receipt");
            check(shapes.emplace(props.at("qpx.annotation-plugin.source_id"),&object).second,"Duplicate mapped ID");
        }
        if (wkb) {
            check(shapes.size()==1 &&
                  shapes.at("geometry")->geometry.equals(Roi::points({{.5,.75}})),"WKB point differs");
        } else {
            check(shapes.size()==3,"Unexpected GeoPackage object count");
            const auto& spot=*shapes.at("gpkg-fid-1");
            const auto& trace=*shapes.at("gpkg-fid-2");
            const auto& region=*shapes.at("gpkg-fid-3");
            const bool reflected=profile=="projected-reflected";
            const bool projected=reflected || profile=="projected" || profile=="projected-local-srs";
            if(reflected) {
                check(spot.geometry.equals(Roi::points({{3.5,.75}})) &&
                      trace.geometry.equals(Roi::polyline({{3,.5},{2,1.5},{1,1}})),"Reflected point or line differs");
            } else {
                check(spot.geometry.equals(Roi::points({{.5,.75}})) &&
                      trace.geometry.equals(Roi::polyline({{1,.5},{2,1.5},{3,1}})),"Point or line differs");
            }
            check(region.geometry.polygon_parts().at(0).holes.size()==1 &&
                  region.geometry.area()==10 && !region.geometry.contains({2,1.5}) &&
                  region.geometry.contains({.5,.5}),"Polygon hole or area differs");
            const auto& facts=spot.properties->metadata();
            const bool renamed=profile=="quoted-big-endian";
            check(facts.at("qpx.annotation-plugin.source.gpkg.layer")== (renamed?"microscopy \"regions\"":"rois") &&
                  facts.at("qpx.annotation-plugin.source.gpkg.fid")=="1" &&
                  facts.at("qpx.annotation-plugin.source.gpkg.srs_id")==
                      (profile=="projected-local-srs"?"10001":projected?"3857":"-1"),"GeoPackage source facts differ");
            if(projected) {
                check(facts.at("qpx.annotation-plugin.source.gpkg.source_crs")=="EPSG:3857","Saved CRS differs");
                const auto coefficients=reflected?"[-1,0,-996,0,1,-2000]":"[1,0,1000,0,1,-2000]";
                check(facts.at("qpx.annotation-plugin.options").find(
                    std::string("\"source_to_pixels\":")+coefficients)!=std::string::npos,"Saved affine differs");
            }
            if (renamed) check(spot.name=="gpkg-fid-1","Unnamed feature fallback differs");
            else check(spot.name=="Spot" && trace.name=="Trace" && region.name=="Region","Saved feature names differ");
        }
        std::cout<<"Verified native annotation geometry and source facts after saved project reopen\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
