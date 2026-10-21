#include "qupathxx/stains.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool v) { if(!v) throw std::runtime_error("stain check failed"); }
template<class F> void invalid(F f) {
    try { f(); } catch(const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid stain parameters accepted");
}
int main() {
    try {
        qpx::StainTransform identity({{1,0,0},{0,1,0}});
        qpx::PixelBuffer pixels(3,1);
        pixels.writable_bytes()={255,255,255,255,0,10,100,255,10,10,10,0};
        const auto result=identity.deconvolve(pixels);
        check(result.width()==3 && result.row_stride()==36);
        check(result.values()[0]==0 && std::abs(result.values()[3]-std::log10(255.))<1e-6);
        check(std::isnan(result.values()[6]));
        pixels.writable_bytes()[11]=128;
        check(std::isnan(identity.optical_densities(pixels).values()[6]));
        check(qpx::optical_density(0)==qpx::optical_density(1));
        check(qpx::optical_density(255,200)==0);
        invalid([]{qpx::StainTransform t({{1,0,0},{1,1e-10,0}});});
        invalid([]{qpx::StainTransform t({{1,0,0},{0,1,0},{1,1,0}});});
        invalid([]{qpx::StainTransform t({{0,0,0},{0,1,0}});});
        invalid([]{qpx::optical_density(-1);});
        std::atomic<bool> cancelled{true};
        try { identity.deconvolve(pixels,&cancelled); throw std::logic_error("not cancelled"); }
        catch(const std::runtime_error& e) { check(std::string(e.what())=="stain transform cancelled"); }
        pixels.writable_bytes().pop_back();
        invalid([&]{identity.deconvolve(pixels);});
        {
            qpx::PixelBuffer over_budget(8192,2731);
            invalid([&]{identity.deconvolve(over_budget);});
        }
        qpx::StainTransform huge({{1e308,0,0},{0,1e308,0}});
        check(huge.stains()[0][0]==1);
        std::cout<<"stains passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
