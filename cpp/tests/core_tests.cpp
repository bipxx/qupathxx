#include "qupathxx/core.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool value) { if (!value) throw std::runtime_error("check failed"); }
int main() {
    try {
        qpx::Document d(qpx::Image(2,1,{0,30,60,120,150,180}));
        auto id = d.add_rectangle({0,0,2,1}); d.set_calibration(.5,.25);
        check(d.area_um2(id) == .25); check(d.mean_intensity(id) == 90);
        try { d.add_rectangle({-1,0,1,1}); throw std::runtime_error("accepted invalid ROI"); }
        catch (const std::invalid_argument&) {}
        check(d.annotations().size() == 1);
        qpx::Viewport v; v.fit(1000,500,800,600);
        auto p = v.screen_to_image({300,200}); v.zoom_at({300,200},2);
        auto q = v.screen_to_image({300,200}); check(std::abs(p.x-q.x)<1e-10 && std::abs(p.y-q.y)<1e-10);
        check(d.remove_annotation(id)); check(!d.remove_annotation(id));
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
