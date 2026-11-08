#include "qupathxx/spatial_density.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("density check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool hit=false;try {fn();}catch(const E&) {hit=true;}check(hit);}
void near(double a,double b) {check(std::abs(a-b)<=1e-12*std::max({1.,std::abs(a),std::abs(b)}));}
class MetadataOnlyServer final:public ImageServer {
public:
    ImageMetadata info;mutable std::atomic<int> reads{0};
    MetadataOnlyServer() {info.width=info.height=100;info.levels={{100,100,1}};}
    const ImageMetadata& metadata() const override {return info;}
    std::string reader_id() const override {return "density-metadata-only";}
    PixelBuffer read_region(const RegionRequest&) const override {++reads;throw std::runtime_error("density must not read pixels");}
    void close() override {}
    bool closed() const override {return false;}
};
std::uint64_t cell(Document& d,double x,double y) {return d.add_cell(Roi::rectangle({x-.1,y-.1,.2,.2}));}
void analytic_oracle() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);d.set_calibration(2,.5);
    const auto window=d.add_rectangle({5,5,20,20}),empty=d.add_rectangle({30,30,5,5});
    const auto t1=d.add_rectangle({10,10,10,10}),t2=d.add_rectangle({15,10,10,10});
    const auto exclusion=d.add_rectangle({14,10,2,10});
    std::vector<std::uint64_t> cells;
    // Integer lattice: 16*11 tissue points, minus the closed excluded strip
    // (x=4,5,6). Thus 143 cells, area150-20=130 pixels, not bounding box225.
    for(int y=0;y<=10;++y) for(int x=0;x<=15;++x) cells.push_back(cell(d,x+10,y+10));
    cells.push_back(cell(d,9,15));cells.push_back(cell(d,26,15));
    SpatialDensityRequest request(d,{empty,window},{t2,t1},{exclusion},cells);
    auto result=measure_spatial_density(request);check(result->rows().size()==2 && result->belongs_to(request));
    const auto& row=result->rows()[0];check(row.window_id==window && row.count==143);near(row.area_pixels,130);near(row.area_mm2,.00013);near(*row.density_per_mm2,143/.00013);
    check(row.footprint && row.geometry_sha256.size()==64);
    check(!result->rows()[1].footprint && !result->rows()[1].density_per_mm2 && result->rows()[1].count==0 && result->rows()[1].area_pixels==0);
    auto reversed=cells;std::reverse(reversed.begin(),reversed.end());
    SpatialDensityRequest ordered(d,{window,empty},{t1,t2},{exclusion},reversed);
    check(request.input_sha256()==ordered.input_sha256() && measure_spatial_density(ordered)->sha256()==result->sha256() && !result->belongs_to(ordered));
    auto p=request.options();p.work_bytes*=2;p.snapshot_bytes*=2;p.max_membership_tests*=2;p.max_geometry_operations*=2;p.max_geometry_vertices*=2;
    check(SpatialDensityRequest(d,{window,empty},{t1,t2},{exclusion},cells,p).input_sha256()==request.input_sha256());
    auto zero=measure_spatial_density(SpatialDensityRequest(d,{window},{t1,t2},{exclusion},{}));
    check(zero->rows()[0].count==0 && zero->rows()[0].density_per_mm2==0 && zero->membership_tests()==0);
    auto overlap=measure_spatial_density(SpatialDensityRequest(d,{window,t1},{t1,t2},{},cells));
    check(overlap->rows()[0].count==176 && overlap->rows()[1].count==121);near(overlap->rows()[1].area_pixels,100);
    auto async=std::async(std::launch::async,[request] {return measure_spatial_density(request);});check(async.get()->sha256()==result->sha256());
    auto wrong=std::async(std::launch::async,[&] {rejects<std::runtime_error>([&] {request.matches(d);});});wrong.get();
    check(server->reads.load()==0);
    std::cout<<"Density analytic oracle: clipped overlapping tissue, exclusion edges, inclusive outer edges, independent windows, calibrated area, empty/zero and deterministic identity passed\n";
}
void holes_curves_and_nuclei() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);d.set_calibration(.5,2);
    const auto window=d.add_rectangle({0,0,20,20});
    const auto hole=d.add_roi(Roi::polygon({{0,0},{10,0},{10,10},{0,10}},{{{4,4},{6,4},{6,6},{4,6}}}));
    const auto edge=cell(d,4,5),inside=cell(d,5,5),outside=cell(d,2,2);
    auto result=measure_spatial_density(SpatialDensityRequest(d,{window},{hole},{},{edge,inside,outside}));
    check(result->rows()[0].count==2);near(result->rows()[0].area_pixels,96);
    const auto multipart=d.add_roi(Roi::multipolygon({PolygonPart({{0,0},{2,0},{2,2},{0,2}}),PolygonPart({{8,8},{10,8},{10,10},{8,10}})}));
    auto multi=measure_spatial_density(SpatialDensityRequest(d,{window},{multipart},{},{outside,inside}));
    check(multi->rows()[0].count==1);near(multi->rows()[0].area_pixels,8);
    const auto curved=d.add_roi(Roi::ellipse({0,0,10,10}));
    rejects<std::invalid_argument>([&] {SpatialDensityRequest invalid(d,{window},{curved},{},{outside});});
    SpatialDensityOptions p;p.polygonize_curves=true;
    auto curve=measure_spatial_density(SpatialDensityRequest(d,{window},{curved},{},{inside},p));
    near(curve->rows()[0].area_pixels,d.objects().at(curved).geometry.as_polygon().area());check(curve->rows()[0].count==1);
    const auto narrow=d.add_rectangle({0,0,4,4});
    const auto shifted=d.add_cell(Roi::rectangle({0,0,10,10}),Roi::rectangle({1,1,2,2}));
    const auto fallback=d.add_cell(Roi::rectangle({1,1,2,2}));
    SpatialDensityRequest nucleus(d,{window},{narrow},{},{shifted,fallback});
    check(nucleus.cell_object(0).nucleus && !nucleus.cell_object(1).nucleus);
    check(measure_spatial_density(nucleus)->rows()[0].count==2);
    p={};p.prefer_nucleus=false;check(measure_spatial_density(SpatialDensityRequest(d,{window},{narrow},{},{shifted,fallback},p))->rows()[0].count==1);
    // Area-role Cell must always supply its body, even with nucleus preference.
    near(measure_spatial_density(SpatialDensityRequest(d,{window},{shifted},{},{}))->rows()[0].area_pixels,100);
    const auto excluded_all=measure_spatial_density(SpatialDensityRequest(d,{window},{hole},{window},{inside}));
    check(!excluded_all->rows()[0].density_per_mm2 && excluded_all->rows()[0].area_mm2==0);
    check(server->reads.load()==0);
    std::cout<<"Density topology: holes/closed hole edges, multipart, explicit512-segment curves, nucleus/body preference and complete exclusion passed\n";
}
void rounded_bounds() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);d.set_calibration(1,1);
    const double a=5.903720019708988,b=62.638935451312285;
    const auto tissue=d.add_rectangle({0,0,100,100}),left=d.add_rectangle({b,0,1,20});
    const auto wide=d.add_roi(Roi::polygon({{a,0},{b,0},{b,20},{a,20}}));
    const auto ca=d.add_cell(Roi::rectangle({a-.125,9.875,.25,.25})),cb=d.add_cell(Roi::rectangle({b-.125,9.875,.25,.25}));
    SpatialDensityRequest request(d,{left,wide},{tissue},{},{ca,cb});
    check(request.cell_object(0).centroid.x==a && request.cell_object(1).centroid.x==b);
    const auto result=measure_spatial_density(request);
    check(result->rows()[0].count==1 && result->rows()[1].count==2);
    std::cout<<"Density exact closed edges survive inward-rounded index and footprint extents\n";
}
void admission_and_jobs() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);
    const auto window=d.add_rectangle({0,0,20,20}),tissue=d.add_rectangle({0,0,10,10}),a=cell(d,2,2),b=cell(d,3,3);
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{tissue},{},{a});});d.set_calibration(2,3);
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{}, {tissue},{},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{},{},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window,window},{tissue},{},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{tissue,tissue},{},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{tissue},{tissue,tissue},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{tissue},{},{a,a});});
    rejects<std::out_of_range>([&] {SpatialDensityRequest bad(d,{window},{999999},{},{a});});
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{window},{tissue},{},{window});});
    const auto line=d.add_roi(Roi::polyline({{0,0},{1,1}}));
    rejects<std::invalid_argument>([&] {SpatialDensityRequest bad(d,{line},{tissue},{},{a});});
    SpatialDensityOptions p;p.max_geometry_vertices=7;
    rejects<std::length_error>([&] {SpatialDensityRequest bad(d,{window},{tissue},{},{a},p);});
    p={};p.work_bytes=1024*1024;
    rejects<std::length_error>([&] {SpatialDensityRequest bad(d,{window},{tissue},{},{a},p);});
    p={};p.max_geometry_operations=1;
    rejects<std::length_error>([&] {measure_spatial_density(SpatialDensityRequest(d,{window},{tissue},{},{a},p));});
    p={};p.max_membership_tests=1;
    SpatialDensityRequest limited(d,{window},{tissue},{},{a,b},p);
    rejects<std::length_error>([&] {measure_spatial_density(limited);});
    SpatialDensityRequest request(d,{window},{tissue},{},{a,b});auto direct=measure_spatial_density(request);
    std::atomic<bool> stop{true};rejects<std::runtime_error>([&] {measure_spatial_density(request,&stop);});
    SpatialDensityJob job;check(job.request(request) && job.busy() && !job.request(request));job.wait_work();check(job.busy() && !job.result());
    auto stats=job.wait(d);check(stats.state=="ready" && stats.completed_rows==1 && stats.total_rows==1 && stats.membership_tests==direct->membership_tests() && stats.geometry_operations==direct->geometry_operations());
    check(job.result()->sha256()==direct->sha256());auto held=job.result();
    job.cancel();check(!job.result() && job.poll(d).state=="cancelled" && held->rows()[0].count==2);
    check(job.request(limited) && job.wait(d).state=="failed" && !job.result());
    check(job.request(request));job.cancel();check(job.wait(d).state=="cancelled");
    check(job.request(request));d.rename_annotation(a,"changed");check(job.wait(d).state=="stale" && !job.result());
    check(!request.matches(d) && measure_spatial_density(request)->sha256()==direct->sha256());
    std::optional<SpatialDensityRequest> detached;std::weak_ptr<MetadataOnlyServer> weak;
    {auto source=std::make_shared<MetadataOnlyServer>();weak=source;Document local(source);local.set_calibration(1,1);auto w=local.add_rectangle({0,0,10,10});auto c=cell(local,2,2);detached.emplace(local,std::vector<std::uint64_t>{w},std::vector<std::uint64_t>{w},std::vector<std::uint64_t>{},std::vector<std::uint64_t>{c});}
    check(weak.expired() && measure_spatial_density(*detached)->rows()[0].count==1);
    // A representable final area must survive extreme intermediate scales.
    d.set_calibration(1e308,1e-308);near(measure_spatial_density(SpatialDensityRequest(d,{window},{tissue},{},{}))->rows()[0].area_mm2,.0001);
    d.set_calibration(1e-300,1e-300);rejects<std::overflow_error>([&] {measure_spatial_density(SpatialDensityRequest(d,{window},{tissue},{},{}));});
    check(server->reads.load()==0);
    std::cout<<"Density admission/jobs: calibration, required/duplicate/missing/typed cohorts, geometry/work/predicate quotas, cancel/stale/retry, detached capture and extreme area arithmetic passed\n";
}
int main() {try {analytic_oracle();holes_curves_and_nuclei();rounded_bounds();admission_and_jobs();return 0;}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
