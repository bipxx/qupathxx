// Fresh-process installed-SDK checks for the independently built ICS module.
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
constexpr const char* provider="org.qupathxx.ics";
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read authored source/oracle: "+path.u8string());return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::vector<std::uint8_t>& value){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(value.data()),static_cast<std::streamsize>(value.size()));check(bool(out),"Cannot write isolated test input");}
template<class F>void refuses(F&& call,const std::string& label){bool refused=false;try{call();}catch(const std::exception& e){refused=!std::string(e.what()).empty();}check(refused,label+" was not refused with a diagnostic");}
struct Receipt {std::uint64_t id=0,samples=0;std::string digest;ColorVector means{};};
fs::path project_path(const fs::path& work){return work/"ics-project.qpxproj";}
void store_receipt(const fs::path& work,const AnalysisRecord& record){std::ofstream out(work/"record.txt");out<<record.id<<' '<<record.result.samples<<' '<<record.result.input_sha256;for(auto value:record.result.means)out<<' '<<std::setprecision(17)<<value;out<<'\n';check(bool(out),"Cannot write retained receipt");}
Receipt receipt(const fs::path& work){Receipt r;std::ifstream in(work/"record.txt");in>>r.id>>r.samples>>r.digest>>r.means[0]>>r.means[1]>>r.means[2];check(bool(in)&&r.id&&r.samples==15&&r.digest.size()==64,"Invalid retained receipt");return r;}
void retained(const Document& doc,const Receipt& expected){check(doc.analysis_record_count()==1,"Retained record count changed");const auto value=doc.analysis_record(expected.id);check(value.id==expected.id&&value.result.samples==expected.samples&&value.result.means==expected.means&&value.result.input_sha256==expected.digest,"Exact stored result identity/values changed");}
template<class T>std::vector<std::uint8_t> storage_bytes(const std::vector<T>& data){std::vector<std::uint8_t> result(data.size()*sizeof(T));if(!result.empty())std::memcpy(result.data(),data.data(),result.size());return result;}
void check_channels(const ImageServer& server,const fs::path& fixtures,const std::string& display="channels-main"){
    check(server.read_region({0,0,0,5,3}).bytes()==bytes(fixtures/(display+".rgba")),"Host ICS declared Z0/window display differs");
    const auto metadata=server.raw_metadata();check(metadata&&metadata->width==5&&metadata->height==3&&metadata->channels==3&&metadata->z_planes==2&&metadata->timepoints==1&&metadata->sample_type==SampleType::UInt16&&metadata->channel_names==std::vector<std::string>{"C0","C1","C2"}&&metadata->dimension_order=="CXYZ"&&metadata->pixel_width_um==.25&&metadata->pixel_height_um==.5&&metadata->z_spacing_um==1.5&&!metadata->time_interval_seconds,"Host scalar volume metadata/calibration differs");
    const auto expected=bytes(fixtures/"channels-main.raw");check(expected.size()==180,"Invalid authored UInt16 volume oracle");for(unsigned z=0;z<2;++z){RawRegionRequest request;request.region={0,0,0,5,3};request.channels=3;request.z=z;const auto raw=server.read_raw(request);check(raw.sample_type()==SampleType::UInt16&&storage_bytes(std::get<std::vector<std::uint16_t>>(raw.storage()))==std::vector<std::uint8_t>(expected.begin()+z*90,expected.begin()+(z+1)*90)&&raw.valid()==std::vector<std::uint8_t>(15,1),"Host UInt16 XYZ plane bits/validity differ");}
    const auto observed=server.reader_source();auto companion=fs::u8path(server.source_locator());companion.replace_extension(".ids");check(observed&&observed->scope=="compound_files"&&observed->primary&&observed->companions.size()==1&&observed->companions[0].path==companion.u8string()&&observed->companions[0].bytes==180,"Host did not capture the exact derived IDS companion");
    check(!server.raw_view()&&!server.tiff_view()&&!server.tiff_options()&&!server.ome_zarr_options(),"ICS invented TIFF/Zarr recipe/view access");
}
void check_float(const ImageServer& server,const fs::path& fixtures){
    const auto metadata=server.raw_metadata();check(metadata&&metadata->width==5&&metadata->height==3&&metadata->channels==2&&metadata->z_planes==2&&metadata->sample_type==SampleType::Float32&&metadata->channel_names==std::vector<std::string>{"C0","C1"}&&metadata->dimension_order=="CXYZ"&&!metadata->pixel_width_um&&!metadata->pixel_height_um&&!metadata->z_spacing_um&&!metadata->time_interval_seconds,"Host Float32 volume metadata/unknown calibration differs");
    const auto expected=bytes(fixtures/"float-inline.raw");check(expected.size()==240,"Invalid authored Float32 volume oracle");auto raw_planes=[&]{for(unsigned z=0;z<2;++z){RawRegionRequest request;request.region={0,0,0,5,3};request.channels=2;request.z=z;const auto raw=server.read_raw(request);check(raw.sample_type()==SampleType::Float32&&storage_bytes(std::get<std::vector<float>>(raw.storage()))==std::vector<std::uint8_t>(expected.begin()+z*120,expected.begin()+(z+1)*120)&&raw.valid()==std::vector<std::uint8_t>(15,1),"Host Float32 IEEE bits/Z/validity differ (NaN/Inf/signed zero are not numeric comparisons)");}};
    raw_planes();check(server.read_region({0,0,0,5,3}).bytes()==bytes(fixtures/"float-inline.rgba"),"Host Float32 display/nonfinite transparency differs");raw_planes();const auto observed=server.reader_source();check(observed&&observed->primary&&observed->companions.empty(),"Inline ICS2 acquired an undeclared companion");
}
void create(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    check(!fs::exists(project_path(work)),"Create check needs a fresh work directory");fs::create_directories(work);
    fs::copy_file(fixtures/"channels-main.ics",work/"source-scalar.ics");fs::copy_file(fixtures/"channels-main.ids",work/"source-scalar.ids");fs::copy_file(fixtures/"float-inline.ics",work/"source-float.ics");
    configure_reader_plugins(manifest.u8string());check(reader_catalog_json().find(provider)!=std::string::npos,"Installed catalog omitted configured ICS provider");
    Project project;const auto first=project.add_image((work/"source-scalar.ics").u8string(),provider,"UInt16 XYZ scalar volume");const auto second=project.add_image((work/"source-float.ics").u8string(),provider,"Float32 XYZ bit-exact volume");
    check(first==1&&second==2&&project.images().size()==2&&!project.images()[0].embedded&&project.images()[0].reader==provider,"Native plugin project attach lost linked identity");const auto doc=project.document(first);check_channels(*doc->server(),fixtures);check_float(*project.document(second)->server(),fixtures);
    const auto object=doc->add_rectangle({0,0,5,3});const StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    // Independently window-converted Z0 display bytes feed the same native scientific authority.
    // This tests the new reader without replacing or requalifying the kernel.
    Document reference(Image(5,3,bytes(fixtures/"channels-main.rgb")));const auto ref_object=reference.add_rectangle({0,0,5,3});const auto expected=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==15&&result.means==expected.means&&result.input_sha256==input_sha256(request),"ICS RegionAnalysis differs from authored-byte native reference");const auto kept=doc->keep_analysis(request,result);check(kept&&doc->analysis_record_count()==1,"Keep did not retain one ICS result");store_receipt(work,doc->analysis_record(kept));
    for(const auto id:{first,second}){const auto input=project.document(id)->server()->source_locator();project.apply_source_review(project.review_source_verification(id,*fingerprint_file(input)));}
    project.save_new(project_path(work).u8string());check(!project.dirty(),"Save did not reconcile project state");std::cout<<"Native attach/exact HWC+Z UInt16/Float32 bits/file sets/window/calibration/RegionAnalysis Keep/Save passed\n";
}
void reopen(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);check(doc->server()->reader_source()&&doc->server()->reader_source()->scope=="unopened","Project parsing eagerly accessed ICS provider");retained(*doc,expected);
    configure_reader_plugins(manifest.u8string());check_channels(*doc->server(),fixtures);check_float(*project->document(2)->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,request,analyze(request)),"Exact ICS saved-result replay refused");retained(*doc,expected);project->save_new((work/"exact-resaved.qpxproj").u8string());const auto again=Project::open((work/"exact-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Exact saved source/raw/record identity, replay and resave passed\n";
}
void missing(const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);retained(*doc,expected);const auto metadata=doc->server()->raw_metadata();check(metadata&&doc->server()->reader_source()->scope=="unopened","Offline cached raw metadata lost");
    const RegionRequest display_request{2,1,0,1,1};validate_region(doc->server()->metadata(),display_request);
    RawRegionRequest raw_request;raw_request.region=display_request;raw_request.first_channel=1;raw_request.channels=2;raw_request.z=1;raw_request.validate(*metadata);
    const std::string diagnostic=std::string("Reader plugin provider unavailable: ")+provider;
    auto unavailable=[&](auto&& read,const char* label){bool seen=false;try{read();}catch(const std::exception& e){check(std::string(e.what()).find(diagnostic)==0,std::string(label)+" failed before provider lookup or with an unrelated diagnostic: "+e.what());seen=true;}check(seen,std::string(label)+" did not refuse its unavailable provider");};
    unavailable([&]{doc->server()->read_region(display_request);},"Missing plugin display read");unavailable([&]{doc->server()->read_raw(raw_request);},"Missing plugin raw read");
    retained(*doc,expected);project->save_new((work/"missing-resaved.qpxproj").u8string());const auto again=Project::open((work/"missing-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Missing provider preserves offline metadata/exact historical record and resave; valid display/raw requests reach provider-unavailable refusal\n";
}
void changed_implementation(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);refuses([&]{doc->server()->read_region({0,0,0,1,1});},"Changed implementation read");retained(*doc,expected);
    const auto review=project->review_source_verification(1,*fingerprint_file((work/"source-scalar.ics").u8string()));check(review->comparison()=="changed_implementation"&&review->requires_acceptance(),"Same-version module-byte change escaped implementation review");refuses([&]{project->apply_source_review(review,false);},"Unaccepted implementation change");project->apply_source_review(review,true);check(review->consumed(),"Accepted native implementation review was not consumed");check_channels(*doc->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);const auto value=analyze(request);check(value.means==expected.means&&value.input_sha256!=expected.digest&&!doc->verify_analysis(expected.id,request,value),"Same pixels authorized old implementation identity");retained(*doc,expected);project->save_new((work/"changed-implementation-adopted.qpxproj").u8string());std::cout<<"Changed module requires review; equal samples do not authorize old retained identity\n";
}
void changed_options(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);
    check_channels(*doc->server(),fixtures);const auto replay=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,replay,analyze(replay)),"Changed manifest default altered the saved explicit channel0/window0..255 binding");retained(*doc,expected);
    Project fresh;const auto image=fresh.add_image((work/"source-scalar.ics").u8string(),provider,"Fresh channel1/window0..65535");const auto fresh_doc=fresh.document(image);check_channels(*fresh_doc->server(),fixtures,"channels-main-view");check(bytes(fixtures/"channels-main.rgba")!=bytes(fixtures/"channels-main-view.rgba"),"Window fixture does not distinguish defaults");
    const StainTransform transform({{1,0,0},{0,1,0}});const auto object=fresh_doc->add_rectangle({0,0,5,3});AnalysisRequest request(*fresh_doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    Document reference(Image(5,3,bytes(fixtures/"channels-main-view.rgb")));const auto ref_object=reference.add_rectangle({0,0,5,3});const auto control=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==15&&result.means==control.means&&result.means!=expected.means&&result.input_sha256!=expected.digest,"Fresh changed window did not create its own exact display/input identity");fresh.save_new((work/"changed-options-fresh.qpxproj").u8string());retained(*doc,expected);
    std::cout<<"Changed provider default preserves saved explicit channel0/window0..255/replay; fresh channel1/window0..65535 attachment preserves raw codes and gets distinct display/result identity\n";
}
void changed_source(const fs::path& manifest,const fs::path& fixtures,const fs::path& work,const std::string& mode){
    const auto isolated=work/(mode=="changed-source"?"mut-primary":mode=="changed-companion"?"mut-ids":"mut-set");create(manifest,fixtures,isolated);
    // This fresh process configured once while seeding its isolated baseline.
    // Restoring bytes after a mutation does not restore saved FileVersion stamps.
    const auto expected=receipt(isolated);const auto project=Project::open(project_path(isolated).u8string());const auto doc=project->document(1);check_channels(*doc->server(),fixtures);
    const RegionRequest display_request{2,1,0,1,1};RawRegionRequest raw_request;raw_request.region=display_request;raw_request.first_channel=1;raw_request.channels=2;raw_request.z=1;raw_request.validate(*doc->server()->raw_metadata());
    const auto authored_rgba=bytes(fixtures/"channels-main.rgba"),authored_raw=bytes(fixtures/"channels-main.raw");const auto pixel=static_cast<std::size_t>(1*5+2),at=((15+pixel)*3+1)*2;
    check(doc->server()->read_region(display_request).bytes()==std::vector<std::uint8_t>(authored_rgba.begin()+pixel*4,authored_rgba.begin()+(pixel+1)*4),"Mutation display control failed before source change");
    const auto raw_before=doc->server()->read_raw(raw_request);check(raw_before.sample_type()==SampleType::UInt16&&raw_before.channels()==2&&raw_before.valid()==std::vector<std::uint8_t>{1}&&storage_bytes(std::get<std::vector<std::uint16_t>>(raw_before.storage()))==std::vector<std::uint8_t>(authored_raw.begin()+at,authored_raw.begin()+at+4),"Mutation Z/channel raw control failed before source change");
    const auto target=isolated/(mode=="changed-companion"?"source-scalar.ids":"source-scalar.ics");const auto original=bytes(target);const auto replacement=bytes(fixtures/(mode=="changed-source"?"primary-changed.ics":mode=="changed-companion"?"companion-changed.ids":"file-set-changed.ics"));check(original!=replacement,"Source mutation bytes are unchanged");if(mode=="changed-companion")check(original.size()==replacement.size(),"Companion mutation did not preserve its exact byte length");
    auto refuses_currentness=[&](auto&& read,const char* label){bool seen=false;try{read();}catch(const WorkflowInputChanged& e){const std::string message=e.what();check(message.find("Reader plugin source")!=std::string::npos&&message.find("changed")!=std::string::npos,std::string(label)+" raised an unrelated currentness diagnostic: "+message);seen=true;}catch(const std::exception& e){throw std::runtime_error(std::string(label)+" failed before currentness with an unrelated exception: "+e.what());}check(seen,std::string(label)+" accepted changed source/file-set identity");};
    write(target,replacement);try{refuses_currentness([&]{doc->server()->read_region(display_request);},"Changed source display read");refuses_currentness([&]{doc->server()->read_raw(raw_request);},"Changed source raw read");retained(*doc,expected);}catch(...){write(target,original);throw;}write(target,original);
    std::cout<<mode<<": same valid display and Z1/C1-C2 requests pass before mutation and refuse currentness afterwards; exact historical record unchanged\n";
}

}
int run(int argc,char** argv){try{
    check(argc==5,"Usage: ics_project_consumer MODE MANIFEST FIXTURES_DIR WORK_DIR");const std::string mode=argv[1];const auto manifest=fs::absolute(fs::u8path(argv[2])),fixtures=fs::absolute(fs::u8path(argv[3])),work=fs::absolute(fs::u8path(argv[4]));
    if(mode=="create")create(manifest,fixtures,work);else if(mode=="reopen")reopen(manifest,fixtures,work);else if(mode=="missing")missing(work);else if(mode=="changed-implementation")changed_implementation(manifest,fixtures,work);else if(mode=="changed-source"||mode=="changed-companion"||mode=="changed-file-set")changed_source(manifest,fixtures,work,mode);else if(mode=="changed-options")changed_options(manifest,fixtures,work);else throw std::invalid_argument("Unknown fresh-process mode");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
