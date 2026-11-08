#include "qupathxx/spatial_neighborhood.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("neighborhood check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool hit=false;try {fn();}catch(const E&) {hit=true;}check(hit);}
class MetadataOnlyServer final:public ImageServer {
public:
    ImageMetadata info;mutable std::atomic<int> reads{0};
    MetadataOnlyServer() {info.width=info.height=4096;info.levels={{4096,4096,1}};}
    const ImageMetadata& metadata() const override {return info;}
    std::string reader_id() const override {return "neighborhood-metadata-only";}
    PixelBuffer read_region(const RegionRequest&) const override {++reads;throw std::runtime_error("neighborhood must not read pixels");}
    void close() override {}
    bool closed() const override {return false;}
};
void index_oracle() {
    std::vector<SpatialEntry> entries;
    for(std::uint64_t i=0;i<513;++i) entries.push_back({i+1,{double((i*37)%101)-50,double((i*61)%97)-48,0,0}});
    SpatialIndex index(entries);
    for(auto scales:{Point{1,1},Point{.5,2},Point{4,.25}}) for(double radius:{.5,5.,15.,500.})
        for(int i=0;i<60;++i) {
            const Point query{double(i-30),double((i*7)%65)-32};std::uint64_t expected=0;
            const auto exclude=std::uint64_t(i+1);
            for(const auto& e:entries) {
                if(e.id==exclude) continue;
                const long double dx=(static_cast<long double>(e.bounds.x)-query.x)*scales.x;
                const long double dy=(static_cast<long double>(e.bounds.y)-query.y)*scales.y;
                if(dx*dx+dy*dy<=static_cast<long double>(radius)*radius) ++expected;
            }
            check(index.count_points_within(query,radius,scales.x,scales.y,exclude)==expected);
        }
    SpatialIndex boundary({{1,{5,0,0,0}},{2,{std::nextafter(5.,0.),0,0,0}},
        {3,{std::nextafter(5.,6.),0,0,0}},{4,{3,4,0,0}},{5,{0,0,0,0}},{6,{0,0,0,0}}});
    check(boundary.count_points_within({0,0},5)==5);
    check(boundary.count_points_within({0,0},5,1,1,5)==4);
    check(SpatialIndex({}).count_points_within({0,0},1)==0);
    SpatialIndex area({{1,{0,0,1,1}}});rejects<std::invalid_argument>([&] {area.count_points_within({0,0},1);});
    for(auto radius:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        rejects<std::invalid_argument>([&] {index.count_points_within({0,0},radius);});
    rejects<std::invalid_argument>([&] {index.count_points_within({0,0},1,0,1);});
    std::uint64_t remaining=1;rejects<std::length_error>([&] {index.count_points_within({0,0},1000,1,1,{},nullptr,&remaining);});check(remaining==0);
    std::atomic<bool> stop{true};rejects<std::runtime_error>([&] {index.count_points_within({0,0},1,1,1,{},&stop);});
    const double largest=std::numeric_limits<double>::max();
    SpatialIndex extremes({{1,{largest,0,0,0}}});check(extremes.count_points_within({-largest,0},largest,.25,1)==1);
    std::cout<<"Radius index: independent squared-distance oracle, inclusive/adjacent boundaries, anisotropic pruning, work/cancel and extreme-coordinate cases passed\n";
}
void captured_contracts() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);
    const auto a=d.add_detection(Roi::points({{32,32}})),b=d.add_detection(Roi::points({{35,36}})),c=d.add_detection(Roi::points({{32,32}}));
    SpatialNeighborhoodOptions p;p.radius=5;
    SpatialNeighborhoodRequest request(d,{b,a},{c,b,a},p);auto result=measure_spatial_neighborhoods(request);
    check(result->rows().size()==2 && result->rows()[0].source_id==a && result->rows()[1].source_id==b);
    check(result->rows()[0].count==2 && result->rows()[1].count==2 && result->belongs_to(request));
    check(result->input_sha256()==request.input_sha256() && result->sha256().size()==64);
    check(std::string(result->algorithm())=="qpx.centroid-radius-count/1");
    SpatialNeighborhoodRequest reordered(d,{a,b},{a,c,b},p);
    check(reordered.input_sha256()==request.input_sha256() && measure_spatial_neighborhoods(reordered)->sha256()==result->sha256());
    check(!result->belongs_to(reordered));
    p.exclude_self=false;check(measure_spatial_neighborhoods(SpatialNeighborhoodRequest(d,{a},{a,b,c},p))->rows()[0].count==3);
    p=request.options();p.radius=4.99;check(measure_spatial_neighborhoods(SpatialNeighborhoodRequest(d,{a},{a,b,c},p))->rows()[0].count==1);
    check(SpatialNeighborhoodRequest(d,{a},{b},p).input_sha256()!=SpatialNeighborhoodRequest(d,{a},{b},request.options()).input_sha256());
    auto empty=measure_spatial_neighborhoods(SpatialNeighborhoodRequest(d,{a,b},{},p));check(empty->rows()[0].count==0 && empty->rows()[1].count==0 && empty->distance_tests()==0);
    rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest bad(d,{a,a},{b},p);});
    rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest bad(d,{},{b},p);});
    rejects<std::out_of_range>([&] {SpatialNeighborhoodRequest bad(d,{a},{999999},p);});
    auto bad=p;bad.radius=0;rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest invalid(d,{a},{b},bad);});
    bad=p;bad.units=static_cast<SpatialDistanceUnits>(99);rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest invalid(d,{a},{b},bad);});
    bad=p;bad.snapshot_bytes=0;rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest invalid(d,{a},{b},bad);});
    p=request.options();p.units=SpatialDistanceUnits::Micrometres;
    rejects<std::invalid_argument>([&] {SpatialNeighborhoodRequest invalid(d,{a},{b},p);});
    d.set_calibration(2,.5);auto physical=SpatialNeighborhoodRequest(d,{a},{b,c},p);
    check(physical.x_scale()==2 && physical.y_scale()==.5 && measure_spatial_neighborhoods(physical)->rows()[0].count==1);
    check(!request.matches(d));
    const auto cell=d.add_cell(Roi::rectangle({100,100,20,20}),Roi::rectangle({100,100,4,4}));
    const auto target=d.add_detection(Roi::points({{102,102}}));p=request.options();p.radius=1;
    check(measure_spatial_neighborhoods(SpatialNeighborhoodRequest(d,{cell},{target},p))->rows()[0].count==1);
    p.prefer_nucleus=false;check(measure_spatial_neighborhoods(SpatialNeighborhoodRequest(d,{cell},{target},p))->rows()[0].count==0);
    const auto body=d.add_cell(Roi::rectangle({100,100,4,4}));p.prefer_nucleus=true;
    check(!SpatialNeighborhoodRequest(d,{body},{target},p).source_object(0).nucleus);
    std::atomic<bool> stop{true};rejects<std::runtime_error>([&] {measure_spatial_neighborhoods(request,&stop);});
    auto async=std::async(std::launch::async,[request] {return measure_spatial_neighborhoods(request);});check(async.get()->sha256()==result->sha256());
    auto wrong=std::async(std::launch::async,[&] {rejects<std::runtime_error>([&] {request.matches(d);});});wrong.get();
    check(server->reads.load()==0);
    std::optional<SpatialNeighborhoodRequest> detached;std::weak_ptr<MetadataOnlyServer> weak;
    {auto source=std::make_shared<MetadataOnlyServer>();weak=source;Document local(source);const auto x=local.add_rectangle({1,1,2,2}),y=local.add_rectangle({4,5,2,2});p=request.options();detached.emplace(local,std::vector<std::uint64_t>{x},std::vector<std::uint64_t>{y},p);}
    check(weak.expired() && measure_spatial_neighborhoods(*detached)->rows()[0].count==1);
    std::cout<<"Neighborhood capture: ordered identity, units, nucleus/body, empty/duplicate cohorts, owner checks and reader-free detached execution passed\n";
}
void lifecycle_and_budgets() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);
    std::vector<Annotation> objects;objects.reserve(3000);
    for(int i=0;i<3000;++i) {Annotation a{std::uint64_t(i+1),"Point",Roi::points({{double(i%100)*8,double(i/100)*8}})};a.kind=ObjectKind::Detection;objects.push_back(std::move(a));}
    const auto ids=d.append_objects(objects,{});SpatialNeighborhoodOptions p;p.radius=10;
    SpatialNeighborhoodRequest request(d,ids,ids,p);auto direct=measure_spatial_neighborhoods(request);
    SpatialNeighborhoodJob job;check(job.request(request) && job.busy() && !job.request(request));job.wait_work();check(job.busy() && !job.result());
    check(job.wait(d).state=="ready" && !job.busy());auto held=job.result();check(held && held->sha256()==direct->sha256());
    check(job.poll(d).completed_rows==ids.size() && job.poll(d).distance_tests==direct->distance_tests());
    auto wrong=std::async(std::launch::async,[&] {rejects<std::runtime_error>([&] {job.busy();});});wrong.get();
    auto cancel=std::async(std::launch::async,[&] {job.cancel();});cancel.get();check(!job.result() && job.poll(d).state=="cancelled" && held->rows().size()==ids.size());
    check(job.request(request));job.cancel();check(job.wait(d).state=="cancelled" && !job.result());
    p.radius=5000;SpatialNeighborhoodRequest dense(d,ids,ids,p);check(job.request(dense));
    auto running=job.poll(d);
    while(running.state=="running" && !running.completed_rows) {std::this_thread::yield();running=job.poll(d);}
    check(running.state=="running" && running.completed_rows>0 && running.completed_rows<running.total_rows);
    job.cancel();check(job.wait(d).state=="cancelled" && !job.result());
    p.radius=10;
    check(job.request(request));d.rename_annotation(ids.front(),"during work");check(job.wait(d).state=="stale" && !job.result());
    SpatialNeighborhoodRequest fresh(d,ids,ids,p);check(job.request(fresh) && job.wait(d).state=="ready");
    d.set_calibration(.5,.5);check(job.poll(d).state=="stale" && !job.result());
    auto tiny=p;tiny.snapshot_bytes=1024*1024;rejects<std::length_error>([&] {SpatialNeighborhoodRequest bad(d,ids,ids,tiny);});
    tiny=p;tiny.work_bytes=1024*1024;rejects<std::length_error>([&] {SpatialNeighborhoodRequest bad(d,ids,ids,tiny);});
    tiny=p;tiny.max_distance_tests=1;SpatialNeighborhoodRequest limited(d,ids,ids,tiny);
    check(job.request(limited) && job.wait(d).state=="failed" && !job.result() && job.poll(d).error.find("budget")!=std::string::npos);
    auto budget=p;budget.snapshot_bytes*=2;budget.work_bytes*=2;
    check(SpatialNeighborhoodRequest(d,ids,ids,p).input_sha256()==SpatialNeighborhoodRequest(d,ids,ids,budget).input_sha256());
    {SpatialNeighborhoodJob shutdown;SpatialNeighborhoodRequest captured(d,ids,ids,p);check(shutdown.request(captured));}
    check(server->reads.load()==0);
    std::cout<<"Neighborhood jobs: direct parity, wait/owner/cancel/retry/stale, snapshot/work/comparison bounds and joining shutdown passed\n";
}
int main() {try {index_oracle();captured_contracts();lifecycle_and_budgets();return 0;}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
