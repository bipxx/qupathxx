#include "../qupath_nuclei.hpp"
#include "qupath_response_reference.hpp"
#include <cstring>
#include <iostream>

namespace {
void check(bool value) {if(!value) throw std::runtime_error("compatibility response check failed");}
template<class E,class F> void refuses(F f) {
    try {f();} catch(const E&) {return;}throw std::runtime_error("compatibility response failed to refuse");
}
}
int main() {
    using namespace qpx::cell_detail;
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
    for(const auto& test:qpx_test::response_basin_cases) {
        const auto n=test.width*test.height;std::vector<float> input(n);
        std::memcpy(input.data(),test.input,n*4);const auto saved=input;
        const auto labels=qupath070_response_basins(input,test.width,test.height,10000,64*n+65536);
        check(labels==std::vector<std::uint32_t>(test.labels,test.labels+n) && input==saved);
        refuses<std::length_error>([&]{qupath070_response_basins(input,test.width,test.height,10000,64*n+65535);});
        std::atomic<bool> cancelled{true};
        refuses<std::runtime_error>([&]{qupath070_response_basins(input,test.width,test.height,10000,64*n+65536,&cancelled);});
        check(input==saved);
    }
    std::vector<float> input(9,.2f),response(9);input[4]=1;response[4]=1;
    std::vector<std::uint8_t> domain(9,1);const auto saved=input;
    refuses<std::invalid_argument>([&]{qupath070_response_basins(response,0,3,10,65536);});
    refuses<std::invalid_argument>([&]{qupath070_response_basins(response,3,3,0,65536);});
    auto bad=response;bad[0]=std::numeric_limits<float>::quiet_NaN();
    refuses<std::invalid_argument>([&]{qupath070_response_basins(bad,3,3,10,65536+64*9);});
    bad[0]=std::numeric_limits<float>::infinity();
    refuses<std::invalid_argument>([&]{qupath070_response_basins(bad,3,3,10,65536+64*9);});
    auto two=std::vector<float>(35);two[9]=1;two[25]=1;
    refuses<std::length_error>([&]{qupath070_response_basins(two,7,5,1,65536+64*35);});
    std::cout<<"33 public QuPath response basins exact; plateau, border, FIFO and finite/input/budget/cancellation checks passed\n";
    LogParameters p;p.sigma=0;p.minimum_radius=p.minimum_spacing=0;p.minimum_area=1;p.maximum_area=0;
    p.eight_connected_background=true;p.work_bytes=65536+128*9;
    for(double sigma:{0.,2.5}) {
        const auto result=qupath070_nuclei(input,response,domain,3,3,sigma,p);
        const auto ordinary=qupath070_nuclei(input,response,domain,3,3,sigma,p,nullptr,false);
        check(ordinary.basins.empty() && ordinary.regions.empty() && ordinary.binary.empty() && ordinary.refinement.empty());
        check(ordinary.nuclei.labels==result.nuclei.labels && ordinary.nuclei.count==result.nuclei.count && ordinary.nuclei.candidate_pixels==result.nuclei.candidate_pixels);
        check(result.nuclei.count==1 && result.nuclei.labels[4]==1 && result.nuclei.candidate_pixels==1);
        check(std::count(result.nuclei.labels.begin(),result.nuclei.labels.end(),1)==1);
    }
    auto limited=p;--limited.work_bytes;
    refuses<std::length_error>([&]{qupath070_nuclei(input,response,domain,3,3,0,limited);});
    auto invalid=domain;invalid[0]=2;
    refuses<std::invalid_argument>([&]{qupath070_nuclei(input,response,invalid,3,3,0,p);});
    std::atomic<bool> cancelled{true};
    refuses<std::runtime_error>([&]{qupath070_nuclei(input,response,domain,3,3,0,p,&cancelled);});
    auto filters=p;filters.minimum_spacing=1;
    refuses<std::invalid_argument>([&]{qupath070_nuclei(input,response,domain,3,3,0,filters);});
    check(input==saved && domain==std::vector<std::uint8_t>(9,1));
    std::cout<<"Private sampled nucleus composition, retained candidate counts and atomic failure checks passed\n";
}
