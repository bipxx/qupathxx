#include "qupathxx/core.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool condition) { if (!condition) throw std::runtime_error("image server check failed"); }
template<class Exception, class Action> void rejects(Action action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("expected exception was not raised");
}
int main() {
    try {
        qpx::Image original(2,1,{1,2,3,4,5,6});auto copy=original;
        check(original.pixels_owner()==copy.pixels_owner());auto retained=original.pixels_owner();original=qpx::Image(1,1,{9,8,7});
        check(*retained==std::vector<std::uint8_t>({1,2,3,4,5,6}) && copy.pixels()==*retained);
        qpx::PixelBuffer imported(1,1,std::vector<std::uint8_t>{1,2,3,4});check(imported.bytes()==std::vector<std::uint8_t>({1,2,3,4}));
        rejects<std::invalid_argument>([]{qpx::PixelBuffer bad(1,1,std::vector<std::uint8_t>{1});});
        auto s = qpx::dense_image_server(qpx::Image(2,1,{10,20,30,100,110,120}));
        auto held=s->shared_from_this();
        check(held.get()==s.get() && !held.owner_before(s) && !s.owner_before(held));
        held.reset();
        check(s->metadata().width == 2 && !s->metadata().pixel_width_um);
        auto padded = s->read_region({-1,0,0,4,1});
        check(padded.bytes() == std::vector<std::uint8_t>({0,0,0,0,10,20,30,255,100,110,120,255,0,0,0,0}));
        rejects<std::out_of_range>([&]{ s->read_region({0,0,1,1,1}); });
        rejects<std::invalid_argument>([&]{ s->read_region({0,0,0,0,1}); });
        rejects<std::invalid_argument>([&]{ s->read_region({0,0,0,1000000,1000000}); });
        rejects<std::invalid_argument>([&]{ s->read_region({std::numeric_limits<std::int64_t>::max(),0,0,2,1}); });
        qpx::PixelBuffer alpha(3,1);
        qpx::unpack_argb({0x80402010,0x00000000,0xff123456},alpha);
        check(alpha.bytes() == std::vector<std::uint8_t>({128,64,32,128,0,0,0,0,18,52,86,255}));
        qpx::Document d(s);
        auto id = d.add_rectangle({0,0,2,1});
        rejects<std::runtime_error>([&]{ d.area_um2(id); });
        d.set_calibration(.5,.25); check(d.area_um2(id) == .25);
        d.clear_calibration(); check(!d.pixel_height_um());
        check(d.mean_intensity(id) == 65);
        s->close(); s->close(); check(s->closed());
        rejects<std::runtime_error>([&]{ s->read_region({0,0,0,1,1}); });
        check(padded.bytes()[4] == 10);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
