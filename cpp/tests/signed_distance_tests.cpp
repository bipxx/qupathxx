#include "qupathxx/spatial_frontend.hpp"
#include "spatial_archive.hpp"
#include "spatial_cli.hpp"
#include "sha256.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace qpx;
namespace fs=std::filesystem;
void checked(bool b,int line) {if(!b)throw std::runtime_error("signed distance check failed at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class F>void rejects(F f) {bool hit=false;try{f();}catch(const std::exception&){hit=true;}check(hit);}
bool near(double a,double b) {return std::abs(a-b)<1e-11;}
std::uint64_t cell(Document& d,double x,double y) {return d.add_cell(Roi::rectangle({x-.5,y-.5,1,1}));}
SpatialDistanceOptions options() {SpatialDistanceOptions o;o.metric=SpatialDistanceMetric::SignedBoundary;return o;}
void reseal(std::vector<std::uint8_t>& bytes) {Sha256 h;h.update(bytes.data(),bytes.size()-64);const auto s=h.finish();std::copy(s.begin(),s.end(),bytes.end()-64);}
int main() {try {
    static_assert(sizeof(SpatialDistanceOptions)==32,"preserve historical centroid capture layout");
    auto doc=std::make_shared<Document>(Image::demo());auto& d=*doc;auto o=options();
    const auto region=d.add_rectangle({10,10,20,20});
    const auto inside=cell(d,15,20),outside=cell(d,35,20),edge=cell(d,10,20),corner=cell(d,7,6);
    SpatialDistanceRequest r(d,{inside,outside,edge,corner},{region},o);const auto out=measure_spatial_distances(r);
    check(out->rows()[0].distance==-5 && out->rows()[1].distance==5 && out->rows()[2].distance==0 && !std::signbit(*out->rows()[2].distance));
    check(out->rows()[3].distance==5 && out->distance_tests()==16);
    check(std::string(out->algorithm())=="qpx.signed-boundary-nearest/1");
    check(spatial_distance_csv(*out).find(",metric,geometry_policy\n")!=std::string::npos);
    check(!measure_spatial_distances(SpatialDistanceRequest(d,{inside},{},o))->rows()[0].distance);
    auto physical=o;physical.units=SpatialDistanceUnits::Micrometres;
    rejects([&]{SpatialDistanceRequest bad(d,{inside},{region},physical);});
    d.set_calibration(2,.5);check(!r.matches(d));
    check(measure_spatial_distances(SpatialDistanceRequest(d,{inside},{region},physical))->rows()[0].distance==-5);
    check(near(*measure_spatial_distances(SpatialDistanceRequest(d,{corner},{region},physical))->rows()[0].distance,std::sqrt(40.)));
    const auto hole=d.add_roi(Roi::polygon({{40,10},{70,10},{70,40},{40,40}},{{{50,20},{60,20},{60,30},{50,30}}}));
    const auto in_hole=cell(d,55,25),hole_edge=cell(d,50,25),shell=cell(d,47,25);
    const auto holes=measure_spatial_distances(SpatialDistanceRequest(d,{in_hole,hole_edge,shell},{hole},o));
    check(holes->rows()[0].distance==5 && holes->rows()[1].distance==0 && holes->rows()[2].distance==-3);
    const auto multi=d.add_roi(Roi::multipolygon({PolygonPart({{80,10},{90,10},{90,20},{80,20}}),PolygonPart({{100,10},{110,10},{110,20},{100,20}})}));
    const auto second=cell(d,105,15);check(measure_spatial_distances(SpatialDistanceRequest(d,{second},{multi},o))->rows()[0].distance==-5);
    const auto triangle=d.add_roi(Roi::polygon({{10,50},{30,70},{10,70}}));
    const auto diagonal=cell(d,15,55),diagonal_out=cell(d,20,55);
    check(measure_spatial_distances(SpatialDistanceRequest(d,{diagonal},{triangle},physical))->rows()[0].distance==0);
    const auto close_edge=cell(d,15,55+1e-10);
    const auto close_distance=*measure_spatial_distances(SpatialDistanceRequest(d,{close_edge},{triangle},o))->rows()[0].distance;
    check(close_distance<0 && close_distance!=0 && std::abs(close_distance)<1e-9);
    // Independent analytic distance to scaled line y=x+40: |2*.5*(x-y+40)|/hypot(2,.5).
    check(near(*measure_spatial_distances(SpatialDistanceRequest(d,{diagonal_out},{triangle},physical))->rows()[0].distance,5/std::hypot(2.,.5)));
    const auto same=d.add_rectangle({10,10,20,20});
    check(measure_spatial_distances(SpatialDistanceRequest(d,{inside},{same,region},o))->rows()[0].target_id==region);
    const auto nearer=d.add_rectangle({16,10,20,20});
    const auto overlap=measure_spatial_distances(SpatialDistanceRequest(d,{inside},{region,nearer},o));
    check(overlap->rows()[0].target_id==nearer && overlap->rows()[0].distance==1); // individual boundaries, not union
    const auto nuclear=d.add_cell(Roi::rectangle({10,10,20,20}),Roi::rectangle({11,14,2,2}));
    check(measure_spatial_distances(SpatialDistanceRequest(d,{nuclear},{region},o))->rows()[0].distance==-2);
    auto body=o;body.prefer_nucleus=false;check(measure_spatial_distances(SpatialDistanceRequest(d,{nuclear},{region},body))->rows()[0].distance==-10);
    rejects([&]{SpatialDistanceRequest bad(d,{region},{region},o);});
    rejects([&]{SpatialDistanceRequest bad(d,{inside},{nuclear},o);});
    const auto line=d.add_roi(Roi::polyline({{10,80},{20,80}}));rejects([&]{SpatialDistanceRequest bad(d,{inside},{line},o);});
    const auto ellipse=d.add_roi(Roi::ellipse({40,50,20,10}));rejects([&]{SpatialDistanceRequest bad(d,{inside},{ellipse},o);});
    auto curved=o;curved.polygonize_curves=true;const auto curved_request=SpatialDistanceRequest(d,{inside},{ellipse},curved);
    check(measure_spatial_distances(curved_request)->rows()[0].distance>0);
    auto limited=o;limited.max_distance_tests=3;rejects([&]{measure_spatial_distances(SpatialDistanceRequest(d,{inside},{region},limited));});
    Ring many;for(int i=0;i<10001;++i) {const double t=i*6.283185307179586/10001;many.push_back({100+10*std::cos(t),80+10*std::sin(t)});}
    const auto complex=d.add_roi(Roi::polygon(std::move(many)));rejects([&]{SpatialDistanceRequest bad(d,{inside},{complex},o);});
    auto small=o;small.snapshot_bytes=1024*1024;std::vector<std::uint64_t> excessive(100000,inside);
    rejects([&]{SpatialDistanceRequest bad(d,excessive,{region},small);});
    SpatialNeighborhoodOptions wrong;wrong.metric=SpatialDistanceMetric::SignedBoundary;rejects([&]{wrong.validate();});
    std::atomic<bool> cancel{true};rejects([&]{measure_spatial_distances(curved_request,&cancel);});cancel=false;
    SpatialDistanceJob job;SpatialDistanceRequest captured(d,{inside},{region},o);check(job.request(captured));
    d.set_calibration(1,1);check(job.wait(d).state=="stale" && !job.result());
    SpatialDistanceRequest current(d,{inside,outside,edge},{region,hole},o);auto fresh=measure_spatial_distances(current);
    const auto id=d.keep_spatial(current,fresh,"Signed boundary");const auto record=d.spatial_record(id);
    const auto bytes=SpatialDistanceArchiveCodec::encode(*record);check(bytes[3]==2);
    const auto decoded=SpatialDistanceArchiveCodec::decode(bytes.data(),bytes.size());
    check(decoded->result->sha256()==fresh->sha256() && spatial_distance_csv(*decoded->result)==spatial_distance_csv(*fresh));
    check(SpatialDistanceArchiveCodec::encode(*decoded)==bytes);
    auto bad=bytes;bad[3]=3;reseal(bad);rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    bad=bytes;bad[bad.size()/2]^=1;rejects([&]{SpatialDistanceArchiveCodec::decode(bad.data(),bad.size());});
    rejects([&]{SpatialDistanceArchiveCodec::decode(bytes.data(),bytes.size()-1);});
    SpatialDistanceRequest curve_now(d,{inside},{ellipse},curved);const auto curve_id=d.keep_spatial(curve_now,measure_spatial_distances(curve_now));
    const auto curve_bytes=SpatialDistanceArchiveCodec::encode(*d.spatial_record(curve_id));
    check(SpatialDistanceArchiveCodec::decode(curve_bytes.data(),curve_bytes.size())->result->options().polygonize_curves);
    SpatialDistanceRequest centroid(d,{inside},{region});const auto legacy_id=d.keep_spatial(centroid,measure_spatial_distances(centroid));
    const auto legacy_bytes=SpatialDistanceArchiveCodec::encode(*d.spatial_record(legacy_id));check(legacy_bytes[3]==1);
    check(SpatialDistanceArchiveCodec::encode(*SpatialDistanceArchiveCodec::decode(legacy_bytes.data(),legacy_bytes.size()))==legacy_bytes);
    if(has_project()) {
        Project project;const auto image=project.add_document(doc,"Signed distances");
        const auto root=fs::current_path()/(project.identity()+"-signed-distance");fs::create_directory(root);
        const auto path=root/"signed.qpxproj";project.save_new(path.u8string());auto reopened=Project::open(path.u8string());auto rd=reopened->document(image);
        check(rd->spatial_record(id)->result->sha256()==fresh->sha256());
        const auto replay=SpatialDistanceRequest::replay(*rd,id);check(rd->verify_spatial(id,replay,measure_spatial_distances(replay)));
        detail::SpatialFrontend frontend;frontend.inspect(*rd,id);check(frontend.report().result->options().metric==SpatialDistanceMetric::SignedBoundary);
        frontend.verify(*rd,id);frontend.finish(*rd,true);check(frontend.report().record_status=="current");
        std::vector<std::string> args={"batch","--measure-spatial",path.u8string(),(root/"cli.qpxproj").u8string(),"--image",std::to_string(image),"--source",std::to_string(inside),"--target",std::to_string(region),"--operation","signed-boundary","--units","um"};
        check(detail::spatial_cli(args,cancel)==0);auto cli=Project::open((root/"cli.qpxproj").u8string());
        check(cli->document(image)->spatial_records().back()->result->rows()[0].distance==-5);
        rejects([&]{project.save_new(path.u8string());});
        rd->set_calibration(2,1);rejects([&]{SpatialDistanceRequest::replay(*rd,id);});
        fs::remove_all(root); // Unique test-owned fixture only.
    }
    std::cout<<"Signed boundary numeric, policy, identity, archive, replay, jobs, frontend and CLI checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
