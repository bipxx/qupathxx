#include "qupathxx/core.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool value) { if(!value) throw std::runtime_error("spatial check failed"); }
template<class F> void rejects(F fn) {bool rejected=false;try {fn();}catch(const std::exception&) {rejected=true;}check(rejected);}
void nearest_oracle(const SpatialIndex& index,const std::vector<SpatialEntry>& entries,Point query,
    double sx,double sy,std::optional<std::uint64_t> excluded={}) {
    std::optional<std::uint64_t> expected;long double score=std::numeric_limits<long double>::infinity();
    for(const auto& e:entries) {
        if(excluded && e.id==*excluded) continue;
        const long double dx=(static_cast<long double>(e.bounds.x)-query.x)*sx;
        const long double dy=(static_cast<long double>(e.bounds.y)-query.y)*sy;
        const auto next=dx*dx+dy*dy;
        if(!expected || next<score || (next==score && e.id<*expected)) {expected=e.id;score=next;}
    }
    const auto actual=index.nearest_point(query,sx,sy,excluded);
    check(bool(actual)==bool(expected));if(!expected) return;
    check(actual->id==*expected);
    const auto distance=std::sqrt(static_cast<double>(score));
    check(std::abs(actual->distance-distance)<=1e-9+distance*1e-12);
}
void nearest_contracts() {
    SpatialIndex empty({});check(!empty.nearest_point({0,0}));
    SpatialIndex one({{1,{0,0,0,0}}});check(!one.nearest_point({0,0},1,1,1));
    SpatialIndex overlap({{1,{0,0,0,0}},{4,{0,0,0,0}},{2,{3,4,0,0}}});
    auto result=overlap.nearest_point({0,0},1,1,1);check(result && result->id==4 && result->distance==0);
    std::vector<SpatialEntry> ties{{17,{3,4,0,0}},{2,{-3,-4,0,0}}};
    for(std::uint64_t i=100;i<132;++i) ties.push_back({i,{1000+double(i),1000,0,0}});
    SpatialIndex tied(ties);result=tied.nearest_point({0,0});check(result && result->id==2 && result->distance==5);
    SpatialIndex calibrated({{1,{5,0,0,0}},{2,{0,4,0,0}}});
    check(calibrated.nearest_point({0,0})->id==2);
    result=calibrated.nearest_point({0,0},.5,2);check(result && result->id==1 && result->distance==2.5);
    const auto high=std::numeric_limits<double>::max();
    SpatialIndex extreme({{9,{high,0,0,0}}});
    result=extreme.nearest_point({-high,0},.25,1);check(result && result->id==9 && result->distance==high/2);
    rejects([&] {extreme.nearest_point({-high,0});});
    const auto nan=std::numeric_limits<double>::quiet_NaN();
    for(auto scale:{0.,-1.,nan,std::numeric_limits<double>::infinity()}) rejects([&] {one.nearest_point({0,0},scale,1);});
    rejects([&] {one.nearest_point({nan,0});});
    SpatialIndex rectangles({{9,{0,0,2,2}}});rejects([&] {rectangles.nearest_point({0,0});});
    check(rectangles.query({1,1,0,0})==std::vector<std::uint64_t>{9});
    std::atomic<bool> cancelled{true};rejects([&] {empty.nearest_point({0,0},1,1,{},&cancelled);});
    rejects([&] {tied.nearest_point({0,0},1,1,{},&cancelled);});
    std::mt19937 random(319);std::uniform_int_distribution<int> coordinate(-4096,4096);
    std::vector<SpatialEntry> entries;
    for(std::uint64_t i=1;i<=2048;++i) entries.push_back({i,{double(coordinate(random)),double(coordinate(random)),0,0}});
    SpatialIndex forward(entries);auto reversed=entries;std::reverse(reversed.begin(),reversed.end());SpatialIndex reverse(reversed);
    for(int i=0;i<200;++i) {
        const Point query{double(coordinate(random)),double(coordinate(random))};
        const auto sx=i%2?.5:2.,sy=i%3?1.:4.;
        const auto exclude=i%4?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>(i+1);
        nearest_oracle(forward,entries,query,sx,sy,exclude);nearest_oracle(reverse,entries,query,sx,sy,exclude);
    }
    std::vector<std::future<void>> tasks;
    for(int i=0;i<8;++i) tasks.push_back(std::async(std::launch::async,[&forward,&entries,i] {
        for(int j=0;j<25;++j) nearest_oracle(forward,entries,{double(i*19),double(j*29)},.5,2);
    }));
    for(auto& task:tasks) task.get();
    std::vector<SpatialEntry> large;large.reserve(100000);
    for(std::uint64_t i=0;i<100000;++i) large.push_back({i+1,{double(i%1000)*16,double(i/1000)*32,0,0}});
    SpatialIndex big(large);nearest_oracle(big,large,{3457,876},.5,2);nearest_oracle(big,large,{16,32},1,1,1002);
    // Large opposite-signed spans lose low bits if maxima are reconstructed.
    std::vector<SpatialEntry> spread;
    for(std::uint64_t i=1;i<=32;++i) spread.push_back({i,{-1e16+double(i)*2,double(i),0,0}});
    spread.push_back({33,{1,17,0,0}});spread.push_back({34,{2,18,0,0}});
    nearest_oracle(SpatialIndex(spread),spread,{1,17},1,1);
    std::cout<<"Nearest-point spatial queries: 400 reordered and 200 concurrent exhaustive oracles, 100000-point scenes, anisotropy, ties, identity exclusion, cancellation and numeric limits passed\n";
}
int main() {
    try {
        nearest_contracts();
        Document d(Image::demo());
        auto id=d.add_rectangle({0,0,10,10});
        check(d.query_region({2,2,0,0})==std::vector<std::uint64_t>{id});
        auto builds=d.spatial_stats().builds;
        d.rename_annotation(id,"Renamed"); check(d.pick({2,2})==std::vector<std::uint64_t>{id});
        check(d.spatial_stats().builds==builds && !d.spatial_stats().dirty_entries);
        d.update_rectangle(id,{100,100,10,10});
        check(d.pick({2,2}).empty() && d.pick({102,102})==std::vector<std::uint64_t>{id});
        check(d.undo() && d.pick({2,2})==std::vector<std::uint64_t>{id});
        d.remove_annotation(id); check(d.pick({2,2}).empty());
        check(d.undo() && d.pick({2,2})==std::vector<std::uint64_t>{id});
        std::cout<<"Spatial queries, delta updates, picking and undo passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
