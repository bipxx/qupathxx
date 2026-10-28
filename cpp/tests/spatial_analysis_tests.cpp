#include "qupathxx/spatial_analysis.hpp"
#include "../sha256.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#ifdef QPX_SPATIAL_ORACLE
#include <nlohmann/json.hpp>
#endif
using namespace qpx;
void checked(bool value,int line) {if(!value) throw std::runtime_error("spatial analysis check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E=std::exception,class F> void rejects(F fn) {bool hit=false;try {fn();}catch(const E&) {hit=true;}check(hit);}
bool near(double a,double b) {return std::isfinite(a) && std::abs(a-b)<=1e-9+1e-12*std::abs(b);}
class MetadataOnlyServer final:public ImageServer {
public:
    ImageMetadata info;mutable std::atomic<int> reads{0};bool closed_=false;
    MetadataOnlyServer() {info.width=info.height=4096;info.levels={{4096,4096,1}};}
    const ImageMetadata& metadata() const override {return info;}
    std::string reader_id() const override {return "spatial-metadata-only";}
    PixelBuffer read_region(const RegionRequest&) const override {++reads;throw std::runtime_error("spatial analysis must not read pixels");}
    void close() override {closed_=true;}
    bool closed() const override {return closed_;}
};
void request_and_result_contracts() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);
    const auto a=d.add_detection(Roi::points({{32,32}})),b=d.add_detection(Roi::points({{35,36}})),c=d.add_detection(Roi::points({{32,32}}));
    SpatialDistanceRequest request(d,{b,a},{c,b,a});check(request.source_count()==2 && request.target_count()==3);
    check(request.source_object(0).id==a && request.source_object(1).id==b && request.snapshot_bytes()<=request.options().snapshot_bytes);
    check(request.required_work_bytes()<=request.options().work_bytes && request.matches(d));
    check(request.source_object(0).geometry_sha256.size()==64 && request.provenance()->source_only && request.provenance()->object_id==0);
    const auto revision=d.revision();auto out=measure_spatial_distances(request);check(out->belongs_to(request));
    auto copy=request;check(out->belongs_to(copy));SpatialDistanceRequest repeat(d,{a,b},{a,b,c});
    check(repeat.input_sha256()==request.input_sha256() && !out->belongs_to(repeat));
    check(out->input_sha256()==request.input_sha256() && out->sha256()==measure_spatial_distances(repeat)->sha256());
    check(out->rows().size()==2 && out->rows()[0].source_id==a && out->rows()[0].target_id==c && out->rows()[0].distance==0);
    check(out->rows()[1].target_id==a && out->rows()[1].distance==5 && out->distance_tests()>0 && out->document_revision()==revision);
    check(d.revision()==revision && server->reads.load()==0 && out->payload_bytes()<=request.options().work_bytes);
    auto include=request.options();include.exclude_self=false;SpatialDistanceRequest self(d,{a},{a},include);
    check(measure_spatial_distances(self)->rows()[0].target_id==a && measure_spatial_distances(self)->rows()[0].distance==0);
    auto absent=measure_spatial_distances(SpatialDistanceRequest(d,{a,b},{}));check(absent->rows().size()==2);
    for(const auto& row:absent->rows()) check(!row.distance && !row.target_id && !row.target_revision && !row.target_centroid && !row.target_kind);
    check(absent->distance_tests()==0);
    auto excluded=measure_spatial_distances(SpatialDistanceRequest(d,{a},{a}));check(!excluded->rows()[0].distance);
    SpatialDistanceOptions physical;physical.units=SpatialDistanceUnits::Micrometres;
    rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a},{b},physical);});
    d.set_calibration(2,.5);SpatialDistanceRequest calibrated(d,{a},{b},physical);
    check(calibrated.x_scale()==2 && calibrated.y_scale()==.5 && near(*measure_spatial_distances(calibrated)->rows()[0].distance,std::sqrt(40.)));
    check(!request.matches(d) && out->rows()[1].distance==5);
    Document cloned=d;check(!calibrated.matches(cloned));Document assigned(server);assigned=d;check(!calibrated.matches(assigned));
    server->close();check(calibrated.matches(d));check(measure_spatial_distances(calibrated)->rows().size()==1 && server->reads.load()==0);
    d.clear_calibration();check(!calibrated.matches(d));
    const auto cell=d.add_cell(Roi::rectangle({64,64,100,100}),Roi::rectangle({65,65,2,2}));
    const auto point=d.add_detection(Roi::points({{69,70}}));SpatialDistanceRequest nuclear(d,{cell},{point});
    auto nuclear_result=measure_spatial_distances(nuclear);check(nuclear_result->rows()[0].source_nucleus && nuclear_result->rows()[0].distance==5);
    auto body=nuclear.options();body.prefer_nucleus=false;SpatialDistanceRequest bodies(d,{cell},{point},body);
    check(!bodies.source_object(0).nucleus && bodies.source_object(0).centroid.x==114 && bodies.input_sha256()!=nuclear.input_sha256());
    d.update_nucleus(cell,std::nullopt);SpatialDistanceRequest fallback(d,{cell},{point});check(!fallback.source_object(0).nucleus && fallback.source_object(0).centroid.x==114);
    rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{},{});});
    rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a,a},{b});});
    rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a},{b,b});});
    rejects<std::out_of_range>([&] {SpatialDistanceRequest bad(d,{999999},{b});});
    rejects<std::out_of_range>([&] {request.source_object(99);});
    auto invalid=request.options();invalid.units=static_cast<SpatialDistanceUnits>(7);rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a},{b},invalid);});
    invalid=request.options();invalid.snapshot_bytes=1;rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a},{b},invalid);});
    invalid=request.options();invalid.max_distance_tests=0;rejects<std::invalid_argument>([&] {SpatialDistanceRequest bad(d,{a},{b},invalid);});
    std::atomic<bool> stop{true};rejects<std::runtime_error>([&] {measure_spatial_distances(request,&stop);});
    auto limited=request.options();limited.max_distance_tests=1;
    rejects<std::length_error>([&] {measure_spatial_distances(SpatialDistanceRequest(d,{a,b},{a,b,c},limited));});
    auto budget_only=request.options();budget_only.work_bytes*=2;SpatialDistanceRequest extra_budget(d,{a},{b},budget_only),same_inputs(d,{a},{b});
    check(extra_budget.input_sha256()==same_inputs.input_sha256());
    auto future=std::async(std::launch::async,[request] {return measure_spatial_distances(request);});check(future.get()->sha256()==out->sha256());
    auto wrong=std::async(std::launch::async,[&] {rejects<std::runtime_error>([&] {request.matches(d);});});wrong.get();
    std::cout<<"Spatial captured results: deterministic identity, nucleus/body fallback, anisotropic units, absent targets, limits, clone/stale/thread guards and zero pixel IO passed\n";
}
void lifecycle_and_budget_contracts() {
    auto server=std::make_shared<MetadataOnlyServer>();Document d(server);std::vector<std::uint64_t> ids;
    std::vector<Annotation> objects;objects.reserve(3000);
    for(int i=0;i<3000;++i) {Annotation a{std::uint64_t(i+1),"Point",Roi::points({{double(i%100)*8,double(i/100)*8}})};a.kind=ObjectKind::Detection;objects.push_back(std::move(a));}
    ids=d.append_objects(objects,{});
    SpatialDistanceRequest request(d,ids,ids);auto direct=measure_spatial_distances(request);
    SpatialDistanceJob job;check(job.request(request) && job.busy() && !job.request(request));
    check(job.wait(d).state=="ready" && !job.busy());auto held=job.result();check(held && held->sha256()==direct->sha256());
    const auto finished=job.poll(d);check(finished.completed_rows==ids.size() && finished.distance_tests==held->distance_tests() && job.poll(d).elapsed_ms==finished.elapsed_ms);
    auto wrong=std::async(std::launch::async,[&] {rejects<std::runtime_error>([&] {job.busy();});});wrong.get();
    auto cancelling=std::async(std::launch::async,[&] {job.cancel();});cancelling.get();check(!job.result() && job.poll(d).state=="cancelled" && held->rows().size()==ids.size());
    check(job.request(request));job.cancel();check(job.wait(d).state=="cancelled" && !job.result() && !job.busy());
    check(job.request(request));d.rename_annotation(ids.front(),"changed during analysis");check(job.wait(d).state=="stale" && !job.result());
    SpatialDistanceRequest fresh(d,ids,ids);check(job.request(fresh) && job.wait(d).state=="ready");d.rename_annotation(ids.back(),"after ready");check(job.poll(d).state=="stale" && !job.result());
    auto tiny=fresh.options();tiny.snapshot_bytes=1024*1024;
    rejects<std::length_error>([&] {SpatialDistanceRequest bad(d,ids,ids,tiny);});
    tiny=fresh.options();tiny.work_bytes=1024*1024;rejects<std::length_error>([&] {SpatialDistanceRequest bad(d,ids,ids,tiny);});
    tiny=fresh.options();tiny.max_distance_tests=1;SpatialDistanceRequest refuse(d,ids,ids,tiny);check(job.request(refuse));check(job.wait(d).state=="failed" && !job.result() && job.poll(d).error.find("budget")!=std::string::npos);
    {
        SpatialDistanceJob finishing;SpatialDistanceRequest captured(d,ids,ids);check(finishing.request(captured));
    }
    check(server->reads.load()==0);
    SpatialIndex points({{1,{0,0,0,0}},{2,{3,4,0,0}}});std::uint64_t remaining=1;
    rejects<std::length_error>([&] {points.nearest_point({0,0},1,1,{},nullptr,&remaining);});check(remaining==0);
    check(points.payload_bytes()<=SpatialIndex::estimated_bytes(2));rejects<std::length_error>([] {SpatialIndex::estimated_bytes(SIZE_MAX);});
    // The worker/result retain no ImageServer or live Document ownership.
    std::optional<SpatialDistanceRequest> detached;std::weak_ptr<MetadataOnlyServer> weak;
    {auto image=std::make_shared<MetadataOnlyServer>();weak=image;Document local(image);auto a=local.add_rectangle({1,1,2,2}),b=local.add_rectangle({4,5,2,2});detached.emplace(local,std::vector<std::uint64_t>{a},std::vector<std::uint64_t>{b});}
    check(weak.expired());check(measure_spatial_distances(*detached)->rows()[0].distance==5);
    std::cout<<"Spatial owner jobs: direct parity, cancellation/retry, running/ready staleness, payload/compute admission, shutdown join and reader-free detached execution passed\n";
}
#ifdef QPX_SPATIAL_ORACLE
void upstream_reference_contracts() {
    using Json=nlohmann::json;
    const std::string root=QPX_SPATIAL_SOURCE_ROOT;
    auto bytes=[](const std::string& path) {std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("missing spatial reference: "+path);return std::string(std::istreambuf_iterator<char>(input),{});};
    const auto folder=root+"/tests/fixtures/qupath-spatial-0.7.0/";
    const auto manifest=Json::parse(bytes(folder+"manifest.json"));
    for(const auto& entry:manifest.at("files").items()) {const auto value=bytes(root+"/"+entry.key());Sha256 hash;hash.update(value.data(),value.size());check(value.size()==entry.value().at("bytes").get<std::size_t>() && hash.finish()==entry.value().at("sha256").get<std::string>());}
    const auto contract=Json::parse(bytes(folder+"contract.json")),reference=Json::parse(bytes(folder+"reference.json"));
    check(reference.at("qupath_version")=="0.7.0" && reference.at("upstream_revision")==contract.at("upstream_revision"));
    check(contract.at("cases").size()==26 && reference.at("cases").size()==26);
    std::size_t geometry=0,storage=0,planes=0,polygons=0;
    auto roi=[](const Json& spec) {
        const auto kind=spec.at("kind").get<std::string>();
        if(spec.contains("bounds")) {const auto& b=spec.at("bounds");Rect r{b[0].get<double>(),b[1].get<double>(),b[2].get<double>(),b[3].get<double>()};return kind=="ellipse"?Roi::ellipse(r):Roi::rectangle(r);}
        Ring points;for(const auto& p:spec.at("points")) points.push_back({p[0].get<double>(),p[1].get<double>()});
        return kind=="polygon"?Roi::polygon(points):kind=="polyline"?Roi::polyline(points):Roi::points(points);
    };
    for(std::size_t n=0;n<26;++n) {
        const auto& c=contract.at("cases").at(n);const auto& expected=reference.at("cases").at(n);const auto name=c.at("name").get<std::string>();check(expected.at("name")==name);
        bool nonzero_plane=false,polygon=false;for(const auto& object:c.at("objects")) {nonzero_plane=nonzero_plane || object.value("z",0)!=0 || object.value("t",0)!=0;polygon=polygon || object.at("body").at("kind")=="polygon";}
        if(nonzero_plane) {++planes;continue;}if(polygon && !has_geos()) {++polygons;continue;}
        auto server=std::make_shared<MetadataOnlyServer>();Document d(server);std::map<std::string,std::uint64_t> mapping;
        auto add=[&](const Json& object) {
            const auto body=roi(object.at("body")).translated(32,32);const auto nucleus=object.contains("nucleus")?std::optional<Roi>(roi(object.at("nucleus")).translated(32,32)):std::nullopt;
            const auto id=(nucleus || object.value("object_type",std::string{})=="cell")?d.add_cell(body,nucleus):d.add_detection(body);mapping.emplace(object.at("id").get<std::string>(),id);
        };
        for(const auto& object:c.at("objects")) add(object);
        std::vector<std::uint64_t> targets;for(const auto& id:c.at("targets")) targets.push_back(mapping.at(id.get<std::string>()));
        if(c.contains("generated_targets")) {const auto& g=c.at("generated_targets");for(int i=0;i<g.at("count").get<int>();++i) {const auto x=g.at("start")[0].get<double>()+i*g.at("step")[0].get<double>()+32,y=g.at("start")[1].get<double>()+i*g.at("step")[1].get<double>()+32;targets.push_back(d.add_detection(Roi::points({{x,y}})));}}
        d.set_calibration(c.at("scale")[0].get<double>(),c.at("scale")[1].get<double>());
        SpatialDistanceOptions options;options.units=SpatialDistanceUnits::Micrometres;options.exclude_self=c.at("exclude_self").get<bool>();
        SpatialDistanceRequest request(d,{mapping.at(c.at("source").get<std::string>())},targets,options);auto result=measure_spatial_distances(request);const auto& row=result->rows().at(0);
        check(near(row.source_centroid.x,expected.at("source_centroid")[0].get<double>()+32) && near(row.source_centroid.y,expected.at("source_centroid")[1].get<double>()+32));
        if(expected.at("measured").get<bool>()) {
            check(bool(row.distance));const auto value=expected.at("distance").get<double>();
            if(name.rfind("default_float_",0)==0 || name.rfind("explicit_double_",0)==0) {check(double(float(*row.distance))==value && *row.distance!=value);++storage;}
            else {check(near(*row.distance,value));++geometry;}
        } else {check(!row.distance && !row.target_id);++geometry;}
        check(server->reads.load()==0);
    }
    check(planes==4 && storage==4 && geometry==(has_geos()?18:16) && polygons==(has_geos()?0:2));
    std::cout<<"Spatial pinned QuPath reference: geometry="<<geometry<<" explicit_storage_conversion="<<storage<<" unsupported_planes="<<planes<<" unavailable_polygons="<<polygons<<"; fixed tolerances and translated single-plane geometry passed\n";
}
#endif
int main() {try {
    request_and_result_contracts();lifecycle_and_budget_contracts();
#ifdef QPX_SPATIAL_ORACLE
    upstream_reference_contracts();
#else
    std::cout<<"Spatial pinned QuPath reference unavailable: INTERCHANGE OFF\n";
#endif
    return 0;
}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
