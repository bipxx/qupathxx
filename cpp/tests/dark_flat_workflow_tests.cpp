#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "../dark_flat_cli.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected dark/flat workflow refusal");}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void field(const fs::path& path,std::uint16_t a,std::uint16_t b){qpx_test::raw_pixel_tiff_fixture(path);std::fstream out(path,std::ios::binary|std::ios::in|std::ios::out);out.seekp(160);for(int i=0;i<48;++i)for(auto v:{a,b}){out.put(char(v&255));out.put(char(v>>8));}out.close();check(bool(out),"Write independent constant field fixture");}
struct Capture {std::ostringstream out,err;std::streambuf *old_out,*old_err;Capture():old_out(std::cout.rdbuf(out.rdbuf())),old_err(std::cerr.rdbuf(err.rdbuf())){}~Capture(){std::cout.rdbuf(old_out);std::cerr.rdbuf(old_err);if(!err.str().empty())std::cerr<<"Captured native diagnostic: "<<err.str();}};
void exercise(const fs::path& root){
    const auto image=root/"signal.tif",dark=root/"dark.tif",flat=root/"flat.tif",project_file=root/"input.qpx";
    qpx_test::raw_pixel_tiff_fixture(image);field(dark,10,20);field(flat,20,40);const auto signal_bytes=bytes(image),dark_bytes=bytes(dark),flat_bytes=bytes(flat);
    auto doc=std::make_shared<Document>(open_tiff(image.u8string()));auto d=open_tiff(dark.u8string()),f=open_tiff(flat.u8string());using V=ImageDataValue;
    doc->set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"user-note",V{std::string("preserve")}}}));
    DarkFlatOptions options{{100,200},-0.0};const DarkFlatRequest request(*doc,d,f,options);RawRegionRequest region{{0,0,0,8,6},0,2};
    auto result=std::async(std::launch::async,[&]{return read_dark_flat_block(request,region);}).get();const auto digest=result->sha256();const auto& values=std::get<std::vector<double>>(result->samples().storage());
    check(result->valid_pixels()==48 && result->finite_samples()==std::vector<std::uint64_t>{48,48} && result->nonfinite_samples()==std::vector<std::uint64_t>{0,0},"Native summary counts exact corrected values");
    for(int i=0;i<48;++i){check(std::abs(values[2*i]-(i-10)*10)<=1e-12 && std::abs(values[2*i+1]-(i+80)*10)<=1e-12,"Native TIFF correction matches independent analytical oracle");}
    rejects<WorkflowCaptureLimit>([&]{read_dark_flat_block(request,region,1);});
    std::async(std::launch::async,[&]{rejects<std::logic_error>([&]{request.matches(*doc);});}).get();
    const auto id=keep_dark_flat_recipe(*doc,request);check(dark_flat_recipe_ids(*doc)==std::vector<std::string>{id},"Exact recipe retained once");rejects<WorkflowOutputExists>([&]{keep_dark_flat_recipe(*doc,request);});
    doc->undo();check(dark_flat_recipe_ids(*doc).empty(),"Keep is one Undo command");doc->redo();check(load_dark_flat_recipe(*doc,id).sha256()==id && std::signbit(load_dark_flat_recipe(*doc,id).options.minimum_denominator),"Typed recipe and signed zero restore exactly");
    remove_dark_flat_recipe(*doc,id);check(dark_flat_recipe_ids(*doc).empty() && doc->image_data_properties()->values().count("user-note"),"Remove preserves unrelated properties");doc->undo();
    const auto properties=doc->image_data_properties();auto corrupt=properties->values();auto& all=std::get<V::Map>(corrupt.at("qupathxx.dark-flat-recipes/1").value);auto& record=std::get<V::Map>(all.at(id).value);record["derived_source_sha256"]=V{std::string(64,'a')};doc->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(corrupt)));rejects<std::invalid_argument>([&]{load_dark_flat_recipe(*doc,id);});doc->set_image_data_properties(properties);
    doc->set_calibration(1,1);rejects<WorkflowInputChanged>([&]{keep_dark_flat_recipe(*doc,request);});doc->undo();check(request.matches(*doc),"Calibration Undo restores capture authority");
    if(has_project()){
        Project project;project.add_document(doc);project.save_new(project_file.u8string());auto reopened=Project::open(project_file.u8string())->document(1);const auto replay=replay_dark_flat_recipe(*reopened,id);check(replay.recipe().sha256()==id && read_dark_flat_block(replay,region)->sha256()==digest && std::signbit(replay.recipe().options.minimum_denominator),"Project save/reopen exactly replays correction recipe/source/block");
        *reopened=Document(reopened->server());check(!replay.matches(*reopened),"Same-address document replacement refuses old Keep authority");
        const auto original=bytes(project_file);const auto output=root/"kept.qpx";std::atomic<bool> cancel{false};
        // Use a different normalization so the existing record does not duplicate.
        const std::vector<std::string> keep={"batch","--keep-dark-flat",project_file.u8string(),output.u8string(),"--image","1","--dark",dark.u8string(),"--flat",flat.u8string(),"--scales","50,100"};
        {Capture log;check(detail::dark_flat_cli(keep,cancel)==0,"CLI Keep saves new project");check(log.out.str().find("\"state\":\"saved\"")!=std::string::npos,"Keep reports saved state");}
        auto saved=Project::open(output.u8string())->document(1);auto ids=dark_flat_recipe_ids(*saved);check(ids.size()==2,"CLI preserves existing recipes");const auto other=ids[0]==id?ids[1]:ids[0];auto expected=replay_dark_flat_recipe(*saved,other);const auto expected_block=read_dark_flat_block(expected,region)->sha256();
        {Capture log;check(detail::dark_flat_cli({"batch","--list-dark-flat",output.u8string(),"--image","1"},cancel)==0 && log.out.str().find(other)!=std::string::npos,"CLI list reports retained recipe");}
        const std::vector<std::string> verify={"batch","--verify-dark-flat",output.u8string(),"--image","1","--record",other};
        {Capture log;check(detail::dark_flat_cli(verify,cancel)==0 && log.out.str().find("\"state\":\"verified\"")!=std::string::npos,"CLI exact source/recipe verification");}
        const std::vector<std::string> preview={"batch","--preview-dark-flat",output.u8string(),"--image","1","--record",other,"--width","8","--height","6","--channels","2"};
        {Capture log;check(detail::dark_flat_cli(preview,cancel)==0 && log.out.str().find(expected_block)!=std::string::npos && log.out.str().find("[48,48]")!=std::string::npos,"CLI preview matches native digest and finite counts");}
        const auto output_bytes=bytes(output);{Capture log;check(detail::dark_flat_cli(keep,cancel)==1,"CLI refuses existing project output");}check(bytes(output)==output_bytes,"Existing output never overwritten");
        for(const auto& path:{project_file,image,dark,flat,fs::path(project_file.u8string()+"-wal"),fs::path(project_file.u8string()+"-shm"),fs::path(project_file.u8string()+"-journal")}){auto bad=keep;bad[3]=path.u8string();Capture log;check(detail::dark_flat_cli(bad,cancel)==1,"Input/field/missing SQLite sidecar outputs protected");}
        auto stopped=keep;stopped[3]=(root/"stopped.qpx").u8string();cancel.store(true);{Capture log;check(detail::dark_flat_cli(stopped,cancel)==130,"Cancelled CLI returns130");}cancel.store(false);check(!fs::exists(stopped[3]),"Cancelled Keep leaves no project");
        auto limited=keep;limited[3]=(root/"limited.qpx").u8string();limited.insert(limited.end(),{"--save-snapshot-bytes","1"});{Capture log;check(detail::dark_flat_cli(limited,cancel)==1,"Save budget enforced");}check(!fs::exists(limited[3]),"Bounded save failure leaves no output");
        auto bad=preview;bad.insert(bad.end(),{"--width","8"});rejects<std::invalid_argument>([&]{detail::dark_flat_cli(bad,cancel);});
        check(bytes(project_file)==original,"CLI leaves original project bytes unchanged");
    }
    check(bytes(image)==signal_bytes && bytes(dark)==dark_bytes && bytes(flat)==flat_bytes,"Native and CLI preserve all signal/field bytes");
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{read_dark_flat_block(request,region,256u*1024*1024,&cancel);});
    remove_dark_flat_recipe(*doc,id);
    {std::ofstream changed(flat,std::ios::binary|std::ios::app);changed.put('x');check(bool(changed),"Change only synthetic flat fixture");}rejects<WorkflowInputChanged>([&]{keep_dark_flat_recipe(*doc,request);});doc->undo();rejects<WorkflowInputChanged>([&]{replay_dark_flat_recipe(*doc,id);});
    d->close();fs::rename(dark,root/"missing-dark.tif");rejects([&]{replay_dark_flat_recipe(*doc,id);});
}
int main(int argc,char** argv){try{if(!has_tiff()){std::cout<<"Optional TIFF disabled\n";return 0;}const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("dark-flat-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);exercise(root);std::cout<<"Native dark/flat recipe, worker block, Undo, corruption/source guards, project replay and CLI checks pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
