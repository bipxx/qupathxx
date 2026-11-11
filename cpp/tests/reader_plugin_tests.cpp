#include "../reader_plugin.hpp"
#include "../reader_registry.hpp"
#include "../json_string.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/workflow.hpp"
#ifdef QUPATHXX_HAS_PROJECT
#include "qupathxx/project.hpp"
#endif
#include "reader_plugin_ppm_oracle.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs=std::filesystem;
namespace rd=qpx::detail;
using namespace qpx;
namespace {
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>std::string rejects(F action){try{action();}catch(const E& error){return error.what();}throw std::runtime_error("Missing plugin boundary refusal");}
void write(const fs::path& path,const std::string& value){std::ofstream file(path,std::ios::binary);file.write(value.data(),static_cast<std::streamsize>(value.size()));file.close();check(bool(file),"Write owned plugin fixture");}
std::string read(const fs::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
std::size_t occurrences(const std::string& value,const std::string& token){std::size_t count=0,at=0;while((at=value.find(token,at))!=std::string::npos){++count;at+=token.size();}return count;}
void wait_event(const fs::path& path,const char* marker){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(std::chrono::steady_clock::now()<end){if(read(path).find(marker)!=std::string::npos)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("Plugin did not reach the observed call boundary");}
std::string hash(const fs::path& path){return fingerprint_file(fs::absolute(path).u8string())->sha256;}
fs::path copy_module(const fs::path& original,const fs::path& root){const auto target=root/original.filename();fs::copy_file(original,target);return target;}
std::string plugin_descriptor(const fs::path& module,const std::string& digest,const std::string& id="org.qupathxx.test",const std::string& display="Reader boundary test",const std::string& profile=rd::reader_plugin_profile(),unsigned abi=1,const std::string& dependencies="[]"){
    return "{\"provider_id\":"+rd::json_string(id)+",\"display_name\":"+rd::json_string(display)+",\"version\":\"1.0.0\",\"contract\":\"test-raw/1\",\"abi\":"+std::to_string(abi)+",\"profile\":"+rd::json_string(profile)+",\"capabilities\":{\"display_rgba\":true,\"raw_samples\":true},\"module\":{\"path\":"+rd::json_string(module.filename().u8string())+",\"sha256\":"+rd::json_string(digest)+"},\"dependencies\":"+dependencies+"}";
}
fs::path manifest(const fs::path& root,const std::string& descriptor){const auto path=root/"readers.json";write(path,"{\"schema\":\"qpx.reader-plugins/1\",\"plugins\":["+descriptor+"]}");return path;}
std::shared_ptr<const rd::AdmittedReaderPlugin> admit(const fs::path& root){const auto module=copy_module(fs::u8path(QPX_READER_TEST_MODULE),root);rd::configure_reader_plugins(manifest(root,plugin_descriptor(module,hash(module))).u8string());const auto& catalog=rd::reader_plugin_catalog();check(catalog.readers.size()==1 && catalog.diagnostics.empty(),"Admitted independent raw test module did not load");return catalog.readers.front();}
fs::path fixture(const fs::path& root,const std::string& mode){const auto path=root/(mode+".control");write(path,mode+"\n");return path;}
std::shared_ptr<ImageServer> open_case(const fs::path& root,const std::string& mode,const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin){return rd::open_plugin_image_server(fixture(root,mode).u8string(),plugin);}
void lifecycle_events(const fs::path& root,const std::string& mode,std::size_t destroys=1){const auto log=read(root/(mode+".control.events"));check(occurrences(log,"destroy\n")==destroys && log.find("overlap")==std::string::npos && log.find("destroy-during-read")==std::string::npos,"Plugin calls overlapped or opaque handle was not destroyed exactly once: "+mode);}

void transfer(const fs::path& root,const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin){
    auto server=open_case(root,"uint64",plugin);const auto source=server->reader_source();const auto binding=rd::capture_native_reader_plugin_link(*server);check(rd::is_reader_plugin_server(*server) && server->reader_id()=="org.qupathxx.test" && source && source->scope=="single_file" && source->primary && binding && source->reader==rd::reader_plugin_binding(binding->implementation),"Plugin lost host-owned file/implementation identity");
    const auto& display=server->metadata();const auto metadata=server->raw_metadata();check(display.width==2 && display.height==2 && display.vendor=="independent native test" && display.pixel_width_um==.25 && display.pixel_height_um==.5 && display.levels.size()==1 && display.levels[0].width==2 && display.levels[0].height==2 && display.levels[0].downsample==1 && metadata && metadata->sample_type==SampleType::UInt64 && metadata->channels==2 && metadata->channel_names==std::vector<std::string>{"first","second"} && metadata->z_planes==2 && metadata->timepoints==2 && metadata->dimension_order=="XYCZT" && metadata->pixel_width_um==.25 && metadata->pixel_height_um==.5 && metadata->z_spacing_um==2 && metadata->time_interval_seconds==.125,"Plugin metadata/text/axes were not copied exactly");
    check(!server->raw_view() && !server->tiff_view() && !server->tiff_options() && !server->ome_zarr_options(),"Plugin acquired legacy TIFF/Zarr view objects");
    rejects([&]{server->with_raw_view({});});
    RawRegionRequest request;request.region={0,0,0,2,2};request.channels=2;const auto raw=server->read_raw(request);
    check(raw.valid()==std::vector<std::uint8_t>{1,1,1,1} && std::get<std::vector<std::uint64_t>>(raw.storage())==std::vector<std::uint64_t>{UINT64_C(9007199254740995),UINT64_MAX,UINT64_C(9007199254740996),UINT64_MAX-1,UINT64_C(9007199254740997),UINT64_MAX-2,UINT64_C(9007199254740998),UINT64_MAX-3},"Raw UInt64 HWC values lost bits/precision");
    request.channels=1;request.first_channel=0;request.z=1;request.t=1;check(std::get<std::vector<std::uint64_t>>(server->read_raw(request).storage())==std::vector<std::uint64_t>{UINT64_C(9007199254741105),UINT64_C(9007199254741106),UINT64_C(9007199254741107),UINT64_C(9007199254741108)},"Raw channel/Z/T selection changed");
    request.z=request.t=0;request.first_channel=1;request.region={-1,-1,0,3,3};const auto padded=server->read_raw(request);check(padded.valid()==std::vector<std::uint8_t>{0,0,0,0,1,1,0,1,1} && std::get<std::vector<std::uint64_t>>(padded.storage())==std::vector<std::uint64_t>{0,0,0,0,UINT64_MAX,UINT64_MAX-1,0,UINT64_MAX-2,UINT64_MAX-3},"Raw OOB zero padding or validity changed");
    check(server->read_region({0,0,0,2,2}).bytes()==std::vector<std::uint8_t>{10,20,30,255,11,20,30,255,12,20,30,255,13,20,30,255},"Independent module RGBA transfer changed");
    const auto retained=raw;server->close();server->close();check(server->closed() && std::get<std::vector<std::uint64_t>>(retained.storage())[0]==UINT64_C(9007199254740995),"Closing module invalidated owned raw output");rejects([&]{server->read_region({0,0,0,1,1});});server.reset();lifecycle_events(root,"uint64");
    auto floating=open_case(root,"float64",plugin);request.region={0,0,0,2,2};request.first_channel=0;const auto values=std::get<std::vector<double>>(floating->read_raw(request).storage());const std::uint64_t expected[]={UINT64_C(0x8000000000000000),UINT64_C(0x7ff8000000000042),UINT64_C(0x7ff0000000000000),UINT64_C(0x3ff0000000000000)};for(std::size_t i=0;i<4;++i){std::uint64_t bits;std::memcpy(&bits,&values[i],8);check(bits==expected[i],"Float64 signed zero/NaN/Inf bits changed");}floating.reset();lifecycle_events(root,"float64");
}
void malformed(const fs::path& root,const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin){
    for(const auto* mode:{"bad-width","bad-level-count","bad-type","bad-calibration","bad-text","bad-level"}){rejects([&]{open_case(root,mode,plugin);});lifecycle_events(root,mode);}
    for(const auto* mode:{"short-display","mutate-display","bad-oob-display"}){auto server=open_case(root,mode,plugin);rejects([&]{server->read_region({-1,-1,0,3,3});});server.reset();lifecycle_events(root,mode);}
    for(const auto* mode:{"short-raw","short-validity","bad-validity","bad-oob-raw","mutate-raw"}){auto server=open_case(root,mode,plugin);RawRegionRequest request;request.region={-1,-1,0,3,3};rejects([&]{server->read_raw(request);});server.reset();lifecycle_events(root,mode);}
    for(const auto* mode:{"unsupported","io-error","internal-error"}){auto server=open_case(root,mode,plugin);check(rejects<std::runtime_error>([&]{server->read_region({0,0,0,1,1});}).find("test ")!=std::string::npos,"Module error text was lost");server.reset();lifecycle_events(root,mode);}
    {auto server=open_case(root,"unknown-status",plugin);check(rejects<std::runtime_error>([&]{server->read_region({0,0,0,1,1});}).find("unknown status")!=std::string::npos,"Unknown module status was accepted");}
    {auto server=open_case(root,"buffer-status",plugin);rejects<std::length_error>([&]{server->read_region({0,0,0,1,1});});}
    {auto server=open_case(root,"error-short-buffer",plugin);RawRegionRequest request;rejects<std::length_error>([&]{server->read_raw(request);});server.reset();lifecycle_events(root,"error-short-buffer");}
    for(const auto* mode:{"success-oversize-error","changed-error-pointer","changed-error-capacity"}){auto server=open_case(root,mode,plugin);RawRegionRequest request;rejects<std::invalid_argument>([&]{server->read_raw(request);});server.reset();lifecycle_events(root,mode);}
    {auto server=open_case(root,"invalid-request",plugin);rejects<std::invalid_argument>([&]{server->read_region({0,0,0,1,1});});}
    {auto server=open_case(root,"cancelled-status",plugin);rejects<WorkflowCancelled>([&]{server->read_region({0,0,0,1,1});});}
}
void check_serialized_calls(const fs::path& root,const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin){
    auto server=open_case(root,"slow",plugin);std::promise<void> start;const auto go=start.get_future().share();auto first=std::async(std::launch::async,[&]{go.wait();return server->read_region({0,0,0,1,1}).bytes();});auto second=std::async(std::launch::async,[&]{go.wait();return server->read_region({1,1,0,1,1}).bytes();});start.set_value();check(first.get()==std::vector<std::uint8_t>{10,20,30,255} && second.get()==std::vector<std::uint8_t>{13,20,30,255},"Concurrent reads lost owned results");server.reset();lifecycle_events(root,"slow");
    const auto close_path=root/"close.control";write(close_path,"slow\n");auto close_server=rd::open_plugin_image_server(close_path.u8string(),plugin);auto reading=std::async(std::launch::async,[&]{return close_server->read_region({0,0,0,1,1}).bytes();});wait_event(fs::u8path(close_path.u8string()+".events"),"read-start");auto close=std::async(std::launch::async,[&]{close_server->close();});check(reading.get()==std::vector<std::uint8_t>{10,20,30,255},"Close interrupted an active display read");close.get();close_server->close();close_server.reset();const auto events=read(fs::u8path(close_path.u8string()+".events"));check(events.find("read-end\ndestroy\n")!=std::string::npos && occurrences(events,"destroy\n")==1 && events.find("destroy-during-read")==std::string::npos,"Close was not serialized after the active call");
    auto cancel_server=open_case(root,"wait-cancel",plugin);std::atomic<bool> cancel{false};RawRegionRequest request;auto raw=std::async(std::launch::async,[&]{return rejects<WorkflowCancelled>([&]{cancel_server->read_raw(request,&cancel);});});wait_event(root/"wait-cancel.control.events","read-start");cancel.store(true);check(raw.get().find("cancel")!=std::string::npos && read(root/"wait-cancel.control.events").find("cancel-observed")!=std::string::npos,"Native host cancellation callback was not observed by the module");cancel_server.reset();lifecycle_events(root,"wait-cancel");
    auto stale=open_case(root,"changed-source",plugin);write(root/"changed-source.control","changed bytes after open\n");rejects<WorkflowInputChanged>([&]{stale->read_region({0,0,0,1,1});});stale.reset();lifecycle_events(root,"changed-source");
    const auto changing_path=root/"changing.control";write(changing_path,"slow\n");auto changing=rd::open_plugin_image_server(changing_path.u8string(),plugin);
    auto changed_read=std::async(std::launch::async,[&]() -> std::pair<bool,std::vector<std::uint8_t>> {try{return {false,changing->read_region({0,0,0,1,1}).bytes()};}catch(const WorkflowInputChanged&){return {true,{}};}});
    wait_event(fs::u8path(changing_path.u8string()+".events"),"read-start");const std::string replacement="different bytes during module call\n";bool mutated=false;
#ifdef _WIN32
    const auto writer=CreateFileW(changing_path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(writer==INVALID_HANDLE_VALUE)check(GetLastError()==ERROR_SHARING_VIOLATION,"Concurrent source writer failed for an unrelated reason");
    else {DWORD written=0;const auto wrote=WriteFile(writer,replacement.data(),DWORD(replacement.size()),&written,nullptr);CloseHandle(writer);check(wrote && written==replacement.size(),"Concurrent source writer did not write its exact payload");mutated=true;}
#else
    write(changing_path,replacement);mutated=true;
#endif
    const auto outcome=changed_read.get();
    if(mutated)check(outcome.first,"Supported concurrent source mutation escaped currentness verification");
    else check(!outcome.first && outcome.second==std::vector<std::uint8_t>{10,20,30,255} && read(changing_path)=="slow\n","Refused concurrent writer changed the source or exact display result");
    write(changing_path,"changed after the protected read\n");rejects<WorkflowInputChanged>([&]{changing->read_region({0,0,0,1,1});});changing.reset();
}
void project_attachment(const fs::path& root,const std::shared_ptr<const rd::AdmittedReaderPlugin>& plugin){
#ifdef QUPATHXX_HAS_PROJECT
    const auto path=fixture(root,"project");auto server=rd::open_plugin_image_server(path.u8string(),plugin);auto doc=std::make_shared<Document>(server);Project project;
    Project inconsistent;rejects<std::invalid_argument>([&]{inconsistent.add_document(doc,"forced",path.u8string(),"ppm");});
    const auto image=project.add_document(doc);check(image==1 && project.images().front().reader=="org.qupathxx.test" && !project.images().front().embedded,"Native plugin document lost its captured link during attachment");
#else
    (void)root;(void)plugin;
#endif
}
void environment(const char* name,const std::string& value){
#ifdef _WIN32
    check(_putenv_s(name,value.c_str())==0,"Set own test-process entry mode");
#else
    check(setenv(name,value.c_str(),1)==0,"Set own test-process entry mode");
#endif
}
void loader_case(const std::string& which,const fs::path& root){
    check(fs::create_directories(root),"Create unique loader case directory");const bool dependent=which.find("dependency")!=std::string::npos;const auto source=fs::u8path(dependent?QPX_READER_TEST_DEPENDENT_MODULE:QPX_READER_TEST_MODULE);auto module=copy_module(source,root);auto digest=hash(module);std::string dependencies="[]";
    if(dependent){const auto original=fs::u8path(QPX_READER_TEST_HELPER);const auto helper=root/original.filename();const auto helper_hash=hash(original);if(which!="missing-dependency")fs::copy_file(original,helper);if(which!="unexpected-dependency")dependencies="[{\"path\":"+rd::json_string(helper.filename().u8string())+",\"sha256\":"+rd::json_string(which=="changed-dependency"?std::string(64,'0'):helper_hash)+"}]";}
    if(which=="preloaded-dependency"){
#ifdef _WIN32
        const auto foreign=root/"foreign";fs::create_directory(foreign);const auto helper=copy_module(fs::u8path(QPX_READER_TEST_HELPER),foreign);check(LoadLibraryExW(helper.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32)!=nullptr,"Preload deliberately foreign helper");
#else
        throw std::runtime_error("Preloaded dependency proof requires the admitted Windows profile");
#endif
    }
    if(which=="missing-module")fs::remove(module);
    if(which=="changed-module"){std::ofstream file(module,std::ios::binary|std::ios::app);file.put('!');}
    if(which=="bad-hash")digest=std::string(64,'0');
    if(which.rfind("entry-",0)==0)environment("QPX_READER_TEST_ENTRY_MODE",which.substr(6));
    if(which=="describe-throw")environment("QPX_READER_TEST_ENTRY_MODE","describe-throw");
    const auto descriptor=plugin_descriptor(module,digest,which=="collision"?"ppm":"org.qupathxx.test",which=="descriptor"?"Wrong module name":"Reader boundary test",which=="profile"?"wrong-profile":rd::reader_plugin_profile(),which=="abi"?99:1,dependencies);
    rd::configure_reader_plugins(manifest(root,descriptor).u8string());const auto& catalog=rd::reader_plugin_catalog();const bool valid=which=="valid" || which=="valid-dependency";check(catalog.readers.size()==(valid?1u:0u),"Loader admitted/refused the wrong module: "+which);check(catalog.diagnostics.empty()==valid,"Loader failure lost its owned diagnostic: "+which);
    if(!valid){std::string diagnostics;for(const auto& message:catalog.diagnostics)diagnostics+=message+'\n';std::string expected;
        if(which=="missing-module" || which=="missing-dependency")expected="canonical";
        else if(which=="bad-hash" || which=="changed-module" || which=="changed-dependency")expected="SHA-256 mismatch";
        else if(which=="descriptor")expected="descriptor/ABI provider disagreement";
        else if(which=="collision")expected="reserved or invalid provider ID";
        else if(which=="profile" || which=="abi")expected="ABI/compiler/CRT profile mismatch";
        else if(which=="entry-abi" || which=="entry-size")expected="returned ABI table mismatch";
        else if(which=="entry-null")expected="missing ABI callback";
        else if(which=="entry-throw")expected="entry threw across C ABI";
        else if(which=="describe-throw")expected="describe threw across C ABI";
        else if(which=="unexpected-dependency")expected="undeclared non-system dependency";
        else if(which=="preloaded-dependency")expected="unexpected already-loaded dependency";
        check(!expected.empty() && diagnostics.find(expected)!=std::string::npos,"Loader refused for an unrelated reason in "+which+": "+diagnostics);
        if(which=="missing-module")check(diagnostics.find(module.filename().u8string())!=std::string::npos,"Missing-module diagnostic identified another file");
        if(which=="missing-dependency")check(diagnostics.find(fs::u8path(QPX_READER_TEST_HELPER).filename().u8string())!=std::string::npos,"Missing-dependency diagnostic identified another file");
    }
    const auto builtins=rd::reader_catalog();check(std::count_if(builtins.begin(),builtins.end(),[](const auto& value){return value.provider_id=="ppm" && value.available;})==1,"Rejected module shadowed/disabled a built-in");const auto ppm=root/"builtin.ppm";write(ppm,"P6\n1 1\n255\nABC");check(rd::open_reader(ppm.u8string(),rd::select_reader(ppm.u8string(),"ppm"))->read_region({0,0,0,1,1}).bytes()==std::vector<std::uint8_t>{65,66,67,255},"Rejected module broke built-in decoding");
    if(valid){const auto input=fixture(root,"uint64");const auto server=rd::open_plugin_image_server(input.u8string(),catalog.readers.front());RawRegionRequest request;check(std::get<std::vector<std::uint64_t>>(server->read_raw(request).storage())[0]==UINT64_C(9007199254740995),"Admitted actual dependency did not supply the decoded sample");}
}
}
int main(int argc,char** argv){try{
    if(argc==4 && std::string(argv[1])=="--loader-case"){loader_case(argv[2],fs::absolute(fs::u8path(argv[3])));return 0;}
    check(argc==1,"Unexpected plugin test arguments");const auto root=fs::current_path()/("reader-plugin-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Create unique plugin boundary directory");qpx_test::reader_plugin_ppm_oracle(root);const auto plugin=admit(root);transfer(root,plugin);malformed(root,plugin);check_serialized_calls(root,plugin);project_attachment(root,plugin);std::cout<<"Native plugin typed buffers, metadata/status validation, cancellation and serialized exactly-once destruction passed: "<<root.u8string()<<'\n';
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
