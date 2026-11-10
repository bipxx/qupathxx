#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/marker_rule_io.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/workflow.hpp"
#include "../fluorescence_cli.hpp"
#include "../marker_cli.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected corrected fluorescence refusal");}
void expect_close(double a,double b){check(std::isfinite(a) && std::abs(a-b)<1e-10,"Corrected compartment differs from independent oracle");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void field(const fs::path& path,std::uint16_t a,std::uint16_t b){qpx_test::raw_pixel_tiff_fixture(path,15,9);std::fstream out(path,std::ios::binary|std::ios::in|std::ios::out);out.seekp(160);for(int i=0;i<135;++i)for(auto v:{a,b}){out.put(char(v&255));out.put(char(v>>8));}out.close();check(bool(out),"Write analytical field fixture");}
MarkerRule rule(const std::string& domain){MarkerRule p;p.domain=domain;p.gates={{"DNA",0}};p.gates[0].threshold=105;p.nodes={{MarkerOperation::Gate,0,{}}};return p;}
struct Capture {std::ostringstream out,err;std::streambuf *a,*b;Capture():a(std::cout.rdbuf(out.rdbuf())),b(std::cerr.rdbuf(err.rdbuf())){}~Capture(){std::cout.rdbuf(a);std::cerr.rdbuf(b);if(!err.str().empty())std::cerr<<"Native diagnostic: "<<err.str();}};
void exercise(const fs::path& root){
    const auto signal=root/"signal.tif",dark=root/"dark.tif",flat=root/"flat.tif";qpx_test::raw_pixel_tiff_fixture(signal,15,9);field(dark,10,20);field(flat,20,40);
    const auto original_pixels=bytes(signal),dark_pixels=bytes(dark),flat_pixels=bytes(flat);
    auto doc=std::make_shared<Document>(open_tiff(signal.u8string()));const auto roi=doc->add_rectangle({1,1,12,7},"ROI");
    const DarkFlatRequest correction(*doc,open_tiff(dark.u8string()),open_tiff(flat.u8string()),{{10,20},0});RawRegionRequest region;region.channels=2;
    FluorescenceCellOptions raw_options;raw_options.threshold=100;raw_options.cell_expansion=1.5;auto options=raw_options;options.threshold=90;
    rejects<std::out_of_range>([&]{FluorescenceCellRequest not_kept(*doc,correction,region,roi,options,{0,0});});
    const auto correction_id=keep_dark_flat_recipe(*doc,correction);const FluorescenceCellRequest request(*doc,correction,region,roi,options,{0,0});
    remove_dark_flat_recipe(*doc,correction_id);check(!request.matches(*doc),"Removing an unreferenced correction invalidates pending capture");doc->undo();check(request.matches(*doc),"Correction Undo restores pending capture");
    const FluorescenceCellRequest original(*doc,region,roi,raw_options,{0,0});const auto raw=detect_fluorescence_cells(original);
    const auto run=std::async(std::launch::async,[&]{return detect_fluorescence_cells(request);}).get();
    check(run->measurement_domain()=="qpx.dark-flat-corrected-samples/1" && run->correction_record()==correction_id && run->source_metadata().sample_type==SampleType::Float64,"Corrected run names its source/domain explicitly");
    check(raw->measurement_domain()=="original-channel-samples" && raw->correction_record().empty() && raw->source_metadata().sample_type==SampleType::UInt16,"Default route remains original UInt16");
    check(run->raster().nuclei==raw->raster().nuclei && run->raster().bodies==raw->raster().bodies && run->sha256()!=raw->sha256(),"Known linear correction retains analytical geometry with distinct scientific identity");
    check(run->raster().measurements.size()==1 && run->raster().measurements[0].nucleus.pixels==14 && run->raster().measurements[0].cell.pixels==27,"Analytical corrected mask/compartment sizes");
    expect_close(run->raster().measurements[0].nucleus.channels[0].sum.value(),1401);expect_close(run->raster().measurements[0].cell.channels[0].mean.value(),92);expect_close(run->raster().measurements[0].cytoplasm->channels[1].mean.value(),2513./13-20);
    expect_close(raw->raster().measurements[0].nucleus.channels[0].sum.value(),1541);
    std::atomic<bool> cancel{true};rejects<WorkflowCancelled>([&]{detect_fluorescence_cells(request,&cancel);});
    const auto record=keep_fluorescence_cell_run(*doc,request,*run);check(verify_fluorescence_cell_record(*doc,record,request,*run),"Corrected run record verifies");
    rejects<std::invalid_argument>([&]{remove_dark_flat_recipe(*doc,correction_id);});
    auto properties=doc->image_data_properties();using V=ImageDataValue;auto missing=properties->values();missing.erase("qupathxx.dark-flat-recipes/1");doc->set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(missing)));
    check(!request.matches(*doc) && original.matches(*doc),"Lost corrected association stales only the selected derived domain");rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(*doc,request,*run);});doc->set_image_data_properties(properties);
    auto replay=replay_fluorescence_cell_record(*doc,record);check(detect_fluorescence_cells(replay)->sha256()==run->sha256(),"Versioned corrected recipe replays exactly");
    auto tiny=options;tiny.work_bytes=65536;rejects<WorkflowCaptureLimit>([&]{FluorescenceCellRequest limited(*doc,correction,region,roi,tiny,{0,0});});
    auto empty_options=options;empty_options.threshold=10000;const FluorescenceCellRequest empty_request(*doc,correction,region,roi,empty_options,{0,0});const auto empty_run=detect_fluorescence_cells(empty_request);check(empty_run->raster().measurements.empty(),"Empty corrected result is explicit");
    const auto empty_record=keep_fluorescence_cell_run(*doc,empty_request,*empty_run);check(verify_fluorescence_cell_record(*doc,empty_record,empty_request,*empty_run),"Zero-cell corrected record retains replay identity");remove_fluorescence_cell_record(*doc,empty_record);
    if(!has_geos())return;
    const auto originals=publish_fluorescence_cells(*doc,original,*trace_fluorescence_cells(raw));const auto ids=publish_fluorescence_cells(*doc,request,*trace_fluorescence_cells(run));check(ids.size()==1 && originals.size()==1,"Original and corrected cells coexist");
    const auto cell=doc->annotation(ids[0]);check(cell.properties->metadata().at("fluorescence.profile")=="qpx.fluorescence-cell-publication/2" && cell.properties->metadata().at("fluorescence.measurement_domain")==run->measurement_domain() && cell.properties->metadata().at("fluorescence.correction_record")==correction_id,"Derived publication profile and source association persist");
    expect_close(cell.properties->measurements().at("fluorescence.nucleus.C0.sum"),1401);check(fluorescence_cell_observations_intact(cell) && verify_fluorescence_cell_publications(*doc,ids,request,*run),"Corrected observations/geometry retain native integrity");
    const auto from_cell=replay_fluorescence_cell(*doc,ids[0]);check(detect_fluorescence_cells(from_cell)->sha256()==run->sha256(),"Corrected object recipe replays exactly");
    doc->undo();check(doc->object_count()==2,"Corrected publication is one Undo command");doc->redo();
    remove_fluorescence_cell_record(*doc,record);rejects<std::invalid_argument>([&]{remove_dark_flat_recipe(*doc,correction_id);});doc->undo();
    check(marker_cell_ids(*doc)==originals && marker_cell_ids_for_domain(*doc,run->measurement_domain())==ids,"Domain cohorts never silently mix original and corrected cells");
    const auto corrected_rule=rule(run->measurement_domain()),original_rule=rule(raw->measurement_domain());
    rejects<std::invalid_argument>([&]{MarkerRuleRequest wrong(*doc,ids,original_rule);});
    auto result=preview_marker_rule(MarkerRuleRequest(*doc,ids,corrected_rule));check(result->counts().negative==1 && preview_marker_rule(MarkerRuleRequest(*doc,originals,original_rule))->counts().positive==1,"Same cutoff explicitly distinguishes corrected/original observations");
    keep_marker_rule(*doc,corrected_rule);const MarkerRuleRequest marker(*doc,ids,corrected_rule);result=preview_marker_rule(marker);apply_marker_rule(*doc,marker,*result);check(verify_marker_rule_publication(*doc,*result) && fluorescence_cell_observations_intact(doc->annotation(ids[0])),"Marker publication preserves corrected observations");
    if(has_project()){
        Project project;project.add_document(doc);const auto saved=root/"corrected.qpx";project.save_new(saved.u8string());auto reopened=Project::open(saved.u8string())->document(1);auto fresh=replay_fluorescence_cell_record(*reopened,record);auto repeated=detect_fluorescence_cells(fresh);
        check(repeated->sha256()==run->sha256() && verify_fluorescence_cell_record(*reopened,record,fresh,*repeated) && verify_fluorescence_cell_publications(*reopened,ids,fresh,*repeated),"Project save/reopen preserves all corrected cells, source recipe and run identity");
        auto again=preview_marker_rule(MarkerRuleRequest(*reopened,ids,load_marker_rule(*reopened,corrected_rule.sha256())));check(again->sha256()==result->sha256() && verify_marker_rule_publication(*reopened,*again),"Corrected-domain marker rule and result replay exactly");
        auto cli_doc=std::make_shared<Document>(open_tiff(signal.u8string()));const auto cli_roi=cli_doc->add_rectangle({1,1,12,7},"ROI");const auto cli_correction=DarkFlatRequest(*cli_doc,open_tiff(dark.u8string()),open_tiff(flat.u8string()),{{10,20},0});const auto cli_id=keep_dark_flat_recipe(*cli_doc,cli_correction);
        auto expected=detect_fluorescence_cells(FluorescenceCellRequest(*cli_doc,cli_correction,region,cli_roi,options,{0,0}));Project cli_project;cli_project.add_document(cli_doc);const auto input=root/"cli-input.qpx",output=root/"cli-output.qpx";cli_project.save_new(input.u8string());const auto before=bytes(input);cancel.store(false);
        const std::vector<std::string> args={"batch","--detect-fluorescence",input.u8string(),output.u8string(),"--image","1","--roi","1","--channels","2","--threshold","90","--expansion","1.5","--context","0","--correction",cli_id};
        {Capture log;check(detail::fluorescence_cli(args,cancel)==0 && log.out.str().find(expected->sha256())!=std::string::npos && log.out.str().find(run->measurement_domain())!=std::string::npos,"Actual corrected detection CLI matches native identity/domain");}
        auto out_doc=Project::open(output.u8string())->document(1);const auto cli_cells=marker_cell_ids_for_domain(*out_doc,run->measurement_domain());check(cli_cells.size()==1 && marker_cell_ids(*out_doc).empty(),"CLI publishes corrected cells only");const auto cli_records=fluorescence_cell_record_ids(*out_doc);
        {Capture log;check(detail::fluorescence_cli({"batch","--verify-fluorescence",output.u8string(),"--image","1","--record",cli_records.at(0)},cancel)==0,"Actual CLI replays version2 recipe");}
        const auto rule_file=root/"corrected-rule.json";save_marker_rule_file(corrected_rule,rule_file.u8string());const auto expected_marker=preview_marker_rule(MarkerRuleRequest(*out_doc,cli_cells,corrected_rule));
        {Capture log;check(detail::marker_cli({"batch","--preview-markers",output.u8string(),"--image","1","--rule-file",rule_file.u8string(),"--all-cells"},cancel)==0 && log.out.str().find(expected_marker->sha256())!=std::string::npos,"Marker CLI chooses the explicit corrected domain cohort");}
        for(const auto& target:{dark,flat}){auto bad=args;bad[3]=target.u8string();Capture log;check(detail::fluorescence_cli(bad,cancel)==1,"Correction fields are protected outputs");}
        check(bytes(input)==before,"Corrected CLI leaves input project unchanged");
    }
    check(bytes(signal)==original_pixels && bytes(dark)==dark_pixels && bytes(flat)==flat_pixels,"Derived analysis preserves all original signal/field bytes");
    {std::ofstream changed(flat,std::ios::binary|std::ios::app);changed.put('x');}check(!request.publication_matches(*doc) && original.publication_matches(*doc),"Changed correction field blocks derived publication but leaves original route independent");rejects<WorkflowInputChanged>([&]{require_current_fluorescence_cells(*doc,request,*run);});
}
int main(int argc,char** argv){try{if(!has_tiff()){std::cout<<"Optional TIFF disabled\n";return 0;}const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/("corrected-fluorescence-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);exercise(root);std::cout<<"Corrected fluorescence geometry, compartment values, provenance references, marker domains, native/CLI publication and exact saved replay pass\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
