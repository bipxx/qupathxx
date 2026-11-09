#include "../marker_cli.hpp"
#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <sstream>
#include <iostream>
using namespace qpx;
namespace fs=std::filesystem;
using Json=nlohmann::json;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected marker IO/CLI refusal");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
struct Capture {std::ostringstream out,err;std::streambuf *o=std::cout.rdbuf(out.rdbuf()),*e=std::cerr.rdbuf(err.rdbuf());~Capture(){std::cout.rdbuf(o);std::cerr.rdbuf(e);if(std::uncaught_exceptions() && !err.str().empty())std::cerr<<"Captured marker error: "<<err.str();}};
MarkerRule rule(){MarkerRule p;p.name=u8"A and \u03bcB";MarkerGate a;a.name="A";a.threshold=100;MarkerGate b;b.name="B";b.channel=8;b.threshold=-0.;p.gates={a,b};p.nodes={{MarkerOperation::Gate,0,{}},{MarkerOperation::Gate,1,{}},{MarkerOperation::And,0,{0,1}}};return p;}
void io(const fs::path& root){const auto p=rule();const auto text=marker_rule_to_json(p);const auto decoded=marker_rule_from_json(text);check(decoded.sha256()==p.sha256() && std::signbit(decoded.gates[1].threshold),"Human JSON round trip preserves exact typed recipe/threshold bits");
    const auto file=root/"rule.json";save_marker_rule_file(p,file.u8string());check(load_marker_rule_file(file.u8string()).sha256()==p.sha256(),"Native rule file roundtrip");rejects([&]{save_marker_rule_file(p,file.u8string());});check(bytes(file)==text,"Rule file never overwrites existing output");
    std::atomic<bool> cancel{true};rejects([&]{save_marker_rule_file(p,(root/"cancelled.json").u8string(),&cancel);});rejects([&]{load_marker_rule_file(file.u8string(),&cancel);});check(!fs::exists(root/"cancelled.json"),"Cancelled rule write leaves no file");
    rejects([&]{save_marker_rule_file(p,(root/"guarded.json").u8string(),nullptr,[]{throw std::runtime_error("input changed");});});check(!fs::exists(root/"guarded.json"),"Publication guard leaves no output");
    for(int i=0;i<9;++i){auto j=Json::parse(text);if(i==0)j["schema"]="qpx.marker-rule/2";else if(i==1)j["unknown_field"]=1;else if(i==2)j["gates"][0]["channel"]=1.0;else if(i==3)j["gates"][0]["channel"]=-1;else if(i==4)j["gates"][0]["comparison"]="approx";else if(i==5)j["gates"][0]["units"]="source-sample-sum";else if(i==6)j["gates"][0]["threshold"]="100";else if(i==7)j["nodes"][2]["inputs"]={0,2};else j["nodes"][0]["inputs"]=Json::array();rejects<std::invalid_argument>([&]{marker_rule_from_json(j.dump());});}
    auto duplicate=text;duplicate.insert(duplicate.find('{')+1,"\"schema\":\"qpx.marker-rule/1\",");rejects<std::invalid_argument>([&]{marker_rule_from_json(duplicate);});rejects([&]{marker_rule_from_json(std::string(1024*1024+1,' '));});
    Document doc(Image::demo());keep_marker_rule(doc,p);auto owned=std::make_shared<Document>(doc);Project project;project.add_document(owned);const auto listfile=root/"rules-only.qpx";project.save_new(listfile.u8string());std::atomic<bool> live{false};Capture log;check(detail::marker_cli({"batch","--list-marker-rules",listfile.u8string(),"--image","1"},live)==0 && Json::parse(log.out.str()).at("rules").at(0)==p.sha256(),"CLI rule listing does not need pixel capability");
}
void workflow(const fs::path& root){const auto file=root/"source.tif",source=root/"study.qpx",output=root/"classified.qpx";qpx_test::raw_pixel_tiff_fixture(file,15,9);auto doc=std::make_shared<Document>(open_tiff(file.u8string()));const auto roi=doc->add_rectangle({1,1,12,7},"ROI");RawRegionRequest region;region.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=1.5;
    const FluorescenceCellRequest captured(*doc,region,roi,options,{0,0});const auto ids=publish_fluorescence_cells(*doc,captured,*trace_fluorescence_cells(detect_fluorescence_cells(captured)));check(ids.size()==1,"Independent raw fixture publishes one cell");
    doc->execute({ObjectEdit::create_geometry(Roi::rectangle({0,0,1,1}),"Other cell",0,"",ObjectKind::Cell,Roi::rectangle({0,0,1,1}))},"Other imported cell");check(marker_cell_ids(*doc)==ids,"Native cohort excludes unrelated cells");
    using V=ImageDataValue;doc->set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"user-note",V{std::string("keep")}}}));Project project;project.add_document(doc);project.save_new(source.u8string());const auto original=bytes(source),pixels=bytes(file),recipe=bytes(root/"rule.json");
    std::atomic<bool> cancel{false};std::vector<std::string> preview={"batch","--preview-markers",source.u8string(),"--image","1","--rule-file",(root/"rule.json").u8string(),"--all-cells"};Json expected;
    {Capture log;check(detail::marker_cli(preview,cancel)==0 && log.err.str().empty(),"Actual CLI preview succeeds");expected=Json::parse(log.out.str()).at("result");check(expected.at("counts").at("unknown")==1 && expected.at("counts").at("negative")==0 && expected.at("cohorts").at("unknown")==ids,"CLI shows Unknown cohort without invented Negative");}
    std::vector<std::string> apply={"batch","--apply-markers",source.u8string(),output.u8string(),"--image","1","--rule-file",(root/"rule.json").u8string(),"--all-cells"};
    {Capture log;check(detail::marker_cli(apply,cancel)==0 && log.err.str().empty(),"CLI apply/new-project final guard succeeds");const auto result=Json::parse(log.out.str());check(result.at("changed_cells")==1 && result.at("result").at("result_sha256")==expected.at("result_sha256"),"Apply matches preview and reports native result identity");}
    const auto saved=bytes(output);const auto reopened=Project::open(output.u8string())->document(1);check(reopened->annotation(ids[0]).class_name==rule().unknown_class && fluorescence_cell_observations_intact(reopened->annotation(ids[0])) && load_marker_rule(*reopened,rule().sha256()).sha256()==rule().sha256() && reopened->image_data_properties()->values().count("user-note"),"Saved class/rule/raw facts and unrelated metadata persist");
    auto replay=std::vector<std::string>{"batch","--preview-markers",output.u8string(),"--image","1","--rule",rule().sha256(),"--all-cells"};{Capture log;check(detail::marker_cli(replay,cancel)==0 && Json::parse(log.out.str()).at("result").at("result_sha256")==expected.at("result_sha256"),"Saved exact rule replay reproduces scientific identity");}
    {Capture log;check(detail::marker_cli(apply,cancel)==1,"Existing output refuses apply");}check(bytes(output)==saved,"Existing output never overwritten");
    for(const auto& path:{source,file,root/"rule.json",fs::path(source.u8string()+"-journal"),fs::path(source.u8string()+"-wal"),fs::path(source.u8string()+"-shm")}){auto bad=apply;bad[3]=path.u8string();Capture log;check(detail::marker_cli(bad,cancel)==1,"Protected input/sidecar output refused");}
    check(bytes(source)==original && bytes(file)==pixels && bytes(root/"rule.json")==recipe,"Preview/apply/errors preserve all input bytes");
    auto stopped=apply;stopped[3]=(root/"stopped.qpx").u8string();cancel=true;{Capture log;check(detail::marker_cli(stopped,cancel)==130,"Cancelled command returns130");}cancel=false;check(!fs::exists(stopped[3]),"Cancelled CLI leaves no project");
    auto limited=apply;limited[3]=(root/"limited.qpx").u8string();limited.insert(limited.end(),{"--save-snapshot-bytes","1"});{Capture log;check(detail::marker_cli(limited,cancel)==1,"Bounded save command fails");}check(!fs::exists(limited[3]),"Bounded save failure leaves no project");
    auto explicit_id=preview;explicit_id.back()="--cell";explicit_id.push_back(std::to_string(ids[0]));{Capture log;check(detail::marker_cli(explicit_id,cancel)==0 && Json::parse(log.out.str()).at("result").at("result_sha256")==expected.at("result_sha256"),"Explicit and native all-cell cohorts agree");}
    for(const auto extra:std::vector<std::vector<std::string>>{{"--image","1"},{"--rule","x"},{"--cell","1"},{"--nonsense","1"}}){auto bad=preview;bad.insert(bad.end(),extra.begin(),extra.end());rejects<std::invalid_argument>([&]{detail::marker_cli(bad,cancel);});}
}
int main(){try{std::atomic<bool> cancel{false};check(!detail::marker_cli({"batch","--unrelated"},cancel),"Unrelated dispatcher route");rejects<std::invalid_argument>([&]{detail::marker_cli({"batch","--apply-markers"},cancel);});if(!has_marker_rule_io() || !has_project())return 0;
    const auto root=fs::current_path()/("marker-cli-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Unique marker CLI directory");io(root);if(has_tiff() && has_geos())workflow(root);else std::cout<<"Source classification integration needs TIFF/GEOS; IO/list checks run\n";
    std::cout<<"Marker rule IO/CLI: exact JSON, strict parse, native preview/cohort/apply, protected inputs, saved replay and cancellation/budgets passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
