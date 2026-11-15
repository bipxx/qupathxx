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
void create(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    check(!fs::exists(project_path(work)),"Create check needs a fresh work directory");fs::create_directories(work);
    fs::copy_file(fixtures/"u16-main.nrrd",work/"source-scalar.nrrd");fs::copy_file(fixtures/"float-volume.nrrd",work/"source-float.nrrd");
    configure_reader_plugins(manifest.u8string());check(reader_catalog_json().find(provider)!=std::string::npos,"Installed catalog omitted configured NRRD provider");
    Project project;const auto first=project.add_image((work/"source-scalar.nrrd").u8string(),provider,"UInt16 XYZ scalar volume");const auto second=project.add_image((work/"source-float.nrrd").u8string(),provider,"Float32 XYZ bit-exact volume");
    check(first==1&&second==2&&project.images().size()==2&&!project.images()[0].embedded&&project.images()[0].reader==provider,"Native plugin project attach lost linked identity");const auto doc=project.document(first);check_scalar(*doc->server(),fixtures);check_float(*project.document(second)->server(),fixtures);
    const auto object=doc->add_rectangle({0,0,5,3});const StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    // Independently window-converted Z0 display bytes feed the same native scientific authority.
    // This tests the new reader without replacing or requalifying the kernel.
    Document reference(Image(5,3,bytes(fixtures/"u16-main.rgb")));const auto ref_object=reference.add_rectangle({0,0,5,3});const auto expected=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==15&&result.means==expected.means&&result.input_sha256==input_sha256(request),"NRRD RegionAnalysis differs from authored-byte native reference");const auto kept=doc->keep_analysis(request,result);check(kept&&doc->analysis_record_count()==1,"Keep did not retain one NRRD result");store_receipt(work,doc->analysis_record(kept));
    for(const auto id:{first,second}){const auto input=project.document(id)->server()->source_locator();project.apply_source_review(project.review_source_verification(id,*fingerprint_file(input)));}
    project.save_new(project_path(work).u8string());check(!project.dirty(),"Save did not reconcile project state");std::cout<<"Native attach/exact UInt16+Float32 XYZ bits/window/calibration/RegionAnalysis Keep/Save passed\n";
}
void reopen(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);check(doc->server()->reader_source()&&doc->server()->reader_source()->scope=="unopened","Project parsing eagerly accessed NRRD provider");retained(*doc,expected);
    configure_reader_plugins(manifest.u8string());check_scalar(*doc->server(),fixtures);check_float(*project->document(2)->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,request,analyze(request)),"Exact NRRD saved-result replay refused");retained(*doc,expected);project->save_new((work/"exact-resaved.qpxproj").u8string());const auto again=Project::open((work/"exact-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Exact saved source/raw/record identity, replay and resave passed\n";
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
}
int run(int argc,char** argv){try{
    check(argc==5,"Usage: nrrd_project_consumer MODE MANIFEST FIXTURES_DIR WORK_DIR");const std::string mode=argv[1];const auto manifest=fs::absolute(fs::u8path(argv[2])),fixtures=fs::absolute(fs::u8path(argv[3])),work=fs::absolute(fs::u8path(argv[4]));
    if(mode=="create")create(manifest,fixtures,work);else if(mode=="reopen")reopen(manifest,fixtures,work);else if(mode=="missing")missing(work);else if(mode=="changed-implementation")changed_implementation(manifest,fixtures,work);else if(mode=="changed-source")changed_source(manifest,fixtures,work);else if(mode=="changed-options")changed_options(manifest,fixtures,work);else throw std::invalid_argument("Unknown fresh-process mode");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
