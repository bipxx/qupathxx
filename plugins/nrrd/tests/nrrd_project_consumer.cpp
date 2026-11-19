// Fresh-process installed-SDK checks for the independently built NRRD module.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "qupathxx/analysis.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/input_digest.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs=std::filesystem;
using namespace qpx;
namespace {
constexpr const char* provider="org.qupathxx.nrrd";
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read authored source/oracle: "+path.u8string());return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::vector<std::uint8_t>& value){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(value.data()),static_cast<std::streamsize>(value.size()));check(bool(out),"Cannot write isolated test input");}
template<class F>void refuses(F&& call,const std::string& label){bool refused=false;try{call();}catch(const std::exception& e){refused=!std::string(e.what()).empty();}check(refused,label+" was not refused with a diagnostic");}
struct Receipt {std::uint64_t id=0,samples=0;std::string digest;ColorVector means{};};
fs::path project_path(const fs::path& work){return work/"nrrd-project.qpxproj";}
void store_receipt(const fs::path& work,const AnalysisRecord& record){std::ofstream out(work/"record.txt");out<<record.id<<' '<<record.result.samples<<' '<<record.result.input_sha256;for(auto value:record.result.means)out<<' '<<std::setprecision(17)<<value;out<<'\n';check(bool(out),"Cannot write retained receipt");}
Receipt receipt(const fs::path& work){Receipt r;std::ifstream in(work/"record.txt");in>>r.id>>r.samples>>r.digest>>r.means[0]>>r.means[1]>>r.means[2];check(bool(in)&&r.id&&r.samples==15&&r.digest.size()==64,"Invalid retained receipt");return r;}
void retained(const Document& doc,const Receipt& expected){check(doc.analysis_record_count()==1,"Retained record count changed");const auto value=doc.analysis_record(expected.id);check(value.id==expected.id&&value.result.samples==expected.samples&&value.result.means==expected.means&&value.result.input_sha256==expected.digest,"Exact stored result identity/values changed");}
template<class T>std::vector<std::uint8_t> storage_bytes(const std::vector<T>& data){std::vector<std::uint8_t> result(data.size()*sizeof(T));if(!result.empty())std::memcpy(result.data(),data.data(),result.size());return result;}
void check_scalar(const ImageServer& server,const fs::path& fixtures,const std::string& display="u16-main"){
    check(server.read_region({0,0,0,5,3}).bytes()==bytes(fixtures/(display+".rgba")),"Host NRRD declared Z0/window display differs");
    const auto metadata=server.raw_metadata();check(metadata&&metadata->width==5&&metadata->height==3&&metadata->channels==1&&metadata->z_planes==2&&metadata->timepoints==1&&metadata->sample_type==SampleType::UInt16&&metadata->channel_names==std::vector<std::string>{"Scalar"}&&metadata->dimension_order=="XYZ"&&metadata->pixel_width_um==.25&&metadata->pixel_height_um==.5&&metadata->z_spacing_um==1.5&&!metadata->time_interval_seconds,"Host scalar volume metadata/calibration differs");
    const auto expected=bytes(fixtures/"u16-main.raw");check(expected.size()==60,"Invalid authored UInt16 volume oracle");for(unsigned z=0;z<2;++z){RawRegionRequest request;request.region={0,0,0,5,3};request.channels=1;request.z=z;const auto raw=server.read_raw(request);check(raw.sample_type()==SampleType::UInt16&&storage_bytes(std::get<std::vector<std::uint16_t>>(raw.storage()))==std::vector<std::uint8_t>(expected.begin()+z*30,expected.begin()+(z+1)*30)&&raw.valid()==std::vector<std::uint8_t>(15,1),"Host UInt16 XYZ plane bits/validity differ");}
    check(!server.raw_view()&&!server.tiff_view()&&!server.tiff_options()&&!server.ome_zarr_options(),"NRRD invented TIFF/Zarr recipe/view access");
}
void check_float(const ImageServer& server,const fs::path& fixtures){
    const auto metadata=server.raw_metadata();check(metadata&&metadata->width==5&&metadata->height==3&&metadata->channels==1&&metadata->z_planes==2&&metadata->sample_type==SampleType::Float32&&metadata->channel_names==std::vector<std::string>{"Scalar"}&&metadata->dimension_order=="XYZ"&&!metadata->pixel_width_um&&!metadata->pixel_height_um&&!metadata->z_spacing_um&&!metadata->time_interval_seconds,"Host Float32 volume metadata/unknown calibration differs");
    const auto expected=bytes(fixtures/"float-volume.raw");check(expected.size()==120,"Invalid authored Float32 volume oracle");auto raw_planes=[&]{for(unsigned z=0;z<2;++z){RawRegionRequest request;request.region={0,0,0,5,3};request.channels=1;request.z=z;const auto raw=server.read_raw(request);check(raw.sample_type()==SampleType::Float32&&storage_bytes(std::get<std::vector<float>>(raw.storage()))==std::vector<std::uint8_t>(expected.begin()+z*60,expected.begin()+(z+1)*60)&&raw.valid()==std::vector<std::uint8_t>(15,1),"Host Float32 IEEE bits/Z/validity differ (NaN/Inf/signed zero are not numeric comparisons)");}};
    raw_planes();check(server.read_region({0,0,0,5,3}).bytes()==bytes(fixtures/"float-volume.rgba"),"Host Float32 display/nonfinite transparency differs");raw_planes();
}
void check_source_set(const ImageServer& server,const std::vector<fs::path>& companions){
    const auto observed=server.reader_source();
    check(observed&&observed->primary,"Host omitted the observed primary file");
    check(observed->primary->path==server.source_locator(),"Host captured a different primary path");
    if(companions.empty()){
        check(observed->scope=="single_file"&&observed->companions.empty(),"Attached NRRD acquired an undeclared companion");
    }else{
        check(observed->scope=="compound_files"&&observed->companions.size()==companions.size(),"Detached NRRD source is missing its file set");
        for(const auto& companion:companions) {
            const auto item=std::find_if(observed->companions.begin(),observed->companions.end(),
                [&](const auto& payload){return payload.path==companion.u8string();});
            check(item!=observed->companions.end()&&item->bytes==fs::file_size(companion)&&!item->identity.empty(),"Incorrect detached payload path, length or identity");
        }
    }
}
void check_source_files(const ImageServer& server,const fs::path& companion={}){
    check_source_set(server,companion.empty()?std::vector<fs::path>{}:std::vector<fs::path>{companion});
}
std::vector<fs::path> scalar_companions(const fs::path& work,bool detached,const std::string& series){
    if(!series.empty())return {work/(series+"-project-u16-01.part"),work/(series+"-project-u16-02.part")};
    if(detached)return {work/"detached-u16-header.raw"};
    return {};
}
void create(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,bool detached=false,const std::string& series={}){
    check(!fs::exists(project_path(work)),"Create check needs a fresh work directory");fs::create_directories(work);
    const auto scalar=work/(detached?"source-scalar.nhdr":"source-scalar.nrrd");
    const auto floating=work/(detached?"source-float.nhdr":"source-float.nrrd");
    fs::copy_file(fixtures/(series.empty()?(detached?"detached-u16-header.nhdr":"u16-main.nrrd"):series+"-project-u16.nhdr"),scalar);
    fs::copy_file(fixtures/(detached?"detached-f32-header.nhdr":"float-volume.nrrd"),floating);
    const auto companions=scalar_companions(work,detached,series);
    for(const auto& payload:companions)fs::copy_file(fixtures/payload.filename(),payload);
    if(detached)fs::copy_file(fixtures/"detached-f32-header.raw.gz",work/"detached-f32-header.raw.gz");
    configure_reader_plugins(manifest.u8string());check(reader_catalog_json().find(provider)!=std::string::npos,"Installed catalog omitted configured NRRD provider");
    Project project;const auto first=project.add_image(scalar.u8string(),provider,"UInt16 XYZ scalar volume");const auto second=project.add_image(floating.u8string(),provider,"Float32 XYZ bit-exact volume");
    check(first==1&&second==2&&project.images().size()==2&&!project.images()[0].embedded&&project.images()[0].reader==provider,"Native plugin project attach lost linked identity");const auto doc=project.document(first);check_scalar(*doc->server(),fixtures);check_float(*project.document(second)->server(),fixtures);
    check_source_set(*doc->server(),companions);
    check_source_files(*project.document(second)->server(),detached?work/"detached-f32-header.raw.gz":fs::path{});
    const auto object=doc->add_rectangle({0,0,5,3});const StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    // Independently window-converted Z0 display bytes feed the same native scientific authority.
    // This tests the new reader without replacing or requalifying the kernel.
    Document reference(Image(5,3,bytes(fixtures/"u16-main.rgb")));const auto ref_object=reference.add_rectangle({0,0,5,3});const auto expected=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==15&&result.means==expected.means&&result.input_sha256==input_sha256(request),"NRRD RegionAnalysis differs from authored-byte native reference");const auto kept=doc->keep_analysis(request,result);check(kept&&doc->analysis_record_count()==1,"Keep did not retain one NRRD result");store_receipt(work,doc->analysis_record(kept));
    for(const auto id:{first,second}){const auto input=project.document(id)->server()->source_locator();project.apply_source_review(project.review_source_verification(id,*fingerprint_file(input)));}
    project.save_new(project_path(work).u8string());check(!project.dirty(),"Save did not reconcile project state");std::cout<<"Native attach/exact UInt16+Float32 XYZ bits/window/calibration/RegionAnalysis Keep/Save passed\n";
}
void reopen(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,bool detached=false,const std::string& series={}){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);check(doc->server()->reader_source()&&doc->server()->reader_source()->scope=="unopened","Project parsing eagerly accessed NRRD provider");retained(*doc,expected);
    configure_reader_plugins(manifest.u8string());check_scalar(*doc->server(),fixtures);check_float(*project->document(2)->server(),fixtures);
    check_source_set(*doc->server(),scalar_companions(work,detached,series));
    check_source_files(*project->document(2)->server(),detached?work/"detached-f32-header.raw.gz":fs::path{});
    const auto request=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,request,analyze(request)),"Exact NRRD saved-result replay refused");retained(*doc,expected);project->save_new((work/"exact-resaved.qpxproj").u8string());const auto again=Project::open((work/"exact-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Exact saved source/raw/record identity, replay and resave passed\n";
}
void missing(const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);retained(*doc,expected);const auto metadata=doc->server()->raw_metadata();check(metadata&&doc->server()->reader_source()->scope=="unopened","Offline cached raw metadata lost");
    const RegionRequest display_request{2,1,0,1,1};validate_region(doc->server()->metadata(),display_request);
    RawRegionRequest raw_request;raw_request.region=display_request;raw_request.first_channel=0;raw_request.channels=1;raw_request.z=1;raw_request.validate(*metadata);
    const std::string diagnostic=std::string("Reader plugin provider unavailable: ")+provider;
    auto unavailable=[&](auto&& read,const char* label){bool seen=false;try{read();}catch(const std::exception& e){check(std::string(e.what()).find(diagnostic)==0,std::string(label)+" failed before provider lookup or with an unrelated diagnostic: "+e.what());seen=true;}check(seen,std::string(label)+" did not refuse its unavailable provider");};
    unavailable([&]{doc->server()->read_region(display_request);},"Missing plugin display read");unavailable([&]{doc->server()->read_raw(raw_request);},"Missing plugin raw read");
    retained(*doc,expected);project->save_new((work/"missing-resaved.qpxproj").u8string());const auto again=Project::open((work/"missing-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Missing provider preserves offline metadata/exact historical record and resave; valid display/raw requests reach provider-unavailable refusal\n";
}
void changed_implementation(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);refuses([&]{doc->server()->read_region({0,0,0,1,1});},"Changed implementation read");retained(*doc,expected);
    const auto review=project->review_source_verification(1,*fingerprint_file((work/"source-scalar.nrrd").u8string()));check(review->comparison()=="changed_implementation"&&review->requires_acceptance(),"Same-version module-byte change escaped implementation review");refuses([&]{project->apply_source_review(review,false);},"Unaccepted implementation change");project->apply_source_review(review,true);check(review->consumed(),"Accepted native implementation review was not consumed");check_scalar(*doc->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);const auto value=analyze(request);check(value.means==expected.means&&value.input_sha256!=expected.digest&&!doc->verify_analysis(expected.id,request,value),"Same pixels authorized old implementation identity");retained(*doc,expected);project->save_new((work/"changed-implementation-adopted.qpxproj").u8string());std::cout<<"Changed module requires review; equal samples do not authorize old retained identity\n";
}
void changed_options(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);
    check_scalar(*doc->server(),fixtures);const auto replay=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,replay,analyze(replay)),"Changed manifest default altered the saved explicit window0..255 binding");retained(*doc,expected);
    Project fresh;const auto image=fresh.add_image((work/"source-scalar.nrrd").u8string(),provider,"Fresh explicit window0..65535");const auto fresh_doc=fresh.document(image);check_scalar(*fresh_doc->server(),fixtures,"u16-main-wide");check(bytes(fixtures/"u16-main.rgba")!=bytes(fixtures/"u16-main-wide.rgba"),"Window fixture does not distinguish defaults");
    const StainTransform transform({{1,0,0},{0,1,0}});const auto object=fresh_doc->add_rectangle({0,0,5,3});AnalysisRequest request(*fresh_doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    Document reference(Image(5,3,bytes(fixtures/"u16-main-wide.rgb")));const auto ref_object=reference.add_rectangle({0,0,5,3});const auto control=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==15&&result.means==control.means&&result.means!=expected.means&&result.input_sha256!=expected.digest,"Fresh changed window did not create its own exact display/input identity");fresh.save_new((work/"changed-options-fresh.qpxproj").u8string());retained(*doc,expected);
    std::cout<<"Changed provider default preserves saved explicit window0..255/replay; fresh window0..65535 attachment preserves raw codes and gets distinct display/result identity\n";
}
void changed_source(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);check_scalar(*doc->server(),fixtures);
    const RegionRequest display_request{2,1,0,1,1};RawRegionRequest raw_request;raw_request.region=display_request;raw_request.first_channel=0;raw_request.channels=1;
    const auto authored_rgba=bytes(fixtures/"u16-main.rgba"),authored_raw=bytes(fixtures/"u16-main.raw");const auto pixel=static_cast<std::size_t>(1*5+2);raw_request.z=1;
    const auto display_before=doc->server()->read_region(display_request);check(display_before.bytes()==std::vector<std::uint8_t>(authored_rgba.begin()+pixel*4,authored_rgba.begin()+(pixel+1)*4),"Source-change display control request failed before mutation");
    const auto raw_before=doc->server()->read_raw(raw_request);check(raw_before.width()==1&&raw_before.height()==1&&raw_before.channels()==1&&raw_before.sample_type()==SampleType::UInt16&&raw_before.valid()==std::vector<std::uint8_t>{1}&&storage_bytes(std::get<std::vector<std::uint16_t>>(raw_before.storage()))==std::vector<std::uint8_t>(authored_raw.begin()+(15+pixel)*2,authored_raw.begin()+(16+pixel)*2),"Source-change raw control request failed before mutation");
    auto refuses_source_change=[&](auto&& read,const char* label){
        bool detected=false;try{read();}catch(const WorkflowInputChanged& e){const std::string message=e.what();check(message.find("Reader plugin source")!=std::string::npos&&message.find("changed")!=std::string::npos,std::string(label)+" reported an unrelated currentness error: "+message);detected=true;}catch(const std::exception& e){throw std::runtime_error(std::string(label)+" reported an unrelated exception instead of WorkflowInputChanged: "+e.what());}check(detected,std::string(label)+" did not detect changed source");
    };
    const auto source=work/"source-scalar.nrrd";const auto original=bytes(source);const auto replacement=bytes(fixtures/"u16-source-changed.nrrd");check(original!=replacement,"Source mutation fixture is unchanged");write(source,replacement);
    try {refuses_source_change([&]{doc->server()->read_region(display_request);},"Changed source display read");refuses_source_change([&]{doc->server()->read_raw(raw_request);},"Changed source raw read");retained(*doc,expected);}catch(...){write(source,original);throw;}write(source,original);std::cout<<"The same valid display/raw requests pass before mutation and refuse source currentness afterwards; retained payload unchanged\n";
}
template<class Read>void require_source_changed(Read&& read,const char* context){
    try{read();}
    catch(const WorkflowInputChanged& e){
        const std::string message=e.what();
        check(message.find("Reader plugin source")!=std::string::npos&&message.find("changed")!=std::string::npos,
              std::string(context)+" reported an unrelated currentness error: "+message);
        return;
    }
    throw std::runtime_error(std::string(context)+" accepted stale detached source identity");
}
void changed_detached_source(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    configure_reader_plugins(manifest.u8string());
    const auto expected=receipt(work);
    const auto project=Project::open(project_path(work).u8string());
    const auto scalar=project->document(1);
    const auto floating=project->document(2);
    const auto payload=work/"detached-u16-header.raw";
    const auto primary=work/"source-scalar.nhdr";
    const auto float_payload=work/"detached-f32-header.raw.gz";
    const auto float_primary=work/"source-float.nhdr";
    check_scalar(*scalar->server(),fixtures);
    check_float(*floating->server(),fixtures);
    check_source_files(*scalar->server(),payload);
    check_source_files(*floating->server(),float_payload);
    const auto observed=scalar->server()->reader_source();
    const auto original_header=bytes(primary);
    const auto original_payload=bytes(payload);
    const auto replacement=work/"replacement.raw";
    write(replacement,original_payload);
    check(MoveFileExW(replacement.c_str(),payload.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
          "Cannot replace the isolated detached payload");
    check(bytes(primary)==original_header&&bytes(payload)==original_payload,"Byte-identical detached replacement changed source bytes");
    const RegionRequest region{0,0,0,5,3};
    RawRegionRequest raw;
    raw.region=region;
    raw.channels=1;
    raw.z=1;
    require_source_changed([&]{scalar->server()->read_region(region);},"Replacement display read");
    require_source_changed([&]{scalar->server()->read_raw(raw);},"Replacement Z1 raw read");
    Project fresh;
    const auto fresh_id=fresh.add_image(primary.u8string(),provider,"Fresh replacement control");
    const auto fresh_server=fresh.document(fresh_id)->server();
    check_scalar(*fresh_server,fixtures);
    check_source_files(*fresh_server,payload);
    const auto refreshed=fresh_server->reader_source();
    check(refreshed->primary->identity==observed->primary->identity&&
          refreshed->companions.front().identity!=observed->companions.front().identity,
          "Replacement control did not isolate the changed companion file identity");
    const auto float_header=bytes(float_primary);
    std::string redirected(float_header.begin(),float_header.end());
    const auto declaration=float_payload.filename().u8string();
    const auto position=redirected.find(declaration);
    check(position!=std::string::npos,"Cannot locate detached Float32 payload declaration");
    const auto alternative=work/"other-float.raw.gz";
    fs::copy_file(float_payload,alternative);
    redirected.replace(position,declaration.size(),alternative.filename().u8string());
    write(float_primary,{redirected.begin(),redirected.end()});
    require_source_changed([&]{floating->server()->read_raw(raw);},"Changed file-set read");
    retained(*scalar,expected);
    std::cout<<"Byte-identical payload replacement and changed file declaration refuse currentness while the saved result stays exact\n";
}
void changed_series_source(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,const std::string& series){
    configure_reader_plugins(manifest.u8string());
    const auto expected=receipt(work);
    const auto project=Project::open(project_path(work).u8string());
    const auto doc=project->document(1);
    const auto server=doc->server();
    check_scalar(*server,fixtures);
    const auto companions=scalar_companions(work,true,series);
    check_source_set(*server,companions);
    const auto replacement=work/"replacement.part";
    fs::copy_file(companions.front(),replacement);
    check(MoveFileExW(replacement.c_str(),companions.front().c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,
          "Cannot replace an isolated series payload");
    RawRegionRequest raw;raw.region={0,0,0,5,3};raw.z=1;
    require_source_changed([&]{server->read_raw(raw);},"Series companion replacement");
    retained(*doc,expected);
    std::cout<<"A byte-identical series companion replacement invalidates the saved binding\n";
}
template<class Sample> void check_wide(const ImageServer& server,SampleType type,const fs::path& oracle) {
    const auto meta=server.raw_metadata();
    check(meta&&meta->sample_type==type&&meta->width==5&&meta->height==3&&meta->z_planes==2,
          "Wide scalar metadata changed during project use");
    const auto expected=bytes(oracle);
    std::vector<std::uint8_t> observed;
    RawRegionRequest request;request.region={0,0,0,5,3};
    for(request.z=0;request.z<2;++request.z) {
        const auto part=server.read_raw(request);
        const auto values=storage_bytes(std::get<std::vector<Sample>>(part.storage()));
        observed.insert(observed.end(),values.begin(),values.end());
        check(part.valid()==std::vector<std::uint8_t>(15,1),"Wide scalar validity changed");
    }
    check(observed==expected,"Wide scalar bit patterns changed during project use");
}
void wide_project(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,bool reopen) {
    configure_reader_plugins(manifest.u8string());
    auto project=reopen?Project::open(project_path(work).u8string()):std::make_shared<Project>();
    if(!reopen) {
        fs::create_directories(work);
        for(const auto* name:{"list-u64.nhdr","list-u64-01.part","list-u64-02.part",
                              "detached-gzip-f64-header.nhdr","detached-gzip-f64-header.raw.gz"})
            fs::copy_file(fixtures/name,work/name);
        project->add_image((work/"list-u64.nhdr").u8string(),provider,"Exact UInt64");
        project->add_image((work/"detached-gzip-f64-header.nhdr").u8string(),provider,"Exact Float64");
    }
    const auto integer=project->document(1)->server(),floating=project->document(2)->server();
    check_wide<std::uint64_t>(*integer,SampleType::UInt64,fixtures/"list-u64.raw");
    check_wide<double>(*floating,SampleType::Float64,fixtures/"detached-gzip-f64.raw");
    check_source_set(*integer,{work/"list-u64-01.part",work/"list-u64-02.part"});
    check_source_files(*floating,work/"detached-gzip-f64-header.raw.gz");
    if(!reopen) {
        for(const auto id:{1u,2u}) {
            const auto path=project->document(id)->server()->source_locator();
            project->apply_source_review(project->review_source_verification(id,*fingerprint_file(path)));
        }
        project->save_new(project_path(work).u8string());
    }
    std::cout<<"UInt64 and Float64 SDK project samples are bit-exact\n";
}
void time_project(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,bool reopen) {
    configure_reader_plugins(manifest.u8string());
    auto project=reopen?Project::open(project_path(work).u8string()):std::make_shared<Project>();
    if(!reopen) {
        fs::create_directories(work);
        for(const auto* name:{"v4-list.nhdr","frame0.gz","frame1.gz","frame2.gz"})
            fs::copy_file(fixtures/name,work/name);
        project->add_image((work/"v4-list.nhdr").u8string(),provider,"Calibrated time series");
    }
    const auto image=project->document(1)->server();
    const auto metadata=image->raw_metadata();
    check(metadata && metadata->dimension_order=="XYZT" && metadata->z_planes==2 && metadata->timepoints==3 &&
          metadata->pixel_width_um==0.5 && metadata->pixel_height_um==1.25 &&
          metadata->z_spacing_um==3.0 && metadata->time_interval_seconds==0.25,"Saved NRRD axes or calibration changed");
    RawRegionRequest request;
    request.region={0,0,0,2,2};
    std::vector<std::uint8_t> observed;
    for(request.t=0;request.t<3;++request.t)for(request.z=0;request.z<2;++request.z) {
        const auto raw=image->read_raw(request);
        const auto part=storage_bytes(std::get<std::vector<std::uint16_t>>(raw.storage()));
        observed.insert(observed.end(),part.begin(),part.end());
        check(raw.valid()==std::vector<std::uint8_t>(4,1),"Saved NRRD time-plane validity changed");
    }
    check(observed==bytes(fixtures/"v4-list.raw"),"Saved NRRD time-plane raw bits changed");
    check_source_set(*image,{work/"frame0.gz",work/"frame1.gz",work/"frame2.gz"});
    if(!reopen)project->save_new(project_path(work).u8string());
    std::cout<<"NRRD SDK project preserved temporal calibration, all Z/T samples and detached identities\n";
}
}
int run(int argc,char** argv){try{
    check(argc==5,"Usage: nrrd_project_consumer MODE MANIFEST FIXTURES_DIR WORK_DIR");const std::string mode=argv[1];const auto manifest=fs::absolute(fs::u8path(argv[2])),fixtures=fs::absolute(fs::u8path(argv[3])),work=fs::absolute(fs::u8path(argv[4]));
    if(mode=="create"||mode=="detached-create")create(manifest,fixtures,work,mode=="detached-create");
    else if(mode=="reopen"||mode=="detached-reopen")reopen(manifest,fixtures,work,mode=="detached-reopen");
    else if(mode=="list-create"||mode=="numbered-create")create(manifest,fixtures,work,true,mode=="list-create"?"list":"numbered");
    else if(mode=="list-reopen"||mode=="numbered-reopen")reopen(manifest,fixtures,work,true,mode=="list-reopen"?"list":"numbered");
    else if(mode=="missing")missing(work);
    else if(mode=="changed-implementation")changed_implementation(manifest,fixtures,work);
    else if(mode=="changed-source")changed_source(manifest,fixtures,work);
    else if(mode=="changed-detached-source")changed_detached_source(manifest,fixtures,work);
    else if(mode=="changed-list-source"||mode=="changed-numbered-source")changed_series_source(manifest,fixtures,work,mode=="changed-list-source"?"list":"numbered");
    else if(mode=="changed-options")changed_options(manifest,fixtures,work);
    else if(mode=="wide-create"||mode=="wide-reopen")wide_project(manifest,fixtures,work,mode=="wide-reopen");
    else if(mode=="time-create"||mode=="time-reopen")time_project(manifest,fixtures,work,mode=="time-reopen");
    else throw std::invalid_argument("Unknown fresh-process mode");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
