#include "fluorescence_cli.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool v){if(!v)throw std::runtime_error("Fluorescence CLI check failed");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Capture {std::ostringstream out,err;std::streambuf *o=std::cout.rdbuf(out.rdbuf()),*e=std::cerr.rdbuf(err.rdbuf());~Capture(){std::cout.rdbuf(o);std::cerr.rdbuf(e);}};
template<class F>void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Expected argument refusal");}
int main(){try{
    std::atomic<bool> cancel{false};check(!detail::fluorescence_cli({"batch","--unrelated"},cancel));
    rejects([&]{detail::fluorescence_cli({"batch","--detect-fluorescence"},cancel);});
    if(!has_project() || !has_tiff()){std::cout<<"Fluorescence CLI: optional project/TIFF disabled\n";return 0;}
    const auto root=fs::current_path()/("fluorescence-cli-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root));
    const auto input=root/"input.qpx",source=root/"source.tif",output=root/"result.qpx";qpx_test::raw_pixel_tiff_fixture(source,15,9);
    Project project;const auto image=project.add_tiff_image(source.u8string(),{});auto doc=project.document(image);const auto roi=doc->add_rectangle({1,1,12,7},"ROI");project.save_new(input.u8string());const auto original=bytes(input),pixels=bytes(source);
    std::vector<std::string> args={"batch","--detect-fluorescence",input.u8string(),output.u8string(),"--image",std::to_string(image),"--roi",std::to_string(roi),"--channels","2","--threshold","100","--expansion","1.5","--context","0"};
    if(!has_geos())args.push_back("--record-only");
    {Capture log;check(detail::fluorescence_cli(args,cancel)==0);check(log.err.str().empty() && log.out.str().find("\"state\":\"saved\"")!=std::string::npos && log.out.str().find("\"cells\":1")!=std::string::npos);}
    const auto saved=bytes(output);auto reopened=Project::open(output.u8string());auto restored=reopened->document(image);const auto records=fluorescence_cell_record_ids(*restored);check(records.size()==1 && bytes(input)==original && bytes(source)==pixels);
    if(has_geos()){check(restored->object_count()==2);const auto cell=restored->annotation(roi+1);check(cell.kind==ObjectKind::Cell && cell.properties->measurements().at("fluorescence.nucleus.C0.sum")==1541);}
    std::vector<std::string> verify={"batch","--verify-fluorescence",output.u8string(),"--image",std::to_string(image),"--record",records[0]};
    {Capture log;check(detail::fluorescence_cli(verify,cancel)==0 && log.out.str().find("\"state\":\"verified\"")!=std::string::npos);}
    {Capture log;check(detail::fluorescence_cli({"batch","--list-fluorescence",output.u8string(),"--image",std::to_string(image)},cancel)==0 && log.out.str().find(records[0])!=std::string::npos);}
    {Capture log;check(detail::fluorescence_cli(args,cancel)==1);}check(bytes(output)==saved);
    for(const auto& target:{input,source,fs::path(input.u8string()+"-journal")}){auto bad=args;bad[3]=target.u8string();Capture log;check(detail::fluorescence_cli(bad,cancel)==1);}check(bytes(input)==original && bytes(source)==pixels);
    auto empty=args;empty[3]=(root/"empty.qpx").u8string();empty[11]="10000";empty.push_back("--record-only");if(!has_geos())empty.pop_back();
    {Capture log;check(detail::fluorescence_cli(empty,cancel)==0 && log.out.str().find("\"cells\":0")!=std::string::npos);}
    auto cancelled=args;cancelled[3]=(root/"cancelled.qpx").u8string();cancel=true;{Capture log;check(detail::fluorescence_cli(cancelled,cancel)==130);}cancel=false;check(!fs::exists(cancelled[3]));
    auto limited=args;limited[3]=(root/"limited.qpx").u8string();limited.insert(limited.end(),{"--save-snapshot-bytes","1"});{Capture log;check(detail::fluorescence_cli(limited,cancel)==1);}check(!fs::exists(limited[3]));
    const auto cropped=root/"channel-crop.qpx";
    {Capture log;check(detail::fluorescence_cli({"batch","--detect-fluorescence",input.u8string(),cropped.u8string(),"--image",std::to_string(image),"--first-channel","1","--channels","1","--segmentation-channel","0","--x","1","--y","1","--width","12","--height","7","--threshold","200","--expansion","1.5","--context","0","--record-only"},cancel)==0);}
    auto crop_doc=Project::open(cropped.u8string())->document(image);const auto crop_id=fluorescence_cell_record_ids(*crop_doc).at(0);const auto crop_request=replay_fluorescence_cell_record(*crop_doc,crop_id);const auto crop_run=detect_fluorescence_cells(crop_request);
    check(crop_request.object_id()==0 && crop_request.core_region().first_channel==1 && crop_request.core_region().channels==1 && crop_request.core_region().region.x==1 && crop_request.options().segmentation_channel==0 && crop_run->raster().measurements.at(0).nucleus.channels.at(0).sum==2941 && crop_doc->object_count()==1);
    for(const auto& extra:std::vector<std::vector<std::string>>{{"--width","0"},{"--channels","0"},{"--first-channel","-1"},{"--threshold","inf"}}){auto bad=std::vector<std::string>{"batch","--detect-fluorescence",input.u8string(),(root/"invalid.qpx").u8string(),"--image",std::to_string(image)};bad.insert(bad.end(),extra.begin(),extra.end());rejects([&]{detail::fluorescence_cli(bad,cancel);});}check(!fs::exists(root/"invalid.qpx"));
    for(const auto& extra:std::vector<std::vector<std::string>>{{"--threshold","nan"},{"--image","1"},{"--width","3"},{"--method","otsu"},{"--unexpected","1"}}){auto bad=args;bad.insert(bad.end(),extra.begin(),extra.end());rejects([&]{detail::fluorescence_cli(bad,cancel);});}
    verify.back()="unknown";{Capture log;check(detail::fluorescence_cli(verify,cancel)==1);}check(bytes(output)==saved);
    std::cout<<"Fluorescence CLI: native detect/list/replay, exact raw facts, empty record, protected inputs, no overwrite, budgets and cancellation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
