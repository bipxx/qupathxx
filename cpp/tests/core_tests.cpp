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
        {
            qpx::Document features(qpx::Image(4,4,std::vector<std::uint8_t>(48,50)));
            const auto object=features.add_rectangle({0,0,2,1});
            const auto raw=features.shape_measurements(object);
            check(raw.roi.area_px2==2 && raw.roi.length_px==6 && !raw.roi.area_um2);
            features.set_calibration(.5,.25);
            const auto calibrated=features.shape_measurements(object);
            check(*calibrated.roi.area_um2==.25 && *calibrated.roi.length_um==2.5);
            features.shape_measurements(object); check(features.feature_cache_stats().hits==1);
            qpx::Document copied=features; features.clear_feature_cache();
            check(copied.feature_cache_stats().entries==0 && copied.feature_cache_stats().hits==0);
            check(*copied.shape_measurements(object).roi.length_um==2.5);
            qpx::Document moved=std::move(copied);
            check(*moved.shape_measurements(object).roi.length_um==2.5 && moved.feature_cache_stats().hits==1);
            qpx::Document assigned(qpx::Image(4,4,std::vector<std::uint8_t>(48,50)));
            assigned.shape_measurements(assigned.add_rectangle({0,0,1,1}));
            assigned=moved; moved.clear_feature_cache();
            check(assigned.feature_cache_stats().entries==0);
            check(*assigned.shape_measurements(object).roi.length_um==2.5);
            features.set_feature_cache_limit(0); features.shape_measurements(object);
            check(features.feature_cache_stats().entries==0);
        }
        qpx::Viewport v; v.fit(1000,500,800,600);
        auto p = v.screen_to_image({300,200}); v.zoom_at({300,200},2);
        auto q = v.screen_to_image({300,200}); check(std::abs(p.x-q.x)<1e-10 && std::abs(p.y-q.y)<1e-10);
        check(d.remove_annotation(id)); check(!d.remove_annotation(id));
        qpx::PixelBuffer pixels(1,1); pixels.writable_bytes()={10,110,210,255};
        qpx::DisplaySettings display; display.black=10; display.white=210; display.gamma=2;
        check(qpx::display_pixels(pixels,display)==std::vector<std::uint8_t>({0,180,255,255}));
        check(pixels.bytes()==std::vector<std::uint8_t>({10,110,210,255}));
        display.mode=qpx::DisplayMode::Red;
        check(qpx::display_pixels(pixels,display)==std::vector<std::uint8_t>({0,0,0,255}));
        v.center_on({750,250},800,600); auto center=v.screen_to_image({400,300});
        check(center.x==750 && center.y==250);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
