#include "qupathxx/marker_rules.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/source_identity.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <cmath>
#include <future>
#include <iostream>
#include <filesystem>
#include <numeric>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class E=std::exception,class F>void rejects(F f){try{f();}catch(const E&){return;}throw std::runtime_error("Expected marker refusal");}
MarkerRule simple(){MarkerRule p;p.gates={{"A"}};p.nodes={{MarkerOperation::Gate,0,{}}};return p;}
MarkerRule pair(MarkerOperation operation){auto p=simple();p.gates[0].channel=1;p.gates[0].threshold=44;auto b=p.gates[0];b.name="B";b.channel=2;b.comparison=MarkerComparison::Less;b.threshold=0;p.gates.push_back(b);p.nodes.push_back({MarkerOperation::Gate,1,{}});p.nodes.push_back({operation,0,{0,1}});return p;}
void pure(){
    auto g=simple().gates[0];check(evaluate_marker_gate(g,{0,1})==MarkerTruth::Negative,"Zero is a measured value, not missing");g.comparison=MarkerComparison::GreaterEqual;check(evaluate_marker_gate(g,{0,1})==MarkerTruth::Positive,"Inclusive threshold edge");
    g.comparison=MarkerComparison::Less;check(evaluate_marker_gate(g,{-1,1})==MarkerTruth::Positive && evaluate_marker_gate(g,{0,1})==MarkerTruth::Negative,"Strict signed threshold");g.comparison=MarkerComparison::LessEqual;check(evaluate_marker_gate(g,{0,1})==MarkerTruth::Positive,"Inclusive less-than edge");
    for(const auto value:std::vector<MarkerObservation>{{std::nullopt,1},{NAN,9},{INFINITY,9},{0,0}})check(evaluate_marker_gate(g,value)==MarkerTruth::Unknown,"Absent/nonfinite/no finite samples remain Unknown");
    g.minimum_finite_samples=2;check(evaluate_marker_gate(g,{0,1})==MarkerTruth::Unknown,"Explicit minimum finite samples");
    g.statistic=MarkerStatistic::SampleSD;g.minimum_finite_samples=1;check(evaluate_marker_gate(g,{0,1})==MarkerTruth::Unknown,"Sample SD needs at least two finite samples");
    for(const auto operation:{MarkerOperation::And,MarkerOperation::Or}){auto p=pair(operation);for(const auto a:{MarkerTruth::Negative,MarkerTruth::Positive,MarkerTruth::Unknown})for(const auto b:{MarkerTruth::Negative,MarkerTruth::Positive,MarkerTruth::Unknown}){
        const auto value=[](MarkerTruth t,bool less){return MarkerObservation{t==MarkerTruth::Unknown?std::optional<double>{}:std::optional<double>{t==MarkerTruth::Positive?(less?-1.:45.):(less?1.:44.)},1};};
        const auto result=evaluate_marker_rule(p,{value(a,false),value(b,true)});MarkerTruth expected;
        if(operation==MarkerOperation::And)expected=a==MarkerTruth::Negative || b==MarkerTruth::Negative?MarkerTruth::Negative:a==MarkerTruth::Unknown || b==MarkerTruth::Unknown?MarkerTruth::Unknown:MarkerTruth::Positive;
        else expected=a==MarkerTruth::Positive || b==MarkerTruth::Positive?MarkerTruth::Positive:a==MarkerTruth::Unknown || b==MarkerTruth::Unknown?MarkerTruth::Unknown:MarkerTruth::Negative;
        check(result.gates==std::vector<MarkerTruth>{a,b} && result.phenotype==expected,"Independent complete three-state AND/OR truth tables");}}
    check(marker_measurement_key(simple().gates[0])=="fluorescence.nucleus.C0.mean","Native channel/compartment/statistic key");
    for(int i=0;i<8;++i){auto p=simple();if(i==0)p.gates[0].threshold=NAN;else if(i==1)p.gates[0].comparison=MarkerComparison(9);else if(i==2)p.gates[0].units=MarkerUnits::SourceSampleSum;else if(i==3)p.domain="display-rgb";else if(i==4)p.unknown_class=p.negative_class;else if(i==5)p.gates[0].minimum_finite_samples=0;else if(i==6)p.nodes[0].inputs={0};else p.gates[0].channel=65536;rejects<std::invalid_argument>([&]{p.validate();});}
    auto p=pair(MarkerOperation::And);p.nodes[2].inputs={1,2};rejects<std::invalid_argument>([&]{p.validate();});p=pair(MarkerOperation::Or);p.nodes[2].inputs={0,0};rejects<std::invalid_argument>([&]{p.validate();});p=pair(MarkerOperation::And);p.nodes.pop_back();rejects<std::invalid_argument>([&]{p.validate();});rejects<std::invalid_argument>([&]{evaluate_marker_rule(simple(),{});});
}
void storage(const fs::path& root){
    Document doc(Image::demo());using V=ImageDataValue;doc.set_image_data_properties(std::make_shared<ImageDataProperties>(V::Map{{"user-note",V{std::string("keep")}}}));const auto before=doc.image_data_properties();
    auto p=pair(MarkerOperation::Or);p.gates[1].threshold=-0.;const auto id=keep_marker_rule(doc,p);check(id==p.sha256() && marker_rule_ids(doc)==std::vector<std::string>{id},"Exact native rule identity/list");
    const auto restored=load_marker_rule(doc,id);check(restored.sha256()==id && std::signbit(restored.gates[1].threshold),"Native recipe preserves threshold IEEE sign bit");rejects<WorkflowOutputExists>([&]{keep_marker_rule(doc,p);});
    doc.undo();check(doc.image_data_properties()->equals(*before),"Keep rule is one undo");doc.redo();remove_marker_rule(doc,id);check(doc.image_data_properties()->equals(*before),"Remove preserves unrelated metadata");doc.undo();
    auto values=doc.image_data_properties()->values();auto& record=std::get<V::Map>(std::get<V::Map>(values.at("qupathxx.marker-rules/1").value).at(id).value);record["schema"]=V{std::string("qpx.marker-rule/2")};doc.set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(values)));rejects<std::invalid_argument>([&]{load_marker_rule(doc,id);});doc.undo();
    values=doc.image_data_properties()->values();auto& gates=std::get<V::List>(std::get<V::Map>(std::get<V::Map>(values.at("qupathxx.marker-rules/1").value).at(id).value).at("gates").value);std::get<V::Map>(gates[0].value)["threshold"]=V{99.};doc.set_image_data_properties(std::make_shared<ImageDataProperties>(std::move(values)));rejects<std::invalid_argument>([&]{marker_rule_ids(doc);});doc.undo();
    if(has_project()){auto owned=std::make_shared<Document>(doc);Project project;project.add_document(owned);project.save_new((root/"rules.qpx").u8string());const auto open=Project::open((root/"rules.qpx").u8string())->document(1);check(load_marker_rule(*open,id).sha256()==id && open->image_data_properties()->equals(*doc.image_data_properties()),"Exact rule and unrelated metadata survive save/reopen");}
    auto maximum=simple();maximum.gates.clear();maximum.nodes.clear();for(unsigned i=0;i<64;++i){auto gate=p.gates[0];gate.name="G"+std::to_string(i);maximum.gates.push_back(gate);maximum.nodes.push_back({MarkerOperation::Gate,i,{}});}std::vector<std::uint32_t> all(64);std::iota(all.begin(),all.end(),0);maximum.nodes.push_back({MarkerOperation::And,0,all});while(maximum.nodes.size()<256)maximum.nodes.push_back({MarkerOperation::Or,0,{0,std::uint32_t(maximum.nodes.size()-1)}});const auto maxid=keep_marker_rule(doc,maximum);check(load_marker_rule(doc,maxid).sha256()==maxid,"Declared maximal gate/node graph fits native property codec");
}
class Reader final:public ImageServer {
    ImageMetadata display_;std::shared_ptr<RawImageMetadata> raw_=std::make_shared<RawImageMetadata>();std::shared_ptr<TiffView> view_=std::make_shared<TiffView>();std::shared_ptr<const ReaderSource> source_=make_reader_source("marker-reference","","memory");
public:
    mutable unsigned raw_reads=0;bool shut=false;
    Reader(){display_.width=15;display_.height=9;display_.levels={{15,9,1}};raw_->width=15;raw_->height=9;raw_->channels=3;raw_->sample_type=SampleType::Float64;raw_->levels=display_.levels;raw_->channel_names={"DNA","A","B"};}
    const ImageMetadata& metadata()const override{return display_;}
    std::string reader_id()const override{return "marker-reference";}
    std::shared_ptr<const ReaderSource> reader_source()const override{return source_;}
    std::shared_ptr<const TiffView> tiff_view()const override{return view_;}
    std::shared_ptr<const RawImageMetadata> raw_metadata(const std::atomic<bool>*)const override{return raw_;}
    PixelBuffer read_region(const RegionRequest&)const override{throw std::runtime_error("Marker work must not read RGB");}
    RawPixelBuffer read_raw(const RawRegionRequest& r,const std::atomic<bool>*)const override{++raw_reads;RawPixelBuffer out(r.region.width,r.region.height,r.channels,SampleType::Float64);std::fill(out.writable_valid().begin(),out.writable_valid().end(),1);auto& values=std::get<std::vector<double>>(out.writable_storage());
        for(std::int64_t y=0;y<r.region.height;++y)for(std::int64_t x=0;x<r.region.width;++x){const auto xx=x+r.region.x,yy=y+r.region.y;for(unsigned c=0;c<r.channels;++c){const auto channel=r.first_channel+c;values[(std::size_t(y)*r.region.width+x)*r.channels+c]=channel==0?(yy>=3 && yy<=5 && ((xx>=3 && xx<=5) || (xx>=9 && xx<=11))?100:0):channel==1?double(xx+10*yy):xx<7?NAN:-double(xx+10*yy);}}return out;}
    void close()override{shut=true;}bool closed()const override{return shut;}
};
std::vector<std::uint64_t> cells(Document& doc,RawRegionRequest region,FluorescenceCellOptions options){const FluorescenceCellRequest request(doc,region,0,options,{0,0});return publish_fluorescence_cells(doc,request,*trace_fluorescence_cells(detect_fluorescence_cells(request)));}
void workflow(){
    auto reader=std::make_shared<Reader>();Document doc(reader);RawRegionRequest region;region.region={0,0,0,15,9};region.channels=3;FluorescenceCellOptions p;p.threshold=50;p.cell_expansion=1.5;const auto ids=cells(doc,region,p);check(ids.size()==2,"Independent two-blob source");const auto reads=reader->raw_reads;
    const auto rule=pair(MarkerOperation::And);doc.set_object_edit_state(ids[0],true,0x123456,.8);const auto raw=doc.annotation(ids[0]).properties;const auto geometry=doc.annotation(ids[0]).geometry;
    MarkerRuleRequest request(doc,{ids[1],ids[0]},rule);const auto result=std::async(std::launch::async,[request]{return preview_marker_rule(request);}).get();
    check(result->counts().negative==1 && result->counts().positive==1 && !result->counts().unknown && result->gate_counts()[1].unknown==1,"Native raw facts and Kleene AND; NaN channel is Unknown");
    check(result->cohort(MarkerTruth::Negative)==std::vector<std::uint64_t>{ids[0]} && result->gate_cohort(1,MarkerTruth::Unknown)==std::vector<std::uint64_t>{ids[0]} && result->gate_states().size()==4,"Native root/gate cohorts and flat batched states");
    for(const auto statistic:{MarkerStatistic::Mean,MarkerStatistic::Sum,MarkerStatistic::Minimum,MarkerStatistic::Maximum,MarkerStatistic::SampleSD}){auto p=simple();auto& g=p.gates[0];g.channel=1;g.statistic=statistic;g.units=statistic==MarkerStatistic::Sum?MarkerUnits::SourceSampleSum:MarkerUnits::SourceSample;g.threshold=statistic==MarkerStatistic::Mean?44:statistic==MarkerStatistic::Sum?396:statistic==MarkerStatistic::Minimum?33:statistic==MarkerStatistic::Maximum?55:0;
        const auto stats=preview_marker_rule(MarkerRuleRequest(doc,ids,p));check(stats->counts().positive==(statistic==MarkerStatistic::SampleSD?2:1) && stats->counts().unknown==0,"All declared native raw statistics select the independent scalar domain");}
    auto or_rule=pair(MarkerOperation::Or);const auto or_result=preview_marker_rule(MarkerRuleRequest(doc,ids,or_rule));check(or_result->counts().unknown==1 && or_result->counts().positive==1,"Unknown OR false is not Negative");
    const auto stored=keep_marker_rule(doc,rule);check(request.matches(doc),"Keeping a rule does not invalidate unchanged observation capture");
    MarkerRuleRequest another(doc,ids,rule);check(another.input_sha256()==request.input_sha256(),"Scientific identity differs from private capture authority");rejects<WorkflowInputChanged>([&]{apply_marker_rule(doc,another,*result);});
    std::atomic<bool> cancelled{true};rejects<WorkflowCancelled>([&]{MarkerRuleRequest stopped(doc,ids,rule,64u*1024*1024,&cancelled);});rejects<WorkflowCancelled>([&]{preview_marker_rule(request,&cancelled);});rejects<WorkflowCancelled>([&]{apply_marker_rule(doc,request,*result,&cancelled);});
    check(!verify_marker_rule_publication(doc,*result),"Unapplied classes are not a verified publication");
    check(marker_cell_ids(doc)==ids,"Native all-cell cohort selection");
    check(apply_marker_rule(doc,request,*result)==ids,"Native compound classification reports changed IDs");
    check(verify_marker_rule_publication(doc,*result),"Post-Apply verification permits own revision changes");
    const auto a=doc.annotation(ids[0]);check(a.class_name==rule.negative_class && a.geometry.equals(geometry) && fluorescence_cell_observations_intact(a) && a.properties->measurements()==raw->measurements() && a.properties->metadata()==raw->metadata(),"Publication preserves geometry and original observations");
    check(a.locked() && !a.color_rgb() && !a.class_probability(),"Existing class policy preserves lock and clears explicit color/probability");doc.undo();check(doc.annotation(ids[0]).class_name.empty() && doc.annotation(ids[1]).class_name.empty() && doc.annotation(ids[0]).properties->equals(*raw),"One undo restores class/edit state for entire cohort");doc.redo();
    const MarkerRuleRequest replay(doc,ids,load_marker_rule(doc,stored));const auto fresh=preview_marker_rule(replay);check(fresh->sha256()==result->sha256() && apply_marker_rule(doc,replay,*fresh).empty(),"Saved exact rule replay is scientific-identical and already-applied classes are a no-op");
    doc.classify_objects({ids[0]},"Manual override");check(!verify_marker_rule_publication(doc,*result),"Changed class rejects historical publication verification");doc.undo();check(verify_marker_rule_publication(doc,*result),"Undo restores expected class/fact publication");
    doc.rename_annotation(ids[0],"Display name");check(!replay.matches(doc),"Selected edit makes old preview stale");rejects<WorkflowInputChanged>([&]{apply_marker_rule(doc,replay,*fresh);});
    check(verify_marker_rule_publication(doc,*result),"Cosmetic rename preserves observed publication verification");
    MarkerRuleRequest current(doc,ids,rule);check(preview_marker_rule(current)->sha256()==result->sha256(),"Cosmetic rename does not change observed scientific identity");
    auto changed=doc.annotation(ids[0]).properties->metadata();changed["fluorescence.measurement_domain"]="rgb-preview";doc.set_object_properties(ids[0],std::make_shared<ObjectProperties>(changed,raw->measurements()));rejects<std::invalid_argument>([&]{MarkerRuleRequest bad(doc,ids,rule);});doc.undo();
    doc.update_roi(ids[0],Roi::rectangle({0,0,15,9}));rejects<std::invalid_argument>([&]{MarkerRuleRequest edited(doc,ids,rule);});doc.undo();
    auto wide=simple();wide.gates.clear();wide.nodes.clear();for(unsigned i=0;i<64;++i){auto gate=rule.gates[0];gate.name="G"+std::to_string(i);wide.gates.push_back(gate);wide.nodes.push_back({MarkerOperation::Gate,i,{}});}std::vector<std::uint32_t> inputs(64);std::iota(inputs.begin(),inputs.end(),0);wide.nodes.push_back({MarkerOperation::And,0,inputs});std::vector<std::uint64_t> many(100000);std::iota(many.begin(),many.end(),1);
    rejects<WorkflowCaptureLimit>([&]{MarkerRuleRequest bounded(doc,many,wide,4u*1024*1024);});
    rejects<std::invalid_argument>([&]{MarkerRuleRequest duplicate(doc,{ids[0],ids[0]},rule);});rejects<std::invalid_argument>([&]{MarkerRuleRequest tiny(doc,ids,rule,1024);});
    MarkerRuleRequest empty(doc,{},rule);const auto no_rows=preview_marker_rule(empty);check(no_rows->rows().empty() && no_rows->counts().unknown==0 && apply_marker_rule(doc,empty,*no_rows).empty(),"Empty cohort is a valid native no-op");
    check(std::async(std::launch::async,[&]{try{current.matches(doc);}catch(const std::logic_error&){return true;}return false;}).get(),"Document matches stays on owner thread");
    MarkerRuleRequest bound(doc,ids,rule);doc=Document(reader);check(!bound.matches(doc),"Same-address Document replacement rejects old capture");check(reader->raw_reads==reads,"Marker capture/evaluation/keep/classification never read pixels");
}
void saved_observations(const fs::path& root){if(!has_tiff() || !has_project())return;const auto file=root/"source.tif";qpx_test::raw_pixel_tiff_fixture(file,15,9);auto doc=std::make_shared<Document>(open_tiff(file.u8string()));RawRegionRequest region;region.region={0,0,0,15,9};region.channels=2;FluorescenceCellOptions options;options.threshold=100;options.cell_expansion=0;const auto ids=cells(*doc,region,options);
    auto p=simple();p.gates[0].compartment=MarkerCompartment::Cytoplasm;const auto missing=preview_marker_rule(MarkerRuleRequest(*doc,ids,p));check(missing->counts().unknown==ids.size(),"Empty cytoplasm is Unknown, not a zero-intensity Negative");
    p=simple();p.gates[0].channel=8;check(preview_marker_rule(MarkerRuleRequest(*doc,ids,p))->counts().unknown==ids.size(),"Unmeasured original channel remains Unknown");
    p=simple();p.gates[0].threshold=100;const auto rule=keep_marker_rule(*doc,p);const MarkerRuleRequest request(*doc,ids,p);const auto result=preview_marker_rule(request);apply_marker_rule(*doc,request,*result);Project project;project.add_document(doc);project.save_new((root/"classified.qpx").u8string());const auto reopened=Project::open((root/"classified.qpx").u8string())->document(1);
    const MarkerRuleRequest replay(*reopened,ids,load_marker_rule(*reopened,rule));check(preview_marker_rule(replay)->sha256()==result->sha256() && reopened->annotation(ids[0]).class_name==doc->annotation(ids[0]).class_name && fluorescence_cell_observations_intact(reopened->annotation(ids[0])),"Saved rule/class/raw-fact associations replay exactly after project reopen");
}
int main(){try{const auto root=fs::current_path()/("marker-rules-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Unique marker test directory");pure();storage(root);if(has_geos()){workflow();saved_observations(root);}else std::cout<<"Marker source/cell integration requires optional GEOS; pure rule/codec tests run\n";
    std::cout<<"Marker rules: typed units/thresholds, complete three-state logic, DAG/codec integrity, native cohorts/classification/undo, observation-only snapshot and exact replay passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
