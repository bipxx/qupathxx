#include "qupathxx/core.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("geometry check failed"); }
Ring square(double a,double b) { return {{a,a},{b,a},{b,b},{a,b}}; }
int main() {
    try {
        auto ellipse=Roi::ellipse({0,0,4,4});
        check(std::abs(ellipse.area()-4*3.141592653589793)<1e-10);
        check(ellipse.contains({2,2}) && !ellipse.contains({0,0}));
        auto line=Roi::polyline({{0,0},{3,0},{3,4}});
        check(line.perimeter()==7 && line.distance({1,2})==2);
        check(line.scaled_length(2,1)==10 && line.scaled_length(2,2)==14);
        check(std::abs(ellipse.scaled_length(2,2)-8*3.141592653589793)<1e-10);
        bool invalid_scale=false;
        try {line.scaled_length(0,1);} catch(const std::invalid_argument&) {invalid_scale=true;}
        check(invalid_scale);
        if(has_geos()) {
            auto hole=Roi::polygon(square(0,10),{square(2,8)});
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
