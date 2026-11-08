#include "../scientific_cell_numeric.hpp"
#include "scientific_cell_reference.hpp"
#include <cstring>
#include <iostream>
#include <random>

namespace {
void require(bool value) {if(!value) throw std::runtime_error("physical grid numerical contract failed");}
template<class Exception,class Function> void rejects(Function function) {
    try {function();} catch(const Exception&) {return;}
    throw std::runtime_error("physical grid numerical admission did not reject");
}
double number(std::uint64_t bits) {double value;std::memcpy(&value,&bits,sizeof(value));return value;}
std::uint64_t bits(double value) {std::uint64_t result;std::memcpy(&result,&value,sizeof(value));return result;}
bool close(double a,double b) {return std::abs(a-b)<=2e-12+2e-12*std::abs(b);}
}
void check_scientific_cell_numeric() {
    using namespace qpx;using namespace qpx::cell_detail;
    constexpr std::size_t budget=256ULL*1024*1024;
    const auto inf=std::numeric_limits<double>::infinity();
    for(const auto& test:physical_reference::comparisons)
        require(physical_scaled_order(physical_multiply(test.a,test.b),test.ae,physical_multiply(test.c,test.d),test.be)==test.order);
    for(const auto& test:physical_reference::distances) {
        std::vector<double> initial(test.width*test.height,inf);for(auto site:test.sites) initial[site]=0;
        auto values=initial;std::vector<std::uint32_t> nearest;
        require(physical_distance_work_bytes(test.width,test.height,!test.outside)==test.bytes);
        physical_distance(values,test.width,test.height,{test.sx,test.sy},test.outside,test.outside?nullptr:&nearest,test.bytes);
        for(std::size_t i=0;i<values.size();++i) require(bits(values[i])==test.squared[i]);
        if(!test.outside && nearest!=test.nearest) {
            for(std::size_t i=0;i<nearest.size();++i) if(nearest[i]!=test.nearest[i]) {
                std::cerr<<"physical nearest mismatch in "<<test.width<<'x'<<test.height<<" at "<<i<<": "<<nearest[i]<<" expected "<<test.nearest[i]<<'\n';break;
            }
            require(false);
        }
        auto short_budget=initial;
        rejects<std::length_error>([&]{physical_distance(short_budget,test.width,test.height,{test.sx,test.sy},test.outside,test.outside?nullptr:&nearest,test.bytes-1);});
        require(short_budget==initial);
        auto scaled=initial;
        physical_distance(scaled,test.width,test.height,{2*test.sx,2*test.sy},test.outside,nullptr,budget);
        for(std::size_t i=0;i<values.size();++i) require(scaled[i]==4*values[i]);
    }
    for(const auto& test:physical_reference::gaussians) {
        const PhysicalGridMetric metric(test.sx,test.sy);const auto sigmas=metric.sigma_pixels(test.sigma);
        const auto rx=scientific_gaussian_radius(sigmas.first),ry=scientific_gaussian_radius(sigmas.second);
        require(physical_gaussian_visits(test.width,test.height,rx,ry)==test.visits);
        require(physical_gaussian_work_bytes(test.width,test.height,sigmas.first,sigmas.second)==test.bytes);
        auto values=test.values;
        physical_gaussian(values,test.valid,test.width,test.height,metric,test.sigma,test.bytes);
        for(std::size_t i=0;i<values.size();++i) require(close(values[i],number(test.expected[i])));
        auto short_budget=test.values;
        rejects<std::length_error>([&]{physical_gaussian(short_budget,test.valid,test.width,test.height,metric,test.sigma,test.bytes-1);});
        require(short_budget==test.values);
        auto scaled=test.values;
        physical_gaussian(scaled,test.valid,test.width,test.height,{2*test.sx,2*test.sy},2*test.sigma,budget);
        require(scaled==values);
    }
    // Independent exhaustive site scan on exactly representable small binary
    // metrics. It shares neither the production envelope nor its wide comparator.
    std::mt19937 rng(161);const double spacings[]={.25,.5,1.,1.25,1.5,2.,4.};
    for(unsigned trial=0;trial<1024;++trial) {
        const auto w=1+rng()%11,h=1+rng()%13,n=w*h;
        const auto sx=spacings[rng()%7],sy=spacings[rng()%7];const bool outside=trial%2;
        std::vector<double> initial(n,inf);for(auto& value:initial) if(rng()%7==0) value=0;
        auto values=initial;std::vector<std::uint32_t> map;
        physical_distance(values,w,h,{sx,sy},outside,outside?nullptr:&map,budget);
        for(unsigned i=0;i<n;++i) {
            double expected=inf;std::uint32_t owner=std::numeric_limits<std::uint32_t>::max();
            for(unsigned j=0;j<n;++j) if(initial[j]==0) {
                const auto dx=double(i%w)-double(j%w),dy=double(i/w)-double(j/w);
                const auto cost=dx*dx*(sx*sx)+dy*dy*(sy*sy);
                if(cost<expected) {expected=cost;owner=j;}
            }
            if(outside) {
                const auto x=double(i%w),y=double(i/w);
                expected=std::min(expected,std::min(std::min((x+1)*(x+1),(w-x)*(w-x))*sx*sx,
                                                 std::min((y+1)*(y+1),(h-y)*(h-y))*sy*sy));
            } else require(map[i]==owner);
            require(values[i]==expected);
        }
    }
    const ScientificGrid grid(20,20,{1,7,3},{2,5,2});
    const auto metric=PhysicalGridMetric::capture(grid,.5,.25);
    require(metric.x_um()==(7./3.)*.5 && metric.y_um()==.625);
    const auto sigma=metric.sigma_pixels(1);require(sigma.first==1/metric.x_um() && sigma.second==1/.625);
    require(metric.area_um2()==metric.x_um()*metric.y_um());
    rejects<std::invalid_argument>([&]{PhysicalGridMetric::capture(grid,{},.25);});
    rejects<std::invalid_argument>([&]{PhysicalGridMetric::capture(grid,.5,{});});
    rejects<std::invalid_argument>([&]{PhysicalGridMetric::capture(grid,0.,.25);});
    rejects<std::invalid_argument>([&]{PhysicalGridMetric::capture(grid,.5,inf);});
    rejects<std::overflow_error>([&]{PhysicalGridMetric::capture(grid,std::numeric_limits<double>::max(),.25);});
    rejects<std::invalid_argument>([]{PhysicalGridMetric invalid(0,1);});
    rejects<std::invalid_argument>([]{PhysicalGridMetric invalid(1,-1);});
    rejects<std::overflow_error>([]{PhysicalGridMetric(1e300,1).sigma_pixels(1e-300);});
    rejects<std::overflow_error>([]{PhysicalGridMetric(1e-300,1).sigma_pixels(1e300);});
    rejects<std::invalid_argument>([]{PhysicalGridMetric(1,1).sigma_pixels(-1);});
    std::vector<double> values={0,inf,inf};std::vector<std::uint32_t> nearest;
    rejects<std::overflow_error>([&]{physical_distance(values,3,1,{1e154,1},false,nullptr,budget);});
    rejects<std::overflow_error>([&]{physical_distance(values,3,1,{1e-200,1},false,nullptr,budget);});
    rejects<std::invalid_argument>([&]{physical_distance(values,3,1,{1,1},true,&nearest,budget);});
    rejects<std::invalid_argument>([&]{physical_distance(values,2,1,{1,1},false,nullptr,budget);});
    rejects<std::invalid_argument>([&]{physical_distance(values,3,1,{1,1},false,nullptr,0);});
    values[1]=1;rejects<std::invalid_argument>([&]{physical_distance(values,3,1,{1,1},false,nullptr,budget);});
    values[1]=std::numeric_limits<double>::quiet_NaN();rejects<std::invalid_argument>([&]{physical_distance(values,3,1,{1,1},false,nullptr,budget);});
    values[1]=inf;std::atomic<bool> cancelled{true};
    rejects<std::runtime_error>([&]{physical_distance(values,3,1,{1,1},false,nullptr,budget,&cancelled);});
    std::vector<double> one={std::numeric_limits<double>::quiet_NaN()};std::vector<std::uint8_t> valid={0};
    physical_gaussian(one,valid,1,1,{1,2},0,budget);require(one[0]==0);
    one[0]=std::numeric_limits<double>::quiet_NaN();valid[0]=1;
    rejects<std::invalid_argument>([&]{physical_gaussian(one,valid,1,1,{1,2},1,budget);});
    one[0]=1;valid[0]=2;rejects<std::invalid_argument>([&]{physical_gaussian(one,valid,1,1,{1,2},1,budget);});
    valid[0]=1;rejects<std::length_error>([&]{physical_gaussian(one,valid,1,1,{1,2},2000,budget);});
    rejects<std::runtime_error>([&]{physical_gaussian(one,valid,1,1,{1,2},1,budget,&cancelled);});
    std::vector<double> overflow(9,std::numeric_limits<double>::max());std::vector<std::uint8_t> opaque(9,1);
    rejects<std::overflow_error>([&]{physical_gaussian(overflow,opaque,3,3,{1,1},1,budget);});
    std::vector<double> large(100000,1);std::vector<std::uint8_t> support(100000,1);
    rejects<std::length_error>([&]{physical_gaussian(large,support,100000,1,{1,1},1000,budget);});
    require(large.front()==1 && large.back()==1);
    std::vector<double> tall(6000000,0);
    rejects<std::length_error>([&]{physical_distance(tall,1,tall.size(),{1,1},false,nullptr,budget);});
    require(tall.front()==0 && tall.back()==0);
    std::cout<<"Native physical grid metrics, weighted distances, Gaussian support and admission passed\n";
}
