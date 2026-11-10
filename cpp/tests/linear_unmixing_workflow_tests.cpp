#include "qupathxx/linear_unmixing_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/linear_unmixing_io.hpp"
#include "../linear_unmixing_cli.hpp"
#include <sstream>
#include "qupathxx/workflow.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <cstring>
using namespace qpx;
namespace fs=std::filesystem;
using V=ImageDataValue;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected linear unmixing workflow refusal");}
void close(double value,double expected){check(std::isfinite(value) && std::abs(value-expected)<=1e-11*std::max(1.,std::abs(expected)),"Native retained unmixing differs from independent oracle");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void field(const fs::path& path,std::uint16_t a,std::uint16_t b){qpx_test::raw_pixel_tiff_fixture(path);std::fstream out(path,std::ios::binary|std::ios::in|std::ios::out);out.seekp(160);for(int i=0;i<48;++i)for(auto v:{a,b}){out.put(char(v&255));out.put(char(v>>8));}out.close();check(bool(out),"Write analytical constant fields");}
LinearMixingMatrix matrix(const Document& doc){const auto m=doc.server()->raw_metadata();LinearMixingMatrix p;p.inputs={{0,m->channel_names.at(0)},{1,m->channel_names.at(1)}};p.components={"A","B"};p.coefficients={1,.25,.5,1};return p;}
void same(const RawPixelBuffer& a,const RawPixelBuffer& b){const auto& x=std::get<std::vector<double>>(a.storage());const auto& y=std::get<std::vector<double>>(b.storage());check(a.valid()==b.valid() && x.size()==y.size() && std::memcmp(x.data(),y.data(),x.size()*sizeof(double))==0,"Replayed outputs retain exact coefficient/operator/sample identity");}
void schema(){LinearUnmixRecipe p;p.signal={"synthetic.tif","tiff",std::string(64,'a'),0};p.matrix.inputs={{0,"C0"},{1,"C1"}};p.matrix.components={"A","B"};p.matrix.coefficients={1,-0.0,.5,1};p.input_source_sha256=p.signal.source_sha256;p.operator_sha256=std::string(64,'b');p.derived_source_sha256=std::string(64,'c');
    const auto roundtrip=LinearUnmixRecipe::from_value(p.to_value());check(roundtrip.sha256()==p.sha256() && std::signbit(roundtrip.matrix.coefficients[1]),"Native recipe schema works without optional solver/reader");
    for(int i=0;i<5;++i){auto bad=p;if(i==0)bad.algorithm="unknown";else if(i==1)bad.input_source_sha256=std::string(64,'d');else if(i==2)bad.signal.series=65536;else if(i==3)bad.operator_sha256="bad";else bad.correction_record=std::string(64,'e');rejects<std::invalid_argument>([&]{bad.to_value();});}
    auto record=p.to_value();std::get<V::Map>(record.value)["extra"]=V{true};rejects<std::invalid_argument>([&]{LinearUnmixRecipe::from_value(record);});
}
void json_schema(){LinearMixingMatrix p;p.inputs={{0,"C0"},{1,"C1"}};p.components={"A","B"};p.coefficients={1,-0.0,.5,1};
    if(!has_linear_mixing_io()){rejects<std::runtime_error>([&]{linear_mixing_matrix_to_json(p);});return;}
    const auto text=linear_mixing_matrix_to_json(p);auto roundtrip=linear_mixing_matrix_from_json(text);check(roundtrip.sha256()==p.sha256() && std::signbit(roundtrip.coefficients[1]),"JSON preserves binary64 coefficient bits and signed zero");
    auto duplicate=text;const auto key=duplicate.find("\"schema\"");check(key!=std::string::npos,"Generated matrix schema field");duplicate.insert(key,"\"schema\":\"duplicate\",");rejects<std::invalid_argument>([&]{linear_mixing_matrix_from_json(duplicate);});
    for(const auto& invalid:{std::string("null"),std::string("{}"),std::string("[true]"),std::string(20,'[')+"0"+std::string(20,']')})rejects<std::invalid_argument>([&]{linear_mixing_matrix_from_json(invalid);});rejects<std::length_error>([&]{linear_mixing_matrix_from_json(std::string(1024*1024+1,' '));});
    const auto coefficients=text.find("\"coefficients\"");const auto first=text.find("1.0",coefficients);check(first!=std::string::npos,"Generated floating coefficient");auto natural=text;natural.replace(first,3,"1");check(linear_mixing_matrix_from_json(natural).sha256()==p.sha256(),"Small JSON integer coefficients map exactly to binary64");auto lossy=text;lossy.replace(first,3,"9007199254740993");rejects<std::invalid_argument>([&]{linear_mixing_matrix_from_json(lossy);});
}
struct Capture {std::ostringstream out,err;std::streambuf *a,*b;Capture():a(std::cout.rdbuf(out.rdbuf())),b(std::cerr.rdbuf(err.rdbuf())){}~Capture(){std::cout.rdbuf(a);std::cerr.rdbuf(b);if(!err.str().empty())std::cerr<<"Native CLI diagnostic: "<<err.str();}};
void cli(const fs::path& root,const fs::path& input,const fs::path& signal,const fs::path& dark,const fs::path& flat,const LinearMixingMatrix& p,const std::string& correction){
    const auto matrix_file=root/"mixing.json",output=root/"cli.qpx";auto edited=p;edited.components[0]="CLI component";save_linear_mixing_matrix_file(edited,matrix_file.u8string());check(load_linear_mixing_matrix_file(matrix_file.u8string()).sha256()==edited.sha256(),"Native guarded matrix file round trip");const auto matrix_bytes=bytes(matrix_file),project_bytes=bytes(input);std::atomic<bool> cancel{false};
    std::vector<std::string> keep={"batch","--keep-unmix",input.u8string(),output.u8string(),"--image","1","--matrix",matrix_file.u8string(),"--correction-record",correction};
    {Capture log;check(detail::linear_unmixing_cli(keep,cancel)==0 && log.out.str().find("\"state\":\"saved\"")!=std::string::npos,"Native CLI retains and saves explicit corrected unmixing");}
    auto saved=Project::open(output.u8string())->document(1);const LinearUnmixRequest expected(*saved,replay_dark_flat_recipe(*saved,correction),edited);const auto id=expected.recipe().sha256();check(load_linear_unmix_recipe(*saved,id).sha256()==id,"CLI recipe equals native authority");
    {Capture log;check(detail::linear_unmixing_cli({"batch","--list-unmix",output.u8string(),"--image","1"},cancel)==0 && log.out.str().find(id)!=std::string::npos,"CLI lists retained record");}
    const std::vector<std::string> verify={"batch","--verify-unmix",output.u8string(),"--image","1","--record",id};
    fs::rename(matrix_file,root/"matrix-moved.json");{Capture log;check(detail::linear_unmixing_cli(verify,cancel)==0 && log.out.str().find(expected.source()->sha256())!=std::string::npos && log.out.str().find(expected.recipe().operator_sha256)!=std::string::npos,"CLI replay uses retained coefficients without reopening authoring file");}fs::rename(root/"matrix-moved.json",matrix_file);
    auto original=edited;original.input_domain="original-channel-samples";const auto original_file=root/"original-mixing.json";save_linear_mixing_matrix_file(original,original_file.u8string());auto raw=keep;raw.resize(raw.size()-2);raw[3]=(root/"cli-original.qpx").u8string();raw[7]=original_file.u8string();{Capture log;check(detail::linear_unmixing_cli(raw,cancel)==0,"Native CLI original-source route");}
    const auto saved_bytes=bytes(output);{Capture log;check(detail::linear_unmixing_cli(keep,cancel)==1,"CLI refuses existing output");}check(bytes(output)==saved_bytes,"CLI existing output bytes stay unchanged");
    for(const auto& path:{input,signal,dark,flat,matrix_file,fs::path(input.u8string()+"-wal"),fs::path(input.u8string()+"-shm"),fs::path(input.u8string()+"-journal")}){auto bad=keep;bad[3]=path.u8string();Capture log;check(detail::linear_unmixing_cli(bad,cancel)==1,"CLI protects project/source/field/matrix/sidecar outputs");}
    auto limited=keep;limited[3]=(root/"limited.qpx").u8string();limited.insert(limited.end(),{"--save-snapshot-bytes","1"});{Capture log;check(detail::linear_unmixing_cli(limited,cancel)==1,"CLI native save work budget");}check(!fs::exists(limited[3]),"Failed save leaves no project");
    auto stopped=keep;stopped[3]=(root/"cancelled.qpx").u8string();cancel.store(true);{Capture log;check(detail::linear_unmixing_cli(stopped,cancel)==130,"CLI cancellation status");}cancel.store(false);check(!fs::exists(stopped[3]),"Cancelled CLI leaves no output");
    auto duplicate=keep;duplicate.insert(duplicate.end(),{"--image","1"});rejects<std::invalid_argument>([&]{detail::linear_unmixing_cli(duplicate,cancel);});auto wrong=keep;wrong[3]=(root/"wrong-domain.qpx").u8string();wrong[7]=original_file.u8string();{Capture log;check(detail::linear_unmixing_cli(wrong,cancel)==1,"CLI refuses corrected parent with original-domain matrix");}check(!fs::exists(wrong[3]),"Domain failure does not publish");
    rejects([&]{save_linear_mixing_matrix_file(edited,matrix_file.u8string());});check(bytes(matrix_file)==matrix_bytes && bytes(input)==project_bytes,"CLI and matrix IO preserve inputs");
}
void exercise(const fs::path& root,bool handoff){
    const auto signal=root/"signal.tif",dark=root/"dark.tif",flat=root/"flat.tif",project_file=root/"unmix.qpx";
    qpx_test::raw_pixel_tiff_fixture(signal);field(dark,10,20);field(flat,20,40);const auto signal_before=bytes(signal),dark_before=bytes(dark),flat_before=bytes(flat);
    auto doc=std::make_shared<Document>(open_tiff(signal.u8string()));doc->set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"user-note",V{std::string("preserve")}}}));
    const auto p=matrix(*doc);const LinearUnmixRequest original(*doc,p);const auto prepared_model=std::async(std::launch::async,[&]{return LinearUnmixModel(p);}).get();const LinearUnmixRequest prepared(*doc,prepared_model);check(prepared.recipe().sha256()==original.recipe().sha256() && prepared_model.sha256()==prepared.recipe().operator_sha256,"Worker-prepared model owner capture retains exact operator/recipe identity");RawRegionRequest region{{0,0,0,8,6},0,4};
    const auto out=std::async(std::launch::async,[&]{return original.source()->read(region);}).get();const auto& values=std::get<std::vector<double>>(out.storage());
    for(int i=0;i<48;++i){close(values[4*i],(6.*i-200)/7);close(values[4*i+1],(4.*i+800)/7);close(values[4*i+2],0);close(values[4*i+3],0);}
    const auto id=keep_linear_unmix_recipe(*doc,original);check(linear_unmix_recipe_ids(*doc)==std::vector<std::string>{id},"Retain exact native recipe once");rejects<WorkflowOutputExists>([&]{keep_linear_unmix_recipe(*doc,original);});
    doc->undo();check(linear_unmix_recipe_ids(*doc).empty(),"Keep has one Undo");doc->redo();check(load_linear_unmix_recipe(*doc,id).sha256()==id,"Redo restores exact recipe");
    remove_linear_unmix_recipe(*doc,id);check(linear_unmix_recipe_ids(*doc).empty() && doc->image_data_properties()->values().count("user-note"),"Remove preserves unrelated properties");doc->undo();
    std::async(std::launch::async,[&]{rejects<std::logic_error>([&]{original.matches(*doc);});}).get();
    Document other(doc->server());check(!original.matches(other),"Different document cannot reuse capture authority");
    doc->set_calibration(1,1);check(!original.matches(*doc),"Calibration change invalidates capture");rejects<WorkflowInputChanged>([&]{original.require_current(*doc);});doc->undo();check(original.matches(*doc),"Calibration Undo restores capture");
    const auto d=open_tiff(dark.u8string()),f=open_tiff(flat.u8string());const DarkFlatRequest correction(*doc,d,f,{{10,20},0});auto corrected_matrix=p;corrected_matrix.input_domain="qpx.dark-flat-corrected-samples/1";
    rejects<std::out_of_range>([&]{LinearUnmixRequest premature(*doc,correction,corrected_matrix);});
    const auto correction_id=keep_dark_flat_recipe(*doc,correction);const LinearUnmixRequest corrected(*doc,correction,corrected_matrix);
    rejects<std::invalid_argument>([&]{LinearUnmixRequest wrong_domain(*doc,correction,p);});rejects<std::invalid_argument>([&]{LinearUnmixRequest wrong_domain(*doc,corrected_matrix);});
    remove_dark_flat_recipe(*doc,correction_id);check(!corrected.matches(*doc),"Missing parent invalidates pending unmixing capture");rejects<WorkflowInputChanged>([&]{keep_linear_unmix_recipe(*doc,corrected);});doc->undo();check(corrected.matches(*doc),"Parent Undo restores pending capture");
    const auto corrected_id=keep_linear_unmix_recipe(*doc,corrected);check(linear_unmix_recipes_reference_correction(*doc,correction_id),"Retained recipe references its correction");rejects<std::invalid_argument>([&]{remove_dark_flat_recipe(*doc,correction_id);});
    const auto corrected_out=corrected.source()->read(region);const auto& cv=std::get<std::vector<double>>(corrected_out.storage());for(int i=0;i<48;++i){close(cv[4*i],(6.*i-240)/7);close(cv[4*i+1],(4.*i+680)/7);close(cv[4*i+2],0);close(cv[4*i+3],0);}
    check(corrected.recipe().correction_record==correction_id && corrected.recipe().operator_sha256!=original.recipe().operator_sha256 && corrected.source()->sha256()!=original.source()->sha256(),"Domains and parent/source/operator identities remain explicit");
    auto zero=p;zero.coefficients[1]=-0.0;const LinearUnmixRequest signed_zero(*doc,zero);const auto zero_id=keep_linear_unmix_recipe(*doc,signed_zero);
    check(std::signbit(load_linear_unmix_recipe(*doc,zero_id).matrix.coefficients[1]),"Retained typed record keeps negative zero");
    const auto properties=doc->image_data_properties();
    for(int i=0;i<9;++i){auto corrupt=properties->values();auto& all=std::get<V::Map>(corrupt.at("qupathxx.linear-unmix-recipes/1").value);auto& record=std::get<V::Map>(all.at(id).value);
        if(i==0)record["algorithm"]=V{std::string("unknown")};else if(i==1)record["schema"]=V{std::string("qpx.linear-unmix-recipe/2")};else if(i==2)record["domain"]=V{std::string("original-channel-samples")};
        else if(i==3)record["operator_sha256"]=V{std::string(64,'a')};else if(i==4)record["correction_record"]=V{correction_id};else if(i==5)record["input_source_sha256"]=V{std::string(64,'a')};else if(i==6)record["extra"]=V{true};else if(i==7)record.erase("matrix");else std::get<V::Map>(record.at("signal").value)["series"]=V{-1.0};
        doc->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(corrupt)));rejects<std::invalid_argument>([&]{load_linear_unmix_recipe(*doc,id);});rejects<std::invalid_argument>([&]{linear_unmix_recipe_ids(*doc);});doc->set_image_data_properties(properties);}
    auto other_matrix=p;other_matrix.components[0]="Different component";rejects<WorkflowInputChanged>([&]{restore_linear_unmix_recipe(*doc,original.recipe(),LinearUnmixModel(other_matrix));});
    auto changed_operator=original.recipe();changed_operator.operator_sha256=std::string(64,'a');rejects<WorkflowInputChanged>([&]{restore_linear_unmix_recipe(*doc,changed_operator);});
    rejects<WorkflowInputChanged>([&]{restore_linear_unmix_recipe(*doc,corrected.recipe());});rejects<WorkflowInputChanged>([&]{restore_linear_unmix_recipe(*doc,original.recipe(),correction);});same(corrected_out,restore_linear_unmix_recipe(*doc,corrected.recipe(),correction).source()->read(region));
    auto orphan=properties->values();orphan.erase("qupathxx.dark-flat-recipes/1");doc->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(orphan)));check(!corrected.matches(*doc),"Corrupt/orphan parent never falls back to original samples");rejects<std::out_of_range>([&]{replay_linear_unmix_recipe(*doc,corrected_id);});doc->set_image_data_properties(properties);check(corrected.matches(*doc),"Restoring exact parent restores match");
    if(has_project()){Project project;project.add_document(doc);project.save_new(project_file.u8string());auto reopened=Project::open(project_file.u8string())->document(1);
        auto replay=replay_linear_unmix_recipe(*reopened,id);check(replay.recipe().sha256()==id && replay.source()->sha256()==original.source()->sha256(),"Project restores exact original unmix source/recipe");same(out,replay.source()->read(region));same(corrected_out,replay_linear_unmix_recipe(*reopened,corrected_id).source()->read(region));
        auto zero_replay=replay_linear_unmix_recipe(*reopened,zero_id);check(std::signbit(zero_replay.recipe().matrix.coefficients[1]),"Project round trip preserves signed-zero matrix");
        cli(root,project_file,signal,dark,flat,corrected_matrix,correction_id);
        *reopened=Document(reopened->server());check(!replay.matches(*reopened),"Same-address document replacement invalidates capture");}
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{LinearUnmixRequest stopped(*doc,p,&cancel);});rejects<WorkflowCancelled>([&]{keep_linear_unmix_recipe(*doc,original,&cancel);});rejects<WorkflowCancelled>([&]{replay_linear_unmix_recipe(*doc,id,&cancel);});
    remove_linear_unmix_recipe(*doc,corrected_id);check(!linear_unmix_recipes_reference_correction(*doc,correction_id),"Removing unmix recipe releases its parent reference");remove_dark_flat_recipe(*doc,correction_id);doc->undo();doc->undo();check(load_linear_unmix_recipe(*doc,corrected_id).sha256()==corrected_id,"Removal Undo restores parent reference");
    check(bytes(signal)==signal_before && bytes(dark)==dark_before && bytes(flat)==flat_before,"Recipe/project/worker operations leave original and field bytes unchanged");
    if(handoff){std::ofstream report(root/"handoff.json");report<<"{\"original_record\":\""<<id<<"\",\"corrected_record\":\""<<corrected_id<<"\",\"original_source\":\""<<original.source()->sha256()<<"\",\"corrected_source\":\""<<corrected.source()->sha256()<<"\",\"corrected_operator\":\""<<corrected.recipe().operator_sha256<<"\"}\n";report.close();check(bool(report),"Write retained valid CLI handoff identities");return;}
    {std::ofstream changed(flat,std::ios::binary|std::ios::app);changed.put('x');check(bool(changed),"Change only synthetic flat source");}rejects<WorkflowInputChanged>([&]{corrected.source()->read(region);});rejects<WorkflowInputChanged>([&]{replay_linear_unmix_recipe(*doc,corrected_id);});
    remove_linear_unmix_recipe(*doc,id);{std::ofstream changed(signal,std::ios::binary|std::ios::app);changed.put('x');check(bool(changed),"Change only synthetic signal source");}rejects<WorkflowInputChanged>([&]{keep_linear_unmix_recipe(*doc,original);});
    const auto recipe=original.recipe();rejects<WorkflowInputChanged>([&]{restore_linear_unmix_recipe(*doc,recipe);});doc->server()->close();rejects<WorkflowInputChanged>([&]{original.source()->read(region);});
}
int main(int argc,char** argv){try{schema();json_schema();if(!has_tiff() || !has_linear_unmixing()){std::cout<<"Optional native TIFF/unmixing capability unavailable\n";return 0;}const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("linear-unmix-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);exercise(root,argc>2 && std::string(argv[2])=="--handoff");std::cout<<"Native unmixing recipe/source/operator, worker values, parent references, Undo, project replay, matrix IO, CLI and stale/cancel checks pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
