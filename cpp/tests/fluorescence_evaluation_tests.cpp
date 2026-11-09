#include "fluorescence_evaluation.hpp"
#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/core.hpp"
#include "nucls_png_fixture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>
using namespace qpx;
using namespace fluorescence_evaluation;
namespace fs=std::filesystem;
using Json=nlohmann::json;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void expect_close(double a,double b){check(std::isfinite(a) && std::abs(a-b)<1e-12,"Independent metric oracle differs");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected evaluation refusal");}
void write(const fs::path& file,const std::string& bytes){std::ofstream out(file,std::ios::binary);out.write(bytes.data(),std::streamsize(bytes.size()));check(bool(out),"Write independently generated annotation");}
std::optional<std::size_t> best_training_candidate(const Json& candidates){
    std::optional<std::size_t> best;
    const auto key=[](const Json& c){const auto& p=c.at("protocol").at("segmentation");return std::make_tuple(c.at("object_f1").get<double>(),c.at("foreground_dice").get<double>(),-p.at("sigma").get<double>(),-p.at("peak_prominence").get<double>());};
    for(std::size_t i=0;i<candidates.size();++i)if(candidates[i].at("non_f1_gates_passed").get<bool>() && (!best || key(candidates[i])>key(candidates[*best])))best=i;
    return best;
}
void selection_oracle(){
    const auto candidate=[](bool eligible,double f1,double dice,double sigma,double prominence){return Json{{"non_f1_gates_passed",eligible},{"object_f1",f1},{"foreground_dice",dice},{"protocol",{{"segmentation",{{"sigma",sigma},{"peak_prominence",prominence}}}}}};};
    Json values=Json::array({candidate(true,.7,.8,1,1.5),candidate(false,1,1,0,.5),candidate(true,.8,.8,2,3)});
    check(best_training_candidate(values)==2,"Training selection excludes failed non-F1 gates before maximizing F1");
    values.push_back(candidate(true,.8,.9,2,3));check(best_training_candidate(values)==3,"Training tie uses Dice");
    values.push_back(candidate(true,.8,.9,1,3));values.push_back(candidate(true,.8,.9,1,1.5));check(best_training_candidate(values)==5,"Training tie uses lower sigma then prominence");
    values.push_back(values[5]);check(best_training_candidate(values)==5,"Identical training ties retain declared grid order");
    check(!best_training_candidate(Json::array({candidate(false,1,1,0,.5)})),"No eligible training recipe is explicit, not fallback selection");
}
void analytical(const fs::path& root){
    const Labels a{4,1,{1,1,2,2}},same{4,1,{2,2,1,1}},split{4,1,{1,2,3,3}},merge{4,1,{1,1,1,1}},empty{4,1,{0,0,0,0}};
    auto m=compare(a,same);check(m.true_positive==2 && !m.false_positive && !m.false_negative && !m.splits && !m.merges,"Label numbers do not choose instance matching");expect_close(m.f1,1);expect_close(m.dice,1);expect_close(*m.mean_iou,1);
    m=compare(a,split);check(m.true_positive==2 && m.false_positive==1 && !m.false_negative && m.splits==1 && !m.merges,"Equal-half IoU matching, split and unmatched fragment");expect_close(m.f1,.8);expect_close(m.split_rate,.5);expect_close(*m.mean_iou,.75);expect_close(m.dice,1);
    m=compare(a,merge);check(m.true_positive==1 && m.false_negative==1 && !m.false_positive && m.merges==1,"Merged instance cannot match both references");expect_close(m.f1,2./3);expect_close(m.merge_rate,1);
    m=compare(empty,empty);expect_close(m.f1,1);expect_close(m.dice,1);check(!m.mean_iou,"Empty match IoU is absent");
    m=compare(a,empty);expect_close(m.f1,0);expect_close(m.dice,0);check(m.false_negative==2,"Missing foreground remains false negatives");m=compare(empty,a);check(m.false_positive==2,"Extra foreground remains false positives");
    m=compare({3,3,{0,0,0,0,1,0,0,0,0}},{3,3,{1,0,0,0,0,0,0,0,0}});check(m.reference_border==0 && m.prediction_border==1 && m.true_positive==0,"Border objects retained, disjoint instance unmatched");
    expect_close(*median({4,1,3,2}),2.5);check(!median({}),"Empty median is absent");
    std::vector<unsigned char> rgba={7,99,0,0,0,0,0,255,9,0,0,255,0,0,0,0,7,13,55,255,9,0,0,255};
    const auto expected=equal_value_components(3,2,rgba);check(expected.pixels==std::vector<std::uint32_t>{1,0,2,0,1,2},"Equal-value diagonal8-connectivity ignores other channels and alpha");
    auto disconnected=rgba;disconnected[16]=8;check(areas(equal_value_components(3,2,disconnected)).size()==4,"Touching different first-channel values remain separate");
    std::string rows;for(unsigned y=0;y<2;++y){rows+=char(0);rows.append(reinterpret_cast<const char*>(rgba.data()+12*y),12);}const auto png=nucls_png_fixture::png(3,2,rows,6);
    const auto file=root/"rgba.png";write(file,png);check(annotation(detail::WorkflowInput(file.u8string(),32768)).pixels==expected.pixels,"Independent PNG decodes to known instances");
    auto broken=png;broken[broken.size()-5]^=1;write(root/"bad-crc.png",broken);rejects([&]{annotation(detail::WorkflowInput((root/"bad-crc.png").u8string(),32768));});
    write(root/"rgb.png",nucls_png_fixture::png(1,1,std::string(4,'\0')));rejects([&]{annotation(detail::WorkflowInput((root/"rgb.png").u8string(),32768));});
    rejects([&]{compare(a,{1,1,{1}});});rejects([&]{areas({1,1,{10001}});});
    Samples expected_values;expected_values.add(10);expected_values.add(20);RawCellCompartment c;c.pixels=c.valid_pixels=2;c.channels.resize(1);auto& v=c.channels[0];v.finite_samples=2;v.sum=30;v.mean=15;v.sample_standard_deviation=std::sqrt(50.);verify_samples(expected_values,c,1e-12);v.mean=16;rejects([&]{verify_samples(expected_values,c,1e-12);});
}
const Json& admitted(const Json& files,const detail::WorkflowInput& input){
    for(const auto& file:files)if(file.at("filename")==fs::u8path(input.path()).filename().u8string()){
        check(file.at("material").at("category")=="admitted_open" && file.at("license")=="CC0-1.0","Evaluation input is not admitted CC0 material");
        check(file.at("sha256")==input.sha256() && file.at("bytes")==input.bytes(),"Evaluation bytes differ from preselected admitted input");return file;}
    throw std::invalid_argument("Input is absent from the selected training/validation inventory");
}
struct Panel {
    detail::WorkflowInput image,mask,inventory;
    Json manifest;Labels truth;std::unique_ptr<Document> doc;RawRegionRequest region;
    std::optional<RawPixelBuffer> raw;std::vector<Samples> reference;
    Panel(const std::string& i,const std::string& m,const std::string& f,bool training):
        image(i,4u*1024*1024),mask(m,4u*1024*1024),inventory(f,2u*1024*1024),manifest(Json::parse(detail::read_workflow_input(inventory))){
        const auto& section=manifest.at("fluorescence_evaluation");const auto& files=(training?section.at("training"):section).at("files");admitted(files,image);admitted(files,mask);
        truth=annotation(mask);doc=std::make_unique<Document>(open_tiff(image.path()));const auto metadata=doc->server()->raw_metadata();
        check(metadata && metadata->sample_type==SampleType::UInt16 && metadata->channels==1 && metadata->width==truth.width && metadata->height==truth.height,"Pinned single-channel uint16 panel axes differ");
        region.region={0,0,0,metadata->width,metadata->height};raw=doc->server()->read_raw(region);
        check(std::all_of(raw->valid().begin(),raw->valid().end(),[](auto v){return v==1;}),"Pinned raw oracle has missing samples");
        reference.resize(areas(truth).size());const auto& values=std::get<std::vector<std::uint16_t>>(raw->storage());
        for(std::size_t at=0;at<values.size();++at)if(const auto label=truth.pixels[at])reference.at(label).add(values[at]);
    }
    void verify()const{image.verify(true);mask.verify(true);inventory.verify(true);}
};
Json evaluate(Panel& panel,const Json& protocol){
    check(protocol.at("schema")=="qpx.bbbc039-pilot/1" || protocol.at("schema")=="qpx.bbbc039-pilot/2","Unknown evaluation protocol");
    check(protocol.at("acceptance")==panel.manifest.at("fluorescence_evaluation").at("protocol").at("acceptance"),"Follow-up must retain frozen acceptance gates");
    check(protocol.at("matching").at("iou_minimum")==.5 && protocol.at("matching").at("significant_overlap_fraction")==.1,"Unsupported matching policy");
    const auto& parameters=protocol.at("segmentation");check(parameters.at("threshold_method")=="otsu256" && parameters.at("units")=="pixels","Unsupported segmentation policy");
    auto& doc=*panel.doc;const auto& truth=panel.truth;const auto& r=panel.region;FluorescenceCellOptions options;options.threshold_method=FluorescenceThreshold::Otsu256;
    options.segmentation_channel=parameters.at("segmentation_channel");options.sigma=parameters.at("sigma");options.peak_prominence=parameters.at("peak_prominence");options.min_nucleus_area=parameters.at("min_nucleus_area");options.cell_expansion=parameters.at("cell_expansion");options.work_bytes=parameters.at("work_bytes");
    const std::array<std::uint32_t,2> context=parameters.at("context");const FluorescenceCellRequest request(doc,r,0,options,context);const auto run=detect_fluorescence_cells(request);const auto& result=run->raster();
    check(result.width==truth.width && result.height==truth.height,"Evaluation requires original full-image lattice");const Labels predicted{truth.width,truth.height,result.nuclei};const auto metrics=compare(truth,predicted);
    const auto& values=std::get<std::vector<std::uint16_t>>(panel.raw->storage());const auto& reference=panel.reference;
    std::vector<Samples> nuclear(result.measurements.size()+1),body(nuclear.size()),cyto(nuclear.size());
    for(std::size_t i=0;i<values.size();++i){const auto n=result.nuclei[i],b=result.bodies[i];if(n)nuclear.at(n).add(values[i]);if(b){body.at(b).add(values[i]);if(!n)cyto.at(b).add(values[i]);}}
    const auto& gates=protocol.at("acceptance");const auto numeric=gates.at("native_measurement_relative_tolerance").get<double>();
    for(const auto& m:result.measurements){verify_samples(nuclear.at(m.label),m.nucleus,numeric);verify_samples(body.at(m.label),m.cell,numeric);
        if(cyto.at(m.label).count){check(bool(m.cytoplasm),"Native cytoplasm unexpectedly absent");verify_samples(cyto.at(m.label),*m.cytoplasm,numeric);}else check(!m.cytoplasm,"Native empty cytoplasm not absent");}
    Json matches=Json::array();std::vector<double> relative;std::size_t unavailable=0;
    for(const auto& pair:metrics.matches){const auto a=reference.at(pair.reference).mean(),b=nuclear.at(pair.prediction).mean();std::optional<double> error;if(a!=0)error=std::abs(b-a)/std::abs(a);else if(b==0)error=0;
        if(error)relative.push_back(*error);else ++unavailable;matches.push_back({{"reference",pair.reference},{"prediction",pair.prediction},{"iou",pair.iou},{"reference_raw_mean",a},{"prediction_raw_mean",b},{"relative_mean_error",error?Json(*error):Json(nullptr)}});}
    const auto median_error=median(relative);const bool other_passed=metrics.dice>=gates.at("foreground_dice_minimum").get<double>() && metrics.split_rate<=gates.at("split_rate_maximum").get<double>() && metrics.merge_rate<=gates.at("merge_rate_maximum").get<double>() && median_error && !unavailable && *median_error<=gates.at("matched_mean_relative_error_median_maximum").get<double>();
    const bool passed=other_passed && metrics.f1>=gates.at("object_f1_minimum").get<double>();
    require_current_fluorescence_cells(doc,request,*run);
    return {{"schema","qpx.fluorescence-pilot-result/1"},{"protocol",protocol},{"image_sha256",panel.image.sha256()},{"annotation_sha256",panel.mask.sha256()},{"inventory_sha256",panel.inventory.sha256()},{"run_sha256",run->sha256()},{"resolved_threshold",result.resolved_threshold},
        {"reference_instances",metrics.references},{"predicted_instances",metrics.predictions},{"true_positive",metrics.true_positive},{"false_positive",metrics.false_positive},{"false_negative",metrics.false_negative},{"foreground_dice",metrics.dice},{"precision",metrics.precision},{"recall",metrics.recall},{"object_f1",metrics.f1},{"mean_matched_iou",metrics.mean_iou?Json(*metrics.mean_iou):Json(nullptr)},
        {"splits",metrics.splits},{"merges",metrics.merges},{"split_rate",metrics.split_rate},{"merge_rate",metrics.merge_rate},{"reference_border_instances",metrics.reference_border},{"prediction_border_instances",metrics.prediction_border},{"median_matched_mean_relative_error",median_error?Json(*median_error):Json(nullptr)},{"unavailable_matched_mean_errors",unavailable},{"native_measurements_verified",true},{"non_f1_gates_passed",other_passed},{"pilot_passed",passed},{"matches",matches}};
}
Json train(Panel& panel){
    const auto& section=panel.manifest.at("fluorescence_evaluation");const auto& grid=section.at("training").at("recipe_selection");
    check(grid.at("schema")=="qpx.bbbc039-training-grid/1" && grid.at("sigma_pixels")==Json::array({0,1,2}) && grid.at("peak_prominence_pixels")==Json::array({.5,1.5,3}),"Training grid differs from predeclared finite candidates");
    Json candidates=Json::array();for(const double sigma:grid.at("sigma_pixels"))for(const double prominence:grid.at("peak_prominence_pixels")){
        auto protocol=section.at("protocol");protocol["segmentation"]["sigma"]=sigma;protocol["segmentation"]["peak_prominence"]=prominence;candidates.push_back(evaluate(panel,protocol));}
    const auto best=best_training_candidate(candidates);
    return {{"schema","qpx.fluorescence-training-selection/1"},{"grid",grid},{"image_sha256",panel.image.sha256()},{"annotation_sha256",panel.mask.sha256()},{"inventory_sha256",panel.inventory.sha256()},
        {"selected_index",best?Json(*best):Json(nullptr)},{"selected_protocol",best?candidates[*best].at("protocol"):Json(nullptr)},{"candidates",candidates}};
}
int main(int argc,char** argv){try{
    const auto root=fs::current_path()/("fluorescence-evaluation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Unique evaluation fixture directory");analytical(root);selection_oracle();
    if(argc==4 || (argc==5 && (std::string(argv[1])=="--training" || std::string(argv[1])=="--followup"))){
        const bool training=argc==5 && std::string(argv[1])=="--training",followup=argc==5 && !training;const auto first=argc==5?2:1;
        Panel panel(argv[first],argv[first+1],argv[first+2],training);
        const auto report=training?train(panel):evaluate(panel,panel.manifest.at("fluorescence_evaluation").at(followup?"followup_protocol":"protocol"));panel.verify();std::cout<<report.dump(2)<<'\n';
        return training?(report.at("selected_index").is_null()?1:0):(report.at("pilot_passed").get<bool>()?0:1);
    }
    if(argc!=1)throw std::invalid_argument("Usage: qupathxx_fluorescence_evaluation_tests [--training | --followup] IMAGE MASK FIXTURE_MANIFEST; no args runs independent self-tests");
    std::cout<<"Independent evaluation and finite training-selection oracles passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
