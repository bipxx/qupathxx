// Standalone timing of the production header. Compile with an alternate header
// macro to compare revisions; kernel setup and scratch allocations are included.
#ifndef QPX_GAUSSIAN_HEADER
#define QPX_GAUSSIAN_HEADER "../cpp/masked_gaussian.hpp"
#endif
#include QPX_GAUSSIAN_HEADER
#ifdef QPX_GAUSSIAN_BASELINE_HEADER
#define qpx qpx_baseline
#include QPX_GAUSSIAN_BASELINE_HEADER
#undef qpx
#endif
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    const auto size=argc>1?std::stoul(argv[1]):512;
    const auto repeats=argc>2?std::stoul(argv[2]):5;
    if(size<32 || size>2048 || repeats<3 || repeats>25) return 2;
    std::cout<<std::setprecision(17)<<"{\"size\":"<<size<<",\"repeats\":"<<repeats<<",\"cases\":[";
    bool first=true;
    for(const std::string domain_name:{"full","holes"}) for(double sigma:{1.,4.,16.}) {
        std::vector<double> input(size*size);
        std::vector<std::uint8_t> domain(size*size,1);
        for(std::size_t y=0;y<size;++y) for(std::size_t x=0;x<size;++x) {
            const auto i=y*size+x;
            if(domain_name=="holes" && ((x*13+y*7)%31<5 || (x>size/3 && x<size/2 && y>size/4 && y<size/2))) domain[i]=0;
            input[i]=domain[i]?double((x*37+y*11)%997)/997:0;
        }
        std::vector<double> times,baseline_times,output,baseline_output;
        for(std::size_t repeat=0;repeat<=repeats;++repeat) {
            auto run=[&](bool baseline) {
                auto& destination=baseline?baseline_output:output;destination=input;
                const auto start=std::chrono::steady_clock::now();
#ifdef QPX_GAUSSIAN_BASELINE_HEADER
                if(baseline) qpx_baseline::detail::masked_gaussian(destination,domain,size,size,sigma,[]{});
                else
#endif
                    qpx::detail::masked_gaussian(destination,domain,size,size,sigma,[]{});
                const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(repeat) (baseline?baseline_times:times).push_back(elapsed);
            };
#ifdef QPX_GAUSSIAN_BASELINE_HEADER
            // Alternate the paired order to reduce load/temperature drift.
            if(repeat%2) {run(false);run(true);} else {run(true);run(false);}
            if(std::memcmp(output.data(),baseline_output.data(),output.size()*sizeof(double))) {
                std::cerr<<"Gaussian output bits changed\n";return 1;
            }
#else
            run(false);
#endif
        }
        // Stable bit-pattern digest, independent of standard-library hash.
        std::uint64_t digest=14695981039346656037ull;
        for(double value:output) {
            std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));
            for(int shift=0;shift<64;shift+=8) {digest^=(bits>>shift)&255;digest*=1099511628211ull;}
        }
        auto sorted=times;std::sort(sorted.begin(),sorted.end());
        if(!first) std::cout<<',';first=false;
        std::cout<<"{\"domain\":\""<<domain_name<<"\",\"sigma\":"<<sigma
            <<",\"median_ms\":"<<sorted[sorted.size()/2]<<",\"output_bits_fnv1a64\":\""<<std::hex<<digest<<std::dec<<"\",\"samples_ms\":[";
        for(std::size_t i=0;i<times.size();++i) {if(i) std::cout<<',';std::cout<<times[i];}
        std::cout<<"]";
#ifdef QPX_GAUSSIAN_BASELINE_HEADER
        auto baseline_sorted=baseline_times;std::sort(baseline_sorted.begin(),baseline_sorted.end());
        std::cout<<",\"baseline_median_ms\":"<<baseline_sorted[baseline_sorted.size()/2]<<",\"output_bits_equal\":true,\"baseline_samples_ms\":[";
        for(std::size_t i=0;i<baseline_times.size();++i) {if(i) std::cout<<',';std::cout<<baseline_times[i];}
        std::cout<<"]";
#endif
        std::cout<<"}";
    }
    std::cout<<"]}\n";
}
