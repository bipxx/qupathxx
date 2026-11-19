// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>

using namespace qpx;

void check(bool success) {
    if (!success) throw std::runtime_error("Unexpected saved SVG annotation geometry or facts");
}

void rectangle(const Roi& roi, RoiKind kind, Rect expected) {
    const auto actual=roi.bounds();
    check(roi.kind()==kind && actual.x==expected.x && actual.y==expected.y &&
          actual.width==expected.width && actual.height==expected.height);
}

int main(int argc,char** argv) {
    try {
        if (argc!=4) throw std::invalid_argument("Expected saved project, image reader manifest and SVG profile");
        configure_reader_plugins(argv[2]);
        const auto project=Project::open_without_journal_recovery(argv[1]);
        const auto document=project->document(1);
        std::map<std::string,const Annotation*> shapes;
        for (const auto& item : document->objects()) {
            const auto& a=item.second;
            check(a.properties && a.plane==ObjectPlane(-1,0,0));
            const auto& props=a.properties->metadata();
            check(props.at("qpx.annotation-plugin.provider_id")=="org.qupathxx.svg-annotations");
            check(props.at("qpx.annotation-plugin.canvas_width")=="4" &&
                  props.at("qpx.annotation-plugin.canvas_height")=="3");
            check(shapes.emplace(props.at("qpx.annotation-plugin.source_id"),&a).second);
        }
        if (std::string_view(argv[3])=="writer") {
            check(shapes.size()==5);
            const auto& region=*shapes.at("region");
            rectangle(region.geometry,RoiKind::Rectangle,{1,1,1,1});
            check(region.properties->color_rgb()==0xcc3300 &&
                  region.properties->metadata().at("qpx.annotation-plugin.source.svg.group")=="adjusted" &&
                  region.properties->metadata().at("qpx.annotation-plugin.source.svg.element")=="rect");
            check(shapes.at("triangle")->geometry.equals(Roi::polygon({{.5,.5},{1.5,.5},{1,1.5}})));
            rectangle(shapes.at("ell")->geometry,RoiKind::Ellipse,{2,1.25,1,.5});
            const auto& trace=*shapes.at("trace");
            check(trace.geometry.equals(Roi::polyline({{.5,2},{1.5,2.5},{3,2}})) &&
                  trace.properties->color_rgb()==0x0044ff);
            const auto& ring=shapes.at("ring")->geometry;
            rectangle(ring,RoiKind::Polygon,{0,0,4,3});
            check(ring.polygon_parts().at(0).holes.size()==1 && ring.area()==10 &&
                  ring.contains({.5,.5}) && !ring.contains({2,1.5}));
        } else if (std::string_view(argv[3])=="affine") {
            check(shapes.size()==3);
            const auto& rect=*shapes.at("tilted");
            check(rect.geometry.equals(Roi::polygon({{1,0},{2,0},{2.5,1},{1.5,1}})) &&
                  rect.name=="A & B" && rect.properties->color_rgb()==0x0099ff);
            rectangle(shapes.at("sphere")->geometry,RoiKind::Ellipse,{.5,.25,1,.5});
            check(shapes.at("svg-shape-3")->geometry.equals(Roi::polyline({{0,1},{1,1},{1,2}})));
        } else throw std::invalid_argument("Unknown SVG oracle");
        std::cout<<"Verified exact native SVG geometry after project reopen\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
