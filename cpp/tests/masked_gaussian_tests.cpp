#include "../masked_gaussian.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

int main() {
    try {
        for(auto shape:{std::pair<std::size_t,std::size_t>{1,1},{1,7},{8,1},{3,5},{17,19},{257,3},{259,5}})
        for(double sigma:{0.,1e-300,.1,.6,1.,std::nextafter(2.,0.),2.,std::nextafter(2.,3.),2.1,4.,16.}) for(int pattern=0;pattern<3;++pattern) {
            const auto width=shape.first,height=shape.second;
            std::vector<double> input(width*height);
            std::vector<std::uint8_t> domain(width*height);
            for(std::size_t i=0;i<input.size();++i) {
                domain[i]=pattern==0 || (pattern==1 && i%7!=2);
                input[i]=domain[i]?double((i*37)%997)/113:0;
            }
            auto actual=input;
            qpx::detail::masked_gaussian(actual,domain,width,height,sigma,[]{});
            // Independent direct 2D convolution, rather than separable passes
            // or a copy of the production row/block traversal.
            const auto radius=static_cast<int>(std::ceil(3*sigma));
            auto coefficient=[&](std::int64_t delta) {
                if(!delta) return 1.;const auto d=double(delta)/sigma;return std::exp(-.5*d*d);
            };
            for(std::size_t y=0;y<height;++y) for(std::size_t x=0;x<width;++x) {
                double sum=0,weight=0;
                for(std::size_t yy=0;yy<height;++yy) for(std::size_t xx=0;xx<width;++xx) {
                    const auto dx=static_cast<std::int64_t>(xx)-static_cast<std::int64_t>(x);
                    const auto dy=static_cast<std::int64_t>(yy)-static_cast<std::int64_t>(y);
                    if(std::abs(dx)>radius || std::abs(dy)>radius || !domain[yy*width+xx]) continue;
                    const auto w=coefficient(dx)*coefficient(dy);sum+=input[yy*width+xx]*w;weight+=w;
                }
                const auto expected=weight>0?sum/weight:0;
                if(std::abs(actual[y*width+x]-expected)>128*std::numeric_limits<double>::epsilon()*std::max(1.,std::abs(expected)))
                    throw std::runtime_error("masked Gaussian differs from independent 2D reference");
            }
        }
        std::vector<double> values(259*5,1);
        std::vector<std::uint8_t> domain(values.size(),1);
        std::size_t checkpoints=0;bool stopped=false;
        try {qpx::detail::masked_gaussian(values,domain,259,5,16,[&]{if(++checkpoints==3) throw std::runtime_error("cancelled");});}
        catch(const std::runtime_error&) {stopped=true;}
        if(!stopped || checkpoints!=3) throw std::runtime_error("masked Gaussian cancellation was not propagated");
        std::cout<<"Direct 2D Gaussian, empty/holey domains, block edges and cancellation passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
