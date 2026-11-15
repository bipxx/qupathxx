// Fresh-process installed-SDK checks for the independently built PNG module.
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
constexpr const char* provider="org.qupathxx.png";
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cannot read authored source/oracle: "+path.u8string());return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::vector<std::uint8_t>& value){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(value.data()),static_cast<std::streamsize>(value.size()));check(bool(out),"Cannot write isolated test input");}
template<class F>void refuses(F&& call,const std::string& label){bool refused=false;try{call();}catch(const std::exception& e){refused=!std::string(e.what()).empty();}check(refused,label+" was not refused with a diagnostic");}
struct Receipt {std::uint64_t id=0,samples=0;std::string digest;ColorVector means{};};
fs::path project_path(const fs::path& work){return work/"png-project.qpxproj";}
void store_receipt(const fs::path& work,const AnalysisRecord& record){std::ofstream out(work/"record.txt");out<<record.id<<' '<<record.result.samples<<' '<<record.result.input_sha256;for(auto value:record.result.means)out<<' '<<std::setprecision(17)<<value;out<<'\n';check(bool(out),"Cannot write retained receipt");}
Receipt receipt(const fs::path& work){Receipt r;std::ifstream in(work/"record.txt");in>>r.id>>r.samples>>r.digest>>r.means[0]>>r.means[1]>>r.means[2];check(bool(in)&&r.id&&r.samples==45&&r.digest.size()==64,"Invalid retained receipt");return r;}
void retained(const Document& doc,const Receipt& expected){check(doc.analysis_record_count()==1,"Retained record count changed");const auto value=doc.analysis_record(expected.id);check(value.id==expected.id&&value.result.samples==expected.samples&&value.result.means==expected.means&&value.result.input_sha256==expected.digest,"Exact stored result identity/values changed");}
void check_rgb(const ImageServer& server,const fs::path& fixtures){
    const auto image=server.read_region({0,0,0,9,5});check(image.bytes()==bytes(fixtures/"rgb8-opaque.rgba"),"Host PNG display differs from authored RGB8 pixels");
    const auto metadata=server.raw_metadata();check(metadata&&metadata->width==9&&metadata->height==5&&metadata->channels==3&&metadata->sample_type==SampleType::UInt8&&metadata->channel_names==std::vector<std::string>{"Red","Green","Blue"}&&metadata->pixel_width_um==.25&&metadata->pixel_height_um==.5,"Host raw metadata/calibration differs");
    RawRegionRequest request;request.region={0,0,0,9,5};request.channels=3;const auto raw=server.read_raw(request);check(std::get<std::vector<std::uint8_t>>(raw.storage())==bytes(fixtures/"rgb8-opaque.raw")&&raw.valid()==std::vector<std::uint8_t>(45,1),"Host PNG RGB samples/validity differ");
    check(!server.raw_view()&&!server.tiff_view()&&!server.tiff_options()&&!server.ome_zarr_options(),"PNG invented TIFF/Zarr recipe/view access");
}
void check_u16(const ImageServer& server,const fs::path& fixtures){
    const auto metadata=server.raw_metadata();check(metadata&&metadata->channels==4&&metadata->sample_type==SampleType::UInt16&&metadata->channel_names==std::vector<std::string>{"Red","Green","Blue","Alpha"},"Host UInt16 source metadata differs");
    RawRegionRequest request;request.region={0,0,0,9,5};request.channels=4;const auto raw=server.read_raw(request);const auto& values=std::get<std::vector<std::uint16_t>>(raw.storage());const auto expected=bytes(fixtures/"c6-d16.raw");check(expected.size()==values.size()*2,"UInt16 oracle shape differs");for(std::size_t i=0;i<values.size();++i)check(values[i]==std::uint16_t(expected[2*i]|(unsigned(expected[2*i+1])<<8)),"Host changed a 16-bit scalar sample");check(raw.valid()==std::vector<std::uint8_t>(45,1),"Transparent source samples became invalid");check(server.read_region({0,0,0,9,5}).bytes()==bytes(fixtures/"c6-d16.rgba"),"Host high-byte UInt16 display differs");
}
void create(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    check(!fs::exists(project_path(work)),"Create check needs a fresh work directory");fs::create_directories(work);
    fs::copy_file(fixtures/"rgb8-opaque.png",work/"source-rgb.png");fs::copy_file(fixtures/"c6-d16.png",work/"source-rgba16.png");
    configure_reader_plugins(manifest.u8string());check(reader_catalog_json().find(provider)!=std::string::npos,"Installed catalog omitted configured PNG provider");
    Project project;const auto first=project.add_image((work/"source-rgb.png").u8string(),provider,"Authored RGB8");const auto second=project.add_image((work/"source-rgba16.png").u8string(),provider,"Authored RGBA16");
    check(first==1&&second==2&&project.images().size()==2&&!project.images()[0].embedded&&project.images()[0].reader==provider,"Native plugin project attach lost linked identity");const auto doc=project.document(first);check_rgb(*doc->server(),fixtures);check_u16(*project.document(second)->server(),fixtures);
    const auto object=doc->add_rectangle({0,0,9,5});const StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest request(*doc,object,transform,AnalysisKind::OpticalDensityMeans);const auto result=analyze(request);
    // Independent authored bytes feed the same native scientific authority.
    // This tests the new reader without replacing or requalifying the kernel.
    Document reference(Image(9,5,bytes(fixtures/"rgb8-opaque.raw")));const auto ref_object=reference.add_rectangle({0,0,9,5});const auto expected=analyze(AnalysisRequest(reference,ref_object,transform,AnalysisKind::OpticalDensityMeans));
    check(result.samples==45&&result.means==expected.means&&result.input_sha256==input_sha256(request),"PNG RegionAnalysis differs from authored-byte native reference");const auto kept=doc->keep_analysis(request,result);check(kept&&doc->analysis_record_count()==1,"Keep did not retain one PNG result");store_receipt(work,doc->analysis_record(kept));
    for(const auto id:{first,second}){const auto input=project.document(id)->server()->source_locator();project.apply_source_review(project.review_source_verification(id,*fingerprint_file(input)));}
    project.save_new(project_path(work).u8string());check(!project.dirty(),"Save did not reconcile project state");std::cout<<"Native attach/display/exact UInt8+UInt16/calibration/RegionAnalysis Keep/Save passed\n";
}
void reopen(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);check(doc->server()->reader_source()&&doc->server()->reader_source()->scope=="unopened","Project parsing eagerly accessed PNG provider");retained(*doc,expected);
    configure_reader_plugins(manifest.u8string());check_rgb(*doc->server(),fixtures);check_u16(*project->document(2)->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);check(doc->verify_analysis(expected.id,request,analyze(request)),"Exact PNG saved-result replay refused");retained(*doc,expected);project->save_new((work/"exact-resaved.qpxproj").u8string());const auto again=Project::open((work/"exact-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Exact saved source/raw/record identity, replay and resave passed\n";
}
void missing(const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());const auto doc=project->document(1);retained(*doc,expected);check(doc->server()->raw_metadata()&&doc->server()->reader_source()->scope=="unopened","Offline cached raw metadata lost");refuses([&]{doc->server()->read_region({0,0,0,1,1});},"Missing plugin display read");refuses([&]{RawRegionRequest r;doc->server()->read_raw(r);},"Missing plugin raw read");retained(*doc,expected);project->save_new((work/"missing-resaved.qpxproj").u8string());const auto again=Project::open((work/"missing-resaved.qpxproj").u8string());retained(*again->document(1),expected);std::cout<<"Missing provider preserves offline metadata/exact historical record and resave; reads refuse\n";
}
void changed_implementation(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);refuses([&]{doc->server()->read_region({0,0,0,1,1});},"Changed implementation read");retained(*doc,expected);
    const auto review=project->review_source_verification(1,*fingerprint_file((work/"source-rgb.png").u8string()));check(review->comparison()=="changed_implementation"&&review->requires_acceptance(),"Same-version module-byte change escaped implementation review");refuses([&]{project->apply_source_review(review,false);},"Unaccepted implementation change");project->apply_source_review(review,true);check(review->consumed(),"Accepted native implementation review was not consumed");check_rgb(*doc->server(),fixtures);const auto request=AnalysisRequest::replay(*doc,expected.id);const auto value=analyze(request);check(value.means==expected.means&&value.input_sha256!=expected.digest&&!doc->verify_analysis(expected.id,request,value),"Same pixels authorized old implementation identity");retained(*doc,expected);project->save_new((work/"changed-implementation-adopted.qpxproj").u8string());std::cout<<"Changed module requires review; equal samples do not authorize old retained identity\n";
}
void changed_source(const fs::path& manifest,const fs::path& fixtures,const fs::path& work){
    const auto expected=receipt(work);const auto project=Project::open(project_path(work).u8string());configure_reader_plugins(manifest.u8string());const auto doc=project->document(1);check_rgb(*doc->server(),fixtures);
    const RegionRequest display_request{2,1,0,1,1};RawRegionRequest raw_request;raw_request.region=display_request;raw_request.first_channel=1;raw_request.channels=1;
    const auto authored_rgba=bytes(fixtures/"rgb8-opaque.rgba"),authored_raw=bytes(fixtures/"rgb8-opaque.raw");const auto pixel=static_cast<std::size_t>(1*9+2);
    const auto display_before=doc->server()->read_region(display_request);check(display_before.bytes()==std::vector<std::uint8_t>(authored_rgba.begin()+pixel*4,authored_rgba.begin()+(pixel+1)*4),"Source-change display control request failed before mutation");
    const auto raw_before=doc->server()->read_raw(raw_request);check(raw_before.width()==1&&raw_before.height()==1&&raw_before.channels()==1&&raw_before.sample_type()==SampleType::UInt8&&raw_before.valid()==std::vector<std::uint8_t>{1}&&std::get<std::vector<std::uint8_t>>(raw_before.storage())==std::vector<std::uint8_t>{authored_raw[pixel*3+1]},"Source-change raw control request failed before mutation");
    auto refuses_source_change=[&](auto&& read,const char* label){
        bool detected=false;try{read();}catch(const WorkflowInputChanged& e){const std::string message=e.what();check(message.find("Reader plugin source")!=std::string::npos&&message.find("changed")!=std::string::npos,std::string(label)+" reported an unrelated currentness error: "+message);detected=true;}catch(const std::exception& e){throw std::runtime_error(std::string(label)+" reported an unrelated exception instead of WorkflowInputChanged: "+e.what());}check(detected,std::string(label)+" did not detect changed source");
    };
    const auto source=work/"source-rgb.png";const auto original=bytes(source);const auto replacement=bytes(fixtures/"rgb8-unknown-units.png");check(original!=replacement,"Source mutation fixture is unchanged");write(source,replacement);
    try {refuses_source_change([&]{doc->server()->read_region(display_request);},"Changed source display read");refuses_source_change([&]{doc->server()->read_raw(raw_request);},"Changed source raw read");retained(*doc,expected);}catch(...){write(source,original);throw;}write(source,original);std::cout<<"The same valid display/raw requests pass before mutation and refuse source currentness afterwards; retained payload unchanged\n";
}
}
int run(int argc,char** argv){try{
    check(argc==5,"Usage: png_project_consumer MODE MANIFEST FIXTURES_DIR WORK_DIR");const std::string mode=argv[1];const auto manifest=fs::absolute(fs::u8path(argv[2])),fixtures=fs::absolute(fs::u8path(argv[3])),work=fs::absolute(fs::u8path(argv[4]));
    if(mode=="create")create(manifest,fixtures,work);else if(mode=="reopen")reopen(manifest,fixtures,work);else if(mode=="missing")missing(work);else if(mode=="changed-implementation")changed_implementation(manifest,fixtures,work);else if(mode=="changed-source")changed_source(manifest,fixtures,work);else throw std::invalid_argument("Unknown fresh-process mode");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

int wmain(int argc,wchar_t** wide_argv){try{
    std::vector<std::string> owned;owned.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i){
        const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,nullptr,0,nullptr,nullptr);check(count>0,"Cannot encode Windows argument as UTF8");
        std::string value(static_cast<std::size_t>(count),'\0');check(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide_argv[i],-1,value.data(),count,nullptr,nullptr)==count,"Windows argument UTF8 conversion changed length");value.pop_back();owned.push_back(std::move(value));
    }
    std::vector<char*> argv;argv.reserve(owned.size()+1);for(auto& value:owned)argv.push_back(value.data());argv.push_back(nullptr);return run(argc,argv.data());
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
