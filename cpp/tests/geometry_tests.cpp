#include "qupathxx/core.hpp"
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("geometry check failed"); }
Ring square(double a,double b) { return {{a,a},{b,a},{b,b},{a,b}}; }
void centroid_is(const Roi& roi,double x,double y) {
    const auto point=roi.centroid();
    check(std::abs(point.x-x)<=1e-9+1e-12*std::abs(x));
    check(std::abs(point.y-y)<=1e-9+1e-12*std::abs(y));
}
void centroid_contracts() {
    centroid_is(Roi::rectangle({2,4,8,6}),6,7);
    centroid_is(Roi::ellipse({2,4,8,6}),6,7);
    centroid_is(Roi::points({{0,0},{6,0},{0,3}}),2,1);
    centroid_is(Roi::points({{0,0},{0,0},{6,3}}),2,1);
    centroid_is(Roi::points({{7,-9}}),7,-9);
    centroid_is(Roi::polyline({{0,0},{6,0},{6,2}}),3.75,.25);
    centroid_is(Roi::polyline({{0,0},{0,0},{6,0},{6,2},{6,2}}),3.75,.25);
    const auto high=std::numeric_limits<double>::max();
    centroid_is(Roi::points({{high/2,high/4},{high,high/4}}),high/2+high/4,high/4);
    centroid_is(Roi::polyline({{high/2,0},{high,0}}),high/2+high/4,0);
    bool rejected=false;try {Roi{}.centroid();}catch(const std::invalid_argument&) {rejected=true;}check(rejected);
    if(has_geos()) {
        const auto triangle=Roi::polygon({{0,0},{6,0},{0,6}});
        centroid_is(triangle,2,2);
        centroid_is(Roi::polygon({{0,6},{6,0},{0,0}}),2,2);
        centroid_is(triangle.translated(1000000000,-2000000000),1000000002,-1999999998);
        centroid_is(Roi::polygon(square(0,10),{square(1,3)}),5.125,5.125);
        const auto donut=Roi::polygon(square(0,10),{square(2,8)});
        centroid_is(donut,5,5);check(!donut.contains(donut.centroid()));
        centroid_is(Roi::polygon({{0,0},{2,0},{2,1},{1,1},{1,2},{0,2}}),5./6,5./6);
        const auto parts=Roi::multipolygon({PolygonPart(square(0,2)),
            PolygonPart({{10,0},{14,0},{14,4},{10,4}})});
        centroid_is(parts,9.8,1.8);
        std::vector<std::future<void>> tasks;
        for(int i=0;i<8;++i) tasks.push_back(std::async(std::launch::async,[parts] {
            for(int j=0;j<100;++j) centroid_is(parts,9.8,1.8);
        }));
        for(auto& task:tasks) task.get();
    }
    std::cout<<"ROI centroids: analytic bodies, area/length/point weights, numeric limits and shared snapshots passed; GEOS="<<has_geos()<<'\n';
}
int main() {
    try {
        centroid_contracts();
        auto ellipse=Roi::ellipse({0,0,4,4});
        check(std::abs(ellipse.area()-4*3.141592653589793)<1e-10);
        check(ellipse.contains({2,2}) && !ellipse.contains({0,0}));
        check(ellipse.contains_sample({4,2},RoiSamplingPolicy::PolygonWinding));
        bool invalid_policy=false;
        try {ellipse.contains_sample({2,2},static_cast<RoiSamplingPolicy>(7));} catch(const std::invalid_argument&) {invalid_policy=true;}
        check(invalid_policy);
        auto line=Roi::polyline({{0,0},{3,0},{3,4}});
        check(line.perimeter()==7 && line.distance({1,2})==2);
        check(line.scaled_length(2,1)==10 && line.scaled_length(2,2)==14);
        check(std::abs(ellipse.scaled_length(2,2)-8*3.141592653589793)<1e-10);
        bool invalid_scale=false;
        try {line.scaled_length(0,1);} catch(const std::invalid_argument&) {invalid_scale=true;}
        check(invalid_scale);
        if(has_geos()) {
            auto hole=Roi::polygon(square(0,10),{square(2,8)});
            for(auto policy:{RoiSamplingPolicy::GeometryCoverage,RoiSamplingPolicy::PolygonWinding}) {
                check(hole.contains_sample({1,1},policy) && !hole.contains_sample({5,5},policy));
            }
            check(hole.contains({8,5}) && hole.contains_sample({8,5},RoiSamplingPolicy::GeometryCoverage));
            check(!hole.contains_sample({2,5},RoiSamplingPolicy::PolygonWinding));
            check(hole.contains_sample({8,5},RoiSamplingPolicy::PolygonWinding));
            check(hole.contains({10,5}) && !hole.contains_sample({10,5},RoiSamplingPolicy::PolygonWinding));
            check(hole.contains_sample({0,5},RoiSamplingPolicy::PolygonWinding));
            auto diagonal=Roi::polygon({{16.5,16.5},{239.5,16.5},{16.5,239.5}});
            std::uint64_t winding_samples=0,coverage_samples=0;
            for(int y=16;y<239;++y) for(int x=16;x<239;++x) {
                winding_samples+=diagonal.contains_sample({x+.5,y+.5},RoiSamplingPolicy::PolygonWinding);
                coverage_samples+=diagonal.contains({x+.5,y+.5});
            }
            check(winding_samples==24976 && coverage_samples==25198);
            auto grid_edges=Roi::polygon(square(0,64),{square(16,48)});
            for(int y=-1;y<=65;++y) for(int x=-1;x<=65;++x) {
                const bool outer=x>=0 && x<64 && y>=0 && y<64;
                const bool inner=x>=16 && x<48 && y>=16 && y<48;
                check(grid_edges.contains_sample({double(x),double(y)},RoiSamplingPolicy::PolygonWinding)==(outer && !inner));
            }
            check(hole.area()==64 && hole.perimeter()==64);
            check(!hole.contains({5,5}) && hole.contains({1,1}));
            check(!hole.covers(Roi::rectangle({3,3,1,1})));
            try { Roi::polygon({{0,0},{4,4},{0,4},{4,0}}); throw std::runtime_error("accepted self intersection"); }
            catch(const std::invalid_argument&) {}
            Document d(Image::demo()); auto id=d.add_roi(hole);
            d.update_roi(id,hole.translated(10,10)); check(d.undo() && d.annotation(id).geometry.equals(hole));
        }
        std::cout<<"Analytic shapes, topology, ownership and shape undo passed; GEOS "<<geos_version()<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
